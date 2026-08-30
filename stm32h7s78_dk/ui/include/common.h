#ifndef COMMON_H
#define COMMON_H

#include <lvgl.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================
 * Color Theme – single source for all widgets
 *=============================================*/
#define COLOR_BG      lv_color_hex(0x0A0A12)
#define COLOR_PANEL   lv_color_hex(0x16162A)
#define COLOR_TEXT    lv_color_hex(0xE0E8FF)
#define COLOR_DIM     lv_color_hex(0x6B7280)
#define COLOR_ACCENT  lv_color_hex(0x00E5FF)
#define COLOR_OK      lv_color_hex(0x00FF41)
#define COLOR_WARN    lv_color_hex(0xFF6B00)
#define COLOR_ERROR   lv_color_hex(0xFF0055)

/*=============================================
 * Common math utilities (inline – zero overhead)
 *=============================================*/
static inline float hmi_clamp(float value, float min, float max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static inline float hmi_map(float value,
                            float in_min, float in_max,
                            float out_min, float out_max)
{
    return (value - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static inline float hmi_normalize(float value, float min, float max)
{
    if (max <= min) return 0;
    return hmi_clamp((value - min) / (max - min), 0, 1);
}

static inline lv_point_precise_t hmi_polar_to_point(int32_t cx, int32_t cy,
                                                     float radius, float angle)
{
    float rad = angle * 3.141592653589793f / 180.0f;
    lv_point_precise_t pt = {
        .x = cx + cosf(rad) * radius,
        .y = cy + sinf(rad) * radius
    };
    return pt;
}

/*=============================================
 * Pulse animation helpers
 *=============================================*/
static inline float hmi_pulse_phase(void)
{
    return (float)(lv_tick_get() % 2000) / 2000.0f;
}

static inline float hmi_pulse_brightness(float phase)
{
    return (sinf(phase * 6.283185f) + 1.0f) * 0.5f;
}

/*=============================================
 * Draw event macro – reduces boilerplate
 * Every custom widget has the same 5-line preamble.
 *=============================================*/
#define HMI_DRAW_EVENT_BEGIN(struct_type, obj_member, var_name)          \
    struct_type * var_name = lv_event_get_user_data(event);              \
    lv_layer_t * layer = lv_event_get_layer(event);                     \
    if (var_name == NULL || layer == NULL) return;                      \
    lv_area_t coords;                                                    \
    lv_obj_get_coords(var_name->obj_member, &coords)

#ifdef __cplusplus
}
#endif

#endif /* COMMON_H */
