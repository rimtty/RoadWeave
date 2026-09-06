import { test } from 'node:test';
import assert from 'node:assert/strict';
import {
  initialState,
  reducer,
  voiceLabel,
  concepts,
  type Action,
} from '../lib/experience.ts';
const run = (...actions: Action[]) =>
  actions.reduce(reducer, structuredClone(initialState));
test('PTT requires a grant, and release before grant cannot start transmission', () => {
  let s = run({ type: 'scenario', scenario: 'quiet' }, { type: 'ptt-down' });
  assert.equal(s.voice, 'requesting');
  s = reducer(s, { type: 'ptt-up' });
  s = reducer(s, { type: 'ptt-granted' });
  assert.equal(s.voice, 'idle');
  assert.equal(s.held, false);
});
test('granted PTT stops on release and disconnection', () => {
  const s = run(
    { type: 'scenario', scenario: 'quiet' },
    { type: 'ptt-down' },
    { type: 'ptt-granted' },
  );
  assert.equal(s.voice, 'talking');
  for (const action of [
    { type: 'ptt-up' },
    { type: 'scenario', scenario: 'lost' },
    { type: 'sheet', sheet: 'volume' },
    { type: 'navigate', page: 'radar' },
  ] as Action[]) {
    const next = reducer(s, action);
    assert.equal(next.voice, 'idle');
    assert.equal(next.held, false);
  }
});
test('receive, disconnected, unjoined and empty groups never transmit', () => {
  const cases = [
    run(),
    run({ type: 'scenario', scenario: 'lost' }),
    run({ type: 'leave' }),
    run({ type: 'create', group: '朝のライド' }),
  ];
  for (const state of cases) {
    const s = reducer(reducer(state, { type: 'ptt-down' }), {
      type: 'ptt-granted',
    });
    assert.notEqual(s.voice, 'talking');
  }
  assert.equal(run({ type: 'ptt-down' }).voice, 'busy');
});
test('canceled or superseded group joins cannot complete late', () => {
  let s = run(
    { type: 'leave' },
    { type: 'sheet', sheet: 'join' },
    { type: 'join-start', group: '瀬戸内ライド' },
  );
  const id = s.joinId;
  s = reducer(s, { type: 'sheet', sheet: null });
  assert.equal(reducer(s, { type: 'join-complete', id }).joined, false);
  s = reducer(s, { type: 'join-start', group: '朝のライド' });
  assert.equal(reducer(s, { type: 'join-complete', id }).joined, false);
  assert.equal(
    reducer(s, { type: 'join-complete', id: s.joinId }).group,
    '朝のライド',
  );
});
test('selected PTT target and membership are cleared when leaving', () => {
  const s = run({ type: 'target', id: 'aki' }, { type: 'leave' });
  assert.equal(s.target, null);
  assert.equal(s.members.length, 0);
});
test('stale positions are separate from the voice connection', () => {
  const s = run({ type: 'stale', value: true });
  assert.equal(s.connected, true);
  assert.equal(s.remote, 'aki');
  assert.equal(s.stale, true);
});
test('volume is clamped and mute is reflected in the voice label', () => {
  assert.equal(run({ type: 'master-volume', value: 150 }).masterVolume, 100);
  assert.equal(run({ type: 'master-volume', value: NaN }).masterMuted, true);
  assert.match(voiceLabel(run({ type: 'peer-mute', id: 'aki' })), /消音/);
  assert.match(
    voiceLabel(run({ type: 'peer-volume', id: 'aki', value: -10 })),
    /消音/,
  );
  assert.equal(initialState.members[0].muted, false);
});
test('group names trim whitespace, reject blank and over 12 code points', () => {
  assert.equal(
    run({ type: 'create', group: '  朝のライド  ' }).group,
    '朝のライド',
  );
  assert.equal(run({ type: 'create', group: ' ' }).group, initialState.group);
  assert.equal(
    run({ type: 'create', group: 'あ'.repeat(13) }).group,
    initialState.group,
  );
  assert.equal(
    run({ type: 'create', group: '🚲'.repeat(12) }).members.length,
    0,
  );
  assert.equal(run({ type: 'create', group: '新規' }).scenario, 'quiet');
});
test('each concept has its own default view, while user audio settings persist', () => {
  for (const c of concepts) {
    const s = run(
      { type: 'master-volume', value: 35 },
      { type: 'design', design: c.id },
    );
    assert.equal(s.page, c.home);
    assert.equal(s.masterVolume, 35);
  }
});
