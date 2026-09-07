// Developer regeneration only. Normal builds use checked-in generated C fonts.
// Prerequisites: lv_font_conv 1.5.3 in .tools/dot-font-tools/node_modules,
// static wght=400 instances GeistMono-Regular.ttf and NotoSansJP-Regular.ttf.
import fs from 'node:fs';
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
const repo=fileURLToPath(new URL('../../../../',import.meta.url));
const source=fs.readFileSync(repo+'firmware/experiments/ui_lvgl/main/ui_dot.c','utf8');
const symbols=[...new Set([...source].filter(c=>c.codePointAt(0)>127))].join('');
for(const size of [7,8,9,10,18]) {
  const result=spawnSync(process.execPath,[repo+'.tools/dot-font-tools/node_modules/lv_font_conv/lv_font_conv.js',
    '--font','.tools/dot-font-tools/GeistMono-Regular.ttf','-r','0x20-0x7e','--symbols','—·',
    '--font','.tools/dot-font-tools/NotoSansJP-Regular.ttf','--symbols',symbols,
    '--size',String(size),'--bpp','4','--format','lvgl','--lv-font-name',`rw_dot_font_${size}`,
    '--lv-include','lvgl.h','--no-compress','-o',`firmware/experiments/ui_lvgl/main/fonts/rw_dot_font_${size}.c`],
    {cwd:repo,stdio:'inherit'});
  if(result.status!==0)process.exit(result.status||1);
}
