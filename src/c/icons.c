#include "contour.h"

// Small symbols (about 14 x 14 px) in a thin outline style. They are drawn
// like an SVG: each shape is a set of outlines and only the pixels the
// shape covers change, smoothly mixed with whatever is already under them
// (rain bars, the background). Nothing is ever painted in the background
// colour, so no box shows behind a symbol.
//
// Every shape is a test "is this point inside?" in 1/16 px from the
// symbol's centre. Each screen pixel asks it 16 times (4 x 4) and mixes
// the shape's colour in by how many points were inside.

#define U 16              // 1 px
#define PX(v) ((int)((v) * U))
#define W PX(1.25)        // outline width

typedef bool (*Inside)(int x, int y, const int32_t *p);

static inline uint8_t mix2(uint8_t under, uint8_t col, int k) {  // k of 16
  uint8_t out = 0xC0;
  for (int sh = 0; sh <= 4; sh += 2) {
    int v = (((under >> sh) & 3) * (16 - k) + ((col >> sh) & 3) * k + 8) / 16;
    out |= (uint8_t)(v << sh);
  }
  return out;
}

#ifdef HOST_PREVIEW
void host_px(int x, int y, uint8_t argb, int k);  // tools/preview/stub.c
#endif

static void paint(GContext *ctx, GPoint c, GColor col, Inside in, const int32_t *p) {
  const int R = 9;  // half the box searched round the centre
#ifndef HOST_PREVIEW
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (!fb) return;
  GRect fbb = gbitmap_get_bounds(fb);
#endif
  for (int py = c.y - R; py <= c.y + R; py++) {
#ifndef HOST_PREVIEW
    if (py < 0 || py >= fbb.size.h) continue;
    GBitmapDataRowInfo row = gbitmap_get_data_row_info(fb, py);
#endif
    for (int px = c.x - R; px <= c.x + R; px++) {
#ifndef HOST_PREVIEW
      if (px < row.min_x || px > row.max_x) continue;
#endif
      int k = 0;
      for (int j = 0; j < 4; j++) {
        int sy = (py - c.y) * U + 4 * j - 6;
        for (int i = 0; i < 4; i++) {
          if (in((px - c.x) * U + 4 * i - 6, sy, p)) k++;
        }
      }
      // Three steps only: off, half, full. With 2 bits per colour channel
      // finer steps turn edges into odd in-between colours.
      if (k <= 4) continue;
      k = k >= 12 ? 16 : 8;
#ifdef HOST_PREVIEW
      host_px(px, py, col.argb, k);
#else
      row.data[px] = mix2(row.data[px], col.argb, k);
#endif
    }
  }
#ifndef HOST_PREVIEW
  graphics_release_frame_buffer(ctx, fb);
#endif
}

// ---- Shape pieces (all lengths in 1/16 px) ----

static inline bool disc(int x, int y, int cx, int cy, int r) {
  int dx = x - cx, dy = y - cy;
  return dx * dx + dy * dy <= r * r;
}

static inline bool ring(int x, int y, int cx, int cy, int r, int w) {
  int dx = x - cx, dy = y - cy, d2 = dx * dx + dy * dy;
  int a = r - w / 2, b = r + w / 2;
  return d2 <= b * b && (a <= 0 || d2 >= a * a);
}

// A stroke from (x0, y0) to (x1, y1) with round ends.
static bool seg(int x, int y, int x0, int y0, int x1, int y1, int w) {
  int vx = x1 - x0, vy = y1 - y0, ux = x - x0, uy = y - y0;
  int32_t len2 = vx * vx + vy * vy, t = ux * vx + uy * vy;
  int32_t ex, ey;
  if (t <= 0 || len2 == 0) { ex = ux; ey = uy; }
  else if (t >= len2) { ex = x - x1; ey = y - y1; }
  else { ex = ux - (int32_t)((int64_t)vx * t / len2); ey = uy - (int32_t)((int64_t)vy * t / len2); }
  return ex * ex + ey * ey <= (w / 2) * (w / 2);
}

// Rounded rectangle centred on (0, 0).
static bool rrect(int x, int y, int hw, int hh, int r) {
  if (r < 0) r = 0;
  int dx = (x < 0 ? -x : x) - (hw - r), dy = (y < 0 ? -y : y) - (hh - r);
  if (dx > 0 && dy > 0) return dx * dx + dy * dy <= r * r;
  return dx <= r && dy <= r;
}

