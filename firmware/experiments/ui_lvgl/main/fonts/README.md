# Dot study text fonts

`rw_dot_font_*.c` combines **Geist Mono Regular (400)** for ASCII and **Noto Sans JP Regular (400)** for the Japanese text used in `ui_dot.c`. Generated at 7/8/9/10/18 pixels, 4bpp, with `lv_font_conv 1.5.3`. The large 5×7 dot digits are native geometry in `ui_dot.c`, not these fonts.

Both source fonts use SIL OFL 1.1. Copyright and license notices are included in `GeistMono-OFL.txt` and `NotoSansJP-OFL.txt`; keep them with redistributed generated fonts.

Source files downloaded 2026-09-06 (UTC+9):

| Font | Source | SHA256 |
|---|---|---|
| Geist Mono | https://raw.githubusercontent.com/google/fonts/main/ofl/geistmono/GeistMono%5Bwght%5D.ttf | d00e590b8eb3a59acc329b2d044fd143ae935090b7da33199ebee27cc7de8196 |
| Noto Sans JP | https://raw.githubusercontent.com/google/fonts/main/ofl/notosansjp/NotoSansJP%5Bwght%5D.ttf | c2f3b4d463500a2ddcd3849cded1fceeb9fd6d1c32e6cbecd568453ba50fc68f |

Normal Windows/ESP-IDF builds compile the checked-in C sources and require no font downloads or Node/Python font tools.

To regenerate after changing Japanese strings:

1. Save the verified source TTFs as `.tools/dot-font-tools/GeistMono.ttf` and `NotoSansJP.ttf` from the repository root.
2. Use fontTools 4.59.0 to instantiate the variable fonts at weight 400 (the variable fonts' default weight is thinner than the Web design):

```python
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont
for name in ('GeistMono', 'NotoSansJP'):
    font = TTFont(f'.tools/dot-font-tools/{name}.ttf')
    instantiateVariableFont(font, {'wght': 400}, inplace=True)
    font.save(f'.tools/dot-font-tools/{name}-Regular.ttf')
```

3. Run `npm install --prefix .tools/dot-font-tools lv_font_conv@1.5.3 --no-audit --no-fund` and `node firmware/experiments/ui_lvgl/pc/generate-dot-fonts.mjs`. The generator collects every non-ASCII character from `ui_dot.c` to avoid missing-glyph boxes. Review the generated font and image changes before updating baselines.
