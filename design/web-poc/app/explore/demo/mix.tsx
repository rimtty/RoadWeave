'use client';
import type { CSSProperties } from 'react';
import { useStudy, useView } from './bindings';
import { fleetThemes } from './fleet-model';
import { demoMembers } from './model';
import { DemoDigits } from './graphics';
import './mix.css';

export function MixScreen() {
  const { state, id } = useStudy();
  const view = useView();
  const theme = fleetThemes[id];
  const speakers = [
    ...state.remotes.map((member) => demoMembers.find((p) => p.id === member)!),
    ...(state.mode === 'transmitting'
      ? [
          {
            id: 'self',
            name: '自分',
            colorName: '自分',
            color: '#ff6b00',
            japanese: '自分',
          },
        ]
      : []),
  ];
  return (
    <div
      className={`mix-view mix-${theme.layout} mix-${theme.edge}`}
      data-mix-theme={id}
      style={
        {
          '--mix-accent': id >= 55 ? view.color : theme.accent,
          '--mix-surface': theme.surface,
          '--mix-ink': theme.ink,
          '--mix-muted': theme.muted,
        } as CSSProperties
      }
    >
      <header className="mix-heading">
        <strong>同時通話</strong>
        <span>
          <b>{view.activeCount}</b> / 3
        </span>
      </header>
      <div className="mix-caption">
        <span>
          {state.mode === 'transmitting'
            ? '自分も送信中'
            : state.mode === 'busy'
              ? '送信の空き待ち'
              : '仲間の声を受信中'}
        </span>
        <span>
          {state.muted || state.volume === 0 ? '受信ミュート' : 'LIVE'}
        </span>
      </div>
      <ol className="mix-speakers" aria-label="現在発話している人">
        {speakers.map((peer) => {
          const self = peer.id === 'self';
          const position = self
            ? 0
            : state.positions[peer.id as keyof typeof state.positions];
          const distance = state.stale ? '—' : String(Math.abs(position));
          return (
            <li
              key={peer.id}
              data-mix-peer={peer.id}
              data-self={self}
              data-audible={self || view.audible}
              style={
                {
                  '--mix-member': id >= 55 || self ? peer.color : theme.accent,
                } as CSSProperties
              }
            >
              <div className="mix-person">
                <strong>
                  {id >= 55
                    ? peer.colorName
                    : id === 20
                      ? peer.japanese
                      : peer.name}
                </strong>
                <span>{self ? '送信中' : '受信中'}</span>
              </div>
              <div className="mix-wave" aria-hidden="true">
                {Array.from({ length: 7 }, (_, n) => (
                  <i key={n} style={{ '--bar': n } as CSSProperties} />
                ))}
              </div>
              <div className="mix-position">
                {self ? (
                  <span>PTTを離すと終了</span>
                ) : (
                  <>
                    <span>
                      {state.stale
                        ? '位置未更新'
                        : position > 0
                          ? '前方'
                          : position < 0
                            ? '後方'
                            : 'すぐそば'}
                    </span>
                    <strong>
                      {theme.pixels ? (
                        <DemoDigits text={distance} square />
                      ) : (
                        distance
                      )}
                      <small>m</small>
                    </strong>
                  </>
                )}
              </div>
            </li>
          );
        })}
      </ol>
      <footer className="mix-footer">
        <strong>
          {state.mode === 'busy'
            ? '押したままで空きを待つ'
            : state.mode === 'requesting'
              ? '送信を準備中'
              : state.mode === 'transmitting'
                ? '声を重ねて、話せます'
                : view.activeCount < 3
                  ? 'PTTで会話に参加'
                  : '3人が発話中'}
        </strong>
        <span>自分を含めて、最大3人</span>
      </footer>
    </div>
  );
}
