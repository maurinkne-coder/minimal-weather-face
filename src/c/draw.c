#include "contour.h"

// The face: a rain bar chart round the edge of the screen in 30-minute slots,
// starting at the dot at the top ("now") and running clockwise into the coming
// hours. Dry slots are faint dashes along the edge; rainy slots grow a bar
// inward: the longer the bar, the more rain. The colour grades the amount
// (light < 1 mm/h, medium 1-4, heavy >= 4), solid bars are likely rain and
// outlined bars unlikely rain. The amount is spelled out in the smart line.

#define FRAC 65536

typedef struct {
  GColor bg, fg, dim, accent;
  GColor base, light, medium, heavy;   // rain: base line, < 1, 1-4, >= 4 mm/h
} Theme;

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

#if defined(PBL_ROUND)
#define RAIN_EDGE 4       // base line: pixels from the screen edge
#define RAIN_DEPTH 24     // longest bar (4 mm/h or more)
#else
#define RAIN_EDGE 2
#define RAIN_DEPTH 15
#define CORNER_R 18
#endif
#define BASE_MID (RAIN_EDGE + RAIN_DEPTH / 2)

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

// ---------------------------------------------------------------------------
// Colour
// ---------------------------------------------------------------------------

static Theme theme_for(int id) {
  switch (id) {
    case THEME_DAYLIGHT:
      return (Theme){ GColorWhite, GColorBlack, GColorDarkGray, GColorCobaltBlue,
                      GColorLightGray, GColorPictonBlue, GColorBlue, GColorOxfordBlue };
    case THEME_MONO:
      return (Theme){ GColorBlack, GColorWhite, GColorLightGray, GColorWhite,
                      GColorDarkGray, GColorDarkGray, GColorLightGray, GColorWhite };
    case THEME_GRAPHITE:
      return (Theme){ GColorDarkGray, GColorWhite, GColorLightGray, GColorIcterine,
                      GColorLightGray, GColorPictonBlue, GColorElectricBlue, GColorWhite };
    default: // THEME_MIDNIGHT
      return (Theme){ GColorBlack, GColorWhite, GColorDarkGray, GColorCeleste,
                      GColorCobaltBlue, GColorCobaltBlue, GColorPictonBlue, GColorElectricBlue };
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

// Bar length for an hour's rain (0.1 mm). Linear up to 4 mm/h, with a short
// minimum so a drizzle still shows.
static int bar_len(int32_t p) {
  if (p < 1) return 0;
  if (p > 40) p = 40;
  int len = (int)(p * RAIN_DEPTH / 40);
  return len < 3 ? 3 : len;
}

// Colour grade for an hour's rain (0.1 mm): light, medium or heavy.
static GColor rain_color(const Theme *th, int p) {
  return p >= 40 ? th->heavy : p >= 10 ? th->medium : th->light;
}

static void draw_quad(GContext *ctx, GPoint a, GPoint c, GPoint d, GPoint e, bool filled) {
  GPoint pts[4] = {a, c, d, e};
  GPathInfo info = { .num_points = 4, .points = pts };
  GPath *path = gpath_create(&info);
  if (!path) return;
  if (filled) gpath_draw_filled(ctx, path);
  else gpath_draw_outline(ctx, path);
  gpath_destroy(path);
}

// Rain rate (0.1 mm per hour) for the half hour starting at `slot`. The next
// two hours use the 15-minute data when it covers them; later slots use the
// hourly forecast.
static int slot_rain(time_t slot, bool *likely) {
  int k = wx_index_at(slot);
  uint8_t pr = k >= 0 ? g_wx.prob[k] : 0;
  *likely = pr == PROB_UNKNOWN || pr >= 50;
  if (g_wx.m15_count) {
    int32_t d = (int32_t)(slot - (time_t)g_wx.m15_t0);
    if (d >= 0 && d % 900 == 0 && d / 900 + 1 < g_wx.m15_count) {
      int i = d / 900;
      return (g_wx.m15[i] + g_wx.m15[i + 1]) * 2;   // two quarters -> per hour
    }
  }
  return k >= 0 ? g_wx.precip[k] : 0;
}

static void draw_track(GContext *ctx, GRect b, const Theme *th, time_t now) {
  const int range = wx_range_hours();
  const int range_s = range * 3600;
  graphics_context_set_antialiased(ctx, true);

  // 30-minute slots, aligned to the clock (:00 and :30). Each slot is a faint
  // dash along the edge; where rain is coming, a bar grows inward instead.
  const int32_t slot_gap = FRAC / (range * 2) * 12 / 100;
  time_t slot = now - now % 1800;
  for (; slot < now + range_s; slot += 1800) {
    time_t s0 = slot < now ? now : slot;
    time_t s1 = slot + 1800 > now + range_s ? now + range_s : slot + 1800;
    int32_t f0 = frac_of(s0, now, range_s) + (slot < now ? 0 : slot_gap);
    int32_t f1 = frac_of(s1, now, range_s) - slot_gap;
    if (f1 <= f0) continue;
    if (f1 * 1000 / FRAC > g_view.reveal) break;

    bool likely;
    int rate = slot_rain(slot, &likely);   // 0.1 mm per hour
    int len = bar_len(rate);
    if (!len) {
      graphics_context_set_stroke_color(ctx, th->base);
      graphics_context_set_stroke_width(ctx, 1);
      GPoint prev = track_pt(b, f0, RAIN_EDGE);
      const int n = 4;                      // follow the curve of the edge
      for (int i = 1; i <= n; i++) {
        GPoint p = track_pt(b, f0 + (f1 - f0) * i / n, RAIN_EDGE);
        graphics_draw_line(ctx, prev, p);
        prev = p;
      }
      continue;
    }
    // Colour = how much; solid = likely (50 % or more), outline = unlikely.
    GColor c = rain_color(th, rate);
    graphics_context_set_fill_color(ctx, c);
    graphics_context_set_stroke_color(ctx, c);
    graphics_context_set_stroke_width(ctx, 1);
    draw_quad(ctx, track_pt(b, f0, RAIN_EDGE), track_pt(b, f1, RAIN_EDGE),
              track_pt(b, f1, RAIN_EDGE + len), track_pt(b, f0, RAIN_EDGE + len), likely);
  }

  // "Now" marker on the base line, and the scroll cursor during a wrist flick.
  GPoint now_p = track_pt(b, 0, RAIN_EDGE);
  graphics_context_set_fill_color(ctx, th->bg);
  graphics_fill_circle(ctx, now_p, 6);
  graphics_context_set_fill_color(ctx, th->fg);
  graphics_fill_circle(ctx, now_p, 4);
  if (g_view.scrub >= 0) {
    int32_t f = (int32_t)((int64_t)g_view.scrub * FRAC / (range * 10));
    GPoint p = track_pt(b, f, RAIN_EDGE);
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
  graphics_context_set_text_color(ctx, c);
  graphics_draw_text(ctx, s, font, r, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

// Capital letters for the small blocky font (ASCII and Latin-1 accents).
static void upper_utf8(char *s) {
  for (unsigned char *p = (unsigned char *)s; *p; p++) {
    if (*p >= 'a' && *p <= 'z') {
      *p -= 32;
    } else if (*p == 0xC3 && p[1] >= 0xA0 && p[1] <= 0xBE && p[1] != 0xB7) {
      p[1] -= 0x20;
      p++;
    }
  }
}

void draw_face(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  time_t now = g_view.now;
  Theme th = current_theme(now);

  graphics_context_set_fill_color(ctx, th.bg);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  char hh[4], mm[4], date[16], left[40], right[64], corner[24];
  text_clock_parts(hh, sizeof(hh), mm, sizeof(mm), now);
  text_date(date, sizeof(date), now);
  if (g_view.scrub >= 0) {
    text_scrub(left, sizeof(left), right, sizeof(right), now + (time_t)g_view.scrub * 360);
  } else {
    text_temp(left, sizeof(left), now);
    text_smart(right, sizeof(right), now);
  }
  text_corner(corner, sizeof(corner));

  // Stacked clock, everything centred on the screen: the info row sits on the
  // middle line, hours above it and minutes below it as large as the space
  // inside the rain chart allows, then the date on top and the extra line at
  // the bottom in small capitals of the same blocky font.
  // CAP is the digit height; PAD is the space the font leaves above the digits
  // inside its text box (both measured from the font at that size).
#ifdef PBL_ROUND
  const int CAP = 63, PAD = 26, BOX = 96, MAX_W = 192;
#else
  const int CAP = 66, PAD = 28, BOX = 100, MAX_W = 160;
#endif
  const int HALF = 11;                 // centre line to the digits
  const int SMALL_CAP = 9, SMALL_PAD = 4, SMALL_GAP = 7;
  const int cx = b.origin.x + b.size.w / 2;
  const int cy = b.origin.y + b.size.h / 2;
  const int hh_top = cy - HALF - CAP;
  const int mm_top = cy + HALF;

  upper_utf8(date);
  upper_utf8(corner);
  draw_text_font(ctx, date, g_small_font,
                 GRect(cx - 70, hh_top - SMALL_GAP - SMALL_CAP - SMALL_PAD, 140, 18), th.dim);
  draw_text_font(ctx, hh, g_time_font, GRect(b.origin.x, hh_top - PAD, b.size.w, BOX), th.fg);
  draw_text_font(ctx, mm, g_time_font, GRect(b.origin.x, mm_top - PAD, b.size.w, BOX), th.fg);
  if (corner[0]) {
    draw_text_font(ctx, corner, g_small_font,
                   GRect(cx - 70, mm_top + CAP + SMALL_GAP - SMALL_PAD, 140, 18), th.dim);
  }

  // Info row: temperature in the main colour, then the smart line in the
  // accent colour. Both use the same font so they share one baseline.
  {
    GFont f = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    int top = cy - 13;
    GRect wide = GRect(0, 0, 400, 24);
    int wl = graphics_text_layout_get_content_size(left, f, wide, GTextOverflowModeTrailingEllipsis,
                                                   GTextAlignmentLeft).w;
    int wr = right[0] ? graphics_text_layout_get_content_size(right, f, wide,
                                                              GTextOverflowModeTrailingEllipsis,
                                                              GTextAlignmentLeft).w : 0;
    if (wl + 5 + wr > MAX_W && right[0]) {
      // Too long: drop both to the smaller font before cutting anything off.
      f = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
      top = cy - 9;
      wl = graphics_text_layout_get_content_size(left, f, wide, GTextOverflowModeTrailingEllipsis,
                                                 GTextAlignmentLeft).w;
      wr = graphics_text_layout_get_content_size(right, f, wide, GTextOverflowModeTrailingEllipsis,
                                                 GTextAlignmentLeft).w;
    }
    int gap = wr ? 5 : 0;
    if (wl + gap + wr > MAX_W) wr = MAX_W - wl - gap;
    int x0 = cx - (wl + gap + wr) / 2;
    graphics_context_set_text_color(ctx, th.fg);
    graphics_draw_text(ctx, left, f, GRect(x0, top, wl + 2, 24), GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentLeft, NULL);
    if (wr > 0) {
      graphics_context_set_text_color(ctx, th.accent);
      graphics_draw_text(ctx, right, f, GRect(x0 + wl + gap, top, wr + 2, 24),
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    }
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
