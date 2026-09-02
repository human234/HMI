#include "chart.h"
#include "common.h"

#include <stdio.h>
#include <string.h>

#define HMI_CHART_DEFAULT_POINTS 80
#define HMI_CHART_DEFAULT_MIN    0
#define HMI_CHART_DEFAULT_MAX    100
#define HMI_CHART_X_DT_SEC       0.05f

#define HMI_CHART_PAD_LEFT       44
#define HMI_CHART_PAD_RIGHT      6
#define HMI_CHART_PAD_TOP        12
#define HMI_CHART_PAD_BOTTOM     36

struct hmi_chart_t {
    lv_obj_t * obj;
    lv_chart_series_t * series;
    int32_t y_min;
    int32_t y_max;
    uint16_t point_count;
    float x_dt;
};

static void chart_draw_ticks(lv_event_t * e)
{
    lv_obj_t * chart = lv_event_get_target(e);
    lv_layer_t * layer = lv_event_get_layer(e);
    if (layer == NULL) return;

    hmi_chart_t * hchart = lv_event_get_user_data(e);
    if (hchart == NULL) return;

    lv_area_t coords;
    lv_obj_get_coords(chart, &coords);
    lv_chart_type_t type = lv_chart_get_type(chart);
    if (type == LV_CHART_TYPE_NONE) return;

    int32_t bw = lv_obj_get_style_border_width(chart, LV_PART_MAIN);
    int32_t pad_l = lv_obj_get_style_pad_left(chart, LV_PART_MAIN) + bw;
    int32_t pad_t = lv_obj_get_style_pad_top(chart, LV_PART_MAIN) + bw;
    int32_t w = lv_obj_get_content_width(chart);
    int32_t h = lv_obj_get_content_height(chart);
    int32_t chart_x = coords.x1 + pad_l;
    int32_t chart_y = coords.y1 + pad_t;

    char buf[8];

    int32_t hdiv = 5;
    for (int32_t i = 0; i <= hdiv; i++) {
        int32_t y = chart_y + (h * i) / hdiv;

        lv_draw_line_dsc_t tick;
        lv_draw_line_dsc_init(&tick);
        tick.color = COLOR_DIM;
        tick.width = 1;
        tick.p1 = (lv_point_precise_t){ .x = chart_x - 6, .y = y };
        tick.p2 = (lv_point_precise_t){ .x = chart_x - 1, .y = y };
        lv_draw_line(layer, &tick);

        int32_t span = hchart->y_max - hchart->y_min;
        int32_t val = hchart->y_max - (span * i) / hdiv;
        snprintf(buf, sizeof(buf), "%d", val);

        lv_draw_label_dsc_t lbl;
        lv_draw_label_dsc_init(&lbl);
        lbl.text = buf;
        lbl.color = COLOR_DIM;
        lbl.align = LV_TEXT_ALIGN_RIGHT;
        lv_area_t la = { chart_x - 36, y - 8, chart_x - 9, y + 8 };
        lv_draw_label(layer, &lbl, &la);
    }

    float span = hchart->point_count * hchart->x_dt;
    int32_t vdiv = 8;
    for (int32_t i = 0; i <= vdiv; i++) {
        int32_t x = chart_x + (w * i) / vdiv;

        lv_draw_line_dsc_t tick;
        lv_draw_line_dsc_init(&tick);
        tick.color = COLOR_DIM;
        tick.width = 1;
        tick.p1 = (lv_point_precise_t){ .x = x, .y = chart_y + h };
        tick.p2 = (lv_point_precise_t){ .x = x, .y = chart_y + h + 4 };
        lv_draw_line(layer, &tick);

        float sec = -span + (span * i) / vdiv;
        snprintf(buf, sizeof(buf), "%.1f", (double)sec);

        lv_draw_label_dsc_t lbl;
        lv_draw_label_dsc_init(&lbl);
        lbl.text = buf;
        lbl.color = COLOR_DIM;
        lbl.align = LV_TEXT_ALIGN_CENTER;
        lv_area_t la = { x - 16, chart_y + h + 6, x + 16, chart_y + h + 19 };
        lv_draw_label(layer, &lbl, &la);
    }

    lv_draw_label_dsc_t unit;
    lv_draw_label_dsc_init(&unit);
    unit.text = "t (s)";
    unit.color = COLOR_DIM;
    unit.align = LV_TEXT_ALIGN_CENTER;
    lv_area_t ua = { chart_x, chart_y + h + 21, chart_x + w, chart_y + h + 33 };
    lv_draw_label(layer, &unit, &ua);
}

