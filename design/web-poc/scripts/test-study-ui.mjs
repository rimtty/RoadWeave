import assert from 'node:assert/strict';
import { Window } from 'happy-dom';
import { createServer } from 'vite';
import reactPlugin from '@vitejs/plugin-react';
import path from 'node:path';
import fs from 'node:fs';
const window = new Window({ url: 'http://localhost:3000/explore' });
for (const key of [
  'window',
  'document',
  'navigator',
  'location',
  'history',
  'HTMLElement',
  'Element',
  'Node',
  'NodeFilter',
  'Event',
  'MouseEvent',
  'PointerEvent',
  'KeyboardEvent',
  'FocusEvent',
  'MutationObserver',
  'ResizeObserver',
  'IntersectionObserver',
  'getComputedStyle',
  'requestAnimationFrame',
  'cancelAnimationFrame',
]) {
  if (window[key])
    Object.defineProperty(globalThis, key, {
      value:
        typeof window[key] === 'function' &&
        [
          'getComputedStyle',
          'requestAnimationFrame',
          'cancelAnimationFrame',
        ].includes(key)
          ? window[key].bind(window)
          : window[key],
      configurable: true,
    });
}
globalThis.IS_REACT_ACT_ENVIRONMENT = true;
const server = await createServer({
  configFile: false,
  server: { middlewareMode: true },
  plugins: [reactPlugin()],
  resolve: {
    alias: {
      '@': process.cwd(),
      'next/link': path.resolve('tests/study-link.tsx'),
    },
  },
  ssr: { noExternal: ['@base-ui/react'] },
});
try {
  const React = await import('react');
  const { createRoot } = await import('react-dom/client');
  const { act } = React;
  const { default: Collection } = await server.ssrLoadModule(
    '/app/explore/collection.tsx',
  );
  const host = document.createElement('div');
  document.body.append(host);
  const root = createRoot(host);
  const motionStyle = document.createElement('style');
  motionStyle.textContent = fs.readFileSync(
    'app/explore/demo/studio.css',
    'utf8',
  );
  document.head.append(motionStyle);
  await act(async () => {
    root.render(React.createElement(Collection));
    await new Promise((r) => setTimeout(r, 30));
  });
  assert.equal(document.querySelectorAll('[data-demo-launch]').length, 60);
  assert.deepEqual(
    [...document.querySelectorAll('[data-demo-launch]')].map((el) =>
      Number(el.dataset.demoLaunch),
    ),
    Array.from({ length: 60 }, (_, i) => i + 1),
  );
  const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
  const dialog = () => document.querySelector('[role="dialog"]');
  const screen = () => dialog()?.querySelector('[data-demo-screen]');
  const screenText = () =>
    screen().textContent +
    ' ' +
    [...screen().querySelectorAll('svg[aria-label]')]
      .map((el) => el.getAttribute('aria-label'))
      .join(' ');
  const click = async (el) => {
    assert.ok(el, 'Control exists');
    await act(async () => {
      el.click();
    });
  };
  const button = (text) =>
    [...dialog().querySelectorAll('button')].find(
      (el) => el.textContent.trim() === text,
    );
  const switchEl = (label) =>
    [...dialog().querySelectorAll('.study-switch')]
      .find((el) => el.textContent.includes(label))
      ?.querySelector('[data-slot="switch"]');
  const setSwitch = async (label, value) => {
    const el = switchEl(label);
    assert.ok(el, label);
    if ((el.getAttribute('aria-checked') === 'true') !== value) await click(el);
  };
  const key = async (type, keyValue = ' ') =>
    act(async () => {
      dialog()
        .querySelector('.study-ptt')
        .dispatchEvent(
          new window.KeyboardEvent(type, {
            key: keyValue,
            bubbles: true,
            cancelable: true,
          }),
        );
    });
  const distanceIds = new Set(
    Array.from({ length: 60 }, (_, i) => i + 1).filter(
      (id) => ![5, 25, 26, 27].includes(id),
    ),
  );
  const dotIds = [13, 15, 16, 29, 30, 31, 39, 46];
  for (let id = 1; id <= 60; id++) {
    await click(document.querySelector(`[data-demo-launch="${id}"]`));
    assert.equal(Number(screen().dataset.demoScreen), id);
    await setSwitch('通信接続', true);
    await setSwitch('位置未更新', false);
    await click(button(id >= 55 ? 'ピンク' : 'REN'));
    assert.equal(screen().dataset.voice, 'RX', `#${id} receives`);
    const meterSelector = {
      5: '.rw-bars > i',
      12: '.rw-bars > i',
      14: '.rw-dot-wave circle[data-wave-lit=true]',
      26: '.dv-tx-indicator > i',
      45: '.pp-coral-mark > i',
      47: '.pp-echo-left > i',
    }[id];
    if (meterSelector) {
      const meter = screen().querySelector(meterSelector);
      assert.ok(meter, `#${id} audio graphic exists`);
      assert.match(
        window.getComputedStyle(meter).animation ||
          [...motionStyle.sheet.cssRules]
            .filter(
              (rule) =>
                rule.selectorText &&
                meter.matches(rule.selectorText.replace(/\s+/g, ' ')) &&
                rule.style.getPropertyValue('animation'),
            )
            .map((rule) => rule.style.getPropertyValue('animation'))
            .join(';'),
        /study-audio/,
        `#${id} meter animation is applied while receiving`,
      );
    }
    assert.match(
      screenText(),
      id >= 55 ? /ピンク/ : /REN|レン/,
      `#${id} renders selected member`,
    );
    if (id >= 55)
      assert.equal(screen().style.getPropertyValue('--ci-accent'), '#ff16a5');
    await click(button('500m'));
    if (distanceIds.has(id))
      assert.match(
        screenText(),
        /500/,
        `#${id} native display follows distance`,
      );
    if (dotIds.includes(id)) {
      const svg = screen().querySelector('svg[aria-label="500"]');
      assert.ok(svg, `#${id} dot digits are dynamic`);
      assert.ok(svg.querySelector('circle,rect'), `#${id} renders pixels`);
    }
    await click(
      [...dialog().querySelectorAll('[role="tab"]')].find(
        (el) => el.textContent === '車列・GPS',
      ),
    );
    assert.equal(screen().dataset.page, 'fleet', `#${id} has a second page`);
    assert.equal(
      screen().querySelector('[data-fleet-theme]').dataset.fleetTheme,
      String(id),
    );
    assert.equal(screen().querySelectorAll('[data-fleet-peer]').length, 4);
    assert.deepEqual(
      [...screen().querySelectorAll('[data-fleet-peer]')].map(
        (el) => el.dataset.fleetPeer,
      ),
      ['ren', 'aki', 'self', 'mei'],
      `#${id} queue is sorted by relative position`,
    );
    assert.match(screenText(), /500/, `#${id} fleet distances update`);
    assert.equal(screen().querySelectorAll('[data-gps-peer]').length, 4);
    await click(button('GPSを拡大'));
    assert.ok(
      screen().querySelector('.fleet-expanded'),
      `#${id} map expands in its own theme`,
    );
    await click(button('車列に戻る'));
    const gpsBefore = screen()
      .querySelector('[data-gps-peer="ren"]')
      .getAttribute('transform');
    await click(button('-240m'));
    assert.notEqual(
      screen().querySelector('[data-gps-peer="ren"]').getAttribute('transform'),
      gpsBefore,
      `#${id} GPS follows relative position`,
    );
    assert.deepEqual(
      [...screen().querySelectorAll('[data-fleet-peer]')].map(
        (el) => el.dataset.fleetPeer,
      ),
      ['aki', 'self', 'ren', 'mei'],
      `#${id} queue reorders`,
    );
    await setSwitch('位置未更新', true);
    assert.equal(
      screen().querySelectorAll('[data-gps-peer]').length,
      1,
      `#${id} stale peers hidden`,
    );
    assert.match(screenText(), /車列は確認できません/);
    assert.match(screenText(), /—/);
    await setSwitch('位置未更新', false);
    await click(
      [...dialog().querySelectorAll('[role="tab"]')].find(
        (el) => el.textContent === '声・距離',
      ),
    );
    assert.equal(screen().dataset.page, 'voice');
    await click(button('-240m'));
    if (distanceIds.has(id))
      assert.match(
        screenText(),
        /240/,
        `#${id} renders negative position as magnitude`,
      );
    if ([3, 4, 15].includes(id)) {
      const marker = screen().querySelector('[data-peer-position="primary"]');
      assert.ok(marker);
      const before = marker.getAttribute('transform');
      await click(button('500m'));
      assert.notEqual(
        marker.getAttribute('transform'),
        before,
        `#${id} marker moves across self`,
      );
    }
    await setSwitch('位置未更新', true);
    assert.equal(
      screen().dataset.stale,
      'true',
      switchEl('位置未更新').outerHTML,
    );
    if (distanceIds.has(id))
      assert.match(screenText(), /—/, `#${id} stale position is hidden`);
    await setSwitch('位置未更新', false);
    assert.equal(
      screen().dataset.audioActive,
      'true',
      JSON.stringify({
        id,
        voice: screen().dataset.voice,
        html: screenText(),
        switches: [...dialog().querySelectorAll('[data-slot=switch]')].map(
          (e) => [e.outerHTML],
        ),
      }),
    );
    await setSwitch('音声ミュート', true);
    assert.equal(screen().dataset.audioActive, 'false');
    await setSwitch('音声ミュート', false);
    await key('keydown');
    assert.equal(dialog().querySelector('.study-ptt').dataset.held, 'true');
    assert.match(
      dialog().querySelector('.study-device-status').textContent,
      /待機/,
    );
    await key('keyup');
    await click(button('受信を終えて待受にする'));
    assert.equal(screen().dataset.audioActive, 'false');
    if (meterSelector)
      assert.doesNotMatch(
        window.getComputedStyle(screen().querySelector(meterSelector))
          .animation,
        /study-audio/,
        `#${id} meter stops on idle`,
      );
    await key('keydown');
    await act(async () => {
      await delay(280);
    });
    assert.equal(screen().dataset.voice, 'TX', `#${id} PTT transmits`);
    assert.equal(screen().dataset.audioActive, 'true');
    await key('keyup');
    assert.equal(screen().dataset.voice, 'IDLE');
    assert.equal(screen().dataset.audioActive, 'false');
    await setSwitch('通信接続', false);
    assert.equal(screen().dataset.voice, 'OFF');
    assert.equal(dialog().querySelector('.study-ptt').disabled, true);
    assert.equal(screen().dataset.audioActive, 'false');
    if (distanceIds.has(id))
      assert.match(screenText(), /—/, `#${id} disconnected position is hidden`);
    await setSwitch('通信接続', true);
    await click(button('この案の初期状態に戻す'));
    await click(dialog().querySelector('[data-slot="dialog-close"]'));
    assert.equal(dialog(), null, `#${id} closes`);
    if (id % 10 === 0)
      console.log(
        `PASS ${id}/60 native demos: speaker, distance, stale, mute, busy, PTT, disconnect, reset, close`,
      );
  }
  await click(document.querySelector('[data-demo-launch="46"]'));
  const slider = dialog().querySelector('[data-slot="slider"] input');
  assert.ok(slider);
  await click(button('500m'));
  for (let count = 0; count < 9; count++)
    await act(async () => {
      slider.dispatchEvent(
        new window.KeyboardEvent('keydown', {
          key: 'ArrowRight',
          bubbles: true,
          cancelable: true,
        }),
      );
    });
  assert.ok(
    screen().querySelector('svg[aria-label="509"]'),
    'Slider updates dot numeral geometry to 509',
  );
  await click(button('受信を終えて待受にする'));
  await key('keydown');
  await key('keyup');
  await act(async () => {
    await delay(280);
  });
  assert.equal(
    screen().dataset.voice,
    'IDLE',
    'Short press cannot start delayed transmission',
  );
  await key('keydown');
  await act(async () => {
    await delay(280);
  });
  await act(async () => {
    window.dispatchEvent(new window.Event('blur'));
  });
  assert.equal(screen().dataset.voice, 'IDLE', 'Window blur releases PTT');
  await key('keydown');
  await act(async () => {
    await delay(280);
  });
  await click(
    [...dialog().querySelectorAll('[role="tab"]')].find(
      (el) => el.textContent === '車列・GPS',
    ),
  );
  assert.equal(screen().dataset.voice, 'IDLE', 'Page switch releases PTT');
  await click(
    [...dialog().querySelectorAll('[role="tab"]')].find(
      (el) => el.textContent === '声・距離',
    ),
  );
  await act(async () => {
    dialog()
      .querySelector('.study-ptt')
      .dispatchEvent(
        new window.PointerEvent('pointerdown', {
          button: 0,
          pointerId: 1,
          bubbles: true,
          cancelable: true,
        }),
      );
  });
  await act(async () => {
    await delay(280);
  });
  assert.equal(screen().dataset.voice, 'TX', 'Pointer holds PTT');
  await act(async () => {
    dialog()
      .querySelector('.study-ptt')
      .dispatchEvent(
        new window.PointerEvent('pointercancel', {
          pointerId: 1,
          bubbles: true,
        }),
      );
  });
  assert.equal(
    screen().dataset.voice,
    'IDLE',
    'Pointer cancellation stops PTT',
  );
  await setSwitch('グループに参加', false);
  assert.equal(screen().dataset.voice, 'OFF');
  assert.equal(dialog().querySelector('.study-ptt').disabled, true);
  await setSwitch('グループに参加', true);
  await key('keydown');
  await act(async () => {
    await delay(280);
  });
  await click(dialog().querySelector('[data-slot="dialog-close"]'));
  await click(document.querySelector('[data-demo-launch="46"]'));
  assert.notEqual(
    screen().dataset.voice,
    'TX',
    'Closing cannot retain transmission',
  );
  await click(dialog().querySelector('[data-slot="dialog-close"]'));
  const boardTabs = [...host.querySelectorAll('[role="tab"]')];
  await click(boardTabs.find((el) => el.textContent === '車列・GPS'));
  assert.equal(
    host.querySelectorAll('[data-fleet-theme]').length,
    60,
    'Board compares all 60 fleet screens',
  );
  await click(boardTabs.find((el) => el.textContent === '声・距離'));
  assert.equal(
    host.querySelectorAll('[data-fleet-theme]').length,
    0,
    'Board returns to all voice screens',
  );
  await act(async () => {
    history.replaceState(null, '', '/explore?demo=55&screen=fleet');
    window.dispatchEvent(new window.Event('popstate'));
  });
  assert.equal(screen().dataset.demoScreen, '55');
  assert.equal(
    screen().dataset.page,
    'fleet',
    'Direct link selects fleet screen',
  );
  await click(dialog().querySelector('[data-slot="dialog-close"]'));
  const { DemoDigits, glyphs } = await server.ssrLoadModule(
    '/app/explore/demo/graphics.tsx',
  );
  const { renderToStaticMarkup } = await import('react-dom/server');
  const glyphRenders = new Set();
  for (const digit of '0123456789—') {
    const markup = renderToStaticMarkup(
      React.createElement(DemoDigits, { text: digit }),
    );
    assert.match(markup, /<circle/);
    const dots = (markup.match(/<circle/g) || []).length;
    assert.equal(
      dots,
      glyphs[digit]
        .join('')
        .split('')
        .filter((pixel) => pixel === '1').length,
    );
    glyphRenders.add(markup);
  }
  assert.equal(glyphRenders.size, 11);
  assert.ok(
    motionStyle.textContent.includes('prefers-reduced-motion'),
    'Reduced-motion presentation is defined',
  );
  await act(async () => {
    root.unmount();
  });
  console.log(
    'PASS 60/60: real component events, all native screens, dynamic dot numerals, audio activation, cancellation.',
  );
} finally {
  await server.close();
  await window.happyDOM.close();
}
