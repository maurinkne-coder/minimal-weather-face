#include "contour.h"

// The face: one thin blue line round the edge of the screen, starting at the
// dot at the top ("now") and running clockwise into the coming hours, like a
// contour line. It hugs the edge while it stays dry and bends inward where
// rain is coming: the deeper the bend, the more rain, and each rain spell is
// labelled with its heaviest hour in mm (or inch). Likely rain is full blue,
// dry or unlikely stretches are pale. Small dots mark 00, 06, 12 and 18 o'clock.

#define FRAC 65536

typedef struct {
  GColor bg, fg, dim, accent, rain, rain_light;
} Theme;

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

#if defined(PBL_ROUND)
#define RAIN_EDGE 5       // the line when dry: pixels from the screen edge
#define RAIN_DEPTH 18     // how far in it bends for heavy rain
#define RAIN_W 3
#else
#define RAIN_EDGE 3
#define RAIN_DEPTH 12
#define RAIN_W 2
#define CORNER_R 18
#endif
#define TICK_IN (RAIN_EDGE + RAIN_DEPTH + 5)
#define BASE_MID (RAIN_EDGE + RAIN_DEPTH / 2)

// Text in the middle, as drawn upright, so labels can keep clear of it.
static GRect s_text[5];
static int s_text_n;

#if defined(PBL_ROUND)
static GPoint round_pt(GRect b, int32_t f, int inset) {
  int r = b.size.w / 2 - inset;
  int cx = b.origin.x + b.size.w / 2, cy = b.origin.y + b.size.h / 2;
  int32_t a = ((f % FRAC) + FRAC) % FRAC;
  return GPoint(cx + sin_lookup(a) * r / TRIG_MAX_RATIO,
                cy - cos_lookup(a) * r / TRIG_MAX_RATIO);
}
#else
static GPoint corner_pt(int ccx, int ccy, int c, int32_t angle) {
  return GPoint(ccx + sin_lookup(angle) * c / TRIG_MAX_RATIO,
                ccy - cos_lookup(angle) * c / TRIG_MAX_RATIO);
}

// A point on a rounded rectangle `inset` pixels inside the screen, at
// fraction f (0..FRAC) of the way round, starting top centre, clockwise.
static GPoint rect_pt(GRect b, int32_t f, int inset) {
  int c = CORNER_R;
  int l = b.origin.x + inset, t = b.origin.y + inset;
  int r = b.origin.x + b.size.w - 1 - inset, btm = b.origin.y + b.size.h - 1 - inset;
  int w = r - l, h = btm - t;
  int sx = w - 2 * c, sy = h - 2 * c;
  int arc = c * 157 / 100;
  int32_t per = 2 * sx + 2 * sy + 4 * arc;
  int32_t d = (int32_t)(((int64_t)(((f % FRAC) + FRAC) % FRAC)) * per / FRAC);
  const int32_t q = TRIG_MAX_ANGLE / 4;
  int cx = l + w / 2;

  if (d < sx / 2) return GPoint(cx + d, t);
  d -= sx / 2;
  if (d < arc) return corner_pt(r - c, t + c, c, d * q / arc);
  d -= arc;
  if (d < sy) return GPoint(r, t + c + d);
  d -= sy;
  if (d < arc) return corner_pt(r - c, btm - c, c, q + d * q / arc);
  d -= arc;
  if (d < sx) return GPoint(r - c - d, btm);
  d -= sx;
  if (d < arc) return corner_pt(l + c, btm - c, c, 2 * q + d * q / arc);
  d -= arc;
  if (d < sy) return GPoint(l, btm - c - d);
  d -= sy;
  if (d < arc) return corner_pt(l + c, t + c, c, 3 * q + d * q / arc);
  d -= arc;
  return GPoint(l + c + d, t);
}
#endif

// Where "now" sits when the face is turned: fraction of the way round.
static int32_t s_tilt_f;

