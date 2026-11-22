#include "contour.h"

// ---------------------------------------------------------------------------
// Translations
// ---------------------------------------------------------------------------

enum {
  S_RAIN_NOW, S_RAIN_UNTIL, S_RAIN_IN, S_RAIN_AT, S_DRY_UNTIL, S_FROST,
  S_GUSTS, S_UV, S_SUNSET, S_SUNRISE, S_WAITING, S_NO_LOC, S_NO_NET,
  S_OFF, S_HILO, S_STEPS, S_COUNT
};

static const char *const STR[LANG_COUNT][S_COUNT] = {
  [LANG_EN] = {
    "Rain now", "Rain until %s", "Rain in %d min", "Rain at %s", "Dry until %s",
    "Frost tonight %s", "Gusts %s", "UV %d at %s", "Sunset %s", "Sunrise %s",
    "Waiting for phone", "No location", "No connection", "Weather off",
    "High %s  Low %s", "steps",
  },
  [LANG_DE] = {
    "Es regnet", "Regen bis %s", "Regen in %d Min.", "Regen um %s", "Trocken bis %s",
    "Frost heute Nacht %s", "Böen %s", "UV %d um %s", "Sonnenuntergang %s",
    "Sonnenaufgang %s", "Warte auf Handy", "Kein Standort", "Keine Verbindung",
    "Wetter aus", "Max %s  Min %s", "Schritte",
  },
  [LANG_FR] = {
    "Il pleut", "Pluie jusqu'à %s", "Pluie dans %d min", "Pluie à %s",
    "Sec jusqu'à %s", "Gel cette nuit %s", "Rafales %s", "UV %d à %s",
    "Coucher %s", "Lever %s", "En attente du tél.", "Pas de position",
    "Pas de connexion", "Météo désactivée", "Max %s  Min %s", "pas",
  },
  [LANG_IT] = {
    "Piove", "Pioggia fino alle %s", "Pioggia tra %d min", "Pioggia alle %s",
    "Asciutto fino alle %s", "Gelo stanotte %s", "Raffiche %s", "UV %d alle %s",
    "Tramonto %s", "Alba %s", "In attesa del tel.", "Nessuna posizione",
    "Nessuna connessione", "Meteo disattivato", "Max %s  Min %s", "passi",
  },
  [LANG_ES] = {
    "Llueve", "Lluvia hasta las %s", "Lluvia en %d min", "Lluvia a las %s",
    "Seco hasta las %s", "Helada esta noche %s", "Rachas %s", "UV %d a las %s",
    "Puesta de sol %s", "Amanecer %s", "Esperando al móvil", "Sin ubicación",
    "Sin conexión", "Tiempo desactivado", "Máx %s  Mín %s", "pasos",
  },
};

static const char *const DAYS[LANG_COUNT][7] = {
  [LANG_EN] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"},
  [LANG_DE] = {"SO", "MO", "DI", "MI", "DO", "FR", "SA"},
  [LANG_FR] = {"DIM", "LUN", "MAR", "MER", "JEU", "VEN", "SAM"},
  [LANG_IT] = {"DOM", "LUN", "MAR", "MER", "GIO", "VEN", "SAB"},
  [LANG_ES] = {"DOM", "LUN", "MAR", "MIÉ", "JUE", "VIE", "SÁB"},
};

enum {
  C_CLEAR, C_MOSTLY_CLEAR, C_PARTLY, C_CLOUDY, C_FOG, C_DRIZZLE, C_RAIN,
  C_FREEZING, C_SNOW, C_SHOWERS, C_SNOW_SHOWERS, C_THUNDER, C_COUNT
};

