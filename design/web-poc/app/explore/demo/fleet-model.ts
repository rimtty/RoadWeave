import type { DemoState, MemberId } from './model.ts';
import { demoMembers } from './model.ts';
export type FleetLayout = 'stack' | 'map-first' | 'sidebar' | 'tiles' | 'radar';
export type FleetTheme = {
  accent: string;
  surface: string;
  ink: string;
  muted: string;
  layout: FleetLayout;
  edge: 'soft' | 'sharp' | 'line' | 'curve' | 'dots';
  pixels: boolean;
};
// Each entry follows its corresponding study's palette and geometry, not a global light/dark toggle.
const specs: [
  string,
  string,
  string,
  string,
  FleetLayout,
  FleetTheme['edge'],
][] = [
  ['#527cf0', '#f4f7fb', '#293449', '#67748b', 'stack', 'soft'],
  ['#c8ef87', '#17221a', '#e9f0df', '#b8c5ad', 'sidebar', 'sharp'],
  ['#b9e96c', '#101a32', '#e8edf7', '#aebbd3', 'radar', 'soft'],
  ['#3479ed', '#e9efdd', '#253d2b', '#50644e', 'map-first', 'soft'],
  ['#897dc2', '#edeafb', '#423c64', '#77718d', 'stack', 'soft'],
  ['#414b39', '#f4f1e8', '#31382c', '#6d7564', 'stack', 'line'],
  ['#719571', '#f2f6ee', '#293f2a', '#687b61', 'sidebar', 'soft'],
  ['#111a0e', '#d4ef87', '#182410', '#4e6038', 'stack', 'sharp'],
  ['#a999ff', '#171429', '#efebff', '#bfb4d8', 'radar', 'soft'],
  ['#d7dcac', '#343e2d', '#e8e9d3', '#b5bc9e', 'radar', 'line'],
  ['#81a7ef', '#edf1f7', '#293c57', '#6b7e96', 'tiles', 'soft'],
  ['#a8bb78', '#2e332a', '#d8dfbd', '#a3ac8d', 'stack', 'sharp'],
  ['#ff513d', '#080a08', '#e3e5db', '#b1b6a7', 'stack', 'dots'],
  ['#ff513d', '#080a08', '#e3e5db', '#b1b6a7', 'map-first', 'dots'],
  ['#ff513d', '#080a08', '#e3e5db', '#b1b6a7', 'radar', 'dots'],
  ['#ff513d', '#080a08', '#e3e5db', '#b1b6a7', 'tiles', 'dots'],
  ['#2878ed', '#ffffff', '#1a2534', '#5e6d7f', 'stack', 'soft'],
  ['#b6e888', '#15201a', '#f0f5e9', '#bdcbb8', 'stack', 'sharp'],
  ['#74c9b4', '#112423', '#eef7f2', '#afc6bd', 'radar', 'soft'],
  ['#527dde', '#f4f6fa', '#243655', '#69788d', 'sidebar', 'line'],
  ['#c9ed8e', '#152016', '#f2f6e9', '#b9c7ac', 'map-first', 'sharp'],
  ['#9ab7fb', '#151b2b', '#f1f4fc', '#b5bed6', 'stack', 'line'],
  ['#748f71', '#f1f4ec', '#2c3d29', '#63725e', 'sidebar', 'soft'],
  ['#b1e3d1', '#102521', '#eef8f2', '#afc7bd', 'map-first', 'sharp'],
  ['#d9efc4', '#244235', '#f2f6e9', '#b6ccb8', 'stack', 'soft'],
  ['#ff8f7b', '#291816', '#fff2eb', '#d8b6a8', 'stack', 'sharp'],
  ['#bbc3c5', '#161b20', '#ecf0f1', '#aebdc1', 'sidebar', 'line'],
  ['#efbc75', '#242018', '#f6eddb', '#cdbfa2', 'stack', 'line'],
  ['#ff665a', '#080a08', '#f4f5ee', '#bbc5b4', 'stack', 'dots'],
  ['#356cc0', '#f6f8f4', '#22344b', '#647b88', 'map-first', 'dots'],
  ['#e8b67a', '#100d08', '#f4e0bf', '#c4ad89', 'sidebar', 'dots'],
  ['#d1d9c8', '#0c100c', '#eff2e8', '#b4c3aa', 'stack', 'dots'],
  ['#e6eee9', '#000000', '#f0f2ef', '#acb9b0', 'stack', 'line'],
  ['#80b7ff', '#000000', '#f0f2ef', '#acb9b0', 'sidebar', 'line'],
  ['#cfcaed', '#000000', '#f0f2ef', '#b7b4c7', 'map-first', 'line'],
  ['#b7d5b4', '#000000', '#f0f2ef', '#acb9b0', 'sidebar', 'soft'],
  ['#e3b688', '#000000', '#f0f2ef', '#c8b4a0', 'radar', 'line'],
  ['#fb8b79', '#000000', '#f0f2ef', '#b9aca7', 'stack', 'line'],
  ['#a3d9e1', '#000000', '#e4f5f5', '#afc4c8', 'stack', 'dots'],
  ['#bcc6fe', '#000000', '#f0f2ef', '#b5bdd5', 'tiles', 'line'],
  ['#f4e572', '#000000', '#f4f4ec', '#c6c5ad', 'radar', 'soft'],
  ['#c5f279', '#000000', '#f3f5ed', '#c0c9b1', 'map-first', 'sharp'],
  ['#9adcff', '#000000', '#f4f5f7', '#b8cddd', 'stack', 'soft'],
  ['#ffaccd', '#000000', '#f8f1f6', '#cdc0d5', 'tiles', 'soft'],
  ['#ffa08a', '#000000', '#f9f3ef', '#d5c0b7', 'sidebar', 'soft'],
  ['#9bf0ce', '#000000', '#effaf5', '#bbd5cc', 'stack', 'dots'],
  ['#c2a9ff', '#000000', '#f5f0ff', '#c5bada', 'radar', 'soft'],
  ['#c8fa76', '#000000', '#f3f7ed', '#c7d0bb', 'stack', 'sharp'],
  ['#4385ff', '#000000', '#f5f8ff', '#b6c4e0', 'map-first', 'curve'],
  ['#4defbe', '#000000', '#effbf6', '#b1d3c7', 'sidebar', 'curve'],
  ['#fa54dc', '#000000', '#fff2fc', '#d1b5cf', 'stack', 'curve'],
  ['#e2f24e', '#000000', '#f9fbea', '#c8ccab', 'map-first', 'curve'],
  ['#b1a0ff', '#000000', '#f6f1ff', '#c8bfd9', 'radar', 'curve'],
  ['#7de5d6', '#000000', '#f1fbf8', '#b5d0ca', 'tiles', 'curve'],
  ['#c6ff00', '#000000', '#ffffff', '#bfbfbf', 'stack', 'sharp'],
  ['#c6ff00', '#000000', '#ffffff', '#bfbfbf', 'sidebar', 'line'],
  ['#ff16a5', '#000000', '#ffffff', '#bfbfbf', 'stack', 'curve'],
  ['#c6ff00', '#000000', '#ffffff', '#bfbfbf', 'tiles', 'line'],
  ['#397dff', '#000000', '#ffffff', '#bfbfbf', 'map-first', 'curve'],
  ['#c6ff00', '#000000', '#ffffff', '#bfbfbf', 'stack', 'line'],
];
export const fleetThemes: Record<number, FleetTheme> = Object.fromEntries(
  specs.map(([accent, surface, ink, muted, layout, edge], i) => [
    i + 1,
    {
      accent,
      surface,
      ink,
      muted,
      layout,
      edge,
      pixels: [13, 14, 15, 16, 29, 30, 31, 39, 46].includes(i + 1),
    },
  ]),
);
export type FleetPeer = {
  id: MemberId | 'self';
  name: string;
  color: string;
  along: number;
  distance: number;
  latitude: number;
  longitude: number;
  rank: number;
};
// Move labels, never GPS markers. Each side has its own lane so nearby or
// coincident vehicles keep readable labels without implying a false position.
export function fleetMapLabels(
  points: { id: FleetPeer['id']; x: number; y: number; rank: number }[],
  height: number,
  offset: number,
) {
  const result = new Map<
    FleetPeer['id'],
    { x: number; y: number; left: boolean }
  >();
  for (const left of [true, false]) {
    const lane = points
      .filter((p) => Boolean(p.rank % 2) === left)
      .sort((a, b) => a.y - b.y || a.rank - b.rank);
    const top = 12;
    const bottom = height - 12;
    const gap = 22;
    const ys = lane.map((p, i) =>
      Math.max(top + i * gap, Math.min(bottom, p.y)),
    );
    for (let i = 1; i < ys.length; i++)
      ys[i] = Math.max(ys[i], ys[i - 1] + gap);
    if (ys.length) ys[ys.length - 1] = Math.min(bottom, ys[ys.length - 1]);
    for (let i = ys.length - 2; i >= 0; i--)
      ys[i] = Math.min(ys[i], ys[i + 1] - gap);
    lane.forEach((p, i) =>
      result.set(p.id, { x: 108 + (left ? -offset : offset), y: ys[i], left }),
    );
  }
  return result;
}
// A fictional north-oriented GPS trace, in metres from the device; no map or location service is contacted.
export function gpsPoint(along: number) {
  const north = along * 0.92;
  const east = Math.sin(along / 210) * 72;
  return {
    latitude: 35.2 + north / 111320,
    longitude: 139.0 + east / (111320 * Math.cos((35.2 * Math.PI) / 180)),
    east,
    north,
  };
}
export function fleetView(s: DemoState) {
  const peers = [
    ...demoMembers.map((p) => ({
      id: p.id,
      name: p.name,
      color: p.color,
      along: s.positions[p.id],
      distance: Math.abs(s.positions[p.id]),
      ...gpsPoint(s.positions[p.id]),
    })),
    {
      id: 'self' as const,
      name: '自分',
      color: '#ff6b00',
      along: 0,
      distance: 0,
      ...gpsPoint(0),
    },
  ]
    .sort(
      (a, b) =>
        b.along - a.along ||
        ['aki', 'ren', 'mei', 'self'].indexOf(a.id) -
          ['aki', 'ren', 'mei', 'self'].indexOf(b.id),
    )
    .map((p, i) => ({ ...p, rank: i + 1 }));
  return {
    peers,
    range: Math.max(300, ...peers.map((p) => Math.abs(p.along))) * 1.15,
    available: s.connected && s.joined && !s.stale,
  };
}
