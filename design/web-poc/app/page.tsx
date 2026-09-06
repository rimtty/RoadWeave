'use client';
import Link from 'next/link';
import {
  useEffect,
  useReducer,
  useRef,
  useState,
  type CSSProperties,
} from 'react';
import {
  Radio,
  Mic,
  Volume2,
  VolumeX,
  Users,
  Navigation,
  Compass,
  Settings2,
  ArrowUp,
  ArrowDown,
  ArrowUpRight,
  ChevronRight,
  Check,
  Plus,
  LogOut,
  Link2,
  Battery,
  WifiOff,
  X,
  Play,
  RotateCcw,
  Pause,
  Moon,
  Sun,
  Maximize2,
  CircleHelp,
} from 'lucide-react';
import { Tabs, TabsList, TabsTrigger } from '@/components/ui/tabs';
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogTitle,
} from '@/components/ui/dialog';
import { Slider } from '@/components/ui/slider';
import { Switch } from '@/components/ui/switch';
import {
  concepts,
  initialState,
  reducer,
  voiceLabel,
  type Peer,
  type Design,
  type Page,
  type Scenario,
} from '@/lib/experience';

const pageNames: Record<Page, string> = {
  people: '仲間',
  ride: 'ライド',
  radar: 'レーダー',
};
const pageIcons = { people: Users, ride: Navigation, radar: Compass };
const scenarioOptions: [Scenario, string, string][] = [
  ['quiet', '待受', 'PTTで話せる状態'],
  ['receiving', '受信', 'AKIからの声が届く'],
  ['busy', '発話が重なる', '仲間の発話後に押し直す'],
  ['lost', '接続が切れる', '再接続まで送信しない'],
];
function Avatar({
  peer,
  large = false,
  active = false,
  onClick,
}: {
  peer: Peer;
  large?: boolean;
  active?: boolean;
  onClick?: () => void;
}) {
  return (
    <button
      type="button"
      className={`avatar ${large ? 'hero' : 'mini'} ${peer.tone} ${active ? 'is-speaking' : ''}`}
      aria-label={`${peer.name}の詳細`}
      onClick={onClick}
    >
      <span>{peer.initial}</span>
      {peer.muted && <VolumeX className="avatar-muted" aria-hidden="true" />}
    </button>
  );
}
function Wave({ active }: { active: boolean }) {
  return (
    <span className={`wave ${active ? 'active' : ''}`} aria-hidden="true">
      {Array.from({ length: 7 }, (_, i) => (
        <i key={i} style={{ '--i': i } as CSSProperties} />
      ))}
    </span>
  );
}
export default function Home() {
  const [s, dispatch] = useReducer(reducer, initialState);
  const [dark, setDark] = useState(false);
  const [compact, setCompact] = useState(false);
  const [guide, setGuide] = useState(false);
  const [groupDraft, setGroupDraft] = useState('');
  const [tour, setTour] = useState(false);
  const [tourStep, setTourStep] = useState(0);
  const sRef = useRef(s);
  useEffect(() => {
    sRef.current = s;
  }, [s]);
  const startSwipe = useRef<{ x: number; y: number } | null>(null);
  const concept = concepts.find((c) => c.id === s.design)!;
  const remote = s.members.find((p) => p.id === s.remote);
  const selected = s.members.find((p) => p.id === s.selected);
  const talking = s.voice === 'talking';
  const txTarget = s.target
    ? s.members.find((p) => p.id === s.target)?.name
    : '全員';
  const selectDesign = (id: Design) => {
    setTour(false);
    dispatch({ type: 'design', design: id });
    if (typeof window !== 'undefined')
      window.history.replaceState(null, '', `#${id}`);
  };
  useEffect(() => {
    const apply = () => {
      const id = window.location.hash.slice(1);
      if (concepts.some((c) => c.id === id))
        dispatch({ type: 'design', design: id as Design });
    };
    apply();
    window.addEventListener('hashchange', apply);
    return () => window.removeEventListener('hashchange', apply);
  }, []);
  useEffect(() => {
    if (s.voice !== 'requesting' || !s.held) return;
    const timer = setTimeout(() => dispatch({ type: 'ptt-granted' }), 260);
    return () => clearTimeout(timer);
  }, [s.voice, s.held]);
  useEffect(() => {
    if (!s.joining) return;
    const id = s.joinId;
    const timer = setTimeout(
      () => dispatch({ type: 'join-complete', id }),
      850,
    );
    return () => clearTimeout(timer);
  }, [s.joining, s.joinId]);
  useEffect(() => {
    if (!s.notice) return;
    const timer = setTimeout(
      () => dispatch({ type: 'notice', message: '' }),
      4500,
    );
    return () => clearTimeout(timer);
  }, [s.notice]);
  useEffect(() => {
    const stop = () => dispatch({ type: 'ptt-up' });
    const visibility = () => {
      if (document.hidden) {
        stop();
        setTour(false);
      }
    };
    window.addEventListener('blur', stop);
    document.addEventListener('visibilitychange', visibility);
    return () => {
      window.removeEventListener('blur', stop);
      document.removeEventListener('visibilitychange', visibility);
    };
  }, []);
  useEffect(() => {
    if (!tour) return;
    const steps = [
      () => dispatch({ type: 'scenario', scenario: 'quiet' }),
      () => dispatch({ type: 'scenario', scenario: 'receiving' }),
      () => dispatch({ type: 'speaker', id: 'ren' }),
      () => {
        dispatch({ type: 'scenario', scenario: 'quiet' });
        dispatch({ type: 'stale', value: true });
      },
      () => dispatch({ type: 'scenario', scenario: 'lost' }),
      () => {
        dispatch({ type: 'scenario', scenario: 'quiet' });
        dispatch({ type: 'stale', value: false });
      },
    ];
    let i = 0;

    const timer = setInterval(() => {
      i++;
      if (i >= steps.length) {
        setTour(false);
        return;
      }
      setTourStep(i);
      steps[i]();
    }, 3200);
    return () => clearInterval(timer);
  }, [tour]);
  // Progressive enhancement. Tools modify the same local, simulated experience.
  useEffect(() => {
    const context = (
      document as unknown as {
        modelContext?: {
          registerTool: (
            tool: unknown,
            options: unknown,
          ) => Promise<void> | void;
        };
      }
    ).modelContext;
    if (!context) return;
    const life = new AbortController();
    const register = (tool: unknown) => {
      try {
        Promise.resolve(
          context.registerTool(tool, { signal: life.signal }),
        ).catch(() => {});
      } catch {}
    };
    register({
      name: 'read_roadweave_preview',
      description:
        'Read the local RoadWeave PoC state. No real radio or microphone is connected.',
      inputSchema: {
        type: 'object',
        properties: {},
        additionalProperties: false,
      },
      annotations: { readOnlyHint: true },
      execute: () => {
        const v = sRef.current;
        return {
          design: v.design,
          page: v.page,
          joined: v.joined,
          group: v.group,
          connected: v.connected,
          voice: voiceLabel(v),
          stalePosition: v.stale,
          target: v.target,
        };
      },
    });
    register({
      name: 'set_roadweave_scenario',
      description:
        'Change the local PoC communication scenario. This never transmits real audio.',
      inputSchema: {
        type: 'object',
        properties: {
          scenario: {
            type: 'string',
            enum: ['quiet', 'receiving', 'busy', 'lost'],
          },
        },
        required: ['scenario'],
        additionalProperties: false,
      },
      annotations: { readOnlyHint: false },
      execute: async (input: unknown) => {
        const v = input as { scenario?: Scenario };
        if (!v || !scenarioOptions.some(([id]) => id === v.scenario))
          throw Error('Unknown scenario');
        setTour(false);
        dispatch({ type: 'scenario', scenario: v.scenario! });
        await new Promise((r) =>
          requestAnimationFrame(() => requestAnimationFrame(r)),
        );
        return {
          scenario: sRef.current.scenario,
          voice: voiceLabel(sRef.current),
        };
      },
    });
    return () => life.abort();
  }, []);
  const pttDown = () => {
    setTour(false);
    dispatch({ type: 'ptt-down' });
  };
  const pttUp = () => dispatch({ type: 'ptt-up' });
  const setScenario = (value: Scenario) => {
    setTour(false);
    dispatch({ type: 'scenario', scenario: value });
  };
  const navigate = (page: Page) => {
    dispatch({ type: 'navigate', page });
  };
  const peerButton = (p: Peer, large = false) => (
    <Avatar
      key={p.id}
      peer={p}
      large={large}
      active={s.connected && p.id === s.remote}
      onClick={() => dispatch({ type: 'select', id: p.id })}
    />
  );
  const voiceStrip = (
    <div
      className={`voice-strip ${talking ? 'transmitting' : ''} ${!s.connected ? 'unavailable' : ''}`}
      aria-live="polite"
    >
      <span className="voice-symbol">
        {!s.connected ? (
          <WifiOff size={16} />
        ) : (
          <Wave active={!!remote || talking} />
        )}
      </span>
      <span>{voiceLabel(s)}</span>
    </div>
  );
  function peopleView() {
    if (s.design === 'circle')
      return (
        <div
          className={`people-circle ${remote || talking ? 'has-voice' : 'quiet'}`}
        >
          {voiceStrip}
          <div
            className="hero-person"
            key={talking ? 'you' : (remote?.id ?? 'group')}
          >
            {talking ? (
              <div className="avatar hero self is-speaking">
                <Mic size={42} />
              </div>
            ) : remote ? (
              peerButton(remote, true)
            ) : (
              <button
                className="group-orb"
                onClick={() => dispatch({ type: 'sheet', sheet: 'members' })}
                aria-label="メンバー一覧"
              >
                <Users size={38} />
                <span>{s.members.length + 1}人</span>
              </button>
            )}
            <h3>{talking ? 'あなた' : (remote?.name ?? 'みんな、ここに。')}</h3>
            <span>
              {talking
                ? `${txTarget}に声を届けています`
                : remote
                  ? 'グループ全員への音声'
                  : 'ボタンひとつで、声が届く。'}
            </span>
          </div>
          <div className="friends">
            {s.members
              .filter((p) => p.id !== remote?.id)
              .map((p) => (
                <div className="friend" key={p.id}>
                  {peerButton(p)}
                  <span>{p.name}</span>
                </div>
              ))}
            {remote && (
              <div className="friend">
                <div className="avatar mini self">自</div>
                <span>あなた</span>
              </div>
            )}
          </div>
        </div>
      );
    return (
      <div className="members-view">
        {voiceStrip}
        <div className="member-list">
          {s.members.map((p) => (
            <button
              className={`member-row ${p.id === s.remote ? 'speaking' : ''}`}
              key={p.id}
              onClick={() => dispatch({ type: 'select', id: p.id })}
            >
              <span className={`avatar mini ${p.tone}`}>{p.initial}</span>
              <span>
                <strong>{p.name}</strong>
                <small>
                  {p.id === s.remote
                    ? '話しています'
                    : s.stale && p.id === 'mei'
                      ? '位置が未更新'
                      : '一緒に走っています'}
                </small>
              </span>
              <span className="row-trailing">
                {p.muted ? (
                  <VolumeX size={18} />
                ) : p.id === s.remote ? (
                  <Wave active />
                ) : (
                  <ChevronRight size={16} />
                )}
              </span>
            </button>
          ))}
        </div>
      </div>
    );
  }
  function rideView() {
    return (
      <div className="ride-view">
        {s.design === 'pulse' ? (
          <>
            <div className="ride-leading">
              <span>
                <ArrowUp size={17} />
                前の仲間
              </span>
              <button onClick={() => dispatch({ type: 'select', id: 'aki' })}>
                AKI <ArrowUpRight size={14} />
              </button>
            </div>
            <div className="distance-hero">
              <span>120</span>
              <small>m</small>
            </div>
            <div className="ride-track" aria-hidden="true">
              <i />
              <i />
              <i />
              <span />
            </div>
            <div className="ride-secondary">
              <button onClick={() => dispatch({ type: 'select', id: 'ren' })}>
                <span>
                  <ArrowDown size={15} /> 後ろの REN
                </span>
                <strong>
                  85 <small>m</small>
                </strong>
              </button>
              <div>
                <span>グループの広がり</span>
                <strong>
                  {s.stale ? '—' : '590'} <small>m</small>
                </strong>
              </div>
            </div>
            {voiceStrip}
            <div className="position-age">
              {s.stale ? 'MEIの位置は12秒前' : '位置情報 · 1秒前'}
            </div>
          </>
        ) : (
          <>
            <div className="ride-caption">仲間との距離</div>
            <button
              className="distance-card"
              onClick={() => dispatch({ type: 'select', id: 'aki' })}
            >
              <span className="direction-icon">
                <ArrowUp />
              </span>
              <span>
                <small>前の AKI</small>
                <strong>
                  120 <em>m</em>
                </strong>
              </span>
              <ChevronRight size={18} />
            </button>
            <button
              className="distance-card"
              onClick={() => dispatch({ type: 'select', id: 'ren' })}
            >
              <span className="direction-icon">
                <ArrowDown />
              </span>
              <span>
                <small>後ろの REN</small>
                <strong>
                  85 <em>m</em>
                </strong>
              </span>
              <ChevronRight size={18} />
            </button>
            {voiceStrip}
            <button
              className="text-action"
              onClick={() => dispatch({ type: 'sheet', sheet: 'members' })}
            >
              みんなの位置を見る <ChevronRight size={14} />
            </button>
          </>
        )}
      </div>
    );
  }
  function radarView() {
    return (
      <div className="radar-view">
        <div className="radar-heading">
          <span>
            <Navigation size={12} /> 進行方向が上
          </span>
          <span>外周 500 m</span>
        </div>
        <div
          className="radar-field"
          aria-label="自分中心の相対位置レーダー。背景地図ではありません。"
        >
          <svg className="radar-grid" viewBox="0 0 260 220" aria-hidden="true">
            <circle cx="130" cy="110" r="100" />
            <circle cx="130" cy="110" r="65" />
            <circle cx="130" cy="110" r="32" />
            <path d="M130 10V210M30 110H230" />
            <path
              className="heading-cone"
              d="M130 110L83 20A100 100 0 0 1 177 20Z"
            />
          </svg>
          <div className="radar-self">
            <Navigation size={15} fill="currentColor" />
            <span>自分</span>
          </div>
          {s.members
            .filter((p) => !(s.stale && p.id === 'mei'))
            .map((p) => {
              const rad = (p.bearing * Math.PI) / 180;
              return (
                <button
                  key={p.id}
                  className={`radar-peer ${p.id === s.remote && s.connected ? 'active' : ''}`}
                  style={{
                    left: `${50 + ((Math.sin(rad) * p.distance) / 500) * 38.46}%`,
                    top: `${50 - ((Math.cos(rad) * p.distance) / 500) * 45.45}%`,
                  }}
                  onClick={() => dispatch({ type: 'select', id: p.id })}
                  aria-label={`${p.name}、${p.distance}メートル、詳細を表示`}
                >
                  <span className={`radar-dot ${p.tone}`} />
                  <span>{p.name}</span>
                </button>
              );
            })}
        </div>
        {voiceStrip}
        <div className="radar-footer">
          <span>{s.stale ? 'MEI · 位置未更新' : '5人の位置を表示'}</span>
          <button onClick={() => dispatch({ type: 'sheet', sheet: 'members' })}>
            一覧 <ChevronRight size={13} />
          </button>
        </div>
      </div>
    );
  }
  const open = s.sheet !== null || s.selected !== null;
  return (
    <main
      className={`lab ${s.design} ${dark ? 'night' : ''} ${s.reduced ? 'reduce-motion' : ''}`}
    >
      <header className="lab-header">
        <a
          className="brand"
          href="#circle"
          onClick={() => selectDesign('circle')}
        >
          <Radio />
          RoadWeave<span>Design lab</span>
        </a>
        <div className="header-actions">
          <Link href="/explore" className="collection-link">60のデザインを見る</Link>
          <span className="lab-tag">THREE WAYS TO RIDE</span>
          <button
            className="icon-button"
            aria-label="使い方"
            onClick={() => setGuide(!guide)}
            aria-expanded={guide}
          >
            <CircleHelp size={20} />
          </button>
          <button
            className="icon-button"
            aria-label={dark ? '明るい背景にする' : '暗い背景にする'}
            onClick={() => setDark(!dark)}
          >
            {dark ? <Sun size={20} /> : <Moon size={20} />}
          </button>
        </div>
      </header>
      {guide && (
        <div className="guide">
          <p>
            3つのデザインで同じ操作を試せます。PTTは押している間だけ送信。受信中は送信できません。「待受」に切り替えると話せます。画面は左右スワイプにも対応しています。
          </p>
          <button
            className="icon-button"
            aria-label="使い方を閉じる"
            onClick={() => setGuide(false)}
          >
            <X />
          </button>
        </div>
      )}
      <div className="workspace">
        <aside className="concept-rail">
          <span className="eyebrow">ROADWEAVE / WEB PoC</span>
          <h1>
            一緒に走る。
            <br />
            もっと自然に。
          </h1>
          <Tabs
            value={s.design}
            onValueChange={(v) => selectDesign(v as Design)}
            orientation="vertical"
          >
            <TabsList className="concept-list" aria-label="デザイン案">
              {concepts.map((c) => (
                <TabsTrigger value={c.id} key={c.id} className="concept-choice">
                  <span>{c.number}</span>
                  <div>
                    <strong>{c.title}</strong>
                    <small>{c.subtitle}</small>
                  </div>
                  <ChevronRight className="concept-arrow" size={16} />
                </TabsTrigger>
              ))}
            </TabsList>
          </Tabs>
          <p className="rail-note" key={concept.id}>
            {concept.detail}
          </p>
          <div className="design-caption">
            <span className="color-dot" />{' '}
            {s.design === 'circle'
              ? 'People first'
              : s.design === 'pulse'
                ? 'Ride first'
                : 'Together, in view'}
          </div>
        </aside>
        <section
          className={`stage ${compact ? 'compact' : ''}`}
          aria-label={`${concept.title} 端末プレビュー`}
        >
          <div className="stage-toolbar">
            <span>
              {concept.number} / {concept.title.toUpperCase()}
            </span>
            <button onClick={() => setCompact(!compact)}>
              <Maximize2 size={13} />
              {compact ? '拡大表示' : '240 × 320で見る'}
            </button>
          </div>
          <div
            className="device-wrap"
            onPointerDownCapture={() => setTour(false)}
            onKeyDownCapture={() => setTour(false)}
          >
            <div
              className={`device ${talking ? 'tx' : ''} ${!s.connected ? 'disconnected' : ''}`}
            >
              <div className="device-top">
                <span>9:41</span>
                <span>
                  {s.masterMuted && <VolumeX size={12} />}{' '}
                  {s.connected ? <Radio size={13} /> : <WifiOff size={13} />}
                  <Battery size={17} />
                  82%
                </span>
              </div>
              {s.joined ? (
                <>
                  <div className="group-heading">
                    <button
                      className="group-title"
                      onClick={() =>
                        dispatch({ type: 'sheet', sheet: 'members' })
                      }
                    >
                      <small>
                        {s.connected
                          ? `${s.members.length + 1}人でつながっています`
                          : '接続を待っています'}
                      </small>
                      <h2>
                        {s.group}
                        <ChevronRight size={15} />
                      </h2>
                    </button>
                    <button
                      className="settings-button"
                      aria-label="グループ設定"
                      onClick={() =>
                        dispatch({ type: 'sheet', sheet: 'settings' })
                      }
                    >
                      <Settings2 />
                    </button>
                  </div>
                  <div
                    className="device-content"
                    key={`${s.design}-${s.page}`}
                    onTouchStart={(e) => {
                      startSwipe.current = {
                        x: e.touches[0].clientX,
                        y: e.touches[0].clientY,
                      };
                    }}
                    onTouchEnd={(e) => {
                      const start = startSwipe.current;
                      startSwipe.current = null;
                      if (!start) return;
                      const dx = e.changedTouches[0].clientX - start.x,
                        dy = e.changedTouches[0].clientY - start.y;
                      if (
                        Math.abs(dx) > 55 &&
                        Math.abs(dx) > Math.abs(dy) * 1.4
                      ) {
                        const pages: Page[] = ['people', 'ride', 'radar'];
                        navigate(
                          pages[(pages.indexOf(s.page) + (dx < 0 ? 1 : 2)) % 3],
                        );
                      }
                    }}
                  >
                    {!s.members.length ? (
                      <div className="empty-state">
                        <div className="group-orb">
                          <Users size={38} />
                        </div>
                        <h3>仲間を待っています</h3>
                        <p>近くの端末で「{s.group}」を選ぶと参加できます。</p>
                        <button
                          className="small-primary"
                          onClick={() =>
                            dispatch({
                              type: 'notice',
                              message:
                                'このPoCは仮のグループです。右側の「体験をリセット」で6人の状態へ戻せます。',
                            })
                          }
                        >
                          参加方法
                        </button>
                      </div>
                    ) : s.page === 'people' ? (
                      peopleView()
                    ) : s.page === 'ride' ? (
                      rideView()
                    ) : (
                      radarView()
                    )}
                  </div>
                  <Tabs
                    className="device-tabs"
                    value={s.page}
                    onValueChange={(v) => navigate(v as Page)}
                  >
                    <TabsList className="device-nav" aria-label="端末の画面">
                      {(['people', 'ride', 'radar'] as Page[]).map((page) => {
                        const Icon = pageIcons[page];
                        return (
                          <TabsTrigger
                            key={page}
                            className="nav-item"
                            value={page}
                          >
                            <Icon />
                            <span>{pageNames[page]}</span>
                          </TabsTrigger>
                        );
                      })}
                    </TabsList>
                  </Tabs>
                </>
              ) : (
                <div className="welcome">
                  <div className="welcome-symbol">
                    <Radio size={42} />
                  </div>
                  <span className="welcome-kicker">ROADWEAVE</span>
                  <h2>
                    次の道も、
                    <br />
                    みんなと。
                  </h2>
                  <p>
                    近くの仲間とつながって、
                    <br />
                    ライドを始めましょう。
                  </p>
                  <button
                    className="primary-action"
                    onClick={() => dispatch({ type: 'sheet', sheet: 'join' })}
                  >
                    <Link2 size={18} />
                    グループに参加
                  </button>
                  <button
                    className="secondary-action"
                    onClick={() => {
                      setGroupDraft('');
                      dispatch({ type: 'sheet', sheet: 'create' });
                    }}
                  >
                    <Plus size={18} />
                    グループを作る
                  </button>
                </div>
              )}
              {s.target && s.joined && (
                <button
                  className="target-banner"
                  onClick={() => dispatch({ type: 'target', id: null })}
                >
                  {txTarget}に個別PTT <X size={12} />
                </button>
              )}
            </div>
          </div>
          <div className="hardware">
            <button
              aria-label="全体の音量"
              onClick={() => dispatch({ type: 'sheet', sheet: 'volume' })}
            >
              {s.masterMuted ? <VolumeX /> : <Volume2 />}
            </button>
            <button
              className={`ptt ${s.voice}`}
              aria-label="PTT 押している間だけ話す"
              aria-pressed={s.held}
              onPointerDown={(e) => {
                e.currentTarget.setPointerCapture(e.pointerId);
                pttDown();
              }}
              onPointerUp={pttUp}
              onPointerCancel={pttUp}
              onLostPointerCapture={pttUp}
              onBlur={pttUp}
              onContextMenu={(e) => e.preventDefault()}
              onKeyDown={(e) => {
                if ((e.key === ' ' || e.key === 'Enter') && !e.repeat) {
                  e.preventDefault();
                  pttDown();
                }
              }}
              onKeyUp={(e) => {
                if (e.key === ' ' || e.key === 'Enter') {
                  e.preventDefault();
                  pttUp();
                }
              }}
            >
              {s.voice === 'busy' ? <Pause /> : <Mic />}
              <span>
                {talking
                  ? '話しています'
                  : s.voice === 'requesting'
                    ? '準備中…'
                    : s.voice === 'busy'
                      ? '仲間の発話中'
                      : s.target
                        ? `${txTarget}に話す`
                        : '押して話す'}
              </span>
            </button>
          </div>
          <div className="stage-note">Web PoC · 音声・位置・接続は仮データ</div>
          <output className="notice" aria-live="polite">
            {s.notice}
          </output>
        </section>
        <aside className="scenario-panel">
          <div className="scenario-top">
            <span className="eyebrow">TRY THE EXPERIENCE</span>
            <span className="demo-badge">DEMO</span>
          </div>
          <h2>ライドを試してみる</h2>
          <button
            className={`tour-button ${tour ? 'playing' : ''}`}
            onClick={() => {
              if (!s.joined || !s.members.length) dispatch({ type: 'reset' });
              setTourStep(0);
              if (!tour) dispatch({ type: 'scenario', scenario: 'quiet' });
              setTour(!tour);
            }}
          >
            {tour ? <Pause size={16} /> : <Play size={16} />}
            <span>
              {tour
                ? `体験を一時停止 · ${tourStep + 1}/6`
                : '20秒でひと通り体験'}
            </span>
          </button>
          <fieldset className="scenario-list" aria-label="試す通信状態">
            {scenarioOptions.map(([id, title, sub]) => (
              <button
                className={`scenario-option ${s.scenario === id ? 'selected' : ''}`}
                key={id}
                onClick={() => setScenario(id)}
                aria-pressed={s.scenario === id}
              >
                <span className="scenario-dot" />
                <span>
                  <strong>{title}</strong>
                  <small>{sub}</small>
                </span>
                {s.scenario === id && <Check size={15} />}
              </button>
            ))}
          </fieldset>
          {s.remote && (
            <div className="speaker-picker">
              <span>話している人</span>
              <div>
                {s.members.map((p) => (
                  <button
                    key={p.id}
                    aria-label={`${p.name}が話す`}
                    aria-pressed={s.remote === p.id}
                    onClick={() => {
                      setTour(false);
                      dispatch({ type: 'speaker', id: p.id });
                    }}
                  >
                    {p.initial}
                  </button>
                ))}
              </div>
            </div>
          )}
          <div className="scenario-switch">
            <label htmlFor="stale-position">
              1人の位置が古くなる<small>音声接続とは別の状態</small>
            </label>
            <Switch
              id="stale-position"
              aria-label="1人の位置が古くなる"
              checked={s.stale}
              onCheckedChange={(v) => {
                setTour(false);
                dispatch({ type: 'stale', value: v });
              }}
            />
          </div>
          <div className="scenario-switch">
            <label htmlFor="reduce-motion">動きを控えめに</label>
            <Switch
              id="reduce-motion"
              aria-label="動きを控えめに"
              checked={s.reduced}
              onCheckedChange={(value) => dispatch({ type: 'reduced', value })}
            />
          </div>
          <button
            className="onboard-demo"
            onClick={() => {
              setTour(false);
              dispatch({ type: 'leave' });
            }}
          >
            <Link2 size={15} />
            グループ参加から試す
            <ChevronRight size={14} />
          </button>
          <button
            className="reset-button"
            onClick={() => {
              setTour(false);
              dispatch({ type: 'reset' });
            }}
          >
            <RotateCcw size={13} />
            体験をリセット
          </button>
        </aside>
      </div>
      <footer className="lab-footer">
        <span>Designed for the road. Built around people.</span>
        <span>Webの動作検討用 · 実機の描画性能は別途検証</span>
      </footer>
      <Dialog
        open={open}
        onOpenChange={(value) => {
          if (!value) {
            dispatch({ type: 'select', id: null });
            dispatch({ type: 'sheet', sheet: null });
          }
        }}
      >
        <DialogContent
          className={`product-dialog ${s.design}`}
          showCloseButton={false}
        >
          <button
            className="dialog-close icon-button"
            aria-label="閉じる"
            onClick={() => {
              dispatch({ type: 'select', id: null });
              dispatch({ type: 'sheet', sheet: null });
            }}
          >
            <X size={20} />
          </button>
          {selected ? (
            <>
              <div className="detail-avatar">
                <span className={`avatar hero ${selected.tone}`}>
                  {selected.initial}
                </span>
              </div>
              <DialogTitle className="dialog-person-title">
                {selected.name}
              </DialogTitle>
              <DialogDescription className="dialog-subtitle">
                {s.stale && selected.id === 'mei'
                  ? '位置情報は12秒前 · 現在位置は不明'
                  : `${selected.along > 0 ? '前方' : '後方'} ${Math.abs(selected.along)} m · 位置は1秒前`}
              </DialogDescription>
              <div className="volume-control">
                <label>
                  {selected.name}の音量 <strong>{selected.volume}%</strong>
                </label>
                <Slider
                  aria-label={`${selected.name}の音量`}
                  value={[selected.volume]}
                  onValueChange={(v) =>
                    dispatch({
                      type: 'peer-volume',
                      id: selected.id,
                      value: Array.isArray(v) ? v[0] : v,
                    })
                  }
                />
              </div>
              <div className="setting-row">
                <span>
                  <VolumeX size={18} />
                  この人を消音
                </span>
                <Switch
                  aria-label={`${selected.name}を消音`}
                  checked={selected.muted}
                  onCheckedChange={() =>
                    dispatch({ type: 'peer-mute', id: selected.id })
                  }
                />
              </div>
              <button
                className="primary-action"
                onClick={() => dispatch({ type: 'target', id: selected.id })}
              >
                <Mic size={18} />
                {selected.name}だけに話す
              </button>
              <p className="dialog-hint">
                選択後、PTTを押している間だけ送信します。
              </p>
            </>
          ) : s.sheet === 'volume' ? (
            <>
              <DialogTitle>聞こえ方を調整</DialogTitle>
              <DialogDescription>
                グループ全体のスピーカー音量です。
              </DialogDescription>
              <div className="volume-hero">
                {s.masterMuted ? <VolumeX /> : <Volume2 />}
                <strong>
                  {s.masterVolume}
                  <small>%</small>
                </strong>
              </div>
              <Slider
                aria-label="全体の音量"
                value={[s.masterVolume]}
                onValueChange={(v) =>
                  dispatch({
                    type: 'master-volume',
                    value: Array.isArray(v) ? v[0] : v,
                  })
                }
              />
              <div className="setting-row">
                <span>スピーカーを消音</span>
                <Switch
                  aria-label="スピーカーを消音"
                  checked={s.masterMuted}
                  onCheckedChange={() => dispatch({ type: 'master-mute' })}
                />
              </div>
              <button
                className="primary-action"
                onClick={() => dispatch({ type: 'sheet', sheet: null })}
              >
                完了
              </button>
            </>
          ) : s.sheet === 'members' ? (
            <>
              <DialogTitle>{s.group}</DialogTitle>
              <DialogDescription>
                {s.members.length + 1}人のグループ · 自分からの前後距離
              </DialogDescription>
              <div className="dialog-member-list">
                {s.members.length ? (
                  s.members.map((p) => (
                    <button
                      key={p.id}
                      onClick={() => dispatch({ type: 'select', id: p.id })}
                    >
                      <span className={`avatar mini ${p.tone}`}>
                        {p.initial}
                      </span>
                      <strong>{p.name}</strong>
                      <small>
                        {s.stale && p.id === 'mei'
                          ? '位置未更新'
                          : `${p.along > 0 ? '前' : '後'} ${Math.abs(p.along)} m`}
                      </small>
                      <ChevronRight size={15} />
                    </button>
                  ))
                ) : (
                  <p className="empty-copy">まだ仲間が参加していません。</p>
                )}
              </div>
              <div className="self-row">
                <span className="avatar mini self">自</span>あなた
                <small>この端末</small>
              </div>
            </>
          ) : s.sheet === 'settings' ? (
            <>
              <DialogTitle>ライドの設定</DialogTitle>
              <DialogDescription>
                {s.group} · {s.members.length + 1}人
              </DialogDescription>
              <button
                className="setting-link"
                onClick={() => dispatch({ type: 'sheet', sheet: 'volume' })}
              >
                <Volume2 />
                音量と消音
                <ChevronRight />
              </button>
              <button
                className="setting-link"
                onClick={() => dispatch({ type: 'sheet', sheet: 'members' })}
              >
                <Users />
                メンバー
                <ChevronRight />
              </button>
              {s.target && (
                <button
                  className="setting-link"
                  onClick={() => dispatch({ type: 'target', id: null })}
                >
                  <Mic />
                  全員へのPTTに戻す
                  <ChevronRight />
                </button>
              )}
              <button
                className="setting-link danger"
                onClick={() => dispatch({ type: 'sheet', sheet: 'leave' })}
              >
                <LogOut />
                グループから退出
                <ChevronRight />
              </button>
            </>
          ) : s.sheet === 'leave' ? (
            <>
              <DialogTitle>グループから退出しますか？</DialogTitle>
              <DialogDescription>
                「{s.group}
                」への音声と位置の共有を終了します。あとから参加し直せます。
              </DialogDescription>
              <button
                className="primary-action danger-action"
                onClick={() => dispatch({ type: 'leave' })}
              >
                退出する
              </button>
              <button
                className="secondary-action"
                onClick={() => dispatch({ type: 'sheet', sheet: null })}
              >
                ライドを続ける
              </button>
            </>
          ) : s.sheet === 'join' ? (
            <>
              <DialogTitle>近くのグループ</DialogTitle>
              <DialogDescription>
                参加すると、名前と位置を仲間に共有します。このPoCでは仮のグループを使います。
              </DialogDescription>
              <button
                className="join-card"
                disabled={!!s.joining}
                onClick={() =>
                  dispatch({ type: 'join-start', group: '瀬戸内ライド' })
                }
              >
                <span className="join-icon">
                  <Users />
                </span>
                <span>
                  <strong>瀬戸内ライド</strong>
                  <small>AKI・REN・YUI ほか2人</small>
                </span>
                {s.joining ? <span className="spinner" /> : <ChevronRight />}
              </button>
              <output className="join-status">
                {s.joining
                  ? '参加しています…'
                  : '参加するグループを選んでください'}
              </output>
              <button
                className="text-action"
                onClick={() => {
                  setGroupDraft('');
                  dispatch({ type: 'sheet', sheet: 'create' });
                }}
              >
                自分でグループを作る <Plus size={15} />
              </button>
            </>
          ) : s.sheet === 'create' ? (
            <>
              <DialogTitle>新しいグループ</DialogTitle>
              <DialogDescription>
                仲間が見つけやすい名前をつけましょう。
              </DialogDescription>
              <form
                onSubmit={(e) => {
                  e.preventDefault();
                  dispatch({ type: 'create', group: groupDraft });
                }}
              >
                <label className="input-label" htmlFor="group-name">
                  グループ名
                </label>
                <input
                  id="group-name"
                  value={groupDraft}
                  onChange={(e) => setGroupDraft(e.target.value)}
                  placeholder="例：しまなみ日和"
                  maxLength={24}
                />
                <div className="input-meta">
                  <span>
                    {Array.from(groupDraft.trim()).length > 12
                      ? '12文字以内で入力してください'
                      : '仲間に表示されます'}
                  </span>
                  <span>{Array.from(groupDraft.trim()).length}/12</span>
                </div>
                <button
                  className="primary-action"
                  disabled={
                    !groupDraft.trim() ||
                    Array.from(groupDraft.trim()).length > 12
                  }
                  type="submit"
                >
                  グループを作る
                </button>
              </form>
            </>
          ) : null}
        </DialogContent>
      </Dialog>
    </main>
  );
}
