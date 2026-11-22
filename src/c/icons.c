#include "contour.h"

// Small weather symbols (about 14 x 14 px) in a thin outline style, and the
// charging bolt, drawn with plain shapes so they take any theme colour and
// stay sharp at every tilt.

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

// The cloud as a 1 px outline: the full shape, hollowed out in `bg`.
static void cloud_line(GContext *ctx, int x, int y, GColor c, GColor bg) {
  cloud(ctx, x, y, 0, c);
  fill_circle(ctx, x - 3, y + 1, 2, bg);
  fill_circle(ctx, x + 1, y - 1, 3, bg);
  fill_circle(ctx, x + 4, y + 1, 2, bg);
  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, GRect(x - 3, y + 1, 8, 3), 0, GCornerNone);
}

// Sun: a thin ring with eight short rays.
static void sun(GContext *ctx, int x, int y, int r, GColor c, GColor bg) {
  static const int8_t RAY[8][2] = { {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1} };
  fill_circle(ctx, x, y, r, c);
  if (r >= 2) fill_circle(ctx, x, y, r - 1, bg);
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
    if (day) sun(ctx, x, y, 3, k->sun, k->bg);
    else moon(ctx, x, y, 5, k->cloud, k->bg);
  } else if (code == 2) {                        // partly cloudy
    if (day) sun(ctx, x - 3, y - 3, 2, k->sun, k->bg);
    else moon(ctx, x - 3, y - 3, 4, k->cloud, k->bg);
    cloud(ctx, x + 1, y + 2, 1, k->bg);
    cloud_line(ctx, x + 1, y + 2, k->cloud, k->bg);
  } else if (code == 3) {                        // overcast
    cloud_line(ctx, x, y, k->cloud, k->bg);
  } else if (code == 45 || code == 48) {         // fog
    line(ctx, x - 6, y - 4, x + 4, y - 4, 1, k->cloud);
    line(ctx, x - 4, y, x + 6, y, 1, k->cloud);
    line(ctx, x - 6, y + 4, x + 5, y + 4, 1, k->cloud);
  } else if ((code >= 71 && code <= 77) || code == 85 || code == 86) {  // snow
    cloud_line(ctx, x, y - 3, k->cloud, k->bg);
    fill_circle(ctx, x - 4, y + 4, 1, k->cloud);
    fill_circle(ctx, x, y + 6, 1, k->cloud);
    fill_circle(ctx, x + 4, y + 4, 1, k->cloud);
  } else if (code >= 95) {                       // thunderstorm
    cloud_line(ctx, x, y - 3, k->cloud, k->bg);
    bolt(ctx, x + 1, y + 4, k->sun);
  } else if (code >= 51 && code <= 57) {         // drizzle
    cloud_line(ctx, x, y - 3, k->cloud, k->bg);
    fill_circle(ctx, x - 4, y + 4, 1, k->water);
    fill_circle(ctx, x, y + 5, 1, k->water);
    fill_circle(ctx, x + 4, y + 4, 1, k->water);
  } else {                                       // rain and showers
    cloud_line(ctx, x, y - 3, k->cloud, k->bg);
    line(ctx, x - 4, y + 3, x - 5, y + 6, 1, k->water);
    line(ctx, x, y + 3, x - 1, y + 6, 1, k->water);
    line(ctx, x + 4, y + 3, x + 3, y + 6, 1, k->water);
  }
}

void icon_bolt(GContext *ctx, GPoint c, GColor col) {
  graphics_context_set_antialiased(ctx, true);
  bolt(ctx, c.x, c.y, col);
}

// ---- Symbols for the sides of the middle text ----

static void path(GContext *ctx, GPoint *pts, int n, bool filled, GColor c) {
  GPathInfo info = { .num_points = (uint32_t)n, .points = pts };
  GPath *p = gpath_create(&info);
  if (!p) return;
  if (filled) {
    graphics_context_set_fill_color(ctx, c);
    gpath_draw_filled(ctx, p);
  } else {
    graphics_context_set_stroke_color(ctx, c);
    graphics_context_set_stroke_width(ctx, 1);
    gpath_draw_outline(ctx, p);
  }
  gpath_destroy(p);
}

// Rain chance: a droplet outline that fills from the bottom, 0..100 %.
void icon_drop(GContext *ctx, GPoint c, int level, GColor water, GColor bg) {
  graphics_context_set_antialiased(ctx, true);
  GPoint pts[12];
  int n = 0;
  pts[n++] = GPoint(c.x, c.y - 7);                    // the tip
  for (int deg = 40; deg <= 320; deg += 28) {         // round bottom, r 4.5 round (x, y+2)
    int32_t a = DEG_TO_TRIGANGLE(deg);
    pts[n++] = GPoint(c.x + sin_lookup(a) * 9 / 2 / TRIG_MAX_RATIO,
                      c.y + 2 - cos_lookup(a) * 9 / 2 / TRIG_MAX_RATIO);
  }
  if (level < 0) level = 0;
  if (level > 100) level = 100;
  if (level > 0) {
    path(ctx, pts, n, true, water);
    int top = c.y - 7, bottom = c.y + 7;
    int cut = bottom - (bottom - top) * level / 100;  // rows above this stay empty
    if (cut > top) {
      graphics_context_set_fill_color(ctx, bg);
      graphics_fill_rect(ctx, GRect(c.x - 6, top - 1, 13, cut - top + 1), 0, GCornerNone);
    }
  }
  path(ctx, pts, n, false, water);
}