static int isqrt(int32_t v) {
  if (v <= 0) return 0;
  int32_t r = v > 1 ? v / 2 : 1;
  for (int i = 0; i < 20; i++) {
    int32_t n = (r + v / r) / 2;
    if (n >= r) break;
    r = n;
  }
  return (int)r;
}

// ---- Cloud ----

// A cloud round (ox, oy), `grow` larger (negative shrinks it, for the inside).
static bool cloud_in(int x, int y, int ox, int oy, int grow) {
  return disc(x, y, ox - PX(3.5), oy + PX(1.5), PX(3) + grow) ||
         disc(x, y, ox + PX(1), oy - PX(1), PX(4) + grow) ||
         disc(x, y, ox + PX(4), oy + PX(1.5), PX(3) + grow) ||
         (x >= ox - PX(3.5) && x <= ox + PX(4) && y >= oy + PX(1.5) && y <= oy + PX(4.5) + grow);
}

// p: ox, oy
static bool cloud_line(int x, int y, const int32_t *p) {
  return cloud_in(x, y, p[0], p[1], 0) && !cloud_in(x, y, p[0], p[1], -W);
}

// ---- Sun, moon ----

static const int8_t RAY[8][2] = { {0, -10}, {7, -7}, {10, 0}, {7, 7}, {0, 10}, {-7, 7}, {-10, 0}, {-7, -7} };

// p: cx, cy, r, has a cloud at (p[3], p[4]) to keep clear of
static bool sun_in(int x, int y, const int32_t *p) {
  if (p[5] && cloud_in(x, y, p[3], p[4], PX(1.25))) return false;
  if (ring(x, y, p[0], p[1], p[2], W)) return true;
  int d0 = p[2] + PX(2), d1 = p[2] + PX(3.5);
  for (int i = 0; i < 8; i++) {
    if (seg(x, y, p[0] + RAY[i][0] * d0 / 10, p[1] + RAY[i][1] * d0 / 10,
            p[0] + RAY[i][0] * d1 / 10, p[1] + RAY[i][1] * d1 / 10, W)) return true;
  }
  return false;
}

// Crescent outline. p: cx, cy, r, has cloud, cloud x, cloud y
static bool crescent_in(int x, int y, const int32_t *p) {
  if (p[3] && cloud_in(x, y, p[4], p[5], PX(1.25))) return false;
  int cx = p[0], cy = p[1], r = p[2];
  int bx = cx + r / 2, by = cy - r * 2 / 5, br = r * 4 / 5;  // the bite
  bool shape = disc(x, y, cx, cy, r) && !disc(x, y, bx, by, br);
  bool inner = disc(x, y, cx, cy, r - W) && !disc(x, y, bx, by, br + W);
  return shape && !inner;
}

// ---- Weather ----

static bool fog_in(int x, int y, const int32_t *p) {
  return seg(x, y, PX(-6), PX(-4), PX(4), PX(-4), W) || seg(x, y, PX(-4), 0, PX(6), 0, W) ||
         seg(x, y, PX(-6), PX(4), PX(5), PX(4), W);
}

static bool snow_in(int x, int y, const int32_t *p) {
  return disc(x, y, PX(-4), PX(4.5), PX(1.2)) || disc(x, y, 0, PX(6.5), PX(1.2)) ||
         disc(x, y, PX(4), PX(4.5), PX(1.2));
}

static bool drizzle_in(int x, int y, const int32_t *p) {
  return disc(x, y, PX(-4), PX(4.5), PX(1)) || disc(x, y, 0, PX(5.5), PX(1)) ||
         disc(x, y, PX(4), PX(4.5), PX(1));
}

static bool rain_in(int x, int y, const int32_t *p) {
  return seg(x, y, PX(-4), PX(3.5), PX(-5), PX(6.5), W) || seg(x, y, 0, PX(3.5), PX(-1), PX(6.5), W) ||
         seg(x, y, PX(4), PX(3.5), PX(3), PX(6.5), W);
}

static void bolt(GContext *ctx, int x, int y, GColor c) {
  GPoint pts[] = { {x, y - 6}, {x - 4, y + 1}, {x - 1, y + 1}, {x - 2, y + 6},
                   {x + 4, y - 1}, {x + 1, y - 1}, {x + 3, y - 6} };
  GPathInfo info = { .num_points = 7, .points = pts };
  GPath *p = gpath_create(&info);
  if (!p) return;
  graphics_context_set_antialiased(ctx, true);
  graphics_context_set_fill_color(ctx, c);
  gpath_draw_filled(ctx, p);
  gpath_destroy(p);
}

