import { chromium } from '@playwright/test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';

// Run against the local dev server. Real Chromium geometry is essential:
// DOM-only tests cannot detect clipped text, undersized rows or SVG collisions.
const output = 'outputs/fleet-layout';
await fs.mkdir(output, { recursive: true });
const browser = await chromium.launch({
  channel: process.env.BROWSER_CHANNEL || 'chrome',
  headless: true,
});
const page = await browser.newPage({
  viewport: { width: 1280, height: 960 },
  deviceScaleFactor: 1,
});
const failures = [];
let checked = 0;

async function openStudy(id) {
  const previous = page.locator('.study-dialog');
  if (await previous.count()) {
    await previous.locator('[data-slot="dialog-close"]').click();
    await previous.waitFor({ state: 'detached' });
  }
  await page.evaluate((id) => {
    history.replaceState(null, '', `?demo=${id}&screen=fleet`);
    dispatchEvent(new PopStateEvent('popstate'));
  }, id);
  const screen = page.locator(`.study-dialog [data-fleet-theme="${id}"]`);
  await screen.waitFor();
  await page.locator('.study-dialog').evaluate(async (el) => {
    await Promise.all(
      el.getAnimations().map((animation) => animation.finished.catch(() => {})),
    );
  });
  return screen;
}

async function inspect(context, selector = '.study-dialog .fleet-view') {
  // React state and CSS layout must both settle after a control changes state.
  await page.evaluate(
    () =>
      new Promise((resolve) =>
        requestAnimationFrame(() => requestAnimationFrame(resolve)),
      ),
  );
  const issues = await page.locator(selector).evaluateAll((screens) => {
    const issues = [];
    const box = (el) => el.getBoundingClientRect();
    const outside = (a, b) =>
      a.left < b.left - 1 ||
      a.top < b.top - 1 ||
      a.right > b.right + 1 ||
      a.bottom > b.bottom + 1;
    const intersects = (a, b) =>
      Math.min(a.right, b.right) - Math.max(a.left, b.left) > 1 &&
      Math.min(a.bottom, b.bottom) - Math.max(a.top, b.top) > 1;
    for (const screen of screens) {
      const id = screen.dataset.fleetTheme;
      const add = (message) => issues.push(`#${id}: ${message}`);
      const root = box(screen);
      if (
        screen.closest('.study-device') &&
        root.width > box(screen.closest('.study-device')).width + 1
      )
        add('device overflows its column');
      for (const el of screen.querySelectorAll(
        '.fleet-heading, .fleet-overview, .fleet-composition, .fleet-coordinates, .fleet-map-shell',
      )) {
        if (outside(box(el), root)) add(`${el.className} outside screen`);
      }
      for (const row of screen.querySelectorAll('.fleet-list > li')) {
        if (!row.getClientRects().length) continue;
        const cells = [
          ...row.querySelectorAll(
            '.fleet-rank, .fleet-person > b, .fleet-person > span, .fleet-distance',
          ),
        ];
        for (const el of cells) {
          if (outside(box(el), box(row)))
            add(`${row.dataset.fleetPeer} / ${el.textContent}: outside row`);
        }
        for (let a = 0; a < cells.length; a++)
          for (let b = a + 1; b < cells.length; b++) {
            if (intersects(box(cells[a]), box(cells[b])))
              add(
                `${row.dataset.fleetPeer}: ${cells[a].className || cells[a].tagName} overlaps ${cells[b].className || cells[b].tagName}`,
              );
          }
      }
      // Check text glyph extents, not just their CSS containers.
      const textNodes = document.createTreeWalker(screen, NodeFilter.SHOW_TEXT);
      const textRects = [];
      while (textNodes.nextNode()) {
        const node = textNodes.currentNode;
        if (!node.textContent.trim() || node.parentElement.closest('svg'))
          continue;
        const range = document.createRange();
        range.selectNodeContents(node);
        for (const rect of range.getClientRects()) {
          if (!rect.width || !rect.height) continue;
          if (outside(rect, root))
            add(`text outside screen: ${node.textContent}`);
          const row = node.parentElement.closest('.fleet-list > li');
          if (row && outside(rect, box(row)))
            add(`text outside row: ${node.textContent}`);
          textRects.push({ rect, text: node.textContent });
        }
      }
      for (let a = 0; a < textRects.length; a++)
        for (let b = a + 1; b < textRects.length; b++) {
          if (intersects(textRects[a].rect, textRects[b].rect))
            add(`text collision: ${textRects[a].text} / ${textRects[b].text}`);
        }
      const map = screen.querySelector('.fleet-map');
      const labels = [...map.querySelectorAll('text')];
      if (box(map).height < 20)
        add(
          `map collapsed: ${box(map).height}px in ${root.width}x${root.height}, ${screen.className}`,
        );
      for (const label of labels)
        if (outside(box(label), box(map)))
          add(`map label clipped: ${label.textContent}`);
      for (let a = 0; a < labels.length; a++)
        for (let b = a + 1; b < labels.length; b++) {
          if (intersects(box(labels[a]), box(labels[b])))
            add('map labels overlap');
        }
    }
    return issues;
  });
  failures.push(...issues.map((issue) => `${context} ${issue}`));
  checked += await page.locator(selector).count();
}