static int32_t isqrt(int32_t v) {
  int32_t r = 0;
  while ((r + 1) * (r + 1) <= v) r++;
  return r;
}

// The point at fraction f round the face, `inset` pixels in from the edge,
// measured at right angles to the edge.
static GPoint track_pt(GRect b, int32_t f, int inset) {
  f += s_tilt_f;
#if defined(PBL_ROUND)
  return round_pt(b, f, inset);
#else
  GPoint m = rect_pt(b, f, BASE_MID);
  GPoint a = rect_pt(b, f - 300, BASE_MID);
  GPoint c = rect_pt(b, f + 300, BASE_MID);
  int32_t tx = c.x - a.x, ty = c.y - a.y;
  int32_t len = isqrt(tx * tx + ty * ty);
  if (len < 1) len = 1;
  int32_t nx = -ty, ny = tx; // inward normal for a clockwise path
  return GPoint(m.x + nx * (inset - BASE_MID) / len, m.y + ny * (inset - BASE_MID) / len);
#endif
}

// The fraction of the way round whose direction from the centre is `deg`
// degrees clockwise from straight up.
static int32_t tilt_frac(GRect b, int deg) {
  deg = ((deg % 360) + 360) % 360;
  if (!deg) return 0;
#if defined(PBL_ROUND)
  return (int32_t)deg * FRAC / 360;
#else
  int32_t a = DEG_TO_TRIGANGLE(deg);
  int32_t sx = sin_lookup(a) / 64, sy = -cos_lookup(a) / 64;
  int cx = b.origin.x + b.size.w / 2, cy = b.origin.y + b.size.h / 2;
  int32_t best = 0, best_score = INT32_MIN;
  for (int32_t f = 0; f < FRAC; f += FRAC / 512) {
    GPoint p = rect_pt(b, f, BASE_MID);
    int32_t dx = p.x - cx, dy = p.y - cy;
    int32_t len = isqrt(dx * dx + dy * dy);
    if (len < 1) continue;
    int32_t score = (dx * sx + dy * sy) / len;
    if (score > best_score) { best_score = score; best = f; }
  }
  return best;
#endif
}

// ---------------------------------------------------------------------------
// Colour
// ---------------------------------------------------------------------------

static Theme theme_for(int id) {
  switch (id) {
    case THEME_DAYLIGHT:
      return (Theme){ GColorWhite, GColorBlack, GColorDarkGray, GColorCobaltBlue,
                      GColorBlue, GColorPictonBlue };
    case THEME_MONO:
      return (Theme){ GColorBlack, GColorWhite, GColorLightGray, GColorWhite,
                      GColorWhite, GColorDarkGray };
    case THEME_GRAPHITE:
      return (Theme){ GColorDarkGray, GColorWhite, GColorLightGray, GColorIcterine,
                      GColorElectricBlue, GColorPictonBlue };
    default: // THEME_MIDNIGHT
      return (Theme){ GColorBlack, GColorWhite, GColorDarkGray, GColorCeleste,
                      GColorPictonBlue, GColorCobaltBlue };
  }
}

static bool is_daytime(time_t now) {
  int i = wx_index_at(now);
  if (i >= 0) return g_wx.day[i];
  struct tm *tm = localtime(&now);
  return tm->tm_hour >= 7 && tm->tm_hour < 19;
}

static Theme current_theme(time_t now) {
  int id = g_set.theme;
  if (id == THEME_AUTO) id = is_daytime(now) ? THEME_DAYLIGHT : THEME_MIDNIGHT;
  return theme_for(id);
}

// ---------------------------------------------------------------------------
// Forecast line
// ---------------------------------------------------------------------------

static int32_t frac_of(time_t t, time_t now, int range_s) {
  return (int32_t)((int64_t)(t - now) * FRAC / range_s);
}

// How far the line bends in at time t (0 when dry). Hourly amounts are
// placed at the middle of their hour and blended in between; a square root
// keeps light rain visible while heavy rain still bends deeper.
static int rain_depth_of(int32_t p) {
  if (p < 1) return 0;
  if (p > 40) p = 40;                                  // 4 mm/h fills the depth
  return 2 + (int)(isqrt(p * 100) * (RAIN_DEPTH - 2) / 63);
}