static const char *const COND[LANG_COUNT][C_COUNT] = {
  [LANG_EN] = {"Clear", "Mostly clear", "Partly cloudy", "Cloudy", "Fog", "Drizzle",
               "Rain", "Freezing rain", "Snow", "Showers", "Snow showers", "Thunder"},
  [LANG_DE] = {"Klar", "Meist klar", "Teils bewölkt", "Bewölkt", "Nebel", "Niesel",
               "Regen", "Eisregen", "Schnee", "Schauer", "Schneeschauer", "Gewitter"},
  [LANG_FR] = {"Dégagé", "Plutôt dégagé", "Peu nuageux", "Couvert", "Brouillard",
               "Bruine", "Pluie", "Pluie verglaçante", "Neige", "Averses",
               "Averses de neige", "Orage"},
  [LANG_IT] = {"Sereno", "Quasi sereno", "Poco nuvoloso", "Nuvoloso", "Nebbia",
               "Pioviggine", "Pioggia", "Pioggia gelata", "Neve", "Rovesci",
               "Rovesci di neve", "Temporale"},
  [LANG_ES] = {"Despejado", "Casi despejado", "Algo nublado", "Nublado", "Niebla",
               "Llovizna", "Lluvia", "Lluvia helada", "Nieve", "Chubascos",
               "Chubascos de nieve", "Tormenta"},
};

static const char *tr(int s) {
  int lang = g_set.lang < LANG_COUNT ? g_set.lang : LANG_EN;
  return STR[lang][s];
}

static const char *cond_name(uint8_t code) {
  int c;
  if (code == 0) c = C_CLEAR;
  else if (code == 1) c = C_MOSTLY_CLEAR;
  else if (code == 2) c = C_PARTLY;
  else if (code == 3) c = C_CLOUDY;
  else if (code == 45 || code == 48) c = C_FOG;
  else if (code >= 51 && code <= 55) c = C_DRIZZLE;
  else if (code == 56 || code == 57 || code == 66 || code == 67) c = C_FREEZING;
  else if (code >= 61 && code <= 65) c = C_RAIN;
  else if (code >= 71 && code <= 77) c = C_SNOW;
  else if (code >= 80 && code <= 82) c = C_SHOWERS;
  else if (code == 85 || code == 86) c = C_SNOW_SHOWERS;
  else if (code >= 95) c = C_THUNDER;
  else c = C_CLOUDY;
  int lang = g_set.lang < LANG_COUNT ? g_set.lang : LANG_EN;
  return COND[lang][c];
}

// ---------------------------------------------------------------------------
// Units
// ---------------------------------------------------------------------------

static int div_round(int32_t a, int32_t b) {
  return (int)(a >= 0 ? (a + b / 2) / b : (a - b / 2) / b);
}

void fmt_temp(char *buf, size_t n, int16_t tenths) {
  int v = g_set.temp_unit == TEMP_F ? div_round((int32_t)tenths * 9 + 32 * 50, 50)
                                    : div_round(tenths, 10);
  snprintf(buf, n, "%d°", v);
}

static void fmt_rain(char *buf, size_t n, int tenths_mm) {
  if (g_set.rain_unit == RAIN_IN) {
    int hundredths = div_round(tenths_mm * 100, 254);
    snprintf(buf, n, "%d.%02d in", hundredths / 100, hundredths % 100);
  } else {
    snprintf(buf, n, "%d.%d mm", tenths_mm / 10, tenths_mm % 10);
  }
}

// Rain amount for labels on the ring: "2.8 mm" or ".11 in".
void fmt_rain_short(char *buf, size_t n, int tenths_mm) {
  if (g_set.rain_unit == RAIN_IN) {
    int hundredths = div_round(tenths_mm * 100, 254);
    if (hundredths < 1) hundredths = 1;
    if (hundredths >= 100) snprintf(buf, n, "%d.%02d in", hundredths / 100, hundredths % 100);
    else snprintf(buf, n, ".%02d in", hundredths);
  } else {
    snprintf(buf, n, "%d.%d mm", tenths_mm / 10, tenths_mm % 10);
  }
}

static void fmt_wind(char *buf, size_t n, int kmh) {
  static const uint8_t BFT[] = {1, 6, 12, 20, 29, 39, 50, 62, 75, 89, 103, 118};
  switch (g_set.wind_unit) {
    case WIND_MPH: snprintf(buf, n, "%d mph", div_round(kmh * 1000, 1609)); break;
    case WIND_MS:  snprintf(buf, n, "%d m/s", div_round(kmh * 10, 36)); break;
    case WIND_KT:  snprintf(buf, n, "%d kn", div_round(kmh * 1000, 1852)); break;
    case WIND_BFT: {
      int b = 0;
      while (b < (int)sizeof(BFT) && kmh >= BFT[b]) b++;
      snprintf(buf, n, "%d Bft", b);
      break;
    }
    default: snprintf(buf, n, "%d km/h", kmh); break;
  }
}

