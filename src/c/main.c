#include "contour.h"

Settings g_set;
Weather g_wx;
ViewState g_view;
GFont g_time_font;
GFont g_small_font;

static Window *s_window;
static Layer *s_canvas;
static Animation *s_reveal_anim;
static Animation *s_scrub_anim;
static AppTimer *s_scrub_timer;
static time_t s_last_request;

#define P_SETTINGS 1
#define P_WX_SIZE 9
#define P_WX_CHUNK 10   // 10, 11, 12 ... hold the forecast in 256-byte pieces

// ---------------------------------------------------------------------------
// Storage
// ---------------------------------------------------------------------------

static void settings_default(void) {
  g_set = (Settings){
    .version = SETTINGS_VERSION,
    .theme = THEME_MIDNIGHT,
    .range = 24,
    .corner = CORNER_TEMP,
    .anim = 1,
    .frost = 1,
    .gust = 50,
    .uv = 6,
    .sunline = 1,
    .lang = LANG_EN,
    .clock = CLOCK_AUTO,
    .temp_unit = TEMP_C,
    .rain_unit = RAIN_MM,
    .wind_unit = WIND_KMH,
    .updates = 1,
    .interval = 30,
    .tilt = 0,
    .battline = 1,
    .stepbar = 1,
    .stepgoal = 10000,
  };
}

static void settings_load(void) {
  settings_default();
  if (persist_exists(P_SETTINGS) && persist_get_size(P_SETTINGS) == (int)sizeof(Settings)) {
    Settings s;
    persist_read_data(P_SETTINGS, &s, sizeof(s));
    if (s.version == SETTINGS_VERSION) g_set = s;
  }
}

static void weather_save(void) {
  const uint8_t *p = (const uint8_t *)&g_wx;
  size_t n = sizeof(g_wx);
  for (size_t off = 0, k = 0; off < n; off += PERSIST_DATA_MAX_LENGTH, k++) {
    size_t len = n - off < PERSIST_DATA_MAX_LENGTH ? n - off : PERSIST_DATA_MAX_LENGTH;
    persist_write_data(P_WX_CHUNK + k, p + off, len);
  }
  persist_write_int(P_WX_SIZE, (int32_t)n);
}

static void weather_load(void) {
  memset(&g_wx, 0, sizeof(g_wx));
  if (persist_read_int(P_WX_SIZE) != (int32_t)sizeof(g_wx)) return;
  uint8_t *p = (uint8_t *)&g_wx;
  size_t n = sizeof(g_wx);
  for (size_t off = 0, k = 0; off < n; off += PERSIST_DATA_MAX_LENGTH, k++) {
    size_t len = n - off < PERSIST_DATA_MAX_LENGTH ? n - off : PERSIST_DATA_MAX_LENGTH;
    if (persist_read_data(P_WX_CHUNK + k, p + off, len) != (int)len) {
      memset(&g_wx, 0, sizeof(g_wx));
      return;
    }
  }
}

// ---------------------------------------------------------------------------
// Watch state
// ---------------------------------------------------------------------------

static bool battery_ok(void) {
  return g_view.battery >= 20 || g_view.charging;
}

static bool animations_allowed(void) {
  return g_set.anim && battery_ok() && !quiet_time_is_active();
}

static void update_health(void) {
  g_view.steps = -1;
  g_view.heart = 0;
#if defined(PBL_HEALTH)
  if (g_set.corner == CORNER_STEPS || g_set.stepbar) {
    time_t start = time_start_of_today(), end = time(NULL);
    if (health_service_metric_accessible(HealthMetricStepCount, start, end) & HealthServiceAccessibilityMaskAvailable) {
      g_view.steps = (int32_t)health_service_sum_today(HealthMetricStepCount);
    }
    g_view.step_goal = g_set.stepgoal;
    if (!g_view.step_goal) {
      // Your usual steps for a whole day like today.
      g_view.step_goal = (int32_t)health_service_sum_averaged(HealthMetricStepCount, start,
                                                              start + SECONDS_PER_DAY,
                                                              HealthServiceTimeScopeDailyWeekdayOrWeekend);
    }
    if (g_view.step_goal <= 0) g_view.step_goal = 10000;
  }
  if (g_set.corner == CORNER_HEART) {
    g_view.heart = (int32_t)health_service_peek_current_value(HealthMetricHeartRateBPM);
  }
#endif
}

