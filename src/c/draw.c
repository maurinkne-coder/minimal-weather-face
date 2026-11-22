#include "contour.h"

// The face: one thin line around the edge of the screen, starting at the top
// ("now") and running clockwise into the coming hours. The line sits closer
// to the edge when it is warmer and further in when it is colder, like a
// contour line. It is dimmer at night. Short bars inside it are rain, and
// small dots mark 00, 06, 12 and 18 o'clock.

#define FRAC 65536

typedef struct {
  GColor bg, fg, dim, accent, rain, rain_light;
} Theme;

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

#if defined(PBL_ROUND)
#define LINE_OUT 6        // warmest point: pixels from the screen edge
#define LINE_SPAN 20      // coldest point is this much further in
#define LINE_W 3
#define TICK_IN (LINE_OUT + LINE_SPAN + 4)
#define RAIN_IN (TICK_IN + 4)   // rain bars sit inside the line
#define RAIN_MAX 12
#else
// Less room beside the text on Time 2, so rain bars hug the screen edge
// and the line runs just inside them.
#define RAIN_IN 1
#define RAIN_MAX 7
#define LINE_OUT 11
#define LINE_SPAN 11
#define LINE_W 2
#define TICK_IN 5
#define CORNER_R 18
#endif

#define BASE_MID (LINE_OUT + LINE_SPAN / 2)

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

#if !defined(PBL_ROUND)
static int32_t isqrt(int32_t v) {
  int32_t r = 0;
  while ((r + 1) * (r + 1) <= v) r++;
  return r;
}
#endif

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