hmi_chart_t * hmi_chart_create(lv_obj_t * parent)
{
    hmi_chart_t * chart = lv_malloc(sizeof(hmi_chart_t));
    if (chart == NULL) return NULL;

    memset(chart, 0, sizeof(hmi_chart_t));
    chart->y_min = HMI_CHART_DEFAULT_MIN;
    chart->y_max = HMI_CHART_DEFAULT_MAX;
    chart->point_count = HMI_CHART_DEFAULT_POINTS;
    chart->x_dt = HMI_CHART_X_DT_SEC;

    chart->obj = lv_chart_create(parent);
    lv_obj_set_size(chart->obj, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(chart->obj, COLOR_PANEL, 0);
    lv_obj_set_style_border_width(chart->obj, 0, 0);
    lv_obj_set_style_pad_left(chart->obj, HMI_CHART_PAD_LEFT, 0);
    lv_obj_set_style_pad_right(chart->obj, HMI_CHART_PAD_RIGHT, 0);
    lv_obj_set_style_pad_top(chart->obj, HMI_CHART_PAD_TOP, 0);
    lv_obj_set_style_pad_bottom(chart->obj, HMI_CHART_PAD_BOTTOM, 0);
    lv_obj_set_style_radius(chart->obj, 10, 0);

    lv_chart_set_type(chart->obj, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart->obj, chart->point_count);
    lv_chart_set_range(chart->obj, LV_CHART_AXIS_PRIMARY_Y, chart->y_min, chart->y_max);
    lv_chart_set_update_mode(chart->obj, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_div_line_count(chart->obj, 5, 8);

    lv_obj_set_style_line_color(chart->obj, lv_color_hex(0x2A2A4A), LV_PART_MAIN);
    lv_obj_set_style_line_width(chart->obj, 1, LV_PART_MAIN);
    lv_obj_set_style_line_opa(chart->obj, LV_OPA_50, LV_PART_MAIN);

    chart->series = lv_chart_add_series(chart->obj, COLOR_ACCENT, LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_values(chart->obj, chart->series, LV_CHART_POINT_NONE);
    lv_obj_set_style_line_width(chart->obj, 2, LV_PART_ITEMS);

    lv_obj_add_event_cb(chart->obj, chart_draw_ticks, LV_EVENT_DRAW_POST, chart);

    return chart;
}

void hmi_chart_delete(hmi_chart_t * chart)
{
    if (chart == NULL) return;
    lv_obj_delete(chart->obj);
    lv_free(chart);
}

void hmi_chart_set_range(hmi_chart_t * chart, int32_t min, int32_t max)
{
    if (chart == NULL) return;
    chart->y_min = min;
    chart->y_max = max;
    lv_chart_set_range(chart->obj, LV_CHART_AXIS_PRIMARY_Y, min, max);
}

void hmi_chart_set_point_count(hmi_chart_t * chart, uint16_t count)
{
    if (chart == NULL || count == 0) return;
    chart->point_count = count;
    lv_chart_set_point_count(chart->obj, count);
    lv_chart_set_all_values(chart->obj, chart->series, LV_CHART_POINT_NONE);
}

void hmi_chart_set_series_color(hmi_chart_t * chart, lv_color_t color)
{
    if (chart == NULL) return;
    lv_obj_set_style_line_color(chart->obj, color, LV_PART_ITEMS);
}

void hmi_chart_push_value(hmi_chart_t * chart, int32_t value)
{
    if (chart == NULL || chart->series == NULL) return;
    lv_chart_set_next_value(chart->obj, chart->series, value);
    lv_chart_refresh(chart->obj);
}

void hmi_chart_clear(hmi_chart_t * chart)
{
    if (chart == NULL) return;
    lv_chart_set_all_values(chart->obj, chart->series, LV_CHART_POINT_NONE);
    lv_chart_refresh(chart->obj);
}

lv_obj_t * hmi_chart_get_obj(const hmi_chart_t * chart)
{
    if (chart == NULL) return NULL;
    return chart->obj;
}