void icon_weather(GContext *ctx, GPoint c, uint8_t code, bool day, const IconColors *k) {
  const int32_t top[2] = { 0, PX(-3) };  // cloud raised to make room below
  if (code <= 1) {                               // clear, mostly clear
    if (day) {
      const int32_t s[6] = { 0, 0, PX(3), 0, 0, 0 };
      paint(ctx, c, k->sun, sun_in, s);
    } else {
      const int32_t m[6] = { 0, 0, PX(5.5), 0, 0, 0 };
      paint(ctx, c, k->cloud, crescent_in, m);
    }
  } else if (code == 2) {                        // partly cloudy
    const int32_t cl[2] = { PX(1), PX(2) };
    if (day) {
      const int32_t s[6] = { PX(-3), PX(-3), PX(2.25), cl[0], cl[1], 1 };
      paint(ctx, c, k->sun, sun_in, s);
    } else {
      const int32_t m[6] = { PX(-3), PX(-3), PX(4.5), 1, cl[0], cl[1] };
      paint(ctx, c, k->cloud, crescent_in, m);
    }
    paint(ctx, c, k->cloud, cloud_line, cl);
  } else if (code == 3) {                        // overcast
    const int32_t cl[2] = { 0, 0 };
    paint(ctx, c, k->cloud, cloud_line, cl);
  } else if (code == 45 || code == 48) {         // fog
    paint(ctx, c, k->cloud, fog_in, NULL);
  } else if ((code >= 71 && code <= 77) || code == 85 || code == 86) {  // snow
    paint(ctx, c, k->cloud, cloud_line, top);
    paint(ctx, c, k->cloud, snow_in, NULL);
  } else if (code >= 95) {                       // thunderstorm
    paint(ctx, c, k->cloud, cloud_line, top);
    bolt(ctx, c.x + 1, c.y + 4, k->sun);
  } else if (code >= 51 && code <= 57) {         // drizzle
    paint(ctx, c, k->cloud, cloud_line, top);
    paint(ctx, c, k->water, drizzle_in, NULL);
  } else {                                       // rain and showers
    paint(ctx, c, k->cloud, cloud_line, top);
    paint(ctx, c, k->water, rain_in, NULL);
  }
}

void icon_bolt(GContext *ctx, GPoint c, GColor col) {
  bolt(ctx, c.x, c.y, col);
}

// ---- Rain chance: a drop that fills from the bottom ----

// A teardrop: a round bottom (centre cy, radius r) and straight sides that
// meet it smoothly at a point 2r above the centre.
static bool drop_shape(int x, int y, int cy, int r) {
  if (disc(x, y, 0, cy, r)) return true;
  int tip = cy - 2 * r, py = cy - r / 2, hw = r * 866 / 1000;  // sides touch the circle at 30°
  if (y < tip || y > py) return false;
  int ax = x < 0 ? -x : x;
  return (int32_t)ax * (py - tip) <= (int32_t)hw * (y - tip);
}

#define DROP_CY PX(2)
#define DROP_R PX(4.5)

static bool drop_line(int x, int y, const int32_t *p) {
  return drop_shape(x, y, DROP_CY, DROP_R) && !drop_shape(x, y, DROP_CY, DROP_R - W);
}

// p: the water line (y), anything below it is filled
static bool drop_fill(int x, int y, const int32_t *p) {
  return y >= p[0] && drop_shape(x, y, DROP_CY, DROP_R - W / 2);
}

void icon_drop(GContext *ctx, GPoint c, int level, GColor water) {
  if (level > 100) level = 100;
  if (level > 0) {
    int top = DROP_CY - 2 * DROP_R, bottom = DROP_CY + DROP_R;
    const int32_t cut[1] = { bottom - (bottom - top) * level / 100 };
    paint(ctx, c, water, drop_fill, cut);
  }
  paint(ctx, c, water, drop_line, NULL);
}

// ---- Phone disconnected ----

#define SLASH PX(-6), PX(5.5), PX(6), PX(-5.5)

static bool phone_in(int x, int y, const int32_t *p) {
  if (seg(x, y, SLASH, PX(1.75) + 2 * PX(1.25))) return false;  // gap round the slash
  bool body = rrect(x, y, PX(3.75), PX(6.75), PX(1.75)) &&
              !rrect(x, y, PX(3.75) - W, PX(6.75) - W, PX(1.75) - W);
  return body || seg(x, y, PX(-1), PX(4.25), PX(1), PX(4.25), W);
}