static void draw_ray(GContext *ctx, GRect b, int32_t f, int in0, int in1) {
  graphics_draw_line(ctx, track_pt(b, f, in0), track_pt(b, f, in1));
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

static int temp_inset(int16_t t, int16_t hi, int16_t lo) {
  if (hi <= lo) return LINE_OUT + LINE_SPAN / 2;
  return LINE_OUT + (int)((int32_t)(hi - t) * LINE_SPAN / (hi - lo));
}

static void draw_track(GContext *ctx, GRect b, const Theme *th, time_t now) {
  const int range = wx_range_hours();
  const int range_s = range * 3600;
  const int steps = range * PBL_IF_ROUND_ELSE(6, 4);
  const int shown = steps * g_view.reveal / 1000;

  graphics_context_set_antialiased(ctx, true);

  // Scale: warmest and coldest moment on the line. At least 6 degrees of
  // spread, so a flat day stays a calm line instead of a jagged one.
  int16_t hi = INT16_MIN, lo = INT16_MAX;
  for (int s = 0; s <= steps; s++) {
    bool ok;
    int16_t t = wx_temp_at(now + (time_t)s * range_s / steps, &ok);
    if (!ok) continue;
    if (t > hi) hi = t;
    if (t < lo) lo = t;
  }
  bool have = hi >= lo;
  if (have && hi - lo < 60) {
    int16_t mid = (hi + lo) / 2;
    hi = mid + 30;
    lo = mid - 30;
  }

  // Time dots at 00, 06, 12 and 18 local time.
  graphics_context_set_fill_color(ctx, th->dim);
  for (time_t t = now - now % 3600 + 3600; t < now + range_s; t += 3600) {
    struct tm *tm = localtime(&t);
    if (tm->tm_min != 0 || tm->tm_hour % 6 != 0) continue;
    int32_t f = frac_of(t, now, range_s);
    if (f * 1000 / FRAC > g_view.reveal) continue;
    graphics_fill_circle(ctx, track_pt(b, f, TICK_IN), tm->tm_hour == 0 ? 2 : 1);
  }

  // The temperature line.
  graphics_context_set_stroke_width(ctx, LINE_W);
  GPoint prev = GPoint(0, 0);
  bool prev_ok = false;
  for (int s = 0; s <= shown; s++) {
    int32_t f = (int32_t)s * FRAC / steps;
    time_t t = now + (time_t)s * range_s / steps;
    bool ok;
    int16_t temp = wx_temp_at(t, &ok);
    if (!ok) {
      prev_ok = false;
      continue;
    }
    GPoint p = track_pt(b, f, temp_inset(temp, hi, lo));
    if (prev_ok) {
      int i = wx_index_at(t);
      graphics_context_set_stroke_color(ctx, g_wx.day[i] ? th->fg : th->dim);
      graphics_draw_line(ctx, prev, p);
    }
    prev = p;
    prev_ok = true;
  }
  if (!have) {
    // No forecast yet: a faint resting line.
    graphics_context_set_stroke_color(ctx, th->dim);
    graphics_context_set_stroke_width(ctx, 1);
    for (int s = 0; s < shown; s++) {
      graphics_draw_line(ctx, track_pt(b, (int32_t)s * FRAC / steps, BASE_MID),
                         track_pt(b, (int32_t)(s + 1) * FRAC / steps, BASE_MID));
    }
  }

  // Rain bars, one per hour, inside the line.
  int rain_w = range <= 12 ? 5 : range <= 24 ? PBL_IF_ROUND_ELSE(5, 3) : 2;
  graphics_context_set_stroke_width(ctx, rain_w);
  int first = wx_index_at(now);
  if (first >= 0) {
    for (int k = first; k < g_wx.count; k++) {
      time_t mid = (time_t)g_wx.t0 + (time_t)k * 3600 + 1800;
      if (mid >= now + range_s) break;
      if (mid < now) mid = now;
      if (g_wx.precip[k] < 1) continue;
      int32_t f = frac_of(mid, now, range_s);
      if (f * 1000 / FRAC > g_view.reveal) continue;
      int p = g_wx.precip[k] > 40 ? 40 : g_wx.precip[k];   // cap at 4 mm/h
      int len = 2 + p * (RAIN_MAX - 2) / 40;
      bool likely = g_wx.prob[k] == PROB_UNKNOWN || g_wx.prob[k] >= 50;
      graphics_context_set_stroke_color(ctx, likely ? th->rain : th->rain_light);
      draw_ray(ctx, b, f, RAIN_IN, RAIN_IN + len);
    }
  }

#if defined(PBL_ROUND)
  // High and low of the next 24 hours, next to where they happen.
  if (first >= 0 && g_view.reveal >= 1000) {
    int ih = -1, il = -1;
    for (int k = first; k < g_wx.count && k < first + range; k++) {
      if (ih < 0 || g_wx.temp[k] > g_wx.temp[ih]) ih = k;
      if (il < 0 || g_wx.temp[k] < g_wx.temp[il]) il = k;
    }
    int marks[2] = {ih, il};
    graphics_context_set_text_color(ctx, th->dim);
    for (int m = 0; m < 2; m++) {
      if (m == 1 && g_wx.temp[il] == g_wx.temp[ih]) break;
      time_t t = (time_t)g_wx.t0 + (time_t)marks[m] * 3600;
      if (t < now) t = now;
      int32_t f = frac_of(t, now, range_s);
      if (f < FRAC / 40 || f > FRAC - FRAC / 40) continue; // keep clear of "now"
      GPoint p = track_pt(b, f, RAIN_IN + RAIN_MAX + 12);
      // Keep clear of the weather line beside the middle of the face.
      // Work in the turned frame of the text, then turn back.
      int cx = b.origin.x + b.size.w / 2, cy = b.origin.y + b.size.h / 2;
      int32_t ta = DEG_TO_TRIGANGLE(((g_set.tilt % 360) + 360) % 360);
      int32_t sn = sin_lookup(ta), cs = cos_lookup(ta);
      int32_t dx = p.x - cx, dy = p.y - cy;
      int32_t ux = (dx * cs + dy * sn) / TRIG_MAX_RATIO;
      int32_t uy = (-dx * sn + dy * cs) / TRIG_MAX_RATIO;
      if (uy > -34 && uy < 44) uy = uy < 5 ? -34 : 44;
      if (ux > -75 && ux < 75 && uy > -92 && uy < 68) continue; // would sit on the text
      p = GPoint(cx + (ux * cs - uy * sn) / TRIG_MAX_RATIO,
                 cy + (ux * sn + uy * cs) / TRIG_MAX_RATIO);
      char tb[8];
      fmt_temp(tb, sizeof(tb), g_wx.temp[marks[m]]);
      graphics_draw_text(ctx, tb, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                         GRect(p.x - 18, p.y - 10, 36, 16), GTextOverflowModeFill,
                         GTextAlignmentCenter, NULL);
    }
  }
#endif

  // "Now" marker on the line, and the scroll cursor during a wrist flick.
  bool ok;
  int16_t t_now = wx_temp_at(now, &ok);
  GPoint now_p = track_pt(b, 0, ok ? temp_inset(t_now, hi, lo) : BASE_MID);
  graphics_context_set_fill_color(ctx, th->bg);
  graphics_fill_circle(ctx, now_p, 5);
  graphics_context_set_fill_color(ctx, th->fg);
  graphics_fill_circle(ctx, now_p, 3);
  if (g_view.scrub >= 0) {
    int32_t f = (int32_t)((int64_t)g_view.scrub * FRAC / (range * 10));
    time_t t = now + (time_t)g_view.scrub * 360;
    int16_t ts = wx_temp_at(t, &ok);
    GPoint p = track_pt(b, f, ok ? temp_inset(ts, hi, lo) : BASE_MID);
    graphics_context_set_fill_color(ctx, th->bg);
    graphics_fill_circle(ctx, p, 6);
    graphics_context_set_fill_color(ctx, th->accent);
    graphics_fill_circle(ctx, p, 4);
  }
}

// ---------------------------------------------------------------------------
// Text in the middle
// ---------------------------------------------------------------------------

static void draw_text_line(GContext *ctx, const char *s, const char *font, GRect r, GColor c) {
  graphics_context_set_text_color(ctx, c);
  graphics_draw_text(ctx, s, fonts_get_system_font(font), r, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
}

void draw_face(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  time_t now = g_view.now;
  Theme th = current_theme(now);

  graphics_context_set_fill_color(ctx, th.bg);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

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
  // Stack: date, time, weather, smart line, extra line; centred on cy.
  const int top = cy - (corner[0] ? 74 : 64) + PBL_IF_ROUND_ELSE(0, 6);

  draw_text_line(ctx, date, FONT_KEY_GOTHIC_18_BOLD, GRect(x, top, w, 22), th.dim);
  draw_text_line(ctx, clock, FONT_KEY_BITHAM_42_LIGHT, GRect(x - 10, top + 14, w + 20, 50), th.fg);
  draw_text_line(ctx, line1, FONT_KEY_GOTHIC_24_BOLD, GRect(x, top + 64, w, 30), th.fg);
  draw_text_line(ctx, line2, FONT_KEY_GOTHIC_18, GRect(x, top + 92, w, 24), th.accent);
  if (corner[0]) {
    draw_text_line(ctx, corner, FONT_KEY_GOTHIC_14, GRect(x, top + 116, w, 18), th.dim);
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
