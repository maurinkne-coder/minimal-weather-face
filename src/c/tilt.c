#include "contour.h"

// Turning the face. System fonts can only be drawn upright, so the text layer
// is drawn first, copied out of the frame buffer and set aside. The forecast
// ring is then drawn already turned with plain geometry, so it stays sharp,
// and the text is turned and laid on top of it.
//
// The turn samples the text with bilinear filtering instead of picking the
// nearest pixel: a turned pixel grid never lines up with the screen grid, and
// nearest-pixel sampling makes straight edges ragged and thin strokes uneven.
// Each screen pixel instead mixes the four text pixels round its source
// point, and see-through text pixels let the ring or background show through,
// so the edges stay smooth over any colour.

static GBitmap *s_copy;

void tilt_release(void) {
  if (s_copy) {
    gbitmap_destroy(s_copy);
    s_copy = NULL;
  }
}

bool tilt_begin(GContext *ctx, GRect bounds, GColor bg) {
  if (!s_copy) {
    s_copy = gbitmap_create_blank(bounds.size, GBitmapFormat8Bit);
    if (!s_copy) return false; // not enough memory: leave the face upright
  }
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (!fb) return false;
  uint8_t *dst = gbitmap_get_data(s_copy);
  uint16_t stride = gbitmap_get_bytes_per_row(s_copy);
  for (int y = 0; y < bounds.size.h; y++) {
    uint8_t *row = dst + y * stride;
    memset(row, 0, stride); // 0 = see-through
    // Works for both rectangular and round (circular) frame buffers.
    GBitmapDataRowInfo info = gbitmap_get_data_row_info(fb, y);
    for (int x = info.min_x; x <= info.max_x && x < bounds.size.w; x++) {
      uint8_t v = info.data[x];
      row[x] = v == bg.argb ? 0 : v;
    }
  }
  graphics_release_frame_buffer(ctx, fb);
  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  return true;
}

// One text pixel, or the screen pixel under it when the text is see-through.
static inline uint8_t sample(const uint8_t *src, uint16_t stride, int w, int h, int x, int y,
                             uint8_t under) {
  if (x < 0 || y < 0 || x >= w || y >= h) return under;
  uint8_t v = src[y * stride + x];
  return v ? v : under;
}

void tilt_end(GContext *ctx, GRect bounds, int deg) {
  if (!s_copy) return;
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (!fb) return;
  const uint8_t *src = gbitmap_get_data(s_copy);
  const uint16_t stride = gbitmap_get_bytes_per_row(s_copy);
  const int w = bounds.size.w, h = bounds.size.h;
  const int32_t a = DEG_TO_TRIGANGLE(((deg % 360) + 360) % 360);
  const int32_t sn = sin_lookup(a), cs = cos_lookup(a);  // 1.0 = TRIG_MAX_RATIO (1 << 16)
  // Centre of the turn in 16.16 fixed point, on the pixel grid's middle.
  const int32_t cx = (w << 15), cy = (h << 15);

  for (int y = 0; y < h; y++) {
    GBitmapDataRowInfo info = gbitmap_get_data_row_info(fb, y);
    // Source point of this row's first pixel centre, then step along x.
    // Turning the text clockwise by deg means looking up each screen pixel
    // turned anticlockwise by deg.
    int32_t dy = (y << 16) + (1 << 15) - cy;
    int32_t dx0 = (info.min_x << 16) + (1 << 15) - cx;
    int32_t sx = cx + (int32_t)(((int64_t)dx0 * cs + (int64_t)dy * sn) >> 16) - (1 << 15);
    int32_t sy = cy + (int32_t)(((int64_t)dy * cs - (int64_t)dx0 * sn) >> 16) - (1 << 15);
    for (int x = info.min_x; x <= info.max_x && x < w; x++, sx += cs, sy -= sn) {
      int ix = sx >> 16, iy = sy >> 16;
      if (ix < -1 || iy < -1 || ix >= w || iy >= h) continue;
      uint8_t under = info.data[x];
      uint8_t p00 = sample(src, stride, w, h, ix, iy, under);
      uint8_t p10 = sample(src, stride, w, h, ix + 1, iy, under);
      uint8_t p01 = sample(src, stride, w, h, ix, iy + 1, under);
      uint8_t p11 = sample(src, stride, w, h, ix + 1, iy + 1, under);
      if (p00 == under && p10 == under && p01 == under && p11 == under) continue;
      // Weights in 1/256ths.
      int fx = (sx >> 8) & 0xFF, fy = (sy >> 8) & 0xFF;
      int w00 = (256 - fx) * (256 - fy), w10 = fx * (256 - fy);
      int w01 = (256 - fx) * fy, w11 = fx * fy;  // sum = 65536
      uint8_t out = 0xC0;
      for (int sh = 0; sh <= 4; sh += 2) {
        int32_t v = ((p00 >> sh) & 3) * w00 + ((p10 >> sh) & 3) * w10 +
                    ((p01 >> sh) & 3) * w01 + ((p11 >> sh) & 3) * w11;
        out |= (uint8_t)(((v + 32768) >> 16) << sh);
      }
      info.data[x] = out;
    }
  }
  graphics_release_frame_buffer(ctx, fb);
}