static int rain_depth(time_t t, bool *likely) {
  *likely = false;
  if (g_wx.count == 0) return 0;
  int32_t x = (int32_t)(t - (time_t)g_wx.t0) - 1800;   // seconds past hour 0's middle
  if (x < 0) x = 0;
  int k = x / 3600;
  if (k >= g_wx.count) return 0;
  int32_t frac = x % 3600;
  int32_t a = g_wx.precip[k];
  int32_t c = k + 1 < g_wx.count ? g_wx.precip[k + 1] : a;
  int32_t p = a + (c - a) * frac / 3600;               // 0.1 mm per hour
  uint8_t pr = frac < 1800 || k + 1 >= g_wx.count ? g_wx.prob[k] : g_wx.prob[k + 1];
  // Dry stretches stay pale, so the rain stands out in full blue.
  *likely = p >= 1 && (pr == PROB_UNKNOWN || pr >= 50);
  return rain_depth_of(p);
}

static bool rects_overlap(GRect a, GRect c) {
  return a.origin.x < c.origin.x + c.size.w && c.origin.x < a.origin.x + a.size.w &&
         a.origin.y < c.origin.y + c.size.h && c.origin.y < a.origin.y + a.size.h;
}

// Writes the spell's heaviest hourly amount just inside the line, as close to
// the deepest point as it fits without sitting on the text in the middle.
static bool label_fits(GRect b, GRect box) {
  int cx = b.origin.x + b.size.w / 2, cy = b.origin.y + b.size.h / 2;
#if defined(PBL_ROUND)
  // All four corners inside the circle, with a small margin.
  int r = b.size.w / 2 - RAIN_EDGE - RAIN_DEPTH - 3;   // clear of the line
  for (int i = 0; i < 4; i++) {
    int x = box.origin.x + (i & 1 ? box.size.w : 0) - cx;
    int y = box.origin.y + (i & 2 ? box.size.h : 0) - cy;
    if (x * x + y * y > r * r) return false;
  }
#else
  const int m = RAIN_EDGE + RAIN_DEPTH + 2;            // clear of the line
  if (box.origin.x < b.origin.x + m || box.origin.y < b.origin.y + m ||
      box.origin.x + box.size.w > b.origin.x + b.size.w - m ||
      box.origin.y + box.size.h > b.origin.y + b.size.h - m) return false;
#endif
  // Compare with the text in its own (possibly turned) frame.
  int32_t ta = DEG_TO_TRIGANGLE(((g_set.tilt % 360) + 360) % 360);
  int32_t sn = sin_lookup(ta), cs = cos_lookup(ta);
  int32_t dx = box.origin.x + box.size.w / 2 - cx, dy = box.origin.y + box.size.h / 2 - cy;
  int ux = cx + (int)((dx * cs + dy * sn) / TRIG_MAX_RATIO);
  int uy = cy + (int)((-dx * sn + dy * cs) / TRIG_MAX_RATIO);
  GRect turned = GRect(ux - box.size.w / 2, uy - box.size.h / 2, box.size.w, box.size.h);
  for (int i = 0; i < s_text_n; i++) {
    if (rects_overlap(turned, s_text[i])) return false;
  }
  return true;
}

