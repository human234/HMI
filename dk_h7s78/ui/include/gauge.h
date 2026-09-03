#ifndef GAUGE_H
#define GAUGE_H

#include <lvgl.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HMI_GAUGE_MAX_ZONE 5

typedef enum {
    HMI_GAUGE_STYLE_CYAN,
    HMI_GAUGE_STYLE_PINK,
    HMI_GAUGE_STYLE_AMBER
} hmi_gauge_style_t;

typedef enum {
    HMI_GAUGE_OK      = 0,
    HMI_GAUGE_WARNING = 1,
    HMI_GAUGE_ERROR   = 2,
    HMI_GAUGE_TRIP    = 3
} hmi_gauge_alarm_t;

typedef struct {
    float      min;
    float      max;
    lv_color_t color;
} hmi_gauge_zone_t;

typedef struct hmi_gauge_t hmi_gauge_t;

hmi_gauge_t * hmi_gauge_create(lv_obj_t * parent);
void          hmi_gauge_delete(hmi_gauge_t * gauge);

void          hmi_gauge_set_value(hmi_gauge_t * gauge, float value);
float         hmi_gauge_get_value(const hmi_gauge_t * gauge);

void          hmi_gauge_set_range(hmi_gauge_t * gauge, float min, float max);
void          hmi_gauge_set_precision(hmi_gauge_t * gauge, uint8_t precision);
void          hmi_gauge_set_tick_count(hmi_gauge_t * gauge, uint16_t count);

void          hmi_gauge_set_title(hmi_gauge_t * gauge, const char * title);
void          hmi_gauge_set_unit(hmi_gauge_t * gauge, const char * unit);

bool          hmi_gauge_add_zone(hmi_gauge_t * gauge, float min, float max, lv_color_t color);
void          hmi_gauge_clear_zones(hmi_gauge_t * gauge);

void          hmi_gauge_set_alarm(hmi_gauge_t * gauge, hmi_gauge_alarm_t alarm);
hmi_gauge_alarm_t hmi_gauge_get_alarm(const hmi_gauge_t * gauge);

void          hmi_gauge_set_peak_enable(hmi_gauge_t * gauge, bool enable);
void          hmi_gauge_reset_peak(hmi_gauge_t * gauge);

void          hmi_gauge_set_animation_enable(hmi_gauge_t * gauge, bool enable);

void          hmi_gauge_set_style(hmi_gauge_t * gauge, hmi_gauge_style_t style);

lv_obj_t *    hmi_gauge_get_obj(const hmi_gauge_t * gauge);

void gauge_example(void);

#ifdef __cplusplus
}
#endif

#endif /* GAUGE_H */
