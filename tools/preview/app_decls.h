// Extra declarations so main.c can be syntax-checked on the host.
typedef struct Window Window; typedef struct Animation Animation; typedef struct AppTimer AppTimer;
typedef struct DictionaryIterator DictionaryIterator;
typedef enum { TUPLE_BYTE_ARRAY, TUPLE_CSTRING, TUPLE_UINT, TUPLE_INT } TupleType;
typedef struct { uint32_t key; TupleType type; uint16_t length; union { uint8_t data[0]; int32_t int32; } value[]; } Tuple;
typedef enum { APP_MSG_OK } AppMessageResult;
typedef struct { uint8_t charge_percent; bool is_charging, is_plugged; } BatteryChargeState;
typedef enum { MINUTE_UNIT = 2 } TimeUnits;
typedef enum { ACCEL_AXIS_X } AccelAxisType;
typedef uint32_t AnimationProgress;
#define ANIMATION_NORMALIZED_MAX 65535
typedef enum { AnimationCurveEaseOut, AnimationCurveEaseInOut } AnimationCurve;
typedef struct { void (*setup)(Animation*); void (*update)(Animation*, const AnimationProgress); void (*teardown)(Animation*); } AnimationImplementation;
typedef struct { void (*load)(Window*); void (*appear)(Window*); void (*disappear)(Window*); void (*unload)(Window*); } WindowHandlers;
typedef enum { HealthMetricStepCount, HealthMetricHeartRateBPM } HealthMetric;
typedef enum { HealthServiceAccessibilityMaskAvailable = 1 } HealthServiceAccessibilityMask;
#define PBL_HEALTH 1
#define PERSIST_DATA_MAX_LENGTH 256
enum { MESSAGE_KEY_Theme, MESSAGE_KEY_Range, MESSAGE_KEY_Corner, MESSAGE_KEY_Anim, MESSAGE_KEY_Frost, MESSAGE_KEY_Gust, MESSAGE_KEY_Uv, MESSAGE_KEY_SunLine, MESSAGE_KEY_Lang, MESSAGE_KEY_Clock, MESSAGE_KEY_TempUnit, MESSAGE_KEY_RainUnit, MESSAGE_KEY_WindUnit, MESSAGE_KEY_Updates, MESSAGE_KEY_Interval, MESSAGE_KEY_Tilt, MESSAGE_KEY_BattLine, MESSAGE_KEY_StepBar, MESSAGE_KEY_StepGoal, MESSAGE_KEY_Request, MESSAGE_KEY_JsReady, MESSAGE_KEY_WxT0, MESSAGE_KEY_WxTemp, MESSAGE_KEY_WxPrecip, MESSAGE_KEY_WxProb, MESSAGE_KEY_WxCode, MESSAGE_KEY_WxGust, MESSAGE_KEY_WxUv, MESSAGE_KEY_WxDay, MESSAGE_KEY_WxM15T0, MESSAGE_KEY_WxM15, MESSAGE_KEY_WxRise, MESSAGE_KEY_WxSet, MESSAGE_KEY_WxCurTemp, MESSAGE_KEY_WxCurCode, MESSAGE_KEY_WxCurT, MESSAGE_KEY_WxStatus };
bool persist_exists(uint32_t); int persist_get_size(uint32_t); int persist_read_data(uint32_t, void*, size_t); int persist_write_data(uint32_t, const void*, size_t);
int32_t persist_read_int(uint32_t); int persist_write_int(uint32_t, int32_t);
bool quiet_time_is_active(void); time_t time_start_of_today(void);
HealthServiceAccessibilityMask health_service_metric_accessible(HealthMetric, time_t, time_t);
int32_t health_service_sum_today(HealthMetric); typedef enum { HealthServiceTimeScopeDailyWeekdayOrWeekend } HealthServiceTimeScope; int32_t health_service_sum_averaged(HealthMetric, time_t, time_t, HealthServiceTimeScope); int32_t health_service_peek_current_value(HealthMetric);
bool connection_service_peek_pebble_app_connection(void);
AppMessageResult app_message_outbox_begin(DictionaryIterator**); AppMessageResult app_message_outbox_send(void);
int dict_write_uint8(DictionaryIterator*, uint32_t, uint8_t); Tuple *dict_find(const DictionaryIterator*, uint32_t);
void layer_mark_dirty(Layer*); Animation *animation_create(void); void animation_set_duration(Animation*, uint32_t);
void animation_set_curve(Animation*, AnimationCurve); void animation_set_implementation(Animation*, const AnimationImplementation*);
bool animation_schedule(Animation*); bool animation_unschedule_all(void);
AppTimer *app_timer_register(uint32_t, void (*)(void*), void*); void app_timer_cancel(AppTimer*);
Window *window_create(void); void window_destroy(Window*); void window_set_window_handlers(Window*, WindowHandlers); void window_stack_push(Window*, bool);
Layer *window_get_root_layer(const Window*); Layer *layer_create(GRect); void layer_destroy(Layer*); void layer_add_child(Layer*, Layer*);
void layer_set_update_proc(Layer*, void (*)(Layer*, GContext*));
BatteryChargeState battery_state_service_peek(void); void battery_state_service_subscribe(void (*)(BatteryChargeState)); void battery_state_service_unsubscribe(void);
void tick_timer_service_subscribe(TimeUnits, void (*)(struct tm*, TimeUnits)); void tick_timer_service_unsubscribe(void);
void accel_tap_service_subscribe(void (*)(AccelAxisType, int32_t)); void accel_tap_service_unsubscribe(void);
void app_message_register_inbox_received(void (*)(DictionaryIterator*, void*)); AppMessageResult app_message_open(uint32_t, uint32_t);
void app_event_loop(void);
typedef struct GBitmap GBitmap;
typedef enum { GBitmapFormat8Bit } GBitmapFormat;
typedef enum { GCompOpAssign } GCompOp;
typedef struct { uint8_t *data; int16_t min_x, max_x; } GBitmapDataRowInfo;
GBitmap *gbitmap_create_blank(GSize, GBitmapFormat); void gbitmap_destroy(GBitmap*);
uint8_t *gbitmap_get_data(const GBitmap*); uint16_t gbitmap_get_bytes_per_row(const GBitmap*);
GBitmapDataRowInfo gbitmap_get_data_row_info(const GBitmap*, uint16_t);
GBitmap *graphics_capture_frame_buffer(GContext*); bool graphics_release_frame_buffer(GContext*, GBitmap*);
void graphics_context_set_compositing_mode(GContext*, GCompOp);
void graphics_draw_rotated_bitmap(GContext*, GBitmap*, GPoint, int32_t, GPoint);
enum { RESOURCE_ID_FONT_CLOCK_86 = 1, RESOURCE_ID_FONT_CLOCK_88 = 2, RESOURCE_ID_FONT_SMALL_12 = 3 };
ResHandle resource_get_handle(uint32_t); GFont fonts_load_custom_font(ResHandle); void fonts_unload_custom_font(GFont);
#define SECONDS_PER_DAY 86400