static void battery_handler(BatteryChargeState s) {
  g_view.battery = s.charge_percent;
  g_view.charging = s.is_charging || s.is_plugged;
  if (s_canvas) layer_mark_dirty(s_canvas);
}

// ---------------------------------------------------------------------------
// Talking to the phone
// ---------------------------------------------------------------------------

static void request_weather(bool force) {
  if (!g_set.updates || !battery_ok()) return;
  if (!connection_service_peek_pebble_app_connection()) return;
  time_t now = time(NULL);
  int interval = (g_set.interval ? g_set.interval : 30) * 60;
  if (!force) {
    if (g_wx.fetched && now - (time_t)g_wx.fetched < interval - 60) return;
    if (now - s_last_request < 5 * 60) return; // phone not answering: retry every 5 min
  }
  DictionaryIterator *it;
  if (app_message_outbox_begin(&it) != APP_MSG_OK) return;
  dict_write_uint8(it, MESSAGE_KEY_Request, 1);
  if (app_message_outbox_send() == APP_MSG_OK) s_last_request = now;
}

static int read_int(DictionaryIterator *it, uint32_t key, int fallback) {
  Tuple *t = dict_find(it, key);
  return t ? (int)t->value->int32 : fallback;
}

static size_t read_bytes(DictionaryIterator *it, uint32_t key, uint8_t *dst, size_t max) {
  Tuple *t = dict_find(it, key);
  if (!t || t->type != TUPLE_BYTE_ARRAY) return 0;
  size_t n = t->length < max ? t->length : max;
  memcpy(dst, t->value->data, n);
  return n;
}

static uint32_t le32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool read_weather(DictionaryIterator *it) {
  Tuple *t0 = dict_find(it, MESSAGE_KEY_WxT0);
  if (!t0) return false;

  static uint8_t buf[WX_HOURS * 2];
  Weather w;
  memset(&w, 0, sizeof(w));
  w.t0 = (uint32_t)t0->value->int32;

  size_t n = read_bytes(it, MESSAGE_KEY_WxTemp, buf, WX_HOURS * 2);
  w.count = (uint8_t)(n / 2);
  for (int i = 0; i < w.count; i++) w.temp[i] = (int16_t)(buf[2 * i] | (buf[2 * i + 1] << 8));
  read_bytes(it, MESSAGE_KEY_WxPrecip, w.precip, w.count);
  read_bytes(it, MESSAGE_KEY_WxProb, w.prob, w.count);
  read_bytes(it, MESSAGE_KEY_WxCode, w.code, w.count);
  read_bytes(it, MESSAGE_KEY_WxGust, w.gust, w.count);
  read_bytes(it, MESSAGE_KEY_WxUv, w.uv, w.count);
  read_bytes(it, MESSAGE_KEY_WxDay, w.day, w.count);

  w.m15_t0 = (uint32_t)read_int(it, MESSAGE_KEY_WxM15T0, 0);
  w.m15_count = (uint8_t)read_bytes(it, MESSAGE_KEY_WxM15, w.m15, WX_M15);

  uint8_t sun[WX_SUN * 4];
  size_t rn = read_bytes(it, MESSAGE_KEY_WxRise, sun, sizeof(sun)) / 4;
  for (size_t i = 0; i < rn; i++) w.rise[i] = le32(sun + 4 * i);
  size_t sn = read_bytes(it, MESSAGE_KEY_WxSet, sun, sizeof(sun)) / 4;
  for (size_t i = 0; i < sn; i++) w.set[i] = le32(sun + 4 * i);
  w.sun_count = (uint8_t)(rn < sn ? rn : sn);

  uint8_t hl[WX_SUN * 2];
  w.day0 = (uint32_t)read_int(it, MESSAGE_KEY_WxDay0, 0);
  size_t hn = read_bytes(it, MESSAGE_KEY_WxHi, hl, sizeof(hl)) / 2;
  for (size_t i = 0; i < hn; i++) w.hi[i] = (int16_t)(hl[2 * i] | (hl[2 * i + 1] << 8));
  size_t ln = read_bytes(it, MESSAGE_KEY_WxLo, hl, sizeof(hl)) / 2;
  for (size_t i = 0; i < ln; i++) w.lo[i] = (int16_t)(hl[2 * i] | (hl[2 * i + 1] << 8));
  w.hilo_count = w.day0 ? (uint8_t)(hn < ln ? hn : ln) : 0;

  w.cur_temp = (int16_t)read_int(it, MESSAGE_KEY_WxCurTemp, 0);
  w.cur_code = (uint8_t)read_int(it, MESSAGE_KEY_WxCurCode, 0);
  w.cur_t = (uint32_t)read_int(it, MESSAGE_KEY_WxCurT, 0);
  w.fetched = (uint32_t)time(NULL);
  w.status = WX_OK;

  if (w.count == 0) return false;
  g_wx = w;
  weather_save();
  return true;
}

