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
