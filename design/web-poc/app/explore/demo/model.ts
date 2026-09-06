export type MemberId = 'aki' | 'ren' | 'mei';
export type DemoMode =
  | 'idle'
  | 'receiving'
  | 'requesting'
  | 'transmitting'
  | 'busy';
export type DemoState = {
  selected: MemberId;
  remote: MemberId | null;
  mode: DemoMode;
  held: boolean;
  connected: boolean;
  joined: boolean;
  stale: boolean;
  muted: boolean;
  volume: number;
  target: 'all' | MemberId;
  positions: Record<MemberId, number>;
  elapsed: number;
};
export const demoMembers = [
  {
    id: 'aki',
    name: 'AKI',
    japanese: 'アキ',
    color: '#c6ff00',
    colorName: 'ライム',
  },
  {
    id: 'ren',
    name: 'REN',
    japanese: 'レン',
    color: '#ff16a5',
    colorName: 'ピンク',
  },
  {
    id: 'mei',
    name: 'MEI',
    japanese: 'メイ',
    color: '#397dff',
    colorName: 'ブルー',
  },
] as const;
export function initialDemo(id: number, preview = false): DemoState {
  const selected = id === 57 ? 'ren' : id === 59 ? 'mei' : 'aki';
  const quiet = id === 25 || id === 26 || id === 27;
  return {
    selected,
    remote: quiet ? null : selected,
    mode: id === 26 && preview ? 'transmitting' : quiet ? 'idle' : 'receiving',
    held: false,
    connected: id !== 27,
    joined: true,
    stale: id === 28 || id === 60,
    muted: false,
    volume: 70,
    target: 'all',
    positions: { aki: 120, ren: -85, mei: -240 },
    elapsed: 0,
  };
}
export type DemoAction =
  | { type: 'speaker'; id: MemberId }
  | {
      type:
        | 'quiet'
        | 'down'
        | 'grant'
        | 'up'
        | 'tick'
        | 'mute'
        | 'leave'
        | 'join';
    }
  | { type: 'connect' | 'stale'; value: boolean }
  | { type: 'volume'; value: number }
  | { type: 'position'; id: MemberId; value: number }
  | { type: 'target'; value: DemoState['target'] }
  | { type: 'reset'; id: number };