static bool read_settings(DictionaryIterator *it) {
  if (!dict_find(it, MESSAGE_KEY_Theme)) return false;
  Settings s = g_set;
  s.theme = read_int(it, MESSAGE_KEY_Theme, s.theme);
  s.range = read_int(it, MESSAGE_KEY_Range, s.range);
  s.corner = read_int(it, MESSAGE_KEY_Corner, s.corner);
  s.anim = read_int(it, MESSAGE_KEY_Anim, s.anim);
  s.frost = read_int(it, MESSAGE_KEY_Frost, s.frost);
  s.gust = read_int(it, MESSAGE_KEY_Gust, s.gust);
  s.uv = read_int(it, MESSAGE_KEY_Uv, s.uv);
  s.sunline = read_int(it, MESSAGE_KEY_SunLine, s.sunline);
  s.lang = read_int(it, MESSAGE_KEY_Lang, s.lang);
  s.clock = read_int(it, MESSAGE_KEY_Clock, s.clock);
  s.temp_unit = read_int(it, MESSAGE_KEY_TempUnit, s.temp_unit);
  s.rain_unit = read_int(it, MESSAGE_KEY_RainUnit, s.rain_unit);
  s.wind_unit = read_int(it, MESSAGE_KEY_WindUnit, s.wind_unit);
  s.updates = read_int(it, MESSAGE_KEY_Updates, s.updates);
  s.interval = read_int(it, MESSAGE_KEY_Interval, s.interval);
  s.tilt = read_int(it, MESSAGE_KEY_Tilt, s.tilt);
  s.battline = read_int(it, MESSAGE_KEY_BattLine, s.battline);
  s.stepbar = read_int(it, MESSAGE_KEY_StepBar, s.stepbar);
  s.stepgoal = read_int(it, MESSAGE_KEY_StepGoal, s.stepgoal);
  if (s.tilt <= -360 || s.tilt >= 360) s.tilt = 0;
  if (s.lang >= LANG_COUNT) s.lang = LANG_EN;
  g_set = s;
  if (!g_set.tilt) tilt_release();
  persist_write_data(P_SETTINGS, &g_set, sizeof(g_set));
  return true;
}

static void inbox_received(DictionaryIterator *it, void *ctx) {
  bool dirty = false;
  if (read_settings(it)) {
    update_health();
    dirty = true;
  }
  if (read_weather(it)) dirty = true;

  Tuple *st = dict_find(it, MESSAGE_KEY_WxStatus);
  if (st && st->value->int32 != WX_OK) {
    g_wx.status = (uint8_t)st->value->int32;
    dirty = true;
  }
  if (dict_find(it, MESSAGE_KEY_JsReady)) {
    s_last_request = 0;
    request_weather(false);
  }
  if (dirty) layer_mark_dirty(s_canvas);
}

