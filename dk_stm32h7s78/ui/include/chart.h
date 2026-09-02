#ifndef CHART_H
#define CHART_H

#include <lvgl.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hmi_chart_t hmi_chart_t;

hmi_chart_t * hmi_chart_create(lv_obj_t * parent);
void          hmi_chart_delete(hmi_chart_t * chart);

void          hmi_chart_set_range(hmi_chart_t * chart, int32_t min, int32_t max);
void          hmi_chart_set_point_count(hmi_chart_t * chart, uint16_t count);
void          hmi_chart_set_series_color(hmi_chart_t * chart, lv_color_t color);

void          hmi_chart_push_value(hmi_chart_t * chart, int32_t value);
void          hmi_chart_clear(hmi_chart_t * chart);

lv_obj_t *    hmi_chart_get_obj(const hmi_chart_t * chart);

#ifdef __cplusplus
}
#endif

#endif /* CHART_H */