try {
  await page.goto(
    `${process.env.BASE_URL || 'http://localhost:3000'}/explore?demo=1&screen=fleet`,
  );
  await page.locator('.study-dialog .fleet-view').waitFor();
  await page.evaluate(() => document.fonts.ready);
  await page.locator('.study-dialog [data-slot="dialog-close"]').click();
  await page
    .locator('.study-board-switch')
    .getByRole('tab', { name: '車列・GPS', exact: true })
    .click();
  await page.locator('[data-fleet-theme="60"]').waitFor();
  await inspect('board natural desktop', '.fleet-view');
  for (const width of [240, 300, 360]) {
    const style = await page.addStyleTag({
      content: `.study-screen[data-page=fleet] { width: ${width}px !important; max-width: none !important; }`,
    });
    await inspect(`board ${width}px`, '.fleet-view');
    await style.evaluate((el) => el.remove());
  }
  // Open all sixty using the same deep-link route that users can bookmark.
  for (let id = 1; id <= 60; id++) {
    const screen = await openStudy(id);
    const dialog = page.locator('.study-dialog');
    for (const text of ['通信接続', 'グループに参加'])
      await dialog.getByRole('switch', { name: text, exact: true }).check();
    await dialog
      .getByRole('switch', { name: '位置未更新', exact: true })
      .uncheck();
    await inspect('dialog normal');
    await screen.screenshot({
      path: `${output}/${String(id).padStart(2, '0')}.png`,
    });
    await dialog
      .getByRole('button', { name: 'GPSを拡大', exact: true })
      .click();
    await inspect('expanded');
    await dialog.getByRole('button', { name: '0m', exact: true }).click();
    await inspect('expanded coincident');
    await dialog
      .getByRole('button', { name: '車列に戻る', exact: true })
      .click();
    await inspect('zero / long direction');
    const slider = dialog.getByRole('slider', { name: /の相対位置/ });
    await slider.focus();
    await page.keyboard.press('End');
    await inspect('999m');
    await page.keyboard.press('Home');
    await inspect('-999m');
    await dialog
      .getByRole('switch', { name: '位置未更新', exact: true })
      .check();
    await inspect('stale');
    await dialog
      .getByRole('switch', { name: '通信接続', exact: true })
      .uncheck();
    await inspect('offline');
    if (id % 10 === 0) console.log(`Reviewed ${id}/60 themes`);
  }
  for (const width of [390, 320]) {
    await page.setViewportSize({ width, height: 844 });
    for (let id = 1; id <= 60; id++) {
      await openStudy(id);
      const dialog = page.locator('.study-dialog');
      await dialog
        .getByRole('switch', { name: '通信接続', exact: true })
        .check();
      await dialog
        .getByRole('switch', { name: '位置未更新', exact: true })
        .uncheck();
      await inspect(`mobile viewport ${width}px / normal`);
      await dialog.getByRole('button', { name: '0m', exact: true }).click();
      await inspect(`mobile viewport ${width}px / zero`);
      await dialog
        .getByRole('button', { name: 'GPSを拡大', exact: true })
        .click();
      await inspect(`mobile viewport ${width}px / expanded`);
      await dialog
        .getByRole('switch', { name: '位置未更新', exact: true })
        .check();
      await inspect(`mobile viewport ${width}px / stale expanded`);
      if ([11, 46, 56].includes(id))
        await dialog.locator('.fleet-view').screenshot({
          path: `${output}/mobile-${width}-${id}.png`,
        });
    }
    console.log(`Reviewed all 60 at mobile viewport ${width}px`);
  }
  await fs.writeFile(
    `${output}/report.json`,
    JSON.stringify({ checked, failures }, null, 2),
  );
  assert.equal(
    failures.length,
    0,
    failures.slice(0, 60).join('\n') + `\nTotal: ${failures.length}`,
  );
  console.log(`${checked} real-browser fleet layouts passed.`);
} finally {
  await browser.close();
}
