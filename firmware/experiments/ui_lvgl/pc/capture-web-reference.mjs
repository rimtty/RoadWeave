// Run from this directory after starting the existing Web PoC on localhost:3000.
// Only this script's reference/ and build/results/ files are written.
import { chromium } from '../../../../design/web-poc/node_modules/@playwright/test/index.mjs';
import fs from 'node:fs/promises';
import crypto from 'node:crypto';
import { fileURLToPath } from 'node:url';
const dir=fileURLToPath(new URL('.',import.meta.url));
const output=dir+'reference/';await fs.mkdir(output,{recursive:true});
const browser=await chromium.launch({channel:'chrome',headless:true});
try {
  const page=await browser.newPage({viewport:{width:1440,height:1100},reducedMotion:'reduce'});
  await page.goto(process.env.BASE_URL||'http://localhost:3000/explore',{waitUntil:'networkidle'});
  for(const id of [13,14,15,16]) {
    const screen=page.locator(`[data-demo-screen="${id}"]`);
    await screen.evaluate(el=>Object.assign(el.style,{width:'300px',maxWidth:'300px',height:'400px',minHeight:'400px',transformOrigin:'top left',transform:'scale(.8)',position:'fixed',left:'0px',top:'0px',zIndex:'2147483647'}));
    await screen.scrollIntoViewIfNeeded();
    await screen.screenshot({path:output+`web-dot${id}.png`});
  }
  const sourceFiles=['collection.tsx','explore.css','demo/graphics.tsx','demo/model.ts'];
  const manifest={viewport:[1440,1100],card:[300,400],scale:.8,reducedMotion:true,sources:{}};
  for(const file of sourceFiles) {
    const bytes=await fs.readFile(new URL('../../../../design/web-poc/app/explore/'+file,import.meta.url));
    manifest.sources[file]=crypto.createHash('sha256').update(bytes).digest('hex');
  }
  await fs.writeFile(output+'source.json',JSON.stringify(manifest,null,2)+'\n');
  const cards=[];
  for(const id of [13,14,15,16]) {
    let images='';
    for(const [label,path] of [['Web',output+`web-dot${id}.png`],['LVGL',dir+`build/candidates/dot${id}.png`]]) {
      const b=await fs.readFile(path);images+=`<div><p>${label}</p><img src="data:image/png;base64,${b.toString('base64')}" /></div>`;
    }
    cards.push(`<section><h2>Dot / ${id}</h2><article>${images}</article></section>`);
  }
  await page.setViewportSize({width:1060,height:830});
  await page.setContent(`<style>body{background:#171b16;color:#eef0e9;font:14px sans-serif;margin:20px}main{display:grid;grid-template-columns:1fr 1fr;gap:20px}h2{font-size:16px;margin:0}article{display:flex;gap:12px}p{margin:8px 0}img{width:240px;height:320px}</style><main>${cards.join('')}</main>`);
  await page.locator('img').evaluateAll(images=>Promise.all(images.map(i=>i.decode())));
  await fs.mkdir(dir+'build/results',{recursive:true});
  await page.screenshot({path:dir+'build/results/dot-comparison.png',fullPage:true});
} finally {await browser.close();}
