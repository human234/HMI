#ifndef SLIDER_H
#define SLIDER_H

#include <lvgl.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hmi_slider_t hmi_slider_t;

typedef void (*hmi_slider_event_cb_t)(hmi_slider_t * slider, float value);

hmi_slider_t * hmi_slider_create(lv_obj_t * parent);
void           hmi_slider_delete(hmi_slider_t * slider);

void           hmi_slider_set_value(hmi_slider_t * slider, float value);
float          hmi_slider_get_value(const hmi_slider_t * slider);

void           hmi_slider_set_range(hmi_slider_t * slider, float min, float max);
void           hmi_slider_set_precision(hmi_slider_t * slider, uint8_t precision);
void           hmi_slider_set_unit(hmi_slider_t * slider, const char * unit);
void           hmi_slider_set_title(hmi_slider_t * slider, const char * title);

void           hmi_slider_set_color(hmi_slider_t * slider, lv_color_t color);
void           hmi_slider_on_change(hmi_slider_t * slider, hmi_slider_event_cb_t cb);

lv_obj_t *     hmi_slider_get_obj(const hmi_slider_t * slider);

void slider_example(void);

#ifdef __cplusplus
}
#endif

#endif /* SLIDER_H */