// ---------------------------------------------------------------------------
// Animations
// ---------------------------------------------------------------------------

static void reveal_update(Animation *a, const AnimationProgress p) {
  g_view.reveal = (int)((int64_t)p * 1000 / ANIMATION_NORMALIZED_MAX);
  layer_mark_dirty(s_canvas);
}

static void reveal_teardown(Animation *a) {
  g_view.reveal = 1000;
  s_reveal_anim = NULL;
  layer_mark_dirty(s_canvas);
}

static const AnimationImplementation s_reveal_impl = {
  .update = reveal_update,
  .teardown = reveal_teardown,
};

static void scrub_end(void *data) {
  s_scrub_timer = NULL;
  g_view.scrub = -1;
  layer_mark_dirty(s_canvas);
}

static void scrub_update(Animation *a, const AnimationProgress p) {
  int max = wx_range_hours() * 10 - 1;
  g_view.scrub = (int)((int64_t)p * max / ANIMATION_NORMALIZED_MAX);
  layer_mark_dirty(s_canvas);
}

static void scrub_teardown(Animation *a) {
  s_scrub_anim = NULL;
  s_scrub_timer = app_timer_register(1200, scrub_end, NULL);
}

static const AnimationImplementation s_scrub_impl = {
  .update = scrub_update,
  .teardown = scrub_teardown,
};

static void start_reveal(void) {
  if (!animations_allowed()) {
    g_view.reveal = 1000;
    return;
  }
  g_view.reveal = 0;
  s_reveal_anim = animation_create();
  animation_set_duration(s_reveal_anim, 900);
  animation_set_curve(s_reveal_anim, AnimationCurveEaseOut);
  animation_set_implementation(s_reveal_anim, &s_reveal_impl);
  animation_schedule(s_reveal_anim);
}

// A wrist flick walks a cursor round the track and reads out each hour.
static void tap_handler(AccelAxisType axis, int32_t direction) {
  if (!animations_allowed() || s_scrub_anim || s_reveal_anim || wx_index_at(time(NULL)) < 0) return;
  if (s_scrub_timer) {
    app_timer_cancel(s_scrub_timer);
    s_scrub_timer = NULL;
  }
  s_scrub_anim = animation_create();
  animation_set_duration(s_scrub_anim, wx_range_hours() > 24 ? 6000 : 4000);
  animation_set_curve(s_scrub_anim, AnimationCurveEaseInOut);
  animation_set_implementation(s_scrub_anim, &s_scrub_impl);
  animation_schedule(s_scrub_anim);
}

// ---------------------------------------------------------------------------
// App
// ---------------------------------------------------------------------------

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  g_view.now = time(NULL);
  update_health();
  layer_mark_dirty(s_canvas);
  request_weather(false);
}

static void canvas_update(Layer *layer, GContext *ctx) {
  if (g_view.scrub < 0) g_view.now = time(NULL);
  draw_face(layer, ctx);
}

static void window_load(Window *window) {
#ifdef PBL_ROUND
  g_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_CLOCK_86));
#else
  g_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_CLOCK_88));
#endif
  g_small_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SMALL_12));
  Layer *root = window_get_root_layer(window);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);
  start_reveal();
}

static void window_unload(Window *window) {
  tilt_release();
  layer_destroy(s_canvas);
  s_canvas = NULL;
  fonts_unload_custom_font(g_time_font);
  fonts_unload_custom_font(g_small_font);
}

static void init(void) {
  settings_load();
  weather_load();
  g_view.now = time(NULL);
  g_view.scrub = -1;
  g_view.reveal = 1000;
  battery_handler(battery_state_service_peek());
  update_health();

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, false);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_handler);
  accel_tap_service_subscribe(tap_handler);

  app_message_register_inbox_received(inbox_received);
  app_message_open(1536, 64);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  accel_tap_service_unsubscribe();
  animation_unschedule_all();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
