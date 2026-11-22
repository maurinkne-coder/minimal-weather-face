#include <math.h>
#include <stdlib.h>
#include "../../src/c/contour.h"

Settings g_set; Weather g_wx; ViewState g_view;
GFont g_small_font = "small12";
static int W, H;
struct Layer { int dummy; };
struct GContext { GColor fill, stroke, text; int sw; };

int32_t sin_lookup(int32_t a) { return (int32_t)lround(sin(a * 2 * M_PI / TRIG_MAX_ANGLE) * TRIG_MAX_RATIO); }
int32_t cos_lookup(int32_t a) { return (int32_t)lround(cos(a * 2 * M_PI / TRIG_MAX_ANGLE) * TRIG_MAX_RATIO); }
GRect grect_inset(GRect r, GEdgeInsets i) { return GRect(r.origin.x + i.left, r.origin.y + i.top, r.size.w - i.left - i.right, r.size.h - i.top - i.bottom); }
GRect layer_get_bounds(Layer *l) { (void)l; return GRect(0, 0, W, H); }
GFont fonts_get_system_font(const char *k) { return k; }
bool clock_is_24h_style(void) { return true; }
void graphics_context_set_fill_color(GContext *c, GColor col) { c->fill = col; }
void graphics_context_set_stroke_color(GContext *c, GColor col) { c->stroke = col; }
void graphics_context_set_text_color(GContext *c, GColor col) { c->text = col; }
void graphics_context_set_stroke_width(GContext *c, uint8_t w) { c->sw = w; }
void graphics_context_set_antialiased(GContext *c, bool on) { (void)c; (void)on; }
void graphics_fill_rect(GContext *c, GRect r, uint16_t rad, GCornerMask m) { (void)rad; (void)m;
  printf("[\"rect\",%d,%d,%d,%d,%d]\n", r.origin.x, r.origin.y, r.size.w, r.size.h, c->fill.argb); }
void graphics_fill_circle(GContext *c, GPoint p, uint16_t r) { printf("[\"circle\",%d,%d,%d,%d]\n", p.x, p.y, r, c->fill.argb); }
void graphics_draw_line(GContext *c, GPoint a, GPoint b) { printf("[\"line\",%d,%d,%d,%d,%d,%d]\n", a.x, a.y, b.x, b.y, c->sw, c->stroke.argb); }
void graphics_fill_radial(GContext *c, GRect r, GOvalScaleMode m, uint16_t t, int32_t a0, int32_t a1) { (void)m;
  printf("[\"radial\",%d,%d,%d,%d,%d,%d,%d,%d]\n", r.origin.x, r.origin.y, r.size.w, r.size.h, t, a0, a1, c->fill.argb); }
static void jstr(const char *s) { putchar('"'); for (; *s; s++) { if (*s == '"' || *s == '\\') putchar('\\'); putchar(*s); } putchar('"'); }
void graphics_draw_text(GContext *c, const char *t, GFont f, GRect r, GTextOverflowMode o, GTextAlignment a, void *x) { (void)o; (void)x;
  printf("[\"text\","); jstr(t); printf(",\"%s\",%d,%d,%d,%d,%d,%d]\n", f, r.origin.x, r.origin.y, r.size.w, r.size.h, c->text.argb, a == GTextAlignmentLeft); }

// Synthetic forecast: mild day, cooling to frost overnight, rain in the afternoon.
static void fake(time_t now, int scenario) {
  memset(&g_wx, 0, sizeof g_wx);
  g_wx.t0 = (uint32_t)(now - now % 3600);
  g_wx.count = 72;
  for (int i = 0; i < 72; i++) {
    time_t t = g_wx.t0 + i * 3600; struct tm *tm = localtime(&t);
    double h = tm->tm_hour;
    double base = scenario == 1 ? -2 : (scenario == 2 ? 24 : 7);
    g_wx.temp[i] = (int16_t)lround((base + 7 * sin((h - 9) / 24.0 * 2 * M_PI)) * 10);
    g_wx.day[i] = h >= 7 && h < 17;
    g_wx.code[i] = 2; g_wx.prob[i] = 10; g_wx.gust[i] = 20; g_wx.uv[i] = g_wx.day[i] ? 20 : 0;
    if (i >= 3 && i <= 7) { g_wx.precip[i] = (uint8_t)(i == 5 ? 90 : i * 4); g_wx.prob[i] = 80; g_wx.code[i] = scenario == 1 ? 73 : 61; }
    if (i == 6 && scenario != 1) { g_wx.precip[i] = 60; g_wx.code[i] = 95; }  // thunderstorm
    if (i == 7 && scenario != 1) g_wx.precip[i] = 254;  // extreme
    if (i >= 20 && i <= 22) { g_wx.precip[i] = 3; g_wx.prob[i] = 35; g_wx.code[i] = 51; }
  }
  g_wx.m15_t0 = (uint32_t)(now - now % 1800); g_wx.m15_count = 8;
  if (scenario == 0) g_wx.m15[3] = 2;
  for (int d = 0; d < 4; d++) {
    time_t mid = now - now % 86400 + d * 86400;
    g_wx.rise[d] = (uint32_t)(mid + 7 * 3600 + 20 * 60); g_wx.set[d] = (uint32_t)(mid + 16 * 3600 + 45 * 60);
  }
  g_wx.sun_count = 4;
  g_wx.day0 = (uint32_t)(now - now % 86400); g_wx.hilo_count = 4;
  for (int d = 0; d < 4; d++) { g_wx.lo[d] = (int16_t)(-20 - 10 * d); g_wx.hi[d] = (int16_t)(110 - 5 * d); }
  g_wx.cur_t = (uint32_t)now; g_wx.cur_temp = g_wx.temp[0]; g_wx.cur_code = g_wx.code[0];
}

