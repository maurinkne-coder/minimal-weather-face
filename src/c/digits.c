#include "contour.h"

static GBitmap *s_digits[10];

static const uint32_t DIGIT_IDS[10] = {
  RESOURCE_ID_DIGIT_0, RESOURCE_ID_DIGIT_1, RESOURCE_ID_DIGIT_2, RESOURCE_ID_DIGIT_3,
  RESOURCE_ID_DIGIT_4, RESOURCE_ID_DIGIT_5, RESOURCE_ID_DIGIT_6, RESOURCE_ID_DIGIT_7,
  RESOURCE_ID_DIGIT_8, RESOURCE_ID_DIGIT_9,
};

void digits_load(void) {
  for (int i = 0; i < 10; i++) s_digits[i] = gbitmap_create_with_resource(DIGIT_IDS[i]);
}

void digits_unload(void) {
  for (int i = 0; i < 10; i++) {
    if (s_digits[i]) gbitmap_destroy(s_digits[i]);
    s_digits[i] = NULL;
  }
}

void digits_draw(GContext *ctx, const char *s, int cx, int top, int gap, GColor color) {
  int total = 0, n = 0;
  for (const char *p = s; *p; p++) {
    if (*p < '0' || *p > '9' || !s_digits[*p - '0']) continue;
    total += gbitmap_get_bounds(s_digits[*p - '0']).size.w;
    n++;
  }
  if (!n) return;
  total += gap * (n - 1);

  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  int x = cx - total / 2;
  for (const char *p = s; *p; p++) {
    if (*p < '0' || *p > '9' || !s_digits[*p - '0']) continue;
    GBitmap *bmp = s_digits[*p - '0'];
    // Two-colour image: index 0 is see-through, index 1 takes the clock colour.
    GColor *pal = gbitmap_get_palette(bmp);
    if (pal) {
      pal[0] = GColorClear;
      pal[1] = color;
    }
    GSize sz = gbitmap_get_bounds(bmp).size;
    graphics_draw_bitmap_in_rect(ctx, bmp, GRect(x, top, sz.w, sz.h));
    x += sz.w + gap;
  }
  graphics_context_set_compositing_mode(ctx, GCompOpAssign);
}
