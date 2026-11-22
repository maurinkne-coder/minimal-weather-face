#include "contour.h"

// The face: a thin forecast track that follows the edge of the screen,
// starting at the top ("now") and running clockwise into the coming hours.
// The track's colour is the temperature, it is thinner at night, small bars
// inside it are rain, and notches mark 00, 06, 12 and 18 o'clock.

#define FRAC 65536

typedef struct {
  GColor bg, fg, dim, accent, rain, rain_light, empty;
  bool light, mono;
} Theme;

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

#if defined(PBL_ROUND)
#define TRACK_OUT 5      // gap between screen edge and track
#define TRACK_W 8        // daytime thickness
#define TRACK_W_NIGHT 4
#else
#define TRACK_OUT 4
#define TRACK_W 6
#define TRACK_W_NIGHT 3
#define CORNER_R 18
#endif

#define TRACK_MID (TRACK_OUT + TRACK_W / 2)
#define RAIN_IN (TRACK_OUT + TRACK_W + 4)
#define RAIN_MAX PBL_IF_ROUND_ELSE(14, 12)

#if defined(PBL_ROUND)
static GPoint round_pt(GRect b, int32_t f, int inset) {
  int r = b.size.w / 2 - inset;
  int cx = b.origin.x + b.size.w / 2, cy = b.origin.y + b.size.h / 2;
  int32_t a = f % TRIG_MAX_ANGLE;
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
  int32_t d = (int32_t)(((int64_t)(f % FRAC)) * per / FRAC);
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

static GPoint track_pt(GRect b, int32_t f, int inset) {
#if defined(PBL_ROUND)
  return round_pt(b, f, inset);
#else
  return rect_pt(b, f, inset);
#endif
}

// A short line across the track at fraction f, from `in0` to `in1` pixels
// inside the screen edge, at right angles to the track.
static void draw_ray(GContext *ctx, GRect b, int32_t f, int in0, int in1) {
#if defined(PBL_ROUND)
  graphics_draw_line(ctx, round_pt(b, f, in0), round_pt(b, f, in1));
#else
  GPoint m = rect_pt(b, f, TRACK_MID);
  GPoint a = rect_pt(b, f - 300 + FRAC, TRACK_MID);
  GPoint c = rect_pt(b, f + 300, TRACK_MID);
  int32_t tx = c.x - a.x, ty = c.y - a.y;
  int32_t len = 1;
  while ((len + 1) * (len + 1) <= tx * tx + ty * ty) len++;
  int32_t nx = -ty, ny = tx; // inward normal for a clockwise track
  GPoint p0 = GPoint(m.x + nx * (in0 - TRACK_MID) / len, m.y + ny * (in0 - TRACK_MID) / len);
  GPoint p1 = GPoint(m.x + nx * (in1 - TRACK_MID) / len, m.y + ny * (in1 - TRACK_MID) / len);
  graphics_draw_line(ctx, p0, p1);
#endif
}

// ---------------------------------------------------------------------------
// Colour
// ---------------------------------------------------------------------------

static Theme theme_for(int id) {
  Theme th;
  th.light = false;
  th.mono = false;
  switch (id) {
    case THEME_DAYLIGHT:
      th.bg = GColorWhite; th.fg = GColorBlack; th.dim = GColorDarkGray;
      th.accent = GColorCobaltBlue; th.rain = GColorBlue; th.rain_light = GColorPictonBlue;
      th.empty = GColorLightGray; th.light = true;
      break;
    case THEME_MONO:
      th.bg = GColorBlack; th.fg = GColorWhite; th.dim = GColorLightGray;
      th.accent = GColorWhite; th.rain = GColorWhite; th.rain_light = GColorDarkGray;
      th.empty = GColorDarkGray; th.mono = true;
      break;
    case THEME_GRAPHITE:
      th.bg = GColorDarkGray; th.fg = GColorWhite; th.dim = GColorLightGray;
      th.accent = GColorIcterine; th.rain = GColorElectricBlue; th.rain_light = GColorPictonBlue;
      th.empty = GColorBlack;
      break;
    default: // THEME_MIDNIGHT
      th.bg = GColorBlack; th.fg = GColorWhite; th.dim = GColorLightGray;
      th.accent = GColorCeleste; th.rain = GColorPictonBlue; th.rain_light = GColorCobaltBlue;
      th.empty = GColorDarkGray;
      break;
  }
  return th;
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

static GColor temp_color(const Theme *th, int16_t tenths) {
  if (th->mono) return th->fg;
  static const int16_t LIMIT[] = {-100, -50, 0, 50, 100, 150, 200, 250, 300};
  // Bright set for dark backgrounds, deeper set for white.
  static const uint8_t DARK[] = {
    GColorLavenderIndigoARGB8, GColorVeryLightBlueARGB8, GColorPictonBlueARGB8,
    GColorElectricBlueARGB8, GColorMediumAquamarineARGB8, GColorInchwormARGB8,
    GColorIcterineARGB8, GColorChromeYellowARGB8, GColorOrangeARGB8, GColorFollyARGB8,
  };
  static const uint8_t LIGHT[] = {
    GColorIndigoARGB8, GColorBlueARGB8, GColorBlueMoonARGB8, GColorVividCeruleanARGB8,
    GColorJaegerGreenARGB8, GColorKellyGreenARGB8, GColorLimerickARGB8,
    GColorChromeYellowARGB8, GColorOrangeARGB8, GColorRedARGB8,
  };
  int k = 0;
  while (k < (int)ARRAY_LENGTH(LIMIT) && tenths >= LIMIT[k]) k++;
  return (GColor){ .argb = th->light ? LIGHT[k] : DARK[k] };
}

// ---------------------------------------------------------------------------
// Forecast track
// ---------------------------------------------------------------------------

static int32_t frac_of(time_t t, time_t now, int range_s) {
  return (int32_t)((int64_t)(t - now) * FRAC / range_s);
}

static void draw_band(GContext *ctx, GRect b, int32_t f0, int32_t f1, bool night) {
  int w = night ? TRACK_W_NIGHT : TRACK_W;
#if defined(PBL_ROUND)
  int inset = TRACK_OUT + (TRACK_W - w) / 2;
  int32_t a0 = f0, a1 = f1 + 64; // tiny overlap so segments join
  graphics_fill_radial(ctx, grect_inset(b, GEdgeInsets(inset)), GOvalScaleModeFitCircle,
                       w, a0, a1);
#else
  graphics_context_set_stroke_width(ctx, w);
  graphics_draw_line(ctx, track_pt(b, f0, TRACK_MID), track_pt(b, f1, TRACK_MID));
#endif
}

static void draw_track(GContext *ctx, GRect b, const Theme *th, time_t now) {
  const int range = wx_range_hours();
  const int range_s = range * 3600;
  const int steps = range * PBL_IF_ROUND_ELSE(6, 4);
  const int shown = steps * g_view.reveal / 1000;

  graphics_context_set_antialiased(ctx, true);

  // Temperature band.
  for (int s = 0; s < shown; s++) {
    int32_t f0 = (int32_t)s * FRAC / steps;
    int32_t f1 = (int32_t)(s + 1) * FRAC / steps;
    time_t t = now + (time_t)s * range_s / steps;
    bool ok;
    int16_t temp = wx_temp_at(t, &ok);
    int i = wx_index_at(t);
    GColor c = ok ? temp_color(th, temp) : th->empty;
    graphics_context_set_fill_color(ctx, c);
    graphics_context_set_stroke_color(ctx, c);
    draw_band(ctx, b, f0, f1, !ok || !g_wx.day[i]);
  }

  // Notches at 00, 06, 12 and 18 local time.
  graphics_context_set_stroke_color(ctx, th->bg);
  graphics_context_set_stroke_width(ctx, 2);
  for (time_t t = now - now % 3600 + 3600; t < now + range_s; t += 3600) {
    struct tm *tm = localtime(&t);
    if (tm->tm_min != 0 || tm->tm_hour % 6 != 0) continue;
    int32_t f = frac_of(t, now, range_s);
    if (f * 1000 / FRAC > g_view.reveal) continue;
    int len = tm->tm_hour == 0 ? 2 : 0; // midnight cuts a little deeper
    draw_ray(ctx, b, f, TRACK_OUT - 2, TRACK_OUT + TRACK_W + 1 + len);
  }

  // Rain bars, one per hour, inside the track.
  int rain_w = range <= 12 ? 5 : range <= 24 ? PBL_IF_ROUND_ELSE(5, 4) : 2;
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
    int hi = -1, lo = -1;
    for (int k = first; k < g_wx.count && k < first + range; k++) {
      if (hi < 0 || g_wx.temp[k] > g_wx.temp[hi]) hi = k;
      if (lo < 0 || g_wx.temp[k] < g_wx.temp[lo]) lo = k;
    }
    int marks[2] = {hi, lo};
    graphics_context_set_text_color(ctx, th->dim);
    for (int m = 0; m < 2; m++) {
      if (m == 1 && g_wx.temp[lo] == g_wx.temp[hi]) break;
      time_t t = (time_t)g_wx.t0 + (time_t)marks[m] * 3600;
      if (t < now) t = now;
      int32_t f = frac_of(t, now, range_s);
      if (f < FRAC / 40 || f > FRAC - FRAC / 40) continue; // keep clear of "now"
      GPoint p = track_pt(b, f, RAIN_IN + RAIN_MAX + 12);
      // Keep clear of the weather line beside the middle of the face.
      int cy = b.origin.y + b.size.h / 2;
      if (p.y > cy - 34 && p.y < cy + 44) p.y = p.y < cy + 5 ? cy - 34 : cy + 44;
      char tb[8];
      fmt_temp(tb, sizeof(tb), g_wx.temp[marks[m]]);
      graphics_draw_text(ctx, tb, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                         GRect(p.x - 18, p.y - 10, 36, 16), GTextOverflowModeFill,
                         GTextAlignmentCenter, NULL);
    }
  }
#endif

  // "Now" marker, and the scroll cursor during a wrist flick.
  GPoint now_p = track_pt(b, 0, TRACK_MID);
  graphics_context_set_fill_color(ctx, th->bg);
  graphics_fill_circle(ctx, now_p, 6);
  graphics_context_set_fill_color(ctx, th->fg);
  graphics_fill_circle(ctx, now_p, 4);
  if (g_view.scrub >= 0) {
    int32_t f = (int32_t)((int64_t)g_view.scrub * FRAC / (range * 10));
    GPoint p = track_pt(b, f, TRACK_MID);
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

  draw_track(ctx, b, &th, now);

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
  const int side = PBL_IF_ROUND_ELSE(34, 22);
  const int w = b.size.w - 2 * side;
  const int x = b.origin.x + side;
  // Stack: date 20, time 44, weather 28, smart 22, corner 18 → centred on cy.
  const int top = cy - (corner[0] ? 74 : 64) + PBL_IF_ROUND_ELSE(0, 6);

  draw_text_line(ctx, date, FONT_KEY_GOTHIC_18_BOLD, GRect(x, top, w, 22), th.dim);
  draw_text_line(ctx, clock, FONT_KEY_BITHAM_42_LIGHT, GRect(x - 10, top + 14, w + 20, 50), th.fg);
  draw_text_line(ctx, line1, FONT_KEY_GOTHIC_24_BOLD, GRect(x, top + 64, w, 30), th.fg);
  draw_text_line(ctx, line2, FONT_KEY_GOTHIC_18, GRect(x, top + 92, w, 24), th.accent);
  if (corner[0]) {
    draw_text_line(ctx, corner, FONT_KEY_GOTHIC_14, GRect(x, top + 116, w, 18), th.dim);
  }
}