const stopped = { held: false, mode: 'idle' as const, elapsed: 0 };
export function demoReducer(s: DemoState, a: DemoAction): DemoState {
  switch (a.type) {
    case 'speaker':
      return s.connected && s.joined
        ? { ...s, ...stopped, selected: a.id, remote: a.id, mode: 'receiving' }
        : s;
    case 'quiet':
      return { ...s, ...stopped, remote: null };
    case 'down':
      if (s.held || !s.connected || !s.joined) return s;
      return {
        ...s,
        held: true,
        mode: s.remote ? 'busy' : 'requesting',
        elapsed: 0,
      };
    case 'grant':
      return s.held &&
        s.mode === 'requesting' &&
        s.connected &&
        s.joined &&
        !s.remote
        ? { ...s, mode: 'transmitting' }
        : s;
    case 'up':
      return {
        ...s,
        ...stopped,
        mode: s.remote && s.connected && s.joined ? 'receiving' : 'idle',
      };
    case 'connect':
      return { ...s, ...stopped, connected: a.value, remote: null };
    case 'stale':
      return { ...s, stale: a.value };
    case 'position':
      return Number.isFinite(a.value)
        ? {
            ...s,
            positions: {
              ...s.positions,
              [a.id]: Math.max(-999, Math.min(999, Math.round(a.value))),
            },
          }
        : s;
    case 'volume':
      return Number.isFinite(a.value)
        ? { ...s, volume: Math.max(0, Math.min(100, Math.round(a.value))) }
        : s;
    case 'mute':
      return { ...s, muted: !s.muted };
    case 'target':
      return { ...s, ...stopped, remote: null, target: a.value };
    case 'leave':
      return { ...s, ...stopped, joined: false, remote: null };
    case 'join':
      return { ...s, ...stopped, joined: true, connected: true, remote: null };
    case 'reset':
      return initialDemo(a.id);
    case 'tick':
      return s.mode === 'transmitting' ? { ...s, elapsed: s.elapsed + 1 } : s;
  }
}
export function demoView(s: DemoState, colorOnly = false) {
  const peer = demoMembers.find((p) => p.id === s.selected)!;
  const other =
    demoMembers.find((p) => p.id !== s.selected && s.positions[p.id] < 0) ??
    demoMembers.find((p) => p.id !== s.selected)!;
  const third = demoMembers.find(
    (p) => p.id !== s.selected && p.id !== other.id,
  )!;
  const offline = !s.connected || !s.joined;
  const transmitting = s.mode === 'transmitting';
  const voice = offline
    ? '未接続'
    : s.mode === 'busy'
      ? '使用中'
      : s.mode === 'requesting'
        ? '接続待ち'
        : transmitting
          ? '送信中'
          : s.remote
            ? '受信中'
            : '待受';
  const idLabel = (id: MemberId) => {
    const m = demoMembers.find((p) => p.id === id)!;
    return colorOnly ? m.colorName : m.name;
  };
  const direction = (id: MemberId) =>
    offline || s.stale
      ? '位置未更新'
      : s.positions[id] === 0
        ? 'すぐそば'
        : s.positions[id] > 0
          ? '前方'
          : '後方';
  const distance = (id: MemberId) =>
    offline || s.stale ? '—' : String(Math.abs(s.positions[id]));
  const name = offline ? '—' : idLabel(s.selected);
  const target = s.target === 'all' ? '全員' : idLabel(s.target);
  return {
    name,
    speaker: offline ? '—' : transmitting ? '自分' : idLabel(s.selected),
    stateFocus: offline
      ? '未接続'
      : s.remote
        ? idLabel(s.remote)
        : `${target}へ`,
    range: Math.max(
      250,
      Math.ceil(Math.max(...Object.values(s.positions).map(Math.abs)) / 250) *
        250,
    ),
    initial: offline ? '—' : peer.name[0],
    japanese: offline ? '—' : peer.japanese,
    distance: distance(s.selected),
    direction: direction(s.selected),
    arrow: offline || s.stale ? '—' : s.positions[s.selected] >= 0 ? '↑' : '↓',
    rearName: idLabel(other.id),
    rearInitial: other.name[0],
    rearDistance: distance(other.id),
    rearDirection: direction(other.id),
    thirdName: idLabel(third.id),
    thirdInitial: third.name[0],
    thirdDistance: distance(third.id),
    thirdDirection: direction(third.id),
    voice,
    voiceSentence: offline
      ? '再接続してください'
      : transmitting
        ? `${target}へ送信中`
        : s.mode === 'requesting'
          ? '送信を準備中'
          : s.mode === 'busy'
            ? '仲間の発話を待機'
            : s.remote
              ? `${idLabel(s.remote)}から受信中`
              : 'PTTで話せます',
    speakerLabel: s.remote ? '話している人' : '選択中の仲間',
    connection: offline
      ? '未接続'
      : s.muted || s.volume === 0
        ? '音声ミュート'
        : '音声接続中',
    count: offline ? '0' : '4',
    target: `${target}へ`,
    rx: offline ? 'OFF' : transmitting ? 'TX' : s.remote ? 'RX' : 'IDLE',
    hint: offline
      ? '再接続してください'
      : s.mode === 'busy'
        ? '離して再度押してください'
        : transmitting
          ? 'PTTを離すと終了'
          : s.remote
            ? '仲間の声が届いています'
            : '物理PTTで話す',
    positionHint:
      offline || s.stale ? '距離は確認できません' : '相対距離を表示中',
    color: peer.color,
    otherColor: other.color,
    thirdColor: third.color,
    elapsed: `${Math.floor(s.elapsed / 60)
      .toString()
      .padStart(2, '0')}:${(s.elapsed % 60).toString().padStart(2, '0')}`,
    audible: !offline && !!s.remote && !s.muted && s.volume > 0,
    offline,
  };
}
export type DemoField = keyof ReturnType<typeof demoView>;
export function demoText(s: DemoState, template: string, colorOnly = false) {
  const view = demoView(s, colorOnly);
  return template.replace(/\{(\w+)\}/g, (_, key: string) =>
    String(view[key as DemoField] ?? `{${key}}`),
  );
}