static bool use_24h(void) {
  if (g_set.clock == CLOCK_24) return true;
  if (g_set.clock == CLOCK_12) return false;
  return clock_is_24h_style();
}

// Short clock time, like "15:00" or "3 PM" / "3:45 PM".
static void fmt_clock(char *buf, size_t n, time_t t) {
  struct tm *tm = localtime(&t);
  if (use_24h()) {
    snprintf(buf, n, "%02d:%02d", tm->tm_hour, tm->tm_min);
  } else {
    int h = tm->tm_hour % 12;
    if (h == 0) h = 12;
    const char *ap = tm->tm_hour < 12 ? "AM" : "PM";
    if (tm->tm_min == 0) snprintf(buf, n, "%d %s", h, ap);
    else snprintf(buf, n, "%d:%02d %s", h, tm->tm_min, ap);
  }
}

static time_t hour_start(int i) {
  return (time_t)g_wx.t0 + (time_t)i * 3600;
}

// ---------------------------------------------------------------------------
// Lines on the face
// ---------------------------------------------------------------------------

void text_date(char *buf, size_t n, time_t now) {
  struct tm *tm = localtime(&now);
  int lang = g_set.lang < LANG_COUNT ? g_set.lang : LANG_EN;
  snprintf(buf, n, "%s %d", DAYS[lang][tm->tm_wday], tm->tm_mday);
}

// Hours and minutes for the stacked clock.
void text_clock_parts(char *h, size_t hn, char *m, size_t mn, time_t now) {
  struct tm *tm = localtime(&now);
  int hour = tm->tm_hour;
  if (!use_24h()) {
    hour %= 12;
    if (hour == 0) hour = 12;
    snprintf(h, hn, "%d", hour);
  } else {
    snprintf(h, hn, "%02d", hour);
  }
  snprintf(m, mn, "%02d", tm->tm_min);
}

// The temperature right now, like "16°".
void text_temp(char *buf, size_t n, time_t now) {
  int i = wx_index_at(now);
  if (i < 0) {
    snprintf(buf, n, "--°");
    return;
  }
  bool fresh = g_wx.cur_t && now - (time_t)g_wx.cur_t < 75 * 60 && now >= (time_t)g_wx.cur_t - 900;
  fmt_temp(buf, n, fresh ? g_wx.cur_temp : g_wx.temp[i]);
}

bool text_hilo(char *lo, size_t ln, char *hi, size_t hn, time_t now) {
  if (!g_wx.hilo_count || now < (time_t)g_wx.day0) return false;
  // +2 h so a 23 h or 25 h day (clock change) still lands on the right day.
  int d = (int)((now - (time_t)g_wx.day0 + 7200) / 86400);
  if (d >= g_wx.hilo_count) return false;
  fmt_temp(lo, ln, g_wx.lo[d]);
  fmt_temp(hi, hn, g_wx.hi[d]);
  return true;
}

// Appends the heaviest hour of the coming rain spell, like " · 2.8 mm".
static void add_amount(char *buf, size_t n, int i) {
  int peak = 0;
  for (int k = i; k < i + 8 && k < g_wx.count; k++) {
    if (g_wx.precip[k] > peak) peak = g_wx.precip[k];
    else if (peak && g_wx.precip[k] < 1) break;
  }
  if (peak < 1) return;
  char rb[16];
  fmt_rain_short(rb, sizeof(rb), peak);
  size_t len = strlen(buf);
  snprintf(buf + len, n - len, " · %s", rb);
}