static void draw_rain_label(GContext *ctx, GRect b, const Theme *th, time_t now, int range_s,
                            int k0, int k1, int peak) {
  char tb[16];
  fmt_rain_short(tb, sizeof(tb), g_wx.precip[peak]);
  // Try the peak first, then step outwards through the spell, half an hour at a time.
  int span = (k1 - k0 + 1) * 2;
  for (int i = 0; i <= span * 2; i++) {
    int step = (i + 1) / 2 * (i % 2 ? 1 : -1);
    time_t t = (time_t)g_wx.t0 + (time_t)peak * 3600 + 1800 + (time_t)step * 1800;
    if (t < (time_t)g_wx.t0 + (time_t)k0 * 3600 || t > (time_t)g_wx.t0 + (time_t)(k1 + 1) * 3600) continue;
    if (t < now || t >= now + range_s) continue;
    int32_t f = frac_of(t, now, range_s);
    for (int in = RAIN_EDGE + RAIN_DEPTH + 8; in <= RAIN_EDGE + RAIN_DEPTH + 32; in += 4) {
      GPoint p = track_pt(b, f, in);
      GRect box = GRect(p.x - 20, p.y - 7, 40, 14);
      if (!label_fits(b, box)) continue;
      graphics_context_set_text_color(ctx, th->rain);
      graphics_draw_text(ctx, tb, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                         GRect(p.x - 22, p.y - 10, 44, 16), GTextOverflowModeFill,
                         GTextAlignmentCenter, NULL);
      return;
    }
  }
}

static void draw_track(GContext *ctx, GRect b, const Theme *th, time_t now) {
  const int range = wx_range_hours();
  const int range_s = range * 3600;
  const int steps = range * PBL_IF_ROUND_ELSE(6, 4);
  const int shown = steps * g_view.reveal / 1000;

  graphics_context_set_antialiased(ctx, true);

  // Time dots at 00, 06, 12 and 18 local time, just inside the line.
  graphics_context_set_fill_color(ctx, th->dim);
  for (time_t t = now - now % 3600 + 3600; t < now + range_s; t += 3600) {
    struct tm *tm = localtime(&t);
    if (tm->tm_min != 0 || tm->tm_hour % 6 != 0) continue;
    int32_t f = frac_of(t, now, range_s);
    if (f * 1000 / FRAC > g_view.reveal) continue;
    graphics_fill_circle(ctx, track_pt(b, f, TICK_IN), tm->tm_hour == 0 ? 2 : 1);
  }

  // The rain line, all the way round.
  graphics_context_set_stroke_width(ctx, RAIN_W);
  GPoint prev = GPoint(0, 0);
  for (int s = 0; s <= shown; s++) {
    int32_t f = (int32_t)s * FRAC / steps;
    time_t t = now + (time_t)s * range_s / steps;
    bool likely;
    GPoint p = track_pt(b, f, RAIN_EDGE + rain_depth(t, &likely));
    if (s > 0) {
      graphics_context_set_stroke_color(ctx, likely ? th->rain : th->rain_light);
      graphics_draw_line(ctx, prev, p);
    }
    prev = p;
  }

  // Amount at the heaviest hour of each rain spell (up to 4 spells).
  int first = wx_index_at(now);
  if (first >= 0 && g_view.reveal >= 1000) {
    int labels = 0, peak = -1, k0 = -1;
    for (int k = first; k <= g_wx.count && labels < 4; k++) {
      time_t start = (time_t)g_wx.t0 + (time_t)k * 3600;
      bool in_range = k < g_wx.count && start < now + range_s;
      bool wet = in_range && g_wx.precip[k] >= 1;
      if (wet) {
        if (peak < 0) k0 = k;
        if (peak < 0 || g_wx.precip[k] > g_wx.precip[peak]) peak = k;
      } else if (peak >= 0) {
        draw_rain_label(ctx, b, th, now, range_s, k0, k - 1, peak);
        labels++;
        peak = -1;
      }
      if (!in_range) break;
    }
  }

  // "Now" marker on the line, and the scroll cursor during a wrist flick.
  bool likely;
  GPoint now_p = track_pt(b, 0, RAIN_EDGE + rain_depth(now, &likely));
  graphics_context_set_fill_color(ctx, th->bg);
  graphics_fill_circle(ctx, now_p, 6);
  graphics_context_set_fill_color(ctx, th->fg);
  graphics_fill_circle(ctx, now_p, 4);
  if (g_view.scrub >= 0) {
    int32_t f = (int32_t)((int64_t)g_view.scrub * FRAC / (range * 10));
    time_t t = now + (time_t)g_view.scrub * 360;
    GPoint p = track_pt(b, f, RAIN_EDGE + rain_depth(t, &likely));
    graphics_context_set_fill_color(ctx, th->bg);
    graphics_fill_circle(ctx, p, 6);
    graphics_context_set_fill_color(ctx, th->accent);
    graphics_fill_circle(ctx, p, 4);
  }
}

