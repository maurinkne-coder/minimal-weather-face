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
  GColor steps;                        // step goal bar, warm so it never looks like rain
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

// Any of the 64 Pebble colours from its 0b11RRGGBB code.
static GColor argb(uint8_t v) {
  GColor c;
  c.argb = v;
  return c;
}

// Step line colours to pick from in the settings (index 0 = theme colour).
static const uint8_t STEP_COLORS[] = {
  0,
  0xF8,  // 1 yellow
  0xF4,  // 2 orange
  0xF0,  // 3 red
  0xF6,  // 4 pink
  0xE7,  // 5 violet
  0xDD,  // 6 green
  0xEE,  // 7 mint
  0xFF,  // 8 white
};

static Theme theme_for(int id) {
  switch (id) {
    // bg, text, dim, accent, rain base, light, medium, heavy, steps
    case THEME_OCEAN:
      return (Theme){ argb(0xC1), argb(0xFF), argb(0xD6), argb(0xEF),
                      argb(0xC7), argb(0xCB), argb(0xDF), argb(0xFF), argb(0xF9) };
    case THEME_FOREST:
      return (Theme){ argb(0xC4), argb(0xFF), argb(0xD9), argb(0xEE),
                      argb(0xC6), argb(0xDB), argb(0xDF), argb(0xFF), argb(0xF8) };
    case THEME_EMBER:
      return (Theme){ argb(0xC0), argb(0xFF), argb(0xD5), argb(0xF9),
                      argb(0xC6), argb(0xC6), argb(0xDB), argb(0xDF), argb(0xF4) };
    case THEME_SAND:
      return (Theme){ argb(0xFE), argb(0xC0), argb(0xE4), argb(0xC6),
                      argb(0xEA), argb(0xDB), argb(0xC3), argb(0xC1), argb(0xF4) };
    case THEME_DAYLIGHT:
      return (Theme){ GColorWhite, GColorBlack, GColorDarkGray, GColorCobaltBlue,
                      GColorLightGray, GColorPictonBlue, GColorBlue, GColorOxfordBlue, GColorOrange };
    case THEME_MONO:
      return (Theme){ GColorBlack, GColorWhite, GColorLightGray, GColorWhite,
                      GColorDarkGray, GColorDarkGray, GColorLightGray, GColorWhite, GColorWhite };
    case THEME_GRAPHITE:
      return (Theme){ GColorDarkGray, GColorWhite, GColorLightGray, GColorIcterine,
                      GColorLightGray, GColorPictonBlue, GColorElectricBlue, GColorWhite, GColorChromeYellow };
    default: // THEME_MIDNIGHT
      return (Theme){ GColorBlack, GColorWhite, GColorDarkGray, GColorCeleste,
                      GColorCobaltBlue, GColorCobaltBlue, GColorPictonBlue, GColorElectricBlue, GColorChromeYellow };
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
  Theme th = theme_for(id);
  if (g_set.owncolors && id != THEME_MONO) {
    th.fg = argb(g_set.textcolor);
    th.accent = argb(g_set.accentcolor);
  }
  if (g_set.stepcolor && g_set.stepcolor < sizeof(STEP_COLORS) && id != THEME_MONO) {
    th.steps = argb(STEP_COLORS[g_set.stepcolor]);
  }
  return th;
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

// True for the light backgrounds (Daylight, Sand).
static bool light_bg(const Theme *th) {
  uint8_t b = th->bg.argb;
  return ((b >> 4) & 3) + ((b >> 2) & 3) + (b & 3) >= 7;
}

// Colour of a rain bar from the amount (0.1 mm per hour) and the hour's
// weather code. Theme blues grade light, medium and heavy rain; then fixed
// warning colours: orange for strong rain (8 mm/h and more), red for a
// thunderstorm or a downpour (15 mm/h and more), violet for the most extreme
// (25 mm/h and more, the most the forecast can carry) and for freezing rain
// (ice on the ground). Snow is white (dark grey on light backgrounds). Mono
// keeps its greys.
static GColor rain_color(const Theme *th, int p, uint8_t code) {
#ifdef PBL_COLOR
  if (g_set.theme != THEME_MONO) {
    if (p >= 250) return argb(0xE7);              // extreme, 25 mm/h and more: violet
    if (code >= 95 || p >= 150) return GColorRed;
    if (p >= 80) return GColorOrange;
    if (code == 56 || code == 57 || code == 66 || code == 67) return argb(0xE7);  // freezing rain: violet
    if ((code >= 71 && code <= 77) || code == 85 || code == 86) {
      return light_bg(th) ? GColorDarkGray : GColorWhite;
    }
  }
#endif
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
    int hi = wx_index_at(slot < now ? now : slot);
    GColor c = rain_color(th, rate, hi >= 0 ? g_wx.code[hi] : 0);
    graphics_context_set_fill_color(ctx, c);
    graphics_context_set_stroke_color(ctx, c);
    graphics_context_set_stroke_width(ctx, 1);
    draw_quad(ctx, track_pt(b, f0, RAIN_EDGE), track_pt(b, f1, RAIN_EDGE),
              track_pt(b, f1, RAIN_EDGE + len), track_pt(b, f0, RAIN_EDGE + len), likely);
  }

  // "Now" is a short tick from the screen edge across the base line, and the
  // scroll cursor during a wrist flick a matching tick in the accent colour:
  // small marks that leave the bars and the text in view.
  graphics_context_set_stroke_width(ctx, 2);
  graphics_context_set_stroke_color(ctx, th->fg);
  graphics_draw_line(ctx, track_pt(b, 0, 0), track_pt(b, 0, RAIN_EDGE + 3));
  if (g_view.scrub >= 0) {
    int32_t f = (int32_t)((int64_t)g_view.scrub * FRAC / (range * 10));
    graphics_context_set_stroke_color(ctx, th->accent);
    graphics_draw_line(ctx, track_pt(b, f, 0), track_pt(b, f, RAIN_EDGE + 3));
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

// Everything except the forecast ring: date, clock, info row, lines and
// the bottom line. Drawn on top of the ring.
static void draw_text_layer(GContext *ctx, GRect b, const Theme *thp, time_t now) {
  const Theme th = *thp;
  char hh[4], mm[4], date[16], left[40], right[64], corner[24];
  text_clock_parts(hh, sizeof(hh), mm, sizeof(mm), now);
  text_date(date, sizeof(date), now);
  if (g_view.scrub >= 0) {
    text_scrub(left, sizeof(left), right, sizeof(right), now + (time_t)g_view.scrub * 360);
  } else {
    // With the temperature at the bottom, the middle row is just the smart line.
    if (g_set.corner == CORNER_TEMP) left[0] = '\0';
    else text_temp(left, sizeof(left), now);
    text_smart(right, sizeof(right), now);
  }
  text_corner(corner, sizeof(corner));

  // Stacked clock, everything centred on the screen: the info row sits on the
  // middle line, hours above it and minutes below it as large as the screen
  // allows (the rounded digit corners reach into the rain chart), then the date on top and the extra line at
  // the bottom in small capitals of the same blocky font.
  // CAP is the digit height (the height of the digit images), GAP the space
  // between two digits.
#ifdef PBL_ROUND
  const int CAP = 75, GAP = 13, MAX_W = 192, BAR_W = 166, ROW_W = 228;
#else
  const int CAP = 78, GAP = 14, MAX_W = 160, BAR_W = 174, ROW_W = 190;
#endif
  const int HALF = 15;                 // centre line to the digits
  const int SMALL_CAP = 9, SMALL_PAD = 4, SMALL_GAP = 6;
  const int cx = b.origin.x + b.size.w / 2;
  const int cy = b.origin.y + b.size.h / 2;
  const int hh_top = cy - HALF - CAP;
  const int mm_top = cy + HALF;

  upper_utf8(date);
  upper_utf8(corner);
  draw_text_font(ctx, date, g_small_font,
                 GRect(cx - 70, hh_top - SMALL_GAP - SMALL_CAP - SMALL_PAD, 140, 18), th.dim);
  digits_draw(ctx, hh, cx, hh_top, GAP, th.fg, th.bg);
  digits_draw(ctx, mm, cx, mm_top, GAP, th.fg, th.bg);
  const int corner_y = mm_top + CAP + SMALL_GAP - SMALL_PAD;
  if (g_set.corner == CORNER_TEMP) {
    // Today's low, now and high: the current temperature bright in the middle.
    char lo[12], cur[12], hi[12];
    text_temp(cur, sizeof(cur), now);
    GRect wide = GRect(0, 0, 200, 18);
    if (text_hilo(lo, sizeof(lo), hi, sizeof(hi), now)) {
      const int GAP = 12;
      int wl = graphics_text_layout_get_content_size(lo, g_small_font, wide, GTextOverflowModeFill, GTextAlignmentLeft).w;
      int wc = graphics_text_layout_get_content_size(cur, g_small_font, wide, GTextOverflowModeFill, GTextAlignmentLeft).w;
      int wh = graphics_text_layout_get_content_size(hi, g_small_font, wide, GTextOverflowModeFill, GTextAlignmentLeft).w;
      int x = cx - (wl + wc + wh + 2 * GAP) / 2;
      graphics_context_set_text_color(ctx, th.dim);
      graphics_draw_text(ctx, lo, g_small_font, GRect(x, corner_y, wl + 2, 18), GTextOverflowModeFill, GTextAlignmentLeft, NULL);
      x += wl + GAP;
      graphics_context_set_text_color(ctx, th.fg);
      graphics_draw_text(ctx, cur, g_small_font, GRect(x, corner_y, wc + 2, 18), GTextOverflowModeFill, GTextAlignmentLeft, NULL);
      x += wc + GAP;
      graphics_context_set_text_color(ctx, th.dim);
      graphics_draw_text(ctx, hi, g_small_font, GRect(x, corner_y, wh + 2, 18), GTextOverflowModeFill, GTextAlignmentLeft, NULL);
    } else {
      draw_text_font(ctx, cur, g_small_font, GRect(cx - 70, corner_y, 140, 18), th.fg);
    }
  } else if (corner[0]) {
    draw_text_font(ctx, corner, g_small_font, GRect(cx - 70, corner_y, 140, 18), th.dim);
  }

  // Battery line just above the info row, step goal bar just below it:
  // a faint full-width track with the filled part on top.
  if (g_set.battline) {
    int pct = g_view.battery > 100 ? 100 : g_view.battery;
    GRect r = GRect(cx - BAR_W / 2, cy - 13, BAR_W, 2);
    graphics_context_set_fill_color(ctx, th.dim);
    graphics_fill_rect(ctx, r, 0, GCornerNone);
    GColor c = th.fg;
#ifdef PBL_COLOR
    // Light backgrounds (Daylight, Sand) get the darker green.
    if (g_set.battline == BATT_COLOR && g_set.theme != THEME_MONO) {
      // Colourful: green above 30%, orange down to 10%, red below 10%.
      if (pct > 30) c = light_bg(&th) ? GColorIslamicGreen : GColorGreen;
      else if (pct >= 10) c = GColorOrange;
      else c = GColorRed;
    } else if (pct <= 20 && !g_view.charging && g_set.theme != THEME_MONO) {
      c = GColorRed;
    }
#endif
    graphics_context_set_fill_color(ctx, c);
    graphics_fill_rect(ctx, GRect(r.origin.x, r.origin.y, BAR_W * pct / 100, 2), 0, GCornerNone);
  }
  if (g_set.stepbar && g_view.steps >= 0) {
    int32_t goal = g_view.step_goal > 0 ? g_view.step_goal : 10000;
    int32_t done = g_view.steps > goal ? goal : g_view.steps;
    // Mirrors the battery line: both sit 2 px from the digits and frame the
    // info row evenly.
    GRect r = GRect(cx - BAR_W / 2, cy + 11, BAR_W, 2);
    graphics_context_set_fill_color(ctx, th.dim);
    graphics_fill_rect(ctx, r, 0, GCornerNone);
    graphics_context_set_fill_color(ctx, th.steps);
    int w = (int)(BAR_W * done / goal);
    if (w > 0) graphics_fill_rect(ctx, GRect(r.origin.x, r.origin.y, w, 2), 0, GCornerNone);
  }

  // Info row: temperature in the main colour, then the smart line in the
  // accent colour. Both use the same font so they share one baseline. The
  // weather symbol sits left of the text and, while charging, a lightning
  // bolt right of it, both centred between the two lines.
  {
    bool mono = g_set.theme == THEME_MONO;
    GColor warm = mono ? th.fg : light_bg(&th) ? GColorOrange : GColorChromeYellow;
    int wi = wx_index_at(now);
    bool fresh = g_wx.cur_t && now - (time_t)g_wx.cur_t < 75 * 60;
    bool has_wx = fresh || wi >= 0;
    bool has_bolt = g_view.charging;
    // With symbols the row may run a little wider (over the ring, which it
    // covers anyway); the text leaves 20 px on each side for them.
    const int ICON_HALF = 7, ICON_GAP = 6;
    int max_w = (has_wx || has_bolt) ? ROW_W - 2 * (2 * ICON_HALF + ICON_GAP) : MAX_W;
    GFont f = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    // Lines sit at cy-13 and cy+11 (2 px from the digits each). The capital
    // letters and digits of the text are centred in the 22 px between them.
    int top = cy - 12;
    GRect wide = GRect(0, 0, 400, 24);
    int wl = graphics_text_layout_get_content_size(left, f, wide, GTextOverflowModeTrailingEllipsis,
                                                   GTextAlignmentLeft).w;
    int wr = right[0] ? graphics_text_layout_get_content_size(right, f, wide,
                                                              GTextOverflowModeTrailingEllipsis,
                                                              GTextAlignmentLeft).w : 0;
    if (wl + 5 + wr > max_w && right[0]) {
      // Too long: drop both to the smaller font before cutting anything off.
      f = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
      top = cy - 8;
      wl = graphics_text_layout_get_content_size(left, f, wide, GTextOverflowModeTrailingEllipsis,
                                                 GTextAlignmentLeft).w;
      wr = graphics_text_layout_get_content_size(right, f, wide, GTextOverflowModeTrailingEllipsis,
                                                 GTextAlignmentLeft).w;
    }
    int gap = (wl && wr) ? 5 : 0;
    if (wl + gap + wr > max_w) wr = max_w - wl - gap;
    int x0 = cx - (wl + gap + wr) / 2;
    graphics_context_set_text_color(ctx, th.fg);
    graphics_draw_text(ctx, left, f, GRect(x0, top, wl + 2, 24), GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentLeft, NULL);
    if (wr > 0) {
      graphics_context_set_text_color(ctx, th.accent);
      graphics_draw_text(ctx, right, f, GRect(x0 + wl + gap, top, wr + 2, 24),
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    }
    // Middle of the 22 px between the lines (cy-11 .. cy+10).
    const int icon_y = cy - 1;
    const int text_w = wl + gap + wr;
    if (has_wx) {
      IconColors k = { .cloud = th.fg, .sun = warm, .water = mono ? th.fg : th.medium, .bg = th.bg };
      uint8_t code = fresh ? g_wx.cur_code : g_wx.code[wi];
      icon_weather(ctx, GPoint(x0 - ICON_GAP - ICON_HALF, icon_y), code, is_daytime(now), &k);
    }
    if (has_bolt) icon_bolt(ctx, GPoint(x0 + text_w + ICON_GAP + ICON_HALF, icon_y), warm);
  }

}

void draw_face(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  time_t now = g_view.now;
  Theme th = current_theme(now);

  graphics_context_set_fill_color(ctx, th.bg);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  static int s_tilt_deg = 0;
  static GSize s_tilt_size;
  if (g_set.tilt != s_tilt_deg || s_tilt_size.w != b.size.w || s_tilt_size.h != b.size.h) {
    s_tilt_deg = g_set.tilt;
    s_tilt_size = b.size;
    s_tilt_f = 0;
    s_tilt_f = tilt_frac(b, g_set.tilt);
  }

  // The text always sits on top of the rain bars. Upright, that is just the
  // drawing order. Turned, the text is drawn first and set aside, the ring is
  // drawn already turned with plain geometry (so it stays sharp), and the
  // text is turned and laid over it with smoothed edges.
  if (g_set.tilt % 360 == 0) {
    draw_track(ctx, b, &th, now);
    draw_text_layer(ctx, b, &th, now);
    return;
  }
  draw_text_layer(ctx, b, &th, now);
  bool turned = tilt_begin(ctx, b, th.bg);
  draw_track(ctx, b, &th, now);
  if (turned) tilt_end(ctx, b, g_set.tilt);
}