static bool smart_rain(char *buf, size_t n, time_t now, int i) {
  char tb[16];
  bool raining = false;

  bool m15_ok = g_wx.m15_count > 0 &&
                now >= (time_t)g_wx.m15_t0 - 900 &&
                now < (time_t)g_wx.m15_t0 + g_wx.m15_count * 900;
  if (m15_ok) {
    int s = now < (time_t)g_wx.m15_t0 ? 0 : (int)((now - (time_t)g_wx.m15_t0) / 900);
    if (g_wx.m15[s] >= 1) {
      raining = true;
    } else {
      for (int k = s + 1; k < g_wx.m15_count; k++) {
        if (g_wx.m15[k] >= 1) {
          int mins = (int)(((time_t)g_wx.m15_t0 + k * 900 - now) / 60);
          mins = ((mins + 2) / 5) * 5;
          if (mins < 5) mins = 5;
          snprintf(buf, n, tr(S_RAIN_IN), mins);
          add_amount(buf, n, i);
          return true;
        }
      }
      return false;
    }
  } else if (wx_wet(i)) {
    raining = true;
  } else {
    for (int k = i + 1; k <= i + 2 && k < g_wx.count; k++) {
      if (wx_wet(k)) {
        fmt_clock(tb, sizeof(tb), hour_start(k));
        snprintf(buf, n, tr(S_RAIN_AT), tb);
        add_amount(buf, n, k);
        return true;
      }
    }
    return false;
  }

  if (raining) {
    for (int k = i + 1; k <= i + 12 && k < g_wx.count; k++) {
      if (!wx_wet(k)) {
        fmt_clock(tb, sizeof(tb), hour_start(k));
        snprintf(buf, n, tr(S_RAIN_UNTIL), tb);
        add_amount(buf, n, i);
        return true;
      }
    }
    snprintf(buf, n, "%s", tr(S_RAIN_NOW));
    add_amount(buf, n, i);
    return true;
  }
  return false;
}

static time_t next_sun_event(time_t now, bool *is_rise) {
  time_t best = 0;
  for (int d = 0; d < g_wx.sun_count; d++) {
    time_t r = (time_t)g_wx.rise[d], s = (time_t)g_wx.set[d];
    if (r > now && (best == 0 || r < best)) { best = r; *is_rise = true; }
    if (s > now && (best == 0 || s < best)) { best = s; *is_rise = false; }
  }
  return best;
}

static bool smart_frost(char *buf, size_t n, time_t now, int i) {
  if (!g_set.frost) return false;
  struct tm *tm = localtime(&now);
  int hour = tm->tm_hour;
  bool rise_next = false;
  time_t ev = next_sun_event(now, &rise_next);
  // Evening (from 15:00) or the dark hours before sunrise.
  bool night_ahead = hour >= 15 || (ev && rise_next && hour < 12);
  if (!night_ahead) return false;

  time_t end = now + 15 * 3600;
  for (int d = 0; d < g_wx.sun_count; d++) {
    if ((time_t)g_wx.rise[d] > now) { end = (time_t)g_wx.rise[d] + 2 * 3600; break; }
  }
  int16_t lo = INT16_MAX;
  for (int k = i; k < g_wx.count && hour_start(k) <= end; k++) {
    if (g_wx.temp[k] < lo) lo = g_wx.temp[k];
  }
  if (lo > 0) return false;
  char tb[8];
  fmt_temp(tb, sizeof(tb), lo);
  snprintf(buf, n, tr(S_FROST), tb);
  return true;
}

static bool smart_gusts(char *buf, size_t n, int i) {
  if (!g_set.gust) return false;
  int hi = 0;
  for (int k = i; k <= i + 6 && k < g_wx.count; k++) {
    if (g_wx.gust[k] > hi) hi = g_wx.gust[k];
  }
  if (hi < g_set.gust) return false;
  char wb[16];
  fmt_wind(wb, sizeof(wb), hi);
  snprintf(buf, n, tr(S_GUSTS), wb);
  return true;
}

static bool smart_uv(char *buf, size_t n, int i) {
  if (!g_set.uv) return false;
  int hi = 0, at = i;
  for (int k = i; k <= i + 6 && k < g_wx.count; k++) {
    if (g_wx.uv[k] > hi) { hi = g_wx.uv[k]; at = k; }
  }
  int uv = div_round(hi, 10);
  if (uv < g_set.uv) return false;
  char tb[16];
  fmt_clock(tb, sizeof(tb), hour_start(at));
  snprintf(buf, n, tr(S_UV), uv, tb);
  return true;
}