// Phone disconnected: a phone outline with a slash through it.
void icon_phone_off(GContext *ctx, GPoint c, GColor col, GColor slash, GColor bg) {
  graphics_context_set_antialiased(ctx, true);
  line(ctx, c.x - 3, c.y - 6, c.x + 3, c.y - 6, 1, col);
  line(ctx, c.x - 3, c.y + 6, c.x + 3, c.y + 6, 1, col);
  line(ctx, c.x - 3, c.y - 6, c.x - 3, c.y + 6, 1, col);
  line(ctx, c.x + 3, c.y - 6, c.x + 3, c.y + 6, 1, col);
  line(ctx, c.x - 1, c.y + 4, c.x + 1, c.y + 4, 1, col);      // home button
  line(ctx, c.x - 6, c.y + 5, c.x + 6, c.y - 5, 4, bg);       // gap round the slash
  line(ctx, c.x - 6, c.y + 5, c.x + 6, c.y - 5, 2, slash);
}

// Quiet Time: a crescent moon with a small z.
void icon_quiet(GContext *ctx, GPoint c, GColor col, GColor bg) {
  graphics_context_set_antialiased(ctx, true);
  moon(ctx, c.x - 2, c.y + 1, 5, col, bg);
  line(ctx, c.x + 2, c.y - 6, c.x + 6, c.y - 6, 1, col);
  line(ctx, c.x + 6, c.y - 6, c.x + 2, c.y - 2, 1, col);
  line(ctx, c.x + 2, c.y - 2, c.x + 6, c.y - 2, 1, col);
}

// Wind: an arrow pointing the way the wind blows, `deg` clockwise from up.
void icon_arrow(GContext *ctx, GPoint c, int deg, GColor col) {
  graphics_context_set_antialiased(ctx, true);
  static const int8_t A[7][2] = { {0, -7}, {5, -1}, {1, -1}, {1, 7}, {-1, 7}, {-1, -1}, {-5, -1} };
  int32_t a = DEG_TO_TRIGANGLE(((deg % 360) + 360) % 360);
  int32_t sn = sin_lookup(a), cs = cos_lookup(a);
  GPoint pts[7];
  for (int i = 0; i < 7; i++) {
    int x = A[i][0], y = A[i][1];
    pts[i] = GPoint(c.x + (x * cs - y * sn) / TRIG_MAX_RATIO, c.y + (x * sn + y * cs) / TRIG_MAX_RATIO);
  }
  path(ctx, pts, 7, true, col);
}

// Moon phase: a disc, lit part in `lit`, dark part in `dark`. `phase` runs
// 0..1000 through the month: 0 new, 250 first quarter, 500 full, 750 last.
void icon_moon_phase(GContext *ctx, GPoint c, int phase, GColor lit, GColor dark) {
  const int r = 5;
  bool waxing = phase < 500;
  int32_t a = DEG_TO_TRIGANGLE((waxing ? phase : phase - 500) * 360 / 1000);
  int32_t k = cos_lookup(a);                          // terminator, -1..1 of the half width
  for (int dy = -r; dy <= r; dy++) {
    for (int dx = -r; dx <= r; dx++) {
      int d2 = dx * dx + dy * dy;
      if (d2 > r * r + r) continue;
      int w2 = r * r + r - dy * dy;                   // squared half width of this row
      int w = 0;
      while ((w + 1) * (w + 1) <= w2) w++;
      int32_t t = k * w / TRIG_MAX_RATIO;
      bool on = waxing ? dx > t : dx < t;
      graphics_context_set_fill_color(ctx, on ? lit : dark);
      graphics_fill_rect(ctx, GRect(c.x + dx, c.y + dy, 1, 1), 0, GCornerNone);
    }
  }
}

// Heart rate: a heart outline, filled when `filled`.
void icon_heart(GContext *ctx, GPoint c, bool filled, GColor col) {
  graphics_context_set_antialiased(ctx, true);
  static const int8_t H[10][2] = { {0, -3}, {-2, -5}, {-4, -5}, {-6, -3}, {-6, -1},
                                   {0, 5}, {6, -1}, {6, -3}, {4, -5}, {2, -5} };
  GPoint pts[10];
  for (int i = 0; i < 10; i++) pts[i] = GPoint(c.x + H[i][0], c.y + H[i][1]);
  if (filled) path(ctx, pts, 10, true, col);
  path(ctx, pts, 10, false, col);
}