// ---------------------------------------------------------------------------
// Text in the middle
// ---------------------------------------------------------------------------

static void draw_text_font(GContext *ctx, const char *s, GFont font, GRect r, GColor c) {
  if (s_text_n < (int)ARRAY_LENGTH(s_text) && s[0]) {
    GSize sz = graphics_text_layout_get_content_size(s, font, r, GTextOverflowModeTrailingEllipsis,
                                                     GTextAlignmentCenter);
    s_text[s_text_n++] = GRect(r.origin.x + (r.size.w - sz.w) / 2 - 2, r.origin.y,
                               sz.w + 4, sz.h + 4);
  }
  graphics_context_set_text_color(ctx, c);
  graphics_draw_text(ctx, s, font, r, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void draw_text_line(GContext *ctx, const char *s, const char *font, GRect r, GColor c) {
  draw_text_font(ctx, s, fonts_get_system_font(font), r, c);
}

void draw_face(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  time_t now = g_view.now;
  Theme th = current_theme(now);

  graphics_context_set_fill_color(ctx, th.bg);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  s_text_n = 0;
  char date[16], clock[8], line1[40], line2[64], corner[24];
  text_date(date, sizeof(date), now);
  text_time(clock, sizeof(clock), now);
  if (g_view.scrub >= 0) {
    text_scrub(line1, sizeof(line1), line2, sizeof(line2), now + (time_t)g_view.scrub * 360);
  } else {
    text_weather(line1, sizeof(line1), now);
    text_smart(line2, sizeof(line2), now);
  }
  text_corner(corner, sizeof(corner));

  const int cy = b.origin.y + b.size.h / 2;
  const int side = PBL_IF_ROUND_ELSE(36, 26);
  const int w = b.size.w - 2 * side;
  const int x = b.origin.x + side;
  // Stack: date, big time, weather, smart line, extra line; centred on cy.
  const int time_h = 62, time_dy = 6;    // Inter Light 60
  const int block = 18 + time_h + 28 + 22 + (corner[0] ? 18 : 0);
  const int top = cy - block / 2 + PBL_IF_ROUND_ELSE(22, 2);
  int y = top;
  draw_text_line(ctx, date, FONT_KEY_GOTHIC_18_BOLD, GRect(x, y, w, 22), th.dim);
  y += time_dy;
  draw_text_font(ctx, clock, g_time_font, GRect(b.origin.x, y, b.size.w, time_h), th.fg);
  y += time_h;
  draw_text_line(ctx, line1, FONT_KEY_GOTHIC_24_BOLD, GRect(x, y, w, 30), th.fg);
  y += 28;
  draw_text_line(ctx, line2, FONT_KEY_GOTHIC_18, GRect(x, y, w, 24), th.accent);
  y += 23;
  if (corner[0]) {
    draw_text_line(ctx, corner, FONT_KEY_GOTHIC_14, GRect(x, y, w, 18), th.dim);
  }

  // Turn the text, then draw the forecast line already turned.
  static int s_tilt_deg = 0;
  static GSize s_tilt_size;
  if (g_set.tilt != s_tilt_deg || s_tilt_size.w != b.size.w || s_tilt_size.h != b.size.h) {
    s_tilt_deg = g_set.tilt;
    s_tilt_size = b.size;
    s_tilt_f = 0;
    s_tilt_f = tilt_frac(b, g_set.tilt);
  }
  tilt_apply(ctx, b, g_set.tilt, th.bg);
  draw_track(ctx, b, &th, now);
}
