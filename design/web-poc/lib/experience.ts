export type Design = 'circle' | 'pulse' | 'compass';
export type Page = 'people' | 'ride' | 'radar';
export type Voice = 'idle' | 'requesting' | 'talking' | 'busy';
export type Scenario = 'quiet' | 'receiving' | 'busy' | 'lost';
export type Sheet =
  | 'settings'
  | 'volume'
  | 'members'
  | 'join'
  | 'create'
  | 'leave'
  | null;
export type Peer = {
  id: string;
  name: string;
  initial: string;
  tone: string;
  distance: number;
  bearing: number;
  along: number;
  muted: boolean;
  volume: number;
};
export const peers: Peer[] = [
  {
    id: 'aki',
    name: 'AKI',
    initial: 'A',
    tone: 'blue',
    distance: 120,
    bearing: 8,
    along: 120,
    muted: false,
    volume: 70,
  },
  {
    id: 'ren',
    name: 'REN',
    initial: 'R',
    tone: 'violet',
    distance: 85,
    bearing: 184,
    along: -85,
    muted: false,
    volume: 70,
  },
  {
    id: 'yui',
    name: 'YUI',
    initial: 'Y',
    tone: 'rose',
    distance: 260,
    bearing: 26,
    along: 260,
    muted: false,
    volume: 70,
  },
  {
    id: 'kai',
    name: 'KAI',
    initial: 'K',
    tone: 'mint',
    distance: 210,
    bearing: 215,
    along: -210,
    muted: false,
    volume: 70,
  },
  {
    id: 'mei',
    name: 'MEI',
    initial: 'M',
    tone: 'orange',
    distance: 330,
    bearing: 148,
    along: -330,
    muted: false,
    volume: 70,
  },
];
export const concepts = [
  {
    id: 'circle' as Design,
    number: '01',
    title: 'Circle',
    subtitle: '仲間の声を、すぐそばに。',
    detail:
      '声を中心に、仲間が集まる。丸いアバターと穏やかな動きで、同じ場所にいる感覚を。',
    home: 'people' as Page,
  },
  {
    id: 'pulse' as Design,
    number: '02',
    title: 'Pulse',
    subtitle: '走ることに、集中する。',
    detail:
      '大きな数字、はっきりしたコントラスト。視線を短く向けるだけで、前後の仲間を把握。',
    home: 'ride' as Page,
  },
  {
    id: 'compass' as Design,
    number: '03',
    title: 'Compass',
    subtitle: '離れても、つながっている。',
    detail:
      '仲間との位置関係を、ひとつの景色に。レーダーと声を重ねて、グループの広がりを感じる。',
    home: 'radar' as Page,
  },
];
export type State = {
  design: Design;
  page: Page;
  joined: boolean;
  group: string;
  members: Peer[];
  connected: boolean;
  remote: string | null;
  voice: Voice;
  held: boolean;
  masterVolume: number;
  masterMuted: boolean;
  target: string | null;
  stale: boolean;
  sheet: Sheet;
  selected: string | null;
  joining: string | null;
  joinId: number;
  notice: string;
  scenario: Scenario;
  reduced: boolean;
};
export const initialState: State = {
  design: 'circle',
  page: 'people',
  joined: true,
  group: '瀬戸内ライド',
  members: peers.map((p) => ({ ...p })),
  connected: true,
  remote: 'aki',
  voice: 'idle',
  held: false,
  masterVolume: 60,
  masterMuted: false,
  target: null,
  stale: false,
  sheet: null,
  selected: null,
  joining: null,
  joinId: 0,
  notice: '',
  scenario: 'receiving',
  reduced: false,
};
export type Action =
  | { type: 'design'; design: Design }
  | { type: 'navigate'; page: Page }
  | { type: 'scenario'; scenario: Scenario }
  | { type: 'speaker'; id: string }
  | { type: 'ptt-down' }
  | { type: 'ptt-granted' }
  | { type: 'ptt-up' }
  | { type: 'stale'; value: boolean }
  | { type: 'sheet'; sheet: Sheet }
  | { type: 'select'; id: string | null }
  | { type: 'master-volume'; value: number }
  | { type: 'master-mute' }
  | { type: 'peer-volume'; id: string; value: number }
  | { type: 'peer-mute'; id: string }
  | { type: 'target'; id: string | null }
  | { type: 'join-start'; group: string }
  | { type: 'join-complete'; id: number }
  | { type: 'create'; group: string }
  | { type: 'leave' }
  | { type: 'notice'; message: string }
  | { type: 'reset' }
  | { type: 'reduced'; value: boolean };
const stop = { voice: 'idle' as Voice, held: false };
const clamp = (n: number) =>
  Math.max(0, Math.min(100, Number.isFinite(n) ? Math.round(n) : 0));