int main(int argc, char **argv) {
  // usage: stub W H theme lang scenario range corner scrub hhmm
  W = atoi(argv[1]); H = atoi(argv[2]);
  setenv("TZ", "UTC", 1); tzset();
  struct tm tm = {0}; tm.tm_year = 126; tm.tm_mon = 10; tm.tm_mday = 22;
  int hhmm = argc > 9 ? atoi(argv[9]) : 1407; tm.tm_hour = hhmm / 100; tm.tm_min = hhmm % 100;
  time_t now = timegm(&tm);
  g_set = (Settings){ .version = 1, .theme = atoi(argv[3]), .range = atoi(argv[6]), .corner = atoi(argv[7]),
    .anim = 1, .frost = 1, .gust = 50, .uv = 6, .sunline = 1, .lang = atoi(argv[4]), .clock = 0,
    .updates = 1, .interval = 30, .battline = (uint8_t)(atoi(argv[3]) == 1 ? 2 : 1), .stepbar = 1, .stepgoal = 10000, .tilt = (int16_t)(argc > 10 ? atoi(argv[10]) : 0) };
  if (argc > 11) { g_set.owncolors = 1; g_set.textcolor = (uint8_t)strtol(argv[11], NULL, 16); g_set.accentcolor = (uint8_t)strtol(argv[12], NULL, 16); }
  fake(now, atoi(argv[5]));
  bool scenario0 = atoi(argv[5]) == 0;
  g_view = (ViewState){ .now = now, .reveal = 1000, .scrub = atoi(argv[8]), .battery = 80, .charging = scenario0, .steps = 6214, .heart = 64, .step_goal = 10000, .charge_anim = getenv("CHARGE") ? atoi(getenv("CHARGE")) : -1 };
  struct GContext ctx = {0}; Layer l;
  draw_face(&l, &ctx);
  return 0;
}
bool tilt_begin(GContext *c, GRect b, GColor bg) { (void)c; (void)b; printf("[\"tilt_begin\",%d]\n", bg.argb); return true; }
void tilt_end(GContext *c, GRect b, int deg) { (void)c; (void)b; printf("[\"tilt_end\",%d]\n", deg); }
void tilt_release(void) {}
// Rough text size, enough to test label placement.
GSize graphics_text_layout_get_content_size(const char *t, GFont f, GRect r, GTextOverflowMode o, GTextAlignment a) {
  (void)o; (void)a;
  int n = 0; for (const char *p = t; *p; p++) if ((*p & 0xC0) != 0x80) n++;
  int cw = 7, h = 18;
  if (!strcmp(f, "gothic14b") || !strcmp(f, "gothic14")) { cw = 6; h = 14; }
  else if (!strcmp(f, "gothic18") || !strcmp(f, "gothic18b")) { cw = 7; h = 18; }
  else if (!strcmp(f, "gothic24b")) { cw = 10; h = 24; }
  else if (!strncmp(f, "clock", 5)) { cw = atoi(f + 5) * 82 / 100; h = atoi(f + 5); }
  else if (!strncmp(f, "small", 5)) { cw = 9; h = 12; }
  int w = n * cw; if (w > r.size.w) w = r.size.w;
  return (GSize){(int16_t)w, (int16_t)h};
}
struct GPath { GPathInfo info; };
GPath *gpath_create(const GPathInfo *info) { GPath *p = malloc(sizeof *p); p->info = *info; return p; }
void gpath_destroy(GPath *p) { free(p); }
void gpath_draw_filled(GContext *c, GPath *p) {
  printf("[\"poly\",%d,[", c->fill.argb);
  for (uint32_t i = 0; i < p->info.num_points; i++) printf("%s%d,%d", i ? "," : "", p->info.points[i].x, p->info.points[i].y);
  printf("]]\n");
}
void gpath_draw_outline(GContext *c, GPath *p) {
  printf("[\"polyline\",%d,[", c->stroke.argb);
  for (uint32_t i = 0; i < p->info.num_points; i++) printf("%s%d,%d", i ? "," : "", p->info.points[i].x, p->info.points[i].y);
  printf("]]\n");
}

// The big digits are images on the watch; here they are logged and drawn by render.py.
void digits_draw(GContext *c, const char *s, int cx, int top, int gap, GColor color, GColor bg) {
  (void)c; (void)bg;
  printf("[\"digits\","); jstr(s); printf(",%d,%d,%d,%d]\n", cx, top, gap, color.argb);
}
