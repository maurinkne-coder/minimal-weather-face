// Minimal host-side stand-in for the Pebble SDK, used only to render
// preview images of the face. Drawing calls are printed as JSON lines.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef union { uint8_t argb; } GColor;
typedef struct { int16_t x, y; } GPoint;
typedef struct { int16_t w, h; } GSize;
typedef struct { GPoint origin; GSize size; } GRect;
typedef struct { int16_t top, right, bottom, left; } GEdgeInsets;
#define GPoint(x, y) ((GPoint){(int16_t)(x), (int16_t)(y)})
#define GRect(x, y, w, h) ((GRect){{(int16_t)(x), (int16_t)(y)}, {(int16_t)(w), (int16_t)(h)}})
#define GEdgeInsets(v) ((GEdgeInsets){(v), (v), (v), (v)})
#define ARRAY_LENGTH(a) (sizeof(a) / sizeof((a)[0]))
#define TRIG_MAX_ANGLE 0x10000
#define TRIG_MAX_RATIO 0xffff
#define DEG_TO_TRIGANGLE(a) ((a) * TRIG_MAX_ANGLE / 360)
#ifdef PBL_ROUND
#define PBL_IF_ROUND_ELSE(a, b) (a)
#else
#define PBL_IF_ROUND_ELSE(a, b) (b)
#endif

typedef struct Layer Layer;
typedef struct GContext GContext;
typedef const char *GFont;
typedef enum { GTextOverflowModeWordWrap, GTextOverflowModeTrailingEllipsis, GTextOverflowModeFill } GTextOverflowMode;
typedef enum { GTextAlignmentLeft, GTextAlignmentCenter, GTextAlignmentRight } GTextAlignment;
typedef enum { GCornerNone = 0, GCornersAll = 15 } GCornerMask;
typedef enum { GOvalScaleModeFitCircle } GOvalScaleMode;

#define FONT_KEY_GOTHIC_14 "gothic14"
#define FONT_KEY_GOTHIC_14_BOLD "gothic14b"
#define FONT_KEY_GOTHIC_18 "gothic18"
#define FONT_KEY_GOTHIC_18_BOLD "gothic18b"
#define FONT_KEY_GOTHIC_24_BOLD "gothic24b"
#define FONT_KEY_BITHAM_42_LIGHT "bitham42l"
typedef struct ResHandle *ResHandle;

#define C(name, v) enum { GColor##name##ARGB8 = (v) };
C(Black,0xC0) C(White,0xFF) C(LightGray,0xEA) C(DarkGray,0xD5) C(OxfordBlue,0xC1)
C(Folly,0xF1) C(Orange,0xF4) C(ChromeYellow,0xF8) C(Icterine,0xFD) C(Inchworm,0xED)
C(MediumAquamarine,0xDE) C(ElectricBlue,0xDF) C(PictonBlue,0xDB) C(VeryLightBlue,0xD7)
C(LavenderIndigo,0xE7) C(Indigo,0xD2) C(Blue,0xC3) C(BlueMoon,0xC7) C(VividCerulean,0xCB)
C(JaegerGreen,0xC9) C(KellyGreen,0xD8) C(Limerick,0xE8) C(Red,0xF0) C(CobaltBlue,0xC6)
C(Celeste,0xEF) C(Green,0xCC) C(IslamicGreen,0xC8)
#undef C
#define PBL_COLOR 1
static inline bool gcolor_equal(GColor a, GColor b) { return a.argb == b.argb; }
#define GColorGreen ((GColor){0xCC})
#define GColorIslamicGreen ((GColor){0xC8})
#ifndef GColorRed
#define GColorRed ((GColor){0xF0})
#endif
#ifndef GColorOrange
#define GColorOrange ((GColor){0xF4})
#endif
#define GColorBlack ((GColor){GColorBlackARGB8})
#define GColorWhite ((GColor){GColorWhiteARGB8})
#define GColorLightGray ((GColor){GColorLightGrayARGB8})
#define GColorDarkGray ((GColor){GColorDarkGrayARGB8})
#define GColorCobaltBlue ((GColor){0xC6})
#define GColorBlue ((GColor){GColorBlueARGB8})
#define GColorPictonBlue ((GColor){GColorPictonBlueARGB8})
#define GColorCeleste ((GColor){GColorCelesteARGB8})
#define GColorIcterine ((GColor){GColorIcterineARGB8})
#define GColorElectricBlue ((GColor){GColorElectricBlueARGB8})
#define GColorOxfordBlue ((GColor){GColorOxfordBlueARGB8})

int32_t sin_lookup(int32_t a);
int32_t cos_lookup(int32_t a);
GRect grect_inset(GRect r, GEdgeInsets i);
GRect layer_get_bounds(Layer *l);
GFont fonts_get_system_font(const char *key);
bool clock_is_24h_style(void);
void graphics_context_set_fill_color(GContext *c, GColor col);
void graphics_context_set_stroke_color(GContext *c, GColor col);
void graphics_context_set_text_color(GContext *c, GColor col);
void graphics_context_set_stroke_width(GContext *c, uint8_t w);
void graphics_context_set_antialiased(GContext *c, bool on);
void graphics_fill_rect(GContext *c, GRect r, uint16_t radius, GCornerMask m);
void graphics_fill_circle(GContext *c, GPoint p, uint16_t r);
void graphics_draw_line(GContext *c, GPoint a, GPoint b);
void graphics_fill_radial(GContext *c, GRect r, GOvalScaleMode m, uint16_t inset, int32_t a0, int32_t a1);
void graphics_draw_text(GContext *c, const char *t, GFont f, GRect r, GTextOverflowMode o, GTextAlignment a, void *attr);
GSize graphics_text_layout_get_content_size(const char *t, GFont f, GRect r, GTextOverflowMode o, GTextAlignment a);
typedef struct { uint32_t num_points; GPoint *points; } GPathInfo;
typedef struct GPath GPath;
GPath *gpath_create(const GPathInfo *info);
void gpath_draw_filled(GContext *c, GPath *p);
void gpath_draw_outline(GContext *c, GPath *p);
void gpath_destroy(GPath *p);