export function reducer(s: State, a: Action): State {
  switch (a.type) {
    case 'design':
      return {
        ...s,
        ...stop,
        design: a.design,
        page: concepts.find((c) => c.id === a.design)!.home,
        sheet: null,
        selected: null,
      };
    case 'navigate':
      return { ...s, ...stop, page: a.page };
    case 'scenario':
      return {
        ...s,
        ...stop,
        scenario: a.scenario,
        connected: a.scenario !== 'lost',
        remote:
          s.joined &&
          s.members.length &&
          (a.scenario === 'receiving' || a.scenario === 'busy')
            ? s.members[0].id
            : null,
        notice:
          a.scenario === 'lost'
            ? '接続が切れました。再接続を待っています。'
            : '',
      };
    case 'speaker':
      return s.members.some((p) => p.id === a.id) && s.connected
        ? { ...s, ...stop, remote: a.id, scenario: 'receiving' }
        : s;
    case 'ptt-down':
      if (s.held) return s;
      if (!s.joined)
        return { ...s, notice: 'グループに参加してから話せます。' };
      if (!s.connected)
        return { ...s, ...stop, notice: '再接続してから話せます。' };
      if (!s.members.length)
        return { ...s, ...stop, notice: '仲間が参加すると話せます。' };
      if (s.remote)
        return {
          ...s,
          voice: 'busy',
          held: true,
          notice: `${s.members.find((p) => p.id === s.remote)?.name ?? '仲間'}が話しています。離して、もう一度押してください。`,
        };
      return { ...s, voice: 'requesting', held: true, notice: '' };
    case 'ptt-granted':
      return s.held && s.voice === 'requesting' && s.connected && !s.remote
        ? { ...s, voice: 'talking' }
        : s;
    case 'ptt-up':
      return { ...s, ...stop };
    case 'stale':
      return { ...s, stale: a.value };
    case 'sheet':
      return {
        ...s,
        ...stop,
        sheet: a.sheet,
        selected: null,
        joining: null,
        joinId: s.joinId + 1,
        notice: '',
      };
    case 'select':
      return { ...s, ...stop, selected: a.id, sheet: null };
    case 'master-volume':
      return {
        ...s,
        masterVolume: clamp(a.value),
        masterMuted: clamp(a.value) <= 0,
      };
    case 'master-mute':
      return { ...s, masterMuted: !s.masterMuted };
    case 'peer-volume':
      return {
        ...s,
        members: s.members.map((p) =>
          p.id === a.id ? { ...p, volume: clamp(a.value) } : p,
        ),
      };
    case 'peer-mute':
      return {
        ...s,
        members: s.members.map((p) =>
          p.id === a.id ? { ...p, muted: !p.muted } : p,
        ),
      };
    case 'target':
      return a.id === null || s.members.some((p) => p.id === a.id)
        ? {
            ...s,
            ...stop,
            target: a.id,
            selected: null,
            notice: a.id
              ? `${s.members.find((p) => p.id === a.id)!.name}への個別PTTに切り替えました。`
              : 'グループ全員へのPTTに戻しました。',
          }
        : s;
    case 'join-start':
      return { ...s, joining: a.group, joinId: s.joinId + 1, notice: '' };
    case 'join-complete':
      return s.joining && a.id === s.joinId
        ? {
            ...s,
            ...stop,
            joined: true,
            group: s.joining,
            joining: null,
            members: peers.map((p) => ({ ...p })),
            connected: true,
            remote: null,
            target: null,
            sheet: null,
            stale: false,
            scenario: 'quiet',
            notice: 'グループに参加しました。',
          }
        : s;
    case 'create': {
      const group = a.group.trim();
      return group && Array.from(group).length <= 12
        ? {
            ...s,
            ...stop,
            joined: true,
            group,
            members: [],
            remote: null,
            target: null,
            connected: true,
            sheet: null,
            joining: null,
            joinId: s.joinId + 1,
            stale: false,
            scenario: 'quiet',
            notice: 'グループを作りました。仲間の参加を待っています。',
          }
        : { ...s, notice: 'グループ名を1〜12文字で入力してください。' };
    }
    case 'leave':
      return {
        ...s,
        ...stop,
        joined: false,
        members: [],
        remote: null,
        target: null,
        sheet: null,
        selected: null,
        joining: null,
        joinId: s.joinId + 1,
        notice: 'グループから退出しました。',
      };
    case 'notice':
      return { ...s, notice: a.message };
    case 'reset':
      return {
        ...initialState,
        design: s.design,
        page: concepts.find((c) => c.id === s.design)!.home,
        members: peers.map((p) => ({ ...p })),
        reduced: s.reduced,
        joinId: s.joinId + 1,
      };
    case 'reduced':
      return { ...s, reduced: a.value };
  }
}
export function voiceLabel(s: State): string {
  if (!s.joined) return 'まだ参加していません';
  if (!s.connected) return '再接続しています';
  if (s.voice === 'talking') return 'あなたが話しています';
  if (s.voice === 'requesting') return '送信を準備しています';
  if (s.voice === 'busy') return 'いまは仲間が話しています';
  const p = s.members.find((p) => p.id === s.remote);
  if (p)
    return `${p.name} が話しています${s.masterMuted || p.muted || p.volume === 0 ? ' · 消音' : ''}`;
  return s.members.length ? 'いつでも話せます' : '仲間の参加を待っています';
}
