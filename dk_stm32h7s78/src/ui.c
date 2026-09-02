#include "ui.h"
#include "common.h"
#include "chart.h"

#include <math.h>
#include <string.h>

typedef struct {
    hmi_chart_t * chart;
    float t;
} ui_data_t;

static ui_data_t ui;

static void ui_update_timer(lv_timer_t * timer)
{
    (void)timer;
    ui.t += 0.05f;

    float val = 50.0f + 35.0f * sinf(ui.t * 1.2f) + 10.0f * sinf(ui.t * 0.3f);
    hmi_chart_push_value(ui.chart, (int32_t)val);
}

void create_ui(void)
{
    memset(&ui, 0, sizeof(ui));

    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);

    lv_obj_t * cont = lv_obj_create(scr);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(cont, 4, 0);
    lv_obj_set_style_pad_row(cont, 4, 0);

    lv_obj_t * header = lv_obj_create(cont);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, lv_pct(100), 44);
    lv_obj_set_flex_grow(header, 0);
    lv_obj_set_style_bg_color(header, COLOR_PANEL, 0);
    lv_obj_set_style_radius(header, 12, 0);
    lv_obj_set_style_pad_hor(header, 12, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * title = lv_label_create(header);
    lv_label_set_text(title, "STATISTICAL CHART");
    lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);

    lv_obj_t * title_info = lv_label_create(header);
    lv_label_set_text(title_info, "LIVE TREND");
    lv_obj_set_style_text_color(title_info, COLOR_DIM, 0);

    lv_obj_t * chart_panel = lv_obj_create(cont);
    lv_obj_remove_style_all(chart_panel);
    lv_obj_set_size(chart_panel, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_grow(chart_panel, 1);
    lv_obj_set_style_bg_color(chart_panel, COLOR_PANEL, 0);
    lv_obj_set_style_radius(chart_panel, 14, 0);
    lv_obj_set_style_pad_all(chart_panel, 6, 0);

    ui.chart = hmi_chart_create(chart_panel);
    lv_obj_set_size(hmi_chart_get_obj(ui.chart), lv_pct(100), lv_pct(100));

    lv_timer_create(ui_update_timer, 50, NULL);
}
