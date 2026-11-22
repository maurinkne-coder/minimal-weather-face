#include "contour.h"

// Turning the face. System fonts can only be drawn upright, so the text is
// drawn first, copied out of the frame buffer, and drawn back rotated.
// The forecast line is drawn afterwards with plain geometry, so it stays sharp.

static GBitmap *s_copy;

void tilt_release(void) {
  if (s_copy) {
    gbitmap_destroy(s_copy);
    s_copy = NULL;
  }
}

void tilt_apply(GContext *ctx, GRect bounds, int deg, GColor bg) {
  if (!deg) return;
  if (!s_copy) {
    s_copy = gbitmap_create_blank(bounds.size, GBitmapFormat8Bit);
    if (!s_copy) return; // not enough memory: leave the face upright
  }

  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (!fb) return;
  uint8_t *dst = gbitmap_get_data(s_copy);
  uint16_t stride = gbitmap_get_bytes_per_row(s_copy);
  for (int y = 0; y < bounds.size.h; y++) {
    uint8_t *row = dst + y * stride;
    memset(row, bg.argb, stride);
    // Works for both rectangular and round (circular) frame buffers.
    GBitmapDataRowInfo info = gbitmap_get_data_row_info(fb, y);
    for (int x = info.min_x; x <= info.max_x && x < bounds.size.w; x++) {
      row[x] = info.data[x];
    }
  }
  graphics_release_frame_buffer(ctx, fb);

  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  GPoint centre = GPoint(bounds.size.w / 2, bounds.size.h / 2);
  graphics_context_set_compositing_mode(ctx, GCompOpAssign);
  graphics_draw_rotated_bitmap(ctx, s_copy, centre, DEG_TO_TRIGANGLE(((deg % 360) + 360) % 360), centre);
}
