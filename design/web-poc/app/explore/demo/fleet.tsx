'use client';
/* oxlint-disable jsx-a11y/prefer-tag-over-role -- The route is an inline SVG map with an accessible description. */
import { useState, type CSSProperties } from 'react';
import { useStudy, useView } from './bindings';
import { DemoDigits, DemoVoiceIcon } from './graphics';
import { demoMembers } from './model';
import {
  fleetThemes,
  fleetView,
  fleetMapLabels,
  gpsPoint,
  type FleetPeer,
} from './fleet-model';
import './fleet.css';
function FleetDistance({ value, pixels }: { value: string; pixels: boolean }) {
  return (
    <strong className="fleet-distance">
      {pixels ? <DemoDigits text={value} square /> : value}
      <small>m</small>
    </strong>
  );
}
export function FleetScreen() {
  const [expanded, setExpanded] = useState(false);
  const { state, id } = useStudy();
  const view = useView();
  const theme = fleetThemes[id];
  const fleet = fleetView(state);
  const colorOnly = id >= 55;
  const label = (peer: FleetPeer) =>
    peer.id === 'self'
      ? '自分'
      : colorOnly
        ? demoMembers.find((p) => p.id === peer.id)!.colorName
        : peer.name;
  const screenPeers = fleet.available
    ? fleet.peers
    : [...fleet.peers].sort(
        (a, b) =>
          ['aki', 'ren', 'self', 'mei'].indexOf(a.id) -
          ['aki', 'ren', 'self', 'mei'].indexOf(b.id),
      );
  const mapHeight = expanded ? 180 : theme.layout === 'sidebar' ? 160 : 48;
  const mapCenter = mapHeight / 2;
  const mapScale =
    (mapCenter - (expanded || theme.layout === 'sidebar' ? 14 : 4)) /
    fleet.range;
  const p2 = (along: number) => {
    const p = gpsPoint(along);
    return [108 + p.east * mapScale, mapCenter - p.north * mapScale];
  };
  const route = Array.from({ length: 61 }, (_, i) =>
    p2(-fleet.range + (i * fleet.range) / 30).join(','),
  ).join(' ');
  const selected = fleet.peers.find((p) => p.id === state.selected)!;
  const mapPeers = fleet.peers.filter(
    (peer) => peer.id === 'self' || fleet.available,
  );
  const labels = fleetMapLabels(
    mapPeers.map((peer) => {
      const [x, y] = p2(peer.along);
      return { ...peer, x, y };
    }),
    mapHeight,
    !expanded && theme.layout === 'sidebar' ? 28 : 40,
  );
  return (
    <div
      className={`fleet-view fleet-${theme.layout} fleet-${theme.edge} ${expanded ? 'fleet-expanded' : ''}`}
      data-fleet-theme={id}
      style={
        {
          '--fleet-accent': colorOnly ? view.color : theme.accent,
          '--fleet-surface': theme.surface,
          '--fleet-ink': theme.ink,
          '--fleet-muted': theme.muted,
        } as CSSProperties
      }
    >
      <header className="fleet-heading">
        <strong>車列・GPS</strong>
        <span>
          <DemoVoiceIcon aria-hidden="true" />
          {view.voice}
        </span>
      </header>
      <div className="fleet-overview">
        <span>
          {fleet.available
            ? `自分は ${fleet.peers.find((p) => p.id === 'self')!.rank} / 4 番目`
            : '車列は確認できません'}
        </span>
        <span>{fleet.available ? 'GPS 更新中' : '位置未更新'}</span>
      </div>
      <div className="fleet-composition">
        <ol className="fleet-list" aria-label="進行方向の前から順に並ぶ車列">
          {screenPeers.map((peer) => (
            <li
              key={peer.id}
              data-fleet-peer={peer.id}
              data-selected={peer.id === state.selected}
              data-self={peer.id === 'self'}
              data-speaking={
                peer.id === 'self'
                  ? state.mode === 'transmitting'
                  : state.remotes.includes(peer.id)
              }
              style={
                {
                  '--fleet-member': colorOnly
                    ? peer.color
                    : peer.id === 'self'
                      ? theme.ink
                      : theme.accent,
                } as CSSProperties
              }
            >
              <span className="fleet-rank">
                {fleet.available ? peer.rank : '—'}
              </span>
              <div className="fleet-person">
                <b>{label(peer)}</b>
                <span>
                  {peer.id === 'self'
                    ? '基準位置'
                    : !fleet.available
                      ? '未更新'
                      : peer.along > 0
                        ? '前方'
                        : peer.along < 0
                          ? '後方'
                          : 'すぐそば'}
                </span>
              </div>
              <FleetDistance
                value={
                  peer.id === 'self'
                    ? '0'
                    : fleet.available
                      ? String(peer.distance)
                      : '—'
                }
                pixels={theme.pixels}
              />
            </li>
          ))}
        </ol>
        <div className="fleet-map-shell">
          <div className="fleet-map-top">
            <button
              type="button"
              onClick={() => setExpanded(!expanded)}
              aria-expanded={expanded}
            >
              {expanded ? '車列に戻る' : 'GPSを拡大'}
            </button>
            <span>N ↑</span>
          </div>
          <svg
            className="fleet-map"
            viewBox={
              !expanded && theme.layout === 'sidebar'
                ? `60 0 96 ${mapHeight}`
                : `0 0 216 ${mapHeight}`
            }
            role="img"
            aria-label={
              fleet.available
                ? `仮のGPS位置。${label(selected)}は${view.direction}${view.distance}m。自分は北緯35.20000、東経139.00000。`
                : '位置未更新。仲間の地図上の位置は非表示。'
            }
          >
            {theme.layout === 'radar' ? (
              <g className="fleet-grid">
                <circle cx="108" cy={mapCenter} r={mapCenter * 0.4} />
                <circle cx="108" cy={mapCenter} r={mapCenter * 0.8} />
                <path
                  d={`M108 4V${mapHeight - 4}M${108 - mapCenter} ${mapCenter}H${108 + mapCenter}`}
                />
              </g>
            ) : (
              <g className="fleet-grid">
                <path
                  d={`M0 ${mapCenter / 2}H216M0 ${mapCenter}H216M0 ${mapCenter * 1.5}H216M54 0V${mapHeight}M108 0V${mapHeight}M162 0V${mapHeight}`}
                />
              </g>
            )}
            <polyline className="fleet-route-under" points={route} />
            <polyline className="fleet-route" points={route} />
            {mapPeers.map((peer) => {
              const [x, y] = p2(peer.along);
              const placement = labels.get(peer.id)!;
              const lx = placement.x - x;
              const ly = placement.y - y;
              return (
                <g
                  key={peer.id}
                  data-gps-peer={peer.id}
                  transform={`translate(${x} ${y})`}
                  style={{
                    color: colorOnly
                      ? peer.color
                      : peer.id === 'self'
                        ? theme.ink
                        : theme.accent,
                  }}
                >
                  <path
                    className="fleet-map-leader"
                    d={`M0 0L${lx + (placement.left ? 5 : -5)} ${ly}`}
                  />
                  {(peer.id === 'self' || peer.id === state.selected) && (
                    <circle
                      r={peer.id === 'self' ? 8 : 7}
                      className="fleet-map-halo"
                    />
                  )}
                  <circle
                    r={peer.id === 'self' ? 3.5 : 3}
                    fill="currentColor"
                  />
                  <text
                    x={lx}
                    y={ly}
                    dominantBaseline="central"
                    textAnchor={placement.left ? 'end' : 'start'}
                  >
                    {expanded
                      ? label(peer)
                      : !fleet.available
                        ? '自'
                        : peer.rank}
                  </text>
                </g>
              );
            })}
          </svg>
          <div className="fleet-map-bottom">
            <span>自分を基準に表示</span>
            <span>±{Math.round(fleet.range)}m</span>
          </div>
        </div>
      </div>
      <footer className="fleet-coordinates">
        <span>
          <span className="fleet-location-caption">
            <span>{fleet.available ? label(selected) : 'GPS'}</span>
            <span>発話 {view.activeCount}/3</span>
          </span>
          <b>
            {fleet.available
              ? `${selected.latitude.toFixed(5)}, ${selected.longitude.toFixed(5)}`
              : '位置を再取得してください'}
          </b>
        </span>
        <span>仮GPS</span>
      </footer>
    </div>
  );
}