static bool smart_dry_until(char *buf, size_t n, int i) {
  for (int k = i + 1; k <= i + 24 && k < g_wx.count; k++) {
    if (wx_wet(k)) {
      char tb[16];
      fmt_clock(tb, sizeof(tb), hour_start(k));
      snprintf(buf, n, tr(S_DRY_UNTIL), tb);
      return true;
    }
  }
  return false;
}

static bool smart_sun(char *buf, size_t n, time_t now) {
  if (!g_set.sunline) return false;
  bool rise = false;
  time_t ev = next_sun_event(now, &rise);
  if (!ev || ev - now > 24 * 3600) return false;
  char tb[16];
  fmt_clock(tb, sizeof(tb), ev);
  snprintf(buf, n, tr(rise ? S_SUNRISE : S_SUNSET), tb);
  return true;
}

static void smart_hilo(char *buf, size_t n, int i) {
  int16_t hi = INT16_MIN, lo = INT16_MAX;
  for (int k = i; k < i + 24 && k < g_wx.count; k++) {
    if (g_wx.temp[k] > hi) hi = g_wx.temp[k];
    if (g_wx.temp[k] < lo) lo = g_wx.temp[k];
  }
  char a[8], b[8];
  fmt_temp(a, sizeof(a), hi);
  fmt_temp(b, sizeof(b), lo);
  snprintf(buf, n, tr(S_HILO), a, b);
}

// One sentence with the most important thing, in this order: rain now or
// within 2 hours, frost tonight, wind gusts, UV, "dry until", sunset/sunrise.
void text_smart(char *buf, size_t n, time_t now) {
  int i = wx_index_at(now);
  if (i < 0) {
    int s = !g_set.updates ? S_OFF
          : g_wx.status == WX_NO_LOCATION ? S_NO_LOC
          : g_wx.status == WX_NO_NETWORK ? S_NO_NET
          : S_WAITING;
    snprintf(buf, n, "%s", tr(s));
    return;
  }
  if (smart_rain(buf, n, now, i)) return;
  if (smart_frost(buf, n, now, i)) return;
  if (smart_gusts(buf, n, i)) return;
  if (smart_uv(buf, n, i)) return;
  if (smart_dry_until(buf, n, i)) return;
  if (smart_sun(buf, n, now)) return;
  smart_hilo(buf, n, i);
}

void text_corner(char *buf, size_t n) {
  buf[0] = '\0';
  switch (g_set.corner) {
    case CORNER_BATTERY:
      snprintf(buf, n, "%d%%", g_view.battery);
      break;
    case CORNER_STEPS:
      if (g_view.steps >= 0) snprintf(buf, n, "%d %s", (int)g_view.steps, tr(S_STEPS));
      else snprintf(buf, n, "--");
      break;
    case CORNER_HEART:
      if (g_view.heart > 0) snprintf(buf, n, "%d bpm", (int)g_view.heart);
      else snprintf(buf, n, "-- bpm");
      break;
    default:
      break;
  }
}

// Shown while a wrist flick scrolls through the coming hours.
void text_scrub(char *line1, size_t n1, char *line2, size_t n2, time_t t) {
  int i = wx_index_at(t);
  char cb[16], tb[8];
  fmt_clock(cb, sizeof(cb), t - t % 3600);
  if (i < 0) {
    snprintf(line1, n1, "%s", cb);
    line2[0] = '\0';
    return;
  }
  bool ok;
  fmt_temp(tb, sizeof(tb), wx_temp_at(t, &ok));
  snprintf(line1, n1, "%s  %s", cb, tb);  // e.g. "21:00  6°"
  if (g_wx.precip[i] >= 1) {
    char rb[16];
    fmt_rain(rb, sizeof(rb), g_wx.precip[i]);
    if (g_wx.prob[i] != PROB_UNKNOWN) {
      snprintf(line2, n2, "%s  %d%%", rb, g_wx.prob[i]);
    } else {
      snprintf(line2, n2, "%s", rb);
    }
  } else {
    snprintf(line2, n2, "%s", cond_name(g_wx.code[i]));
  }
}
