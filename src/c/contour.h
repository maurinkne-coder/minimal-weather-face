#pragma once
#include <pebble.h>

// ---- Forecast data, as sent by the phone and kept in persistent storage ----

#define WX_HOURS 72   // hourly entries kept on the watch (3 days)
#define WX_M15 8      // 15-minute rain slots (2 hours)
#define WX_SUN 4      // sunrise/sunset days

#define PROB_UNKNOWN 255

typedef struct {
  uint32_t t0;                 // UTC epoch of hourly[0] (start of an hour)
  uint8_t count;               // number of hourly entries
  int16_t temp[WX_HOURS];      // 0.1 degC
  uint8_t precip[WX_HOURS];    // 0.1 mm in the hour
  uint8_t prob[WX_HOURS];      // % or PROB_UNKNOWN
  uint8_t code[WX_HOURS];      // WMO weather code
  uint8_t gust[WX_HOURS];      // km/h
  uint8_t uv[WX_HOURS];        // 0.1 UV index
  uint8_t day[WX_HOURS];       // 1 = daylight
  uint32_t m15_t0;             // UTC epoch of m15[0]
  uint8_t m15_count;
  uint8_t m15[WX_M15];         // 0.1 mm per 15 min
  uint8_t sun_count;
  uint32_t rise[WX_SUN];
  uint32_t set[WX_SUN];
  uint32_t day0;               // start of the first day of hi/lo (local midnight)
  uint8_t hilo_count;
  int16_t hi[WX_SUN];          // 0.1 degC, daily high
  int16_t lo[WX_SUN];          // 0.1 degC, daily low
  int16_t cur_temp;            // 0.1 degC
  uint8_t cur_code;
  uint32_t cur_t;              // time of the "current" observation
  uint32_t fetched;            // when the watch received it
  uint8_t status;              // WX_OK / WX_NO_LOCATION / WX_NO_NETWORK
} Weather;

enum { WX_OK = 0, WX_NO_LOCATION = 1, WX_NO_NETWORK = 2 };

// ---- Settings ----

enum { THEME_MIDNIGHT, THEME_DAYLIGHT, THEME_MONO, THEME_GRAPHITE, THEME_AUTO,
       THEME_OCEAN, THEME_FOREST, THEME_EMBER, THEME_SAND, THEME_COUNT };
enum { CORNER_NONE, CORNER_BATTERY, CORNER_STEPS, CORNER_HEART, CORNER_TEMP };
enum { LANG_EN, LANG_DE, LANG_FR, LANG_IT, LANG_ES, LANG_COUNT };
enum { CLOCK_AUTO, CLOCK_24, CLOCK_12 };
enum { TEMP_C, TEMP_F };
enum { RAIN_MM, RAIN_IN };
enum { BATT_OFF, BATT_PLAIN, BATT_COLOR };
enum { WIND_KMH, WIND_MPH, WIND_MS, WIND_KT, WIND_BFT };

#define SETTINGS_VERSION 4

typedef struct {
  uint8_t version;
  uint8_t theme;
  uint8_t range;      // hours shown on rectangular watches: 12, 24 or 48
  uint8_t corner;
  uint8_t anim;
  uint8_t frost;
  uint8_t gust;       // km/h threshold, 0 = off
  uint8_t uv;         // UV threshold, 0 = off
  uint8_t sunline;
  uint8_t lang;
  uint8_t clock;
  uint8_t temp_unit;
  uint8_t rain_unit;
  uint8_t wind_unit;
  uint8_t updates;
  uint8_t interval;   // minutes
  int16_t tilt;       // degrees the face is turned clockwise
  uint8_t battline;   // battery line above the info row: BATT_OFF / BATT_PLAIN / BATT_COLOR
  uint8_t stepbar;    // step goal bar below the info row
  uint16_t stepgoal;  // steps, 0 = your usual daily steps
  uint8_t stepcolor;  // 0 = theme colour, else an entry of STEP_COLORS in draw.c
} Settings;

// ---- Things the face shows that come from the watch itself ----

typedef struct {
  time_t now;
  int reveal;         // 0..1000, ring drawing itself in
  int scrub;          // -1 = off, else tenths of an hour ahead of now
  uint8_t battery;
  bool charging;
  int32_t steps;
  int32_t heart;
  int32_t step_goal;  // resolved goal for the step bar
} ViewState;

extern Settings g_set;
extern Weather g_wx;
extern ViewState g_view;
extern GFont g_time_font;   // big blocky digits for the stacked clock
extern GFont g_small_font;  // same style, small capitals for date and labels

// weather.c
int wx_index_at(time_t t);           // hourly index covering t, or -1
bool wx_wet(int i);                  // meaningful rain in hour i
int16_t wx_temp_at(time_t t, bool *ok);
int wx_range_hours(void);            // hours on the ring for this watch

// text.c
void text_date(char *buf, size_t n, time_t now);
void text_clock_parts(char *h, size_t hn, char *m, size_t mn, time_t now);
void text_temp(char *buf, size_t n, time_t now);
bool text_hilo(char *lo, size_t ln, char *hi, size_t hn, time_t now);  // today's low and high
void text_smart(char *buf, size_t n, time_t now);
void text_corner(char *buf, size_t n);
void text_scrub(char *line1, size_t n1, char *line2, size_t n2, time_t t);
void fmt_temp(char *buf, size_t n, int16_t tenths);
void fmt_rain_short(char *buf, size_t n, int tenths_mm);

// draw.c
void draw_face(Layer *layer, GContext *ctx);

// tilt.c: turns everything drawn so far by `deg` degrees round the centre.
void tilt_apply(GContext *ctx, GRect bounds, int deg, GColor bg);
void tilt_release(void);
