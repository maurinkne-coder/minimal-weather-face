#include "contour.h"

static GBitmap *s_digits[10];
// Ink level (0 see-through, 1 and 2 edge shades, 3 full) of each palette
// entry, read once at load: the resource tool may reorder the palette.
static uint8_t s_level[10][4];

static const uint32_t DIGIT_IDS[10] = {
  RESOURCE_ID_DIGIT_0, RESOURCE_ID_DIGIT_1, RESOURCE_ID_DIGIT_2, RESOURCE_ID_DIGIT_3,
  RESOURCE_ID_DIGIT_4, RESOURCE_ID_DIGIT_5, RESOURCE_ID_DIGIT_6, RESOURCE_ID_DIGIT_7,
  RESOURCE_ID_DIGIT_8, RESOURCE_ID_DIGIT_9,
};

void digits_load(void) {
  for (int i = 0; i < 10; i++) {
    s_digits[i] = gbitmap_create_with_resource(DIGIT_IDS[i]);
    GColor *pal = s_digits[i] ? gbitmap_get_palette(s_digits[i]) : NULL;
    for (int k = 0; k < 4; k++) {
      // The images use grey 85/170/255 for the levels, so the red channel
      // (0-3) is the level; a see-through entry is level 0.
      s_level[i][k] = !pal || (pal[k].argb >> 6) == 0 ? 0 : (pal[k].argb >> 4) & 3;
    }
  }
}

// Mixes two colours per channel: k parts of fg to 3 - k parts of bg.
static GColor mix(GColor bg, GColor fg, int k) {
  uint8_t out = 0xC0;
  for (int sh = 0; sh <= 4; sh += 2) {
    int b = (bg.argb >> sh) & 3, f = (fg.argb >> sh) & 3;
    out |= (uint8_t)(((b * (3 - k) + f * k + 1) / 3) << sh);
  }
  return (GColor){ .argb = out };
}

void digits_unload(void) {
  for (int i = 0; i < 10; i++) {
    if (s_digits[i]) gbitmap_destroy(s_digits[i]);
    s_digits[i] = NULL;
  }
}

void digits_draw(GContext *ctx, const char *s, int cx, int top, int gap, GColor color, GColor bg) {
  int total = 0, n = 0;
  for (const char *p = s; *p; p++) {
    if (*p < '0' || *p > '9' || !s_digits[*p - '0']) continue;
    total += gbitmap_get_bounds(s_digits[*p - '0']).size.w;
    n++;
  }
  if (!n) return;
  total += gap * (n - 1);

  const GColor shades[4] = { GColorClear, mix(bg, color, 1), mix(bg, color, 2), color };
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  int x = cx - total / 2;
  for (const char *p = s; *p; p++) {
    if (*p < '0' || *p > '9' || !s_digits[*p - '0']) continue;
    int d = *p - '0';
    GBitmap *bmp = s_digits[d];
    // Four-level image: see-through, two smoothed edge shades, full colour.
    GColor *pal = gbitmap_get_palette(bmp);
    if (pal) {
      for (int k = 0; k < 4; k++) pal[k] = shades[s_level[d][k]];
    }
    GSize sz = gbitmap_get_bounds(bmp).size;
    graphics_draw_bitmap_in_rect(ctx, bmp, GRect(x, top, sz.w, sz.h));
    x += sz.w + gap;
  }
  graphics_context_set_compositing_mode(ctx, GCompOpAssign);
}
