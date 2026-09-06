import { chromium, expect } from '@playwright/test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';

const output = 'outputs/voice-mix';
await fs.mkdir(output, { recursive: true });
const browser = await chromium.launch({
  channel: process.env.BROWSER_CHANNEL || 'chrome',
  headless: true,
});
const page = await browser.newPage({ viewport: { width: 1280, height: 960 } });
const failures = [];
let checked = 0;
page.on('pageerror', (error) => failures.push(error.message));
async function measure(label, selector = '.study-dialog .mix-view') {
  await page.evaluate(
    () =>
      new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r))),
  );
  const issues = await page.locator(selector).evaluate((root) => {
    const issues = [];
    const outside = (a, b) =>
      a.left < b.left - 1 ||
      a.right > b.right + 1 ||
      a.top < b.top - 1 ||
      a.bottom > b.bottom + 1;
    const collision = (a, b) =>
      Math.min(a.right, b.right) - Math.max(a.left, b.left) > 1 &&
      Math.min(a.bottom, b.bottom) - Math.max(a.top, b.top) > 1;
    const texts = [];
    const walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT);
    while (walker.nextNode()) {
      const node = walker.currentNode;
      if (!node.textContent.trim() || node.parentElement.closest('svg'))
        continue;
      const range = document.createRange();
      range.selectNodeContents(node);
      const rect = range.getBoundingClientRect();
      if (outside(rect, root.getBoundingClientRect()))
        issues.push(`outside screen: ${node.textContent}`);
      const row = node.parentElement.closest('li');
      if (row && outside(rect, row.getBoundingClientRect()))
        issues.push(`outside row: ${node.textContent}`);
      texts.push({ rect, text: node.textContent });
    }
    for (let i = 0; i < texts.length; i++)
      for (let j = i + 1; j < texts.length; j++)
        if (collision(texts[i].rect, texts[j].rect))
          issues.push(`collision: ${texts[i].text} / ${texts[j].text}`);
    for (const el of root.querySelectorAll('.mix-position svg, .mix-wave'))
      if (
        outside(
          el.getBoundingClientRect(),
          el.closest('li').getBoundingClientRect(),
        )
      )
        issues.push('meter or dot numeral outside row');
    return issues;
  });
  failures.push(...issues.map((issue) => `${label}: ${issue}`));
  checked++;
}
try {
  await page.goto(
    `${process.env.BASE_URL || 'http://localhost:3000'}/explore?demo=1`,
  );
  await page.locator('.study-dialog').waitFor();
  await page.locator('.study-dialog [data-slot="dialog-close"]').click();
  await page.locator('.study-dialog').waitFor({ state: 'detached' });
  for (const count of [3, 2]) {
    await page
      .getByRole('button', { name: `${count}人で発話`, exact: true })
      .click();
    await expect(page.locator('[data-mix-theme]')).toHaveCount(60);
    await expect(page.locator('[data-mix-peer]')).toHaveCount(60 * count);
    for (let id = 1; id <= 60; id++)
      await measure(
        `board #${id} / ${count} speakers`,
        `[data-mix-theme="${id}"]`,
      );
  }
  await page.getByRole('button', { name: '初期状態', exact: true }).click();
  await page.locator('[data-demo-launch="1"]').click();
  for (let id = 1; id <= 60; id++) {
    await page.locator('.study-dialog [data-slot="dialog-close"]').click();
    await page.locator('.study-dialog').waitFor({ state: 'detached' });
    await page.evaluate((id) => {
      history.replaceState(null, '', `?demo=${id}`);
      dispatchEvent(new PopStateEvent('popstate'));
    }, id);
    const d = page.locator('.study-dialog');
    await d.locator(`[data-demo-screen="${id}"]`).waitFor();
    await d.getByRole('switch', { name: '通信接続', exact: true }).check();
    await d.getByRole('switch', { name: '位置未更新', exact: true }).uncheck();
    await d
      .getByRole('button', { name: '仲間の発話をすべて終了', exact: true })
      .click();
    const names =
      id >= 55 ? ['ライム', 'ピンク', 'ブルー'] : ['AKI', 'REN', 'MEI'];
    const voices = d.locator('.study-controls fieldset').first();
    for (const name of names)
      await voices.getByRole('button', { name, exact: true }).click();
    await expect(d.locator('[data-mix-peer]')).toHaveCount(3);
    for (const width of [240, 300, 360]) {
      const style = await page.addStyleTag({
        content: `.study-device .study-screen { width: ${width}px !important; max-width: none !important; }`,
      });
      await measure(`#${id} three remote ${width}px`);
      await style.evaluate((el) => el.remove());
    }
    await d
      .locator('.mix-view')
      .screenshot({ path: `${output}/${String(id).padStart(2, '0')}.png` });
    const ptt = d.locator('.study-ptt');
    await ptt.focus();
    await page.keyboard.down('Space');
    await expect(ptt).toContainText('空き待ち');
    // Deliver an incoming stream-end while the physical PTT key stays held.
    await voices
      .getByRole('button', { name: names[2], exact: true })
      .evaluate((el) => el.click());
    await expect(d.locator('[data-mix-peer="self"]')).toHaveCount(1);
    await expect(d.locator('[data-mix-peer]')).toHaveCount(3);
    await expect(
      voices.getByRole('button', { name: names[2], exact: true }),
    ).toBeDisabled();
    await measure(`#${id} self plus two remote`);
    await page.keyboard.up('Space');
    await expect(d.locator('[data-mix-peer="self"]')).toHaveCount(0);
    await expect(d.locator('[data-mix-peer]')).toHaveCount(2);
    await measure(`#${id} two remote`);
    await d.getByRole('switch', { name: '位置未更新', exact: true }).check();
    await d.getByRole('switch', { name: '音声ミュート', exact: true }).check();
    await measure(`#${id} stale and muted`);
    assert.equal(
      await d.locator('.mix-speakers [data-audible="true"]').count(),
      0,
    );
    await page.setViewportSize({ width: 320, height: 844 });
    await measure(`#${id} mobile 320px`);
    await page.setViewportSize({ width: 1280, height: 960 });
    if (id % 10 === 0) console.log(`Mixed voice: ${id}/60`);
  }
  for (const design of ['circle', 'pulse', 'compass']) {
    await page.goto(
      `${process.env.BASE_URL || 'http://localhost:3000'}/#${design}`,
    );
    await page.waitForLoadState('networkidle');
    await page.getByRole('button', { name: /2人で同時発話/ }).click();
    await expect(page.locator('[data-live-peer]')).toHaveCount(2);
    const ptt = page.getByRole('button', {
      name: 'PTT 押している間だけ話す',
      exact: true,
    });
    await ptt.focus();
    await page.keyboard.down('Space');
    await expect(page.locator('[data-live-peer="self"]')).toHaveCount(1);
    await expect(page.locator('[data-live-peer]')).toHaveCount(3);
    await page.keyboard.up('Space');
    await expect(page.locator('[data-live-peer]')).toHaveCount(2);
    await page.getByRole('button', { name: /3人で同時発話/ }).click();
    await ptt.focus();
    await page.keyboard.down('Space');
    await expect(ptt).toContainText('空き待ち');
    await page
      .getByRole('button', { name: 'YUIが話す', exact: true })
      .evaluate((el) => el.click());
    await expect(page.locator('[data-live-peer="self"]')).toHaveCount(1);
    await page.screenshot({ path: `${output}/${design}.png` });
    await page.keyboard.up('Space');
    for (const view of ['仲間', 'ライド', 'レーダー']) {
      await page.getByRole('tab', { name: view, exact: true }).click();
      await expect(page.locator('[data-live-peer]')).toHaveCount(2);
    }
    await page.setViewportSize({ width: 390, height: 844 });
    await page
      .locator('.device')
      .screenshot({ path: `${output}/${design}-mobile.png` });
    await page.setViewportSize({ width: 1280, height: 960 });
  }
  await fs.writeFile(
    `${output}/report.json`,
    JSON.stringify({ checked, failures }, null, 2),
  );
  assert.equal(
    failures.length,
    0,
    failures.slice(0, 45).join('\n') + `\nTotal ${failures.length}`,
  );
  console.log(
    `${checked} mixed-voice layouts and original three designs passed.`,
  );
} finally {
  await browser.close();
}
