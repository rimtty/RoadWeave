'use client';
import {
  useEffect,
  useReducer,
  useState,
  useId,
  type CSSProperties,
  type ReactNode,
} from 'react';
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogTitle,
  DialogTrigger,
} from '@/components/ui/dialog';
import { Slider } from '@/components/ui/slider';
import { Switch } from '@/components/ui/switch';
import { DemoProvider } from './bindings';
import { demoMembers, demoReducer, demoView, initialDemo } from './model';
import './studio.css';
import { FleetScreen } from './fleet';
import { StudyPageTabs, useStudyPage, type StudyPage } from './pages';

export function StudyDemo({
  id,
  title,
  screenClass,
  children,
  style,
}: {
  id: number;
  title: string;
  screenClass: string;
  children: ReactNode;
  style?: CSSProperties;
}) {
  const [open, setOpen] = useState(false);
  const boardPage = useStudyPage();
  const [page, setPage] = useState<StudyPage>(boardPage);
  const [state, dispatch] = useReducer(demoReducer, id, (n) =>
    initialDemo(n, true),
  );
  const [running, setRunning] = useState(false);
  const controlsId = useId();
  const view = demoView(state, id >= 55);
  useEffect(() => {
    const sync = () => {
      const requested = Number(
        new URLSearchParams(location.search).get('demo'),
      );
      if (requested === id) {
        dispatch({ type: 'reset', id });
        setOpen(true);
        setPage(
          new URLSearchParams(location.search).get('screen') === 'fleet'
            ? 'fleet'
            : 'voice',
        );
      } else {
        setOpen(false);
        dispatch({ type: 'up' });
        setRunning(false);
      }
    };
    const initialSync = setTimeout(sync, 0);
    window.addEventListener('popstate', sync);
    return () => {
      clearTimeout(initialSync);
      window.removeEventListener('popstate', sync);
    };
  }, [id]);
  useEffect(() => {
    if (!open) return;
    const stop = () => {
      dispatch({ type: 'up' });
      setRunning(false);
    };
    const hidden = () => {
      if (document.hidden) stop();
    };
    window.addEventListener('blur', stop);
    window.addEventListener('pagehide', stop);
    document.addEventListener('visibilitychange', hidden);
    return () => {
      window.removeEventListener('blur', stop);
      window.removeEventListener('pagehide', stop);
      document.removeEventListener('visibilitychange', hidden);
    };
  }, [open]);
  useEffect(() => {
    if (!open || state.mode !== 'requesting') return;
    const timer = setTimeout(() => dispatch({ type: 'grant' }), 260);
    return () => clearTimeout(timer);
  }, [open, state.mode]);
  useEffect(() => {
    if (!open || state.mode !== 'transmitting') return;
    const timer = setInterval(() => dispatch({ type: 'tick' }), 1000);
    return () => clearInterval(timer);
  }, [open, state.mode]);
  useEffect(() => {
    if (!running || !open || !state.connected || !state.joined || state.stale)
      return;
    const timer = setInterval(() => {
      const position = state.positions[state.selected];
      dispatch({
        type: 'position',
        id: state.selected,
        value: position + (position >= 0 ? 5 : -5),
      });
    }, 1000);
    return () => clearInterval(timer);
  }, [
    running,
    open,
    state.connected,
    state.joined,
    state.stale,
    state.positions,
    state.selected,
  ]);
  function changeOpen(value: boolean) {
    if (value) dispatch({ type: 'reset', id });
    else dispatch({ type: 'up' });
    setRunning(false);
    setOpen(value);
    if (value) setPage(boardPage);
    {
      const url = new URL(location.href);
      if (value) {
        url.searchParams.set('demo', String(id));
        url.searchParams.set('screen', boardPage);
      } else {
        url.searchParams.delete('demo');
        url.searchParams.delete('screen');
      }
      history.replaceState(null, '', url);
    }
  }
  const stop = () => dispatch({ type: 'up' });
  const screen = (
    <div
      className={`study-screen ${screenClass}`}
      data-demo-screen={id}
      data-page={open ? page : boardPage}
      data-voice={view.rx}
      data-stale={state.stale || view.offline}
      data-audible={view.audible}
      data-audio-active={
        view.audible || (!view.offline && state.mode === 'transmitting')
      }
      style={
        {
          ...style,
          '--ci-accent': view.color,
          '--demo-peer-color': view.color,
          '--demo-other-color': view.otherColor,
          '--demo-third-color': view.thirdColor,
        } as CSSProperties
      }
    >
      {(open ? page : boardPage) === 'fleet' ? <FleetScreen /> : children}
    </div>
  );
  return (
    <DemoProvider state={state} id={id}>
      {!open ? (
        screen
      ) : (
        <div className={`study-placeholder ${id <= 16 ? 'study-wide' : ''}`}>
          操作デモを開いています
        </div>
      )}
      <Dialog open={open} onOpenChange={changeOpen}>
        <DialogTrigger className="study-launch" data-demo-launch={id}>
          {String(id).padStart(2, '0')} の操作デモ{' '}
          <span aria-hidden="true">↗</span>
        </DialogTrigger>
        <DialogContent
          className="study-dialog"
          aria-describedby={`${controlsId}-description`}
        >
          <div className="study-dialog-heading">
            <DialogTitle>
              {String(id).padStart(2, '0')} / {title}
            </DialogTitle>
            <DialogDescription id={`${controlsId}-description`}>
              仮の音声・位置で操作を試せます。実際の送信やマイク入力は行いません。
            </DialogDescription>
          </div>
          <div className="study-workbench">
            <div className="study-device-column">
              <StudyPageTabs
                value={page}
                onChange={(next) => {
                  stop();
                  setPage(next);
                  const url = new URL(location.href);
                  url.searchParams.set('screen', next);
                  history.replaceState(null, '', url);
                }}
              />
              <div
                className={`study-device rw-board ${id >= 13 && id <= 16 ? 'rw-dot-grid' : ''}`}
              >
                {screen}
              </div>
              <output className="study-device-status" aria-live="polite">
                {view.voiceSentence}
                {state.mode === 'transmitting' ? ` · ${view.elapsed}` : ''}
              </output>
              <button
                className="study-ptt"
                data-held={state.held}
                disabled={view.offline}
                onPointerDown={(e) => {
                  if (e.button !== 0) return;
                  e.preventDefault();
                  e.currentTarget.focus();
                  e.currentTarget.setPointerCapture(e.pointerId);
                  dispatch({ type: 'down' });
                }}
                onPointerUp={stop}
                onPointerCancel={stop}
                onLostPointerCapture={stop}
                onBlur={stop}
                onKeyDown={(e) => {
                  if ((e.key === ' ' || e.key === 'Enter') && !e.repeat) {
                    e.preventDefault();
                    dispatch({ type: 'down' });
                  }
                }}
                onKeyUp={(e) => {
                  if (e.key === ' ' || e.key === 'Enter') {
                    e.preventDefault();
                    stop();
                  }
                }}
              >
                {state.mode === 'transmitting'
                  ? '送信中 · 離すと終了'
                  : state.mode === 'busy'
                    ? '仲間が話しています'
                    : state.mode === 'requesting'
                      ? '送信準備中'
                      : '押している間、話す'}
              </button>
              <p className="study-help">
                マウス・タッチで長押し。フォーカス中は Space / Enter
                でも操作できます。
              </p>
            </div>
            <div className="study-controls">
              <fieldset>
                <legend>仲間の声</legend>
                <div className="study-options">
                  {demoMembers.map((peer) => (
                    <button
                      key={peer.id}
                      aria-pressed={state.remote === peer.id}
                      disabled={view.offline}
                      onClick={() => dispatch({ type: 'speaker', id: peer.id })}
                    >
                      <i
                        style={{ background: peer.color }}
                        aria-hidden="true"
                      />
                      {id >= 55 ? peer.colorName : peer.name}
                    </button>
                  ))}
                </div>
                <button
                  className="study-quiet"
                  onClick={() => dispatch({ type: 'quiet' })}
                  disabled={view.offline}
                >
                  受信を終えて待受にする
                </button>
              </fieldset>
              <fieldset>
                <legend>距離を変える</legend>
                <div className="study-position-label">
                  <span>
                    {id >= 55
                      ? demoMembers.find((p) => p.id === state.selected)!
                          .colorName
                      : state.selected.toUpperCase()}
                  </span>
                  <output>
                    {state.positions[state.selected] === 0
                      ? 'すぐそば'
                      : state.positions[state.selected] > 0
                        ? '前方'
                        : '後方'}{' '}
                    {Math.abs(state.positions[state.selected])}m
                  </output>
                </div>
                <Slider
                  aria-label="選択中の仲間の相対位置"
                  min={-999}
                  max={999}
                  step={1}
                  value={[state.positions[state.selected]]}
                  onValueChange={(v) =>
                    dispatch({
                      type: 'position',
                      id: state.selected,
                      value: Array.isArray(v) ? v[0] : v,
                    })
                  }
                />
                <div className="study-position-presets">
                  {[-240, -85, 0, 120, 500].map((value) => (
                    <button
                      key={value}
                      onClick={() =>
                        dispatch({
                          type: 'position',
                          id: state.selected,
                          value,
                        })
                      }
                    >
                      {value}m
                    </button>
                  ))}
                </div>
                <div className="study-switch">
                  <label htmlFor={`${controlsId}-switch-0`}>走行を再生</label>
                  <Switch
                    id={`${controlsId}-switch-0`}
                    checked={running}
                    onCheckedChange={setRunning}
                    disabled={view.offline || state.stale}
                  />
                </div>
              </fieldset>
              <fieldset>
                <legend>送信先</legend>
                <div className="study-options">
                  {(['all', state.selected] as const).map((target) => (
                    <button
                      key={target}
                      disabled={view.offline}
                      aria-pressed={state.target === target}
                      onClick={() =>
                        dispatch({ type: 'target', value: target })
                      }
                    >
                      {target === 'all'
                        ? '全員'
                        : id >= 55
                          ? demoMembers.find((p) => p.id === target)!.colorName
                          : target.toUpperCase()}
                    </button>
                  ))}
                </div>
                <p className="study-help">送信先を選ぶと待受になります。</p>
              </fieldset>
              <fieldset>
                <legend>接続・音声</legend>
                <div className="study-switch">
                  <label htmlFor={`${controlsId}-switch-1`}>
                    グループに参加
                  </label>
                  <Switch
                    id={`${controlsId}-switch-1`}
                    checked={state.joined}
                    onCheckedChange={(value) => {
                      setRunning(false);
                      dispatch({ type: value ? 'join' : 'leave' });
                    }}
                  />
                </div>
                <div className="study-switch">
                  <label htmlFor={`${controlsId}-switch-2`}>通信接続</label>
                  <Switch
                    id={`${controlsId}-switch-2`}
                    checked={state.connected}
                    onCheckedChange={(value) => {
                      setRunning(false);
                      dispatch({ type: 'connect', value });
                    }}
                    disabled={!state.joined}
                  />
                </div>
                <div className="study-switch">
                  <label htmlFor={`${controlsId}-switch-3`}>位置未更新</label>
                  <Switch
                    id={`${controlsId}-switch-3`}
                    checked={state.stale}
                    onCheckedChange={(value) =>
                      dispatch({ type: 'stale', value })
                    }
                  />
                </div>
                <div className="study-switch">
                  <label htmlFor={`${controlsId}-switch-4`}>音声ミュート</label>
                  <Switch
                    id={`${controlsId}-switch-4`}
                    checked={state.muted}
                    onCheckedChange={() => dispatch({ type: 'mute' })}
                  />
                </div>
                <div className="study-position-label">
                  <span>受信音量</span>
                  <output>{state.volume}%</output>
                </div>
                <Slider
                  aria-label="受信音量"
                  min={0}
                  max={100}
                  value={[state.volume]}
                  onValueChange={(v) =>
                    dispatch({
                      type: 'volume',
                      value: Array.isArray(v) ? v[0] : v,
                    })
                  }
                />
              </fieldset>
              <button
                className="study-reset"
                onClick={() => {
                  dispatch({ type: 'reset', id });
                  setRunning(false);
                }}
              >
                この案の初期状態に戻す
              </button>
            </div>
          </div>
        </DialogContent>
      </Dialog>
    </DemoProvider>
  );
}
