import { chromium } from '@playwright/test';
import fs from 'node:fs/promises';

const output = 'outputs/fleet-layout';
const browser = await chromium.launch({
  channel: process.env.BROWSER_CHANNEL || 'chrome',
  headless: true,
});
try {
  const page = await browser.newPage({
    viewport: { width: 1000, height: 1800 },
  });
  for (let start = 1; start <= 60; start += 20) {
    const cards = await Promise.all(
      Array.from({ length: 20 }, async (_, i) => {
        const id = String(start + i).padStart(2, '0');
        const image = await fs.readFile(`${output}/${id}.png`);
        return `<figure><figcaption>${id} / 車列・GPS</figcaption><img src="data:image/png;base64,${image.toString('base64')}" /></figure>`;
      }),
    );
    await page.setContent(
      `<html lang="ja"><style>body{margin:0;background:#e8e9e4;padding:24px;font:14px sans-serif}main{display:grid;grid-template-columns:repeat(4,1fr);gap:20px}figure{margin:0;min-width:0}figcaption{margin-bottom:8px}img{width:100%;display:block;border-radius:12px}</style><main>${cards.join('')}</main></html>`,
    );
    await page
      .locator('img')
      .evaluateAll((images) =>
        Promise.all(images.map((image) => image.decode())),
      );
    await page.screenshot({
      path: `${output}/contact-${start}-${start + 19}.png`,
      fullPage: true,
    });
  }
} finally {
  await browser.close();
}
