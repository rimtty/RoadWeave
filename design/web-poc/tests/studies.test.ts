import { test } from 'node:test';
import assert from 'node:assert/strict';
import {
  initialDemo,
  demoReducer,
  demoView,
  demoMembers,
  type DemoAction,
} from '../app/explore/demo/model.ts';
import {
  fleetThemes,
  fleetView,
  gpsPoint,
  fleetMapLabels,
} from '../app/explore/demo/fleet-model.ts';
const run = (...actions: DemoAction[]) =>
  actions.reduce(demoReducer, initialDemo(55));
test('GPS labels remain separate for coincident vehicles and near map edges', () => {
  for (const height of [48, 160, 180]) {
    for (const positions of [
      [30, 30, 30, 30],
      [1, 2, 3, 4],
      [height - 4, height - 3, height - 2, height - 1],
    ]) {
      const points = (['aki', 'ren', 'mei', 'self'] as const).map((id, i) => ({
        id,
        rank: i + 1,
        x: 108,
        y: positions[i],
      }));
      const original = structuredClone(points);
      const labels = [...fleetMapLabels(points, height, 40).values()];
      assert.deepEqual(
        points,
        original,
        'Label placement must not move GPS points',
      );
      assert.equal(labels.length, 4);
      for (const p of labels) assert.ok(p.y >= 12 && p.y <= height - 12);
      for (const side of [true, false]) {
        const lane = labels.filter((p) => p.left === side);
        assert.ok(Math.abs(lane[0].y - lane[1].y) >= 22);
      }
    }
  }
});
test('Every study has an explicit fleet theme and coherent initial state', () => {
  assert.deepEqual(
    Object.keys(fleetThemes).map(Number),
    Array.from({ length: 60 }, (_, i) => i + 1),
  );
  for (let id = 1; id <= 60; id++) {
    assert.ok(fleetThemes[id].accent);
    assert.equal(
      new Set(fleetView(initialDemo(id)).peers.map((p) => p.id)).size,
      4,
    );
  }
});
test('PTT grant is conditional; short press, disconnect, leave, target change and release cancel', () => {
  const pending = run({ type: 'quiet' }, { type: 'down' });
  assert.equal(pending.mode, 'requesting');
  for (const action of [
    { type: 'up' },
    { type: 'connect', value: false },
    { type: 'leave' },
    { type: 'target', value: 'ren' },
  ] as DemoAction[]) {
    let s = demoReducer(pending, action);
    s = demoReducer(s, { type: 'grant' });
    assert.notEqual(s.mode, 'transmitting');
    assert.equal(s.held, false);
  }
  assert.equal(demoReducer(pending, { type: 'grant' }).mode, 'transmitting');
});
test('Remote speech owns the channel and is not interrupted by PTT', () => {
  let s = run(
    { type: 'speaker', id: 'ren' },
    { type: 'down' },
    { type: 'grant' },
  );
  assert.equal(s.mode, 'busy');
  assert.equal(s.remote, 'ren');
  s = demoReducer(s, { type: 'up' });
  assert.equal(s.mode, 'receiving');
});
test('All members and distances update text and direction; stale never exposes old coordinates', () => {
  for (const peer of demoMembers) {
    for (const along of [-999, -240, 0, 1, 367, 509, 999]) {
      let s = run(
        { type: 'speaker', id: peer.id },
        { type: 'position', id: peer.id, value: along },
      );
      const v = demoView(s);
      assert.equal(v.name, peer.name);
      assert.equal(v.distance, String(Math.abs(along)));
      assert.equal(
        v.direction,
        along < 0 ? '後方' : along > 0 ? '前方' : 'すぐそば',
      );
      s = demoReducer(s, { type: 'stale', value: true });
      assert.equal(demoView(s).distance, '—');
      assert.equal(fleetView(s).available, false);
    }
  }
});
test('Queue ranks, absolute GPS, stable colors and zero position remain coherent', () => {
  let s = run({ type: 'position', id: 'ren', value: 500 });
  assert.deepEqual(
    fleetView(s).peers.map((p) => p.id),
    ['ren', 'aki', 'self', 'mei'],
  );
  const gps = fleetView(s).peers[0];
  assert.ok(gps.latitude > 35.2);
  s = demoReducer(s, { type: 'position', id: 'ren', value: -500 });
  assert.deepEqual(
    fleetView(s).peers.map((p) => p.id),
    ['aki', 'self', 'mei', 'ren'],
  );
  assert.deepEqual(gpsPoint(0), {
    latitude: 35.2,
    longitude: 139,
    east: 0,
    north: 0,
  });
  for (const peer of demoMembers) {
    const v = demoView(run({ type: 'speaker', id: peer.id }), true);
    assert.equal(v.name, peer.colorName);
    assert.equal(v.color, peer.color);
  }
});
test('Audio activation respects mute and volume while connection stays independent of stale GPS', () => {
  assert.equal(demoView(run()).audible, true);
  assert.equal(demoView(run({ type: 'mute' })).audible, false);
  assert.equal(demoView(run({ type: 'volume', value: 0 })).audible, false);
  assert.equal(demoView(run({ type: 'connect', value: false })).audible, false);
  assert.equal(demoView(run({ type: 'stale', value: true })).audible, true);
  assert.equal(
    run({ type: 'position', id: 'aki', value: Infinity }).positions.aki,
    120,
  );
});