static bool slash_in(int x, int y, const int32_t *p) {
  return seg(x, y, SLASH, PX(1.75));
}

void icon_phone_off(GContext *ctx, GPoint c, GColor col, GColor slash) {
  paint(ctx, c, col, phone_in, NULL);
  paint(ctx, c, slash, slash_in, NULL);
}

// ---- Quiet Time: a crescent with a small z ----

static bool z_in(int x, int y, const int32_t *p) {
  return seg(x, y, PX(2.5), PX(-6.5), PX(6.5), PX(-6.5), W) ||
         seg(x, y, PX(6.5), PX(-6.5), PX(2.5), PX(-2.5), W) ||
         seg(x, y, PX(2.5), PX(-2.5), PX(6.5), PX(-2.5), W);
}

void icon_quiet(GContext *ctx, GPoint c, GColor col) {
  const int32_t m[6] = { PX(-2), PX(1.5), PX(5.5), 0, 0, 0 };
  paint(ctx, c, col, crescent_in, m);
  paint(ctx, c, col, z_in, NULL);
}

// ---- Wind: an arrow pointing the way the wind blows ----

void icon_arrow(GContext *ctx, GPoint c, int deg, GColor col) {
  static const int8_t A[7][2] = { {0, -7}, {5, -1}, {1, -1}, {1, 7}, {-1, 7}, {-1, -1}, {-5, -1} };
  int32_t a = DEG_TO_TRIGANGLE(((deg % 360) + 360) % 360);
  int32_t sn = sin_lookup(a), cs = cos_lookup(a);
  GPoint pts[7];
  for (int i = 0; i < 7; i++) {
    int x = A[i][0], y = A[i][1];
    pts[i] = GPoint(c.x + (x * cs - y * sn) / TRIG_MAX_RATIO, c.y + (x * sn + y * cs) / TRIG_MAX_RATIO);
  }
  GPathInfo info = { .num_points = 7, .points = pts };
  GPath *p = gpath_create(&info);
  if (!p) return;
  graphics_context_set_antialiased(ctx, true);
  graphics_context_set_fill_color(ctx, col);
  gpath_draw_filled(ctx, p);
  gpath_destroy(p);
}

// ---- Moon phase ----

#define MOON_R PX(5.5)

static bool moon_ring(int x, int y, const int32_t *p) {
  return ring(x, y, 0, 0, MOON_R - W / 2, W);
}

// p: waxing, k (terminator, -1000..1000)
static bool moon_lit(int x, int y, const int32_t *p) {
  if (!disc(x, y, 0, 0, MOON_R)) return false;
  int w = isqrt(MOON_R * MOON_R - y * y);   // half width of the disc at this height
  int32_t t = p[1] * w / 1000;
  return p[0] ? x > t : x < -t;
}

// `phase` 0..1000 through the month: 0 new, 250 first quarter, 500 full.
void icon_moon_phase(GContext *ctx, GPoint c, int phase, GColor lit, GColor dark) {
  int32_t k = cos_lookup(DEG_TO_TRIGANGLE(phase * 360 / 1000)) * 1000 / TRIG_MAX_RATIO;
  const int32_t p[2] = { phase < 500, k };
  paint(ctx, c, dark, moon_ring, NULL);
  paint(ctx, c, lit, moon_lit, p);
}

// ---- Heart rate ----

static bool heart_shape(int x, int y, int in) {
  if (disc(x, y, PX(-2.75), PX(-2), PX(3) - in) || disc(x, y, PX(2.75), PX(-2), PX(3) - in)) return true;
  if (y < PX(-2)) return false;
  int ax = x < 0 ? -x : x;
  int hw = PX(5.6) * (PX(6.5) - y) / PX(8.5) - in * 13 / 10;  // sides run down to the point
  return ax <= hw;
}

static bool heart_line(int x, int y, const int32_t *p) {
  return heart_shape(x, y, 0) && !heart_shape(x, y, W);
}

static bool heart_fill(int x, int y, const int32_t *p) {
  return heart_shape(x, y, 0);
}

void icon_heart(GContext *ctx, GPoint c, bool filled, GColor col) {
  paint(ctx, c, col, filled ? heart_fill : heart_line, NULL);
}
