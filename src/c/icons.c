#include "contour.h"

// Small weather symbols (about 14 x 14 px) and the charging bolt, drawn with
// plain shapes so they take any theme colour and stay sharp at every tilt.

static void fill_circle(GContext *ctx, int x, int y, int r, GColor c) {
  graphics_context_set_fill_color(ctx, c);
  graphics_fill_circle(ctx, GPoint(x, y), r);
}

static void line(GContext *ctx, int x0, int y0, int x1, int y1, int w, GColor c) {
  graphics_context_set_stroke_color(ctx, c);
  graphics_context_set_stroke_width(ctx, w);
  graphics_draw_line(ctx, GPoint(x0, y0), GPoint(x1, y1));
}

// A cloud about 14 x 8 px round (x, y). `grow` 1 draws it one pixel larger,
// used in the background colour as a thin gap where it overlaps a sun or moon.
static void cloud(GContext *ctx, int x, int y, int grow, GColor c) {
  fill_circle(ctx, x - 3, y + 1, 3 + grow, c);
  fill_circle(ctx, x + 1, y - 1, 4 + grow, c);
  fill_circle(ctx, x + 4, y + 1, 3 + grow, c);
  graphics_context_set_fill_color(ctx, c);
  graphics_fill_rect(ctx, GRect(x - 3, y + 1 - grow, 8, 4 + 2 * grow), 0, GCornerNone);
}

static void sun(GContext *ctx, int x, int y, int r, GColor c) {
  static const int8_t RAY[8][2] = { {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1} };
  fill_circle(ctx, x, y, r, c);
  for (int i = 0; i < 8; i++) {
    int d0 = r + 2, d1 = r + 3;
    if (RAY[i][0] && RAY[i][1]) { d0 = (r + 2) * 3 / 4 + 1; d1 = (r + 3) * 3 / 4 + 1; }  // diagonals
    line(ctx, x + RAY[i][0] * d0, y + RAY[i][1] * d0, x + RAY[i][0] * d1, y + RAY[i][1] * d1, 1, c);
  }
}

static void moon(GContext *ctx, int x, int y, int r, GColor c, GColor bg) {
  fill_circle(ctx, x, y, r, c);
  fill_circle(ctx, x + r * 2 / 3 + 1, y - r / 2, r - 1, bg);
}

static void bolt(GContext *ctx, int x, int y, GColor c) {
  GPoint pts[] = { {x, y - 6}, {x - 4, y + 1}, {x - 1, y + 1}, {x - 2, y + 6},
                   {x + 4, y - 1}, {x + 1, y - 1}, {x + 3, y - 6} };
  GPathInfo info = { .num_points = 7, .points = pts };
  GPath *p = gpath_create(&info);
  if (!p) return;
  graphics_context_set_fill_color(ctx, c);
  gpath_draw_filled(ctx, p);
  gpath_destroy(p);
}

void icon_weather(GContext *ctx, GPoint c, uint8_t code, bool day, const IconColors *k) {
  graphics_context_set_antialiased(ctx, true);
  const int x = c.x, y = c.y;
  if (code <= 1) {                               // clear, mostly clear
    if (day) sun(ctx, x, y, 3, k->sun);
    else moon(ctx, x, y, 5, k->cloud, k->bg);
  } else if (code == 2) {                        // partly cloudy
    if (day) sun(ctx, x - 3, y - 3, 2, k->sun);
    else moon(ctx, x - 3, y - 3, 4, k->cloud, k->bg);
    cloud(ctx, x + 1, y + 2, 1, k->bg);
    cloud(ctx, x + 1, y + 2, 0, k->cloud);
  } else if (code == 3) {                        // overcast
    cloud(ctx, x, y, 0, k->cloud);
  } else if (code == 45 || code == 48) {         // fog
    line(ctx, x - 6, y - 4, x + 4, y - 4, 2, k->cloud);
    line(ctx, x - 4, y, x + 6, y, 2, k->cloud);
    line(ctx, x - 6, y + 4, x + 5, y + 4, 2, k->cloud);
  } else if ((code >= 71 && code <= 77) || code == 85 || code == 86) {  // snow
    cloud(ctx, x, y - 3, 0, k->cloud);
    fill_circle(ctx, x - 4, y + 4, 1, k->cloud);
    fill_circle(ctx, x, y + 6, 1, k->cloud);
    fill_circle(ctx, x + 4, y + 4, 1, k->cloud);
  } else if (code >= 95) {                       // thunderstorm
    cloud(ctx, x, y - 3, 0, k->cloud);
    bolt(ctx, x + 1, y + 4, k->sun);
  } else if (code >= 51 && code <= 57) {         // drizzle
    cloud(ctx, x, y - 3, 0, k->cloud);
    fill_circle(ctx, x - 4, y + 4, 1, k->water);
    fill_circle(ctx, x, y + 5, 1, k->water);
    fill_circle(ctx, x + 4, y + 4, 1, k->water);
  } else {                                       // rain and showers
    cloud(ctx, x, y - 3, 0, k->cloud);
    line(ctx, x - 4, y + 3, x - 5, y + 6, 2, k->water);
    line(ctx, x, y + 3, x - 1, y + 6, 2, k->water);
    line(ctx, x + 4, y + 3, x + 3, y + 6, 2, k->water);
  }
}

void icon_bolt(GContext *ctx, GPoint c, GColor col) {
  graphics_context_set_antialiased(ctx, true);
  bolt(ctx, c.x, c.y, col);
}
