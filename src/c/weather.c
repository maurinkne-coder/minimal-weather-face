#include "contour.h"

int wx_index_at(time_t t) {
  if (g_wx.count == 0 || t < (time_t)g_wx.t0) {
    return -1;
  }
  int i = (int)((t - (time_t)g_wx.t0) / 3600);
  return i < g_wx.count ? i : -1;
}

bool wx_wet(int i) {
  if (i < 0 || i >= g_wx.count) {
    return false;
  }
  uint8_t p = g_wx.prob[i];
  return g_wx.precip[i] >= 2 && (p == PROB_UNKNOWN || p >= 40);
}

// Temperature at any moment, interpolated between the hourly values.
int16_t wx_temp_at(time_t t, bool *ok) {
  int i = wx_index_at(t);
  if (i < 0) {
    *ok = false;
    return 0;
  }
  *ok = true;
  if (i + 1 >= g_wx.count) {
    return g_wx.temp[i];
  }
  int32_t frac = (int32_t)((t - (time_t)g_wx.t0) % 3600);
  int32_t a = g_wx.temp[i];
  int32_t b = g_wx.temp[i + 1];
  return (int16_t)(a + (b - a) * frac / 3600);
}

int wx_range_hours(void) {
#if defined(PBL_ROUND)
  return 24;
#else
  return g_set.range == 12 || g_set.range == 48 ? g_set.range : 24;
#endif
}

// When the next rain starts, if it is dry now and rain is due within the
// 15-minute nowcast or the next 3 hours; 0 if it is raining or stays dry.
time_t wx_next_rain(time_t now) {
  bool m15_ok = g_wx.m15_count > 0 &&
                now >= (time_t)g_wx.m15_t0 - 900 &&
                now < (time_t)g_wx.m15_t0 + g_wx.m15_count * 900;
  if (m15_ok) {
    int s = now < (time_t)g_wx.m15_t0 ? 0 : (int)((now - (time_t)g_wx.m15_t0) / 900);
    if (g_wx.m15[s] >= 1) return 0;
    for (int k = s + 1; k < g_wx.m15_count; k++) {
      if (g_wx.m15[k] >= 1) return (time_t)g_wx.m15_t0 + k * 900;
    }
  }
  int i = wx_index_at(now);
  if (i < 0 || wx_wet(i)) return 0;
  for (int k = i + 1; k <= i + 3 && k < g_wx.count; k++) {
    if (wx_wet(k)) {
      time_t start = (time_t)g_wx.t0 + k * 3600;
      // The nowcast saw no rain in its window: only trust hours after it.
      if (m15_ok && start < (time_t)g_wx.m15_t0 + g_wx.m15_count * 900) continue;
      return start;
    }
  }
  return 0;
}
