#include <stdio.h>
#include "lvgl.h"
#include "src/libs/lodepng/lodepng.h"
#include "capture.h"

uint32_t capture_frame_hash(void)
{
    const lv_draw_buf_t *buf = lv_display_get_buf_active(NULL);
    uint32_t hash = 2166136261u;
    for (unsigned y = 0; y < 320; y++)
        for (unsigned x = 0; x < 240 * 2; x++)
            hash = (hash ^ buf->data[y * buf->header.stride + x]) * 16777619u;
    return hash;
}

/* Use native stdio for output paths: an absolute Windows drive letter is not
 * an LVGL filesystem driver letter. LodePNG's encoder itself remains shared. */
static bool write_png(const char *path, const unsigned char *rgba)
{
    unsigned char *png = NULL;
    size_t length = 0;
    unsigned err = lodepng_encode32(&png, &length, rgba, 240, 320);
    if (err) { fprintf(stderr, "PNG encode failed: %s\n", lodepng_error_text(err)); return false; }
    FILE *f = fopen(path, "wb");
    bool ok = false;
    if (f) {
        ok = fwrite(png, 1, length, f) == length;
        if (fclose(f) != 0) ok = false;
    }
    lv_free(png);
    if (!ok) fprintf(stderr, "PNG write failed: %s\n", path);
    return ok;
}

/* Export the logical RGB565 frame, independent of window zoom and Windows DPI. */
static unsigned char *frame_rgba(void)
{
    lv_refr_now(NULL);
    const lv_draw_buf_t *buf = lv_display_get_buf_active(NULL);
    if (buf->header.cf != LV_COLOR_FORMAT_RGB565) return NULL;
    unsigned char *out = lv_malloc(240 * 320 * 4);
    if (!out) return NULL;
    for (unsigned y = 0; y < 320; y++) {
        const uint16_t *row = (const uint16_t *)(buf->data + y * buf->header.stride);
        for (unsigned x = 0; x < 240; x++) {
            unsigned p = (y * 240 + x) * 4;
            /* Match LVGL's screenshot conversion rounding exactly. */
            out[p] = (unsigned char)((((row[x] >> 11) & 31) * 2106) >> 8);
            out[p + 1] = (unsigned char)((((row[x] >> 5) & 63) * 1037) >> 8);
            out[p + 2] = (unsigned char)(((row[x] & 31) * 2106) >> 8);
            out[p + 3] = 255;
        }
    }
    return out;
}

bool capture_png(const char *path)
{
    unsigned char *pixels = frame_rgba();
    if (!pixels) return false;
    bool ok = write_png(path, pixels);
    lv_free(pixels);
    return ok;
}

bool capture_diff(const char *reference, const char *output)
{
    lv_draw_buf_t *ref = NULL;
    unsigned w, h;
    /* LVGL's lodepng fork decodes into an lv_draw_buf_t (RGBA byte order). */
    unsigned err = lodepng_decode32_file((unsigned char **)&ref, &w, &h, reference);
    if (err || !ref) return false;
    if (w != 240 || h != 320) { lv_draw_buf_destroy(ref); return false; }
    unsigned char *actual = frame_rgba();
    if (!actual) { lv_draw_buf_destroy(ref); return false; }
    for (unsigned y = 0; y < h; y++) {
        const unsigned char *row = ref->data + y * ref->header.stride;
        for (unsigned x = 0; x < w; x++) {
            unsigned p = (y * w + x) * 4;
            bool changed = actual[p] != row[x * 4] || actual[p + 1] != row[x * 4 + 1] ||
                           actual[p + 2] != row[x * 4 + 2];
            actual[p] = changed ? 255 : (unsigned char)(actual[p] / 4);
            actual[p + 1] = changed ? 0 : (unsigned char)(actual[p + 1] / 4);
            actual[p + 2] = changed ? 160 : (unsigned char)(actual[p + 2] / 4);
        }
    }
    bool ok = write_png(output, actual);
    lv_free(actual);
    lv_draw_buf_destroy(ref);
    return ok;
}
