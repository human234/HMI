#include "hmi.h"
#include "common.h"
#include "gauge.h"
#include "card.h"
#include "button.h"
#include "others/observer/lv_observer.h"
#include "slider.h"

#include <stdio.h>
#include <string.h>

#define CHART_POINTS 80

typedef struct {
    lv_obj_t * tab_btn[3];
    lv_obj_t * tab_label[3];
    lv_obj_t * page_area;
    lv_obj_t * page_status;
    lv_obj_t * page_charts;
    lv_obj_t * page_events;
    lv_obj_t * event_list;
    int current_page;

    lv_obj_t * root_cont;

    hmi_gauge_t * gauge_v;
    hmi_gauge_t * gauge_a;
    hmi_gauge_t * gauge_hz;

    card_t * card_power;
    card_t * card_energy;
    card_t * card_temp;
    card_t * card_speed;

    hmi_btn_t * btn_start;
    hmi_btn_t * btn_stop;
    hmi_btn_t * btn_reset;
    lv_obj_t * btn_destroy;
    float destroy_phase;
    bool system_destroyed;

    lv_obj_t * chart;
    lv_chart_series_t * chart_val;
    lv_chart_series_t * chart_set;

    hmi_slider_t * slider_setpoint;
    hmi_slider_t * slider_limit;

    lv_obj_t * info_label;

    float t;
    float setpoint;
    float limit;
} hmi_data_t;

static hmi_data_t hmi;

static void switch_page(int page);
static void menu_reset_info(lv_timer_t * timer);

static void on_start_click(hmi_btn_t * btn, lv_event_t * e)
{
    (void)e;
    hmi_btn_set_state(btn, HMI_BTN_ACTIVE);
    hmi_btn_set_state(hmi.btn_stop, HMI_BTN_IDLE);
    lv_label_set_text(hmi.info_label, "SYSTEM RUNNING");
    lv_obj_set_style_text_color(hmi.info_label, COLOR_OK, 0);
}

static void on_stop_click(hmi_btn_t * btn, lv_event_t * e)
{
    (void)e;
    hmi_btn_set_state(btn, HMI_BTN_ACTIVE);
    hmi_btn_set_state(hmi.btn_start, HMI_BTN_IDLE);
    lv_label_set_text(hmi.info_label, "SYSTEM STOPPED");
    lv_obj_set_style_text_color(hmi.info_label, COLOR_WARN, 0);
}

static void on_reset_click(hmi_btn_t * btn, lv_event_t * e)
{
    (void)e;
    hmi_btn_set_state(btn, HMI_BTN_ACTIVE);
    hmi_btn_set_state(hmi.btn_start, HMI_BTN_IDLE);
    hmi_btn_set_state(hmi.btn_stop, HMI_BTN_IDLE);
    lv_label_set_text(hmi.info_label, "SYSTEM RESET");
    lv_obj_set_style_text_color(hmi.info_label, COLOR_ACCENT, 0);
}

static void on_tab_click_events(lv_event_t * e)
{
    (void)e;
    switch_page(2);
    lv_subject_t tmp;
    lv_subject_init_int(&tmp, 100);
}

static void menu_popup_close(lv_event_t * e)
{
    lv_obj_t * mbox = lv_event_get_user_data(e);
    if (mbox) lv_obj_delete(mbox);
}

static void menu_system_info(lv_event_t * e)
{
    lv_obj_t * mbox = lv_event_get_user_data(e);
    if (mbox) lv_obj_delete(mbox);
    lv_label_set_text(hmi.info_label, LV_SYMBOL_OK " System: v2.4.1 | Uptime: 127h 34m | Load: 67%");
    lv_obj_set_style_text_color(hmi.info_label, COLOR_ACCENT, 0);
}

static void menu_settings(lv_event_t * e)
{
    lv_obj_t * mbox = lv_event_get_user_data(e);
    if (mbox) lv_obj_delete(mbox);
    switch_page(1);
}

static void menu_diagnostics(lv_event_t * e)
{
    lv_obj_t * mbox = lv_event_get_user_data(e);
    if (mbox) lv_obj_delete(mbox);
    lv_label_set_text(hmi.info_label, LV_SYMBOL_WARNING " Diagnostics: MEM OK | TEMP 52\u00B0C | FAN 3400 RPM | VCC 5.01V");
    lv_obj_set_style_text_color(hmi.info_label, COLOR_WARN, 0);
    lv_timer_t * t = lv_timer_create(menu_reset_info, 3000, NULL);
    lv_timer_set_repeat_count(t, 1);
}

static void menu_help(lv_event_t * e)
{
    lv_obj_t * mbox = lv_event_get_user_data(e);
    if (mbox) lv_obj_delete(mbox);
    lv_label_set_text(hmi.info_label, LV_SYMBOL_BULLET " Help: START=run  STOP=halt  RESET=clear  DESTROY=nuke  |  Tab to switch pages");
    lv_obj_set_style_text_color(hmi.info_label, COLOR_DIM, 0);
    lv_timer_t * t = lv_timer_create(menu_reset_info, 4000, NULL);
    lv_timer_set_repeat_count(t, 1);
}

static void menu_reset_info(lv_timer_t * timer)
{
    (void)timer;
    lv_label_set_text(hmi.info_label, LV_SYMBOL_BULLET " READY");
    lv_obj_set_style_text_color(hmi.info_label, COLOR_DIM, 0);
}

static void on_menu_click(lv_event_t * e)
{
    (void)e;
    lv_obj_t * mbox = lv_msgbox_create(lv_screen_active());
    lv_obj_set_size(mbox, 300, 264);
    lv_obj_center(mbox);
    lv_obj_set_style_bg_color(mbox, COLOR_PANEL, 0);
    lv_obj_set_style_border_color(mbox, lv_color_hex(0x2A2A4A), 0);
    lv_obj_set_style_border_width(mbox, 1, 0);
    lv_obj_set_style_radius(mbox, 12, 0);
    lv_obj_set_style_pad_all(mbox, 8, 0);
    lv_obj_set_style_shadow_color(mbox, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_shadow_width(mbox, 20, 0);
    lv_obj_set_style_shadow_opa(mbox, LV_OPA_30, 0);

    lv_obj_t * title = lv_msgbox_add_title(mbox, "MENU");
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0);

    lv_obj_t * content = lv_msgbox_get_content(mbox);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(content, 4, 0);
    lv_obj_set_style_pad_row(content, 4, 0);

    static const char * items[] = { "System Info", "Settings", "Diagnostics", "Help", "Close" };
    lv_event_cb_t cbs[] = { menu_system_info, menu_settings, menu_diagnostics, menu_help, menu_popup_close };
    for (uint8_t i = 0; i < 5; i++) {
        lv_obj_t * btn = lv_btn_create(content);
        lv_obj_remove_style_all(btn);
        lv_obj_set_size(btn, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1A1A30), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x2A2A4A), LV_STATE_PRESSED);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_pad_all(btn, 10, 0);

        lv_obj_t * lbl = lv_label_create(btn);
        lv_label_set_text(lbl, items[i]);
        lv_obj_center(lbl);
        lv_obj_set_style_text_color(lbl, COLOR_DIM, 0);
        lv_obj_set_style_text_color(lbl, COLOR_TEXT, LV_STATE_PRESSED);
        if (i == 4) {
            lv_obj_set_style_text_color(lbl, COLOR_ERROR, 0);
        }

        lv_obj_add_event_cb(btn, cbs[i], LV_EVENT_CLICKED, mbox);
    }
}

static void on_setpoint_change(hmi_slider_t * slider, float value)
{
    (void)slider;
    hmi.setpoint = value;
}

static void on_limit_change(hmi_slider_t * slider, float value)
{
    (void)slider;
    hmi.limit = value;
}

static void switch_page(int page)
{
    if (page == hmi.current_page) return;

    if (page != 0) lv_obj_add_flag(hmi.page_status, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(hmi.page_status, LV_OBJ_FLAG_HIDDEN);
    if (page != 1) lv_obj_add_flag(hmi.page_charts, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(hmi.page_charts, LV_OBJ_FLAG_HIDDEN);
    if (page != 2) lv_obj_add_flag(hmi.page_events, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(hmi.page_events, LV_OBJ_FLAG_HIDDEN);

    if (page == 0) lv_obj_add_flag(hmi.btn_destroy, LV_OBJ_FLAG_CLICKABLE);
    else lv_obj_remove_flag(hmi.btn_destroy, LV_OBJ_FLAG_CLICKABLE);

    for (int i = 0; i < 3; i++) {
        if (i == page) {
            lv_obj_set_style_text_color(hmi.tab_label[i], COLOR_ACCENT, 0);
            lv_obj_set_style_bg_color(hmi.tab_btn[i], lv_color_darken(COLOR_PANEL, 24), 0);
            lv_obj_set_style_shadow_color(hmi.tab_btn[i], COLOR_ACCENT, 0);
            lv_obj_set_style_shadow_width(hmi.tab_btn[i], 8, 0);
            lv_obj_set_style_shadow_opa(hmi.tab_btn[i], LV_OPA_40, 0);
        } else {
            lv_obj_set_style_text_color(hmi.tab_label[i], COLOR_DIM, 0);
            lv_obj_set_style_bg_color(hmi.tab_btn[i], COLOR_PANEL, 0);
            lv_obj_set_style_shadow_width(hmi.tab_btn[i], 0, 0);
        }
    }

    hmi.current_page = page;
}

static void on_tab_click_status(lv_event_t * e)
{
    (void)e;
    switch_page(0);
}

static void on_tab_click_charts(lv_event_t * e)
{
    (void)e;
    switch_page(1);
}

static void hmi_update_timer(lv_timer_t * timer)
{
    (void)timer;
    hmi.t += 0.05f;

    float v = 400.0f + 60.0f * sinf(hmi.t * 0.7f);
    float a = 45.0f + 25.0f * sinf(hmi.t * 0.9f + 1.2f);
    float hz = 50.0f + 8.0f * sinf(hmi.t * 0.5f + 2.3f);

    hmi_gauge_set_value(hmi.gauge_v, v);
    hmi_gauge_set_value(hmi.gauge_a, a);
    hmi_gauge_set_value(hmi.gauge_hz, hz);

    card_set_value(hmi.card_power, v * a * 0.01f);
    card_push_sample(hmi.card_power, v * a * 0.01f);

    static float energy = 0;
    energy += v * a * 0.0001f;
    if (energy > 9999) energy = 0;
    card_set_value(hmi.card_energy, energy);

    float temp = 45.0f + 15.0f * sinf(hmi.t * 0.3f);
    card_set_value(hmi.card_temp, temp);
    card_push_sample(hmi.card_temp, temp);

    float speed = 1500.0f + 400.0f * sinf(hmi.t * 0.6f + 3.1f);
    card_set_value(hmi.card_speed, speed);
    card_push_sample(hmi.card_speed, speed);

    float chart_val = 50.0f + 35.0f * sinf(hmi.t * 1.2f) + 10.0f * sinf(hmi.t * 0.3f);
    lv_chart_set_next_value(hmi.chart, hmi.chart_val, (int32_t)chart_val);
    lv_chart_set_next_value(hmi.chart, hmi.chart_set, (int32_t)hmi.setpoint);
    lv_chart_refresh(hmi.chart);

    if (hmi.btn_destroy) {
        hmi.destroy_phase += 0.04f;
        if (hmi.destroy_phase > 1.0f) hmi.destroy_phase -= 1.0f;
        lv_obj_invalidate(hmi.btn_destroy);
    }
}

static lv_obj_t * create_tab_btn(lv_obj_t * parent, lv_event_cb_t cb)
{
    lv_obj_t * btn = lv_obj_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_flex_grow(btn, 1);
    lv_obj_set_height(btn, lv_pct(100));
    lv_obj_set_style_bg_color(btn, COLOR_PANEL, 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    return btn;
}

#define COLOR_BLOOD    lv_color_hex(0x6B0000)
#define COLOR_RED_BOLD lv_color_hex(0xFF0033)
#define COLOR_HAZARD   lv_color_hex(0xFFAA00)

static void destroy_btn_draw(lv_event_t * e)
{
    lv_layer_t * layer = lv_event_get_layer(e);
    lv_obj_t * obj = lv_event_get_target(e);

    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    int32_t h = lv_area_get_height(&coords);
    int32_t cy = coords.y1 + h / 2;

    /* 1. Blood-red background with bright red border */
    lv_draw_rect_dsc_t bg;
    lv_draw_rect_dsc_init(&bg);
    bg.bg_color     = COLOR_BLOOD;
    bg.radius       = 20;
    bg.border_color = COLOR_RED_BOLD;
    bg.border_width = 3;
    bg.border_opa   = LV_OPA_80;
    lv_draw_rect(layer, &bg, &coords);

    /* 2. Inner hazard-yellow border ring */
    lv_area_t inner = { coords.x1 + 7, coords.y1 + 7, coords.x2 - 7, coords.y2 - 7 };
    lv_draw_rect_dsc_t ib;
    lv_draw_rect_dsc_init(&ib);
    ib.radius       = 17;
    ib.border_color = COLOR_HAZARD;
    ib.border_width = 1;
    ib.border_opa   = LV_OPA_50;
    lv_draw_rect(layer, &ib, &inner);

    /* 3. Hazard stripe bands at top and bottom */
    int32_t stripe_h = 4;
    for (int32_t x = coords.x1 + 6; x < coords.x2 - 6; x += 12) {
        lv_area_t s = { x, coords.y1 + 3, x + 6, coords.y1 + 3 + stripe_h };
        lv_draw_rect_dsc_t st;
        lv_draw_rect_dsc_init(&st);
        st.bg_color = COLOR_HAZARD;
        st.radius   = 0;
        lv_draw_rect(layer, &st, &s);
        s.y1 = coords.y2 - 3 - stripe_h;
        s.y2 = coords.y2 - 3;
        lv_draw_rect(layer, &st, &s);
    }

    /* 4. Pulsing red LED on the right */
    float brightness = hmi_pulse_brightness(hmi.destroy_phase);
    lv_opa_t led_opa = 80 + (lv_opa_t)(brightness * 120);

    lv_draw_arc_dsc_t led;
    lv_draw_arc_dsc_init(&led);
    led.color       = COLOR_RED_BOLD;
    led.width       = 6;
    led.rounded     = true;
    led.center.x    = coords.x2 - 22;
    led.center.y    = cy;
    led.radius      = 4;
    led.start_angle = 0;
    led.end_angle   = 360;
    led.opa         = led_opa;
    lv_draw_arc(layer, &led);

    /* 5. Pulsing outer glow ring behind the button */
    lv_draw_rect_dsc_t glow;
    lv_draw_rect_dsc_init(&glow);
    glow.bg_color     = COLOR_RED_BOLD;
    glow.radius       = 24;
    glow.bg_opa       = LV_OPA_40 + (lv_opa_t)(brightness * 30);
    lv_area_t glow_a  = coords;
    lv_area_increase(&glow_a, 4, 4);
    lv_draw_rect(layer, &glow, &glow_a);

    /* 6. Warning text */
    lv_draw_label_dsc_t label;
    lv_draw_label_dsc_init(&label);
    label.text  = LV_SYMBOL_WARNING LV_SYMBOL_TRASH " DESTROY " LV_SYMBOL_TRASH LV_SYMBOL_WARNING;
    label.color = lv_color_hex(0xFFFFFF);
    lv_area_t la = { coords.x1 + 8, cy - 10, coords.x2 - 22 - 8, cy + 10 };
    lv_draw_label(layer, &label, &la);
}

static void on_destroy_click(lv_event_t * e)
{
    (void)e;
    if (hmi.system_destroyed) return;
    lv_obj_t * obj = lv_event_get_target(e);
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_has_flag(lv_obj_get_parent(obj), LV_OBJ_FLAG_HIDDEN)) return;

    hmi.system_destroyed = true;

    lv_obj_add_flag(hmi.root_cont, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COLOR_BLOOD, 0);

    lv_obj_t * msg = lv_label_create(scr);
    lv_label_set_text(msg, LV_SYMBOL_WARNING " SYSTEM DESTRUCT " LV_SYMBOL_WARNING);
    lv_obj_set_style_text_color(msg, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(msg, LV_ALIGN_CENTER, 0, 0);
}

static void on_destroy_press(lv_event_t * e)
{
    (void)e;
    lv_obj_t * obj = lv_event_get_target(e);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x3A0000), 0);
}

static void on_destroy_release(lv_event_t * e)
{
    (void)e;
    lv_obj_t * obj = lv_event_get_target(e);
    lv_obj_set_style_bg_color(obj, COLOR_BLOOD, 0);
}

static void create_page_status(lv_obj_t * parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent, 0, 0);

    /*=============================================
     * Row 2: Gauges (flex-grow: takes remaining space)
     *=============================================*/
    lv_obj_t * gauge_row = lv_obj_create(parent);
    lv_obj_remove_style_all(gauge_row);
    lv_obj_set_size(gauge_row, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_grow(gauge_row, 1);
    lv_obj_set_flex_flow(gauge_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(gauge_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    hmi.gauge_v = hmi_gauge_create(gauge_row);
    hmi_gauge_set_style(hmi.gauge_v, HMI_GAUGE_STYLE_CYAN);
    hmi_gauge_set_title(hmi.gauge_v, "VOLTAGE");
    hmi_gauge_set_unit(hmi.gauge_v, "V");
    hmi_gauge_set_range(hmi.gauge_v, 250.0f, 520.0f);
    hmi_gauge_set_precision(hmi.gauge_v, 1);
    hmi_gauge_set_tick_count(hmi.gauge_v, 9);
    hmi_gauge_set_peak_enable(hmi.gauge_v, true);
    hmi_gauge_add_zone(hmi.gauge_v, 250, 320, COLOR_ERROR);
    hmi_gauge_add_zone(hmi.gauge_v, 320, 380, COLOR_WARN);
    hmi_gauge_add_zone(hmi.gauge_v, 380, 480, COLOR_OK);
    hmi_gauge_add_zone(hmi.gauge_v, 480, 520, COLOR_ERROR);

    hmi.gauge_a = hmi_gauge_create(gauge_row);
    hmi_gauge_set_style(hmi.gauge_a, HMI_GAUGE_STYLE_PINK);
    hmi_gauge_set_title(hmi.gauge_a, "CURRENT");
    hmi_gauge_set_unit(hmi.gauge_a, "A");
    hmi_gauge_set_range(hmi.gauge_a, 0.0f, 100.0f);
    hmi_gauge_set_precision(hmi.gauge_a, 1);
    hmi_gauge_set_tick_count(hmi.gauge_a, 10);
    hmi_gauge_set_peak_enable(hmi.gauge_a, true);
    hmi_gauge_add_zone(hmi.gauge_a, 0, 30, COLOR_OK);
    hmi_gauge_add_zone(hmi.gauge_a, 30, 60, COLOR_WARN);
    hmi_gauge_add_zone(hmi.gauge_a, 60, 100, COLOR_ERROR);

    hmi.gauge_hz = hmi_gauge_create(gauge_row);
    hmi_gauge_set_style(hmi.gauge_hz, HMI_GAUGE_STYLE_AMBER);
    hmi_gauge_set_title(hmi.gauge_hz, "FREQUENCY");
    hmi_gauge_set_unit(hmi.gauge_hz, "Hz");
    hmi_gauge_set_range(hmi.gauge_hz, 0.0f, 100.0f);
    hmi_gauge_set_precision(hmi.gauge_hz, 2);
    hmi_gauge_set_tick_count(hmi.gauge_hz, 10);
    hmi_gauge_add_zone(hmi.gauge_hz, 0, 45, COLOR_ERROR);
    hmi_gauge_add_zone(hmi.gauge_hz, 45, 55, COLOR_OK);
    hmi_gauge_add_zone(hmi.gauge_hz, 55, 100, COLOR_ERROR);

    lv_obj_t * gobj_v = hmi_gauge_get_obj(hmi.gauge_v);
    lv_obj_t * gobj_a = hmi_gauge_get_obj(hmi.gauge_a);
    lv_obj_t * gobj_hz = hmi_gauge_get_obj(hmi.gauge_hz);
    lv_obj_set_height(gobj_v, lv_pct(100));
    lv_obj_set_width(gobj_v, lv_pct(30));
    lv_obj_set_height(gobj_a, lv_pct(100));
    lv_obj_set_width(gobj_a, lv_pct(30));
    lv_obj_set_height(gobj_hz, lv_pct(100));
    lv_obj_set_width(gobj_hz, lv_pct(30));

    /*=============================================
     * Row 4: Value cards (fixed height)
     *=============================================*/
    lv_obj_t * card_row = lv_obj_create(parent);
    lv_obj_remove_style_all(card_row);
    lv_obj_set_size(card_row, lv_pct(100), 96);
    lv_obj_set_flex_grow(card_row, 0);
    lv_obj_set_flex_flow(card_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    hmi.card_power = card_create(card_row);
    card_set_unit(hmi.card_power, "kW");
    card_set_range(hmi.card_power, 0, 999);
    card_set_precision(hmi.card_power, 1);
    card_set_state(hmi.card_power, IVC2_STATE_RUNNING);
    card_enable_sparkline(hmi.card_power, true);
    lv_obj_set_size(card_get_object(hmi.card_power), lv_pct(22), lv_pct(100));
    lv_obj_t * child = lv_obj_get_child(card_get_object(hmi.card_power), 0);
    if (child) lv_label_set_text(child, "POWER");

    hmi.card_energy = card_create(card_row);
    card_set_unit(hmi.card_energy, "kWh");
    card_set_range(hmi.card_energy, 0, 9999);
    card_set_precision(hmi.card_energy, 1);
    card_set_state(hmi.card_energy, IVC2_STATE_RUNNING);
    lv_obj_set_size(card_get_object(hmi.card_energy), lv_pct(22), lv_pct(100));
    child = lv_obj_get_child(card_get_object(hmi.card_energy), 0);
    if (child) lv_label_set_text(child, "ENERGY");

    hmi.card_temp = card_create(card_row);
    card_set_unit(hmi.card_temp, "\u00B0C");
    card_set_range(hmi.card_temp, 0, 120);
    card_set_alarm_limit(hmi.card_temp, 20, 80);
    card_set_precision(hmi.card_temp, 1);
    card_set_state(hmi.card_temp, IVC2_STATE_RUNNING);
    card_enable_sparkline(hmi.card_temp, true);
    lv_obj_set_size(card_get_object(hmi.card_temp), lv_pct(22), lv_pct(100));
    child = lv_obj_get_child(card_get_object(hmi.card_temp), 0);
    if (child) lv_label_set_text(child, "TEMP");

    hmi.card_speed = card_create(card_row);
    card_set_unit(hmi.card_speed, "RPM");
    card_set_range(hmi.card_speed, 0, 3000);
    card_set_precision(hmi.card_speed, 0);
    card_set_state(hmi.card_speed, IVC2_STATE_RUNNING);
    card_enable_sparkline(hmi.card_speed, true);
    lv_obj_set_size(card_get_object(hmi.card_speed), lv_pct(22), lv_pct(100));
    child = lv_obj_get_child(card_get_object(hmi.card_speed), 0);
    if (child) lv_label_set_text(child, "SPEED");

    /*=============================================
     * Row 6: Control buttons (fixed height)
     *=============================================*/
    lv_obj_t * btn_row = lv_obj_create(parent);
    lv_obj_remove_style_all(btn_row);
    lv_obj_set_size(btn_row, lv_pct(100), 48);
    lv_obj_set_flex_grow(btn_row, 0);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    hmi.btn_start = hmi_btn_create(btn_row);
    hmi_btn_set_text(hmi.btn_start, "START");
    hmi_btn_set_led_enable(hmi.btn_start, true);
    hmi_btn_set_led_color(hmi.btn_start, COLOR_OK);
    hmi_btn_set_pulse_enable(hmi.btn_start, true);
    hmi_btn_on_click(hmi.btn_start, on_start_click);
    lv_obj_set_size(hmi_btn_get_obj(hmi.btn_start), lv_pct(22), lv_pct(90));

    hmi.btn_stop = hmi_btn_create(btn_row);
    hmi_btn_set_text(hmi.btn_stop, "STOP");
    hmi_btn_set_led_enable(hmi.btn_stop, true);
    hmi_btn_set_led_color(hmi.btn_stop, COLOR_ERROR);
    hmi_btn_on_click(hmi.btn_stop, on_stop_click);
    lv_obj_set_size(hmi_btn_get_obj(hmi.btn_stop), lv_pct(22), lv_pct(90));

    hmi.btn_reset = hmi_btn_create(btn_row);
    hmi_btn_set_text(hmi.btn_reset, "RESET");
    hmi_btn_set_led_enable(hmi.btn_reset, true);
    hmi_btn_set_led_color(hmi.btn_reset, COLOR_WARN);
    hmi_btn_on_click(hmi.btn_reset, on_reset_click);
    lv_obj_set_size(hmi_btn_get_obj(hmi.btn_reset), lv_pct(22), lv_pct(90));

    hmi.btn_destroy = lv_obj_create(btn_row);
    lv_obj_remove_style_all(hmi.btn_destroy);
    lv_obj_set_size(hmi.btn_destroy, lv_pct(22), lv_pct(90));

    lv_obj_add_event_cb(hmi.btn_destroy, destroy_btn_draw, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_event_cb(hmi.btn_destroy, on_destroy_press, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(hmi.btn_destroy, on_destroy_release, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(hmi.btn_destroy, on_destroy_click, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(hmi.btn_destroy, LV_OBJ_FLAG_CLICKABLE);

}

static void chart_draw_ticks(lv_event_t * e)
{
    lv_obj_t * chart = lv_event_get_target(e);
    lv_layer_t * layer = lv_event_get_layer(e);
    if (layer == NULL) return;

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

    lv_color_t tick_color = COLOR_DIM;

    int32_t hdiv = 5;
    for (int32_t i = 0; i <= hdiv; i++) {
        int32_t y = chart_y + (h * i) / hdiv;

        lv_draw_line_dsc_t tick;
        lv_draw_line_dsc_init(&tick);
        tick.color = tick_color;
        tick.width = 1;
        tick.p1 = (lv_point_precise_t){ .x = chart_x - 6, .y = y };
        tick.p2 = (lv_point_precise_t){ .x = chart_x - 1, .y = y };
        lv_draw_line(layer, &tick);

        int32_t val = 100 - (100 * i) / hdiv;
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", val);

        lv_draw_label_dsc_t lbl;
        lv_draw_label_dsc_init(&lbl);
        lbl.text = buf;
        lbl.color = tick_color;
        lv_area_t la = { chart_x - 30, y - 7, chart_x - 8, y + 7 };
        lv_draw_label(layer, &lbl, &la);
    }

    int32_t vdiv = 8;
    for (int32_t i = 0; i <= vdiv; i++) {
        int32_t x = chart_x + (w * i) / vdiv;

        lv_draw_line_dsc_t tick;
        lv_draw_line_dsc_init(&tick);
        tick.color = tick_color;
        tick.width = 1;
        tick.p1 = (lv_point_precise_t){ .x = x, .y = chart_y + h };
        tick.p2 = (lv_point_precise_t){ .x = x, .y = chart_y + h + 5 };
        lv_draw_line(layer, &tick);
    }
}

static void create_page_charts(lv_obj_t * parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent, 0, 0);

    lv_obj_t * title_row = lv_obj_create(parent);
    lv_obj_remove_style_all(title_row);
    lv_obj_set_size(title_row, lv_pct(100), 24);
    lv_obj_set_flex_grow(title_row, 0);
    lv_obj_set_style_pad_hor(title_row, 6, 0);
    lv_obj_set_flex_flow(title_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * title_lbl = lv_label_create(title_row);
    lv_label_set_text(title_lbl, "STATISTICAL CHART");
    lv_obj_set_style_text_color(title_lbl, COLOR_ACCENT, 0);

    lv_obj_t * title_info = lv_label_create(title_row);
    lv_label_set_text(title_info, "LIVE TREND");
    lv_obj_set_style_text_color(title_info, COLOR_DIM, 0);

    /*=============================================
     * Chart - takes remaining space
     *=============================================*/
    lv_obj_t * chart_panel = lv_obj_create(parent);
    lv_obj_remove_style_all(chart_panel);
    lv_obj_set_size(chart_panel, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_grow(chart_panel, 1);
    lv_obj_set_style_bg_color(chart_panel, COLOR_PANEL, 0);
    lv_obj_set_style_radius(chart_panel, 14, 0);
    lv_obj_set_style_pad_all(chart_panel, 6, 0);

    hmi.chart = lv_chart_create(chart_panel);
    lv_obj_set_size(hmi.chart, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(hmi.chart, COLOR_PANEL, 0);
    lv_obj_set_style_border_width(hmi.chart, 0, 0);
    lv_obj_set_style_pad_left(hmi.chart, 34, 0);
    lv_obj_set_style_pad_right(hmi.chart, 2, 0);
    lv_obj_set_style_pad_top(hmi.chart, 2, 0);
    lv_obj_set_style_pad_bottom(hmi.chart, 2, 0);
    lv_obj_set_style_radius(hmi.chart, 10, 0);

    lv_chart_set_type(hmi.chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(hmi.chart, CHART_POINTS);
    lv_chart_set_range(hmi.chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_update_mode(hmi.chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_div_line_count(hmi.chart, 5, 8);

    lv_obj_set_style_line_color(hmi.chart, lv_color_hex(0x2A2A4A), LV_PART_MAIN);
    lv_obj_set_style_line_width(hmi.chart, 1, LV_PART_MAIN);
    lv_obj_set_style_line_opa(hmi.chart, LV_OPA_50, LV_PART_MAIN);

    hmi.chart_val = lv_chart_add_series(hmi.chart, COLOR_ACCENT, LV_CHART_AXIS_PRIMARY_Y);
    hmi.chart_set = lv_chart_add_series(hmi.chart, lv_color_hex(0xFF00AA), LV_CHART_AXIS_PRIMARY_Y);

    lv_chart_set_all_values(hmi.chart, hmi.chart_val, LV_CHART_POINT_NONE);
    lv_chart_set_all_values(hmi.chart, hmi.chart_set, LV_CHART_POINT_NONE);

    lv_obj_set_style_line_width(hmi.chart, 2, LV_PART_ITEMS);

    lv_obj_add_event_cb(hmi.chart, chart_draw_ticks, LV_EVENT_DRAW_POST, NULL);

    /*=============================================
     * Sliders row (fixed height)
     *=============================================*/
    lv_obj_t * slider_row = lv_obj_create(parent);
    lv_obj_remove_style_all(slider_row);
    lv_obj_set_size(slider_row, lv_pct(100), 60);
    lv_obj_set_flex_grow(slider_row, 0);
    lv_obj_set_style_pad_column(slider_row, 6, 0);
    lv_obj_set_flex_flow(slider_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(slider_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    hmi.slider_setpoint = hmi_slider_create(slider_row);
    hmi_slider_set_title(hmi.slider_setpoint, "SETPOINT");
    hmi_slider_set_unit(hmi.slider_setpoint, "");
    hmi_slider_set_range(hmi.slider_setpoint, 0.0f, 100.0f);
    hmi_slider_set_precision(hmi.slider_setpoint, 0);
    hmi_slider_set_color(hmi.slider_setpoint, COLOR_ACCENT);
    hmi_slider_set_value(hmi.slider_setpoint, 50.0f);
    hmi_slider_on_change(hmi.slider_setpoint, on_setpoint_change);
    lv_obj_set_size(hmi_slider_get_obj(hmi.slider_setpoint), lv_pct(46), lv_pct(100));

    hmi.slider_limit = hmi_slider_create(slider_row);
    hmi_slider_set_title(hmi.slider_limit, "LIMIT");
    hmi_slider_set_unit(hmi.slider_limit, "");
    hmi_slider_set_range(hmi.slider_limit, 0.0f, 100.0f);
    hmi_slider_set_precision(hmi.slider_limit, 0);
    hmi_slider_set_color(hmi.slider_limit, lv_color_hex(0xFF00AA));
    hmi_slider_set_value(hmi.slider_limit, 80.0f);
    hmi_slider_on_change(hmi.slider_limit, on_limit_change);
    lv_obj_set_size(hmi_slider_get_obj(hmi.slider_limit), lv_pct(46), lv_pct(100));

    hmi.setpoint = 50.0f;
    hmi.limit = 80.0f;
}

static void create_page_events(lv_obj_t * parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent, 0, 0);

    lv_obj_t * title_row = lv_obj_create(parent);
    lv_obj_remove_style_all(title_row);
    lv_obj_set_size(title_row, lv_pct(100), 24);
    lv_obj_set_flex_grow(title_row, 0);
    lv_obj_set_style_pad_hor(title_row, 6, 0);
    lv_obj_set_flex_flow(title_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * title_lbl = lv_label_create(title_row);
    lv_label_set_text(title_lbl, "EVENT LOG");
    lv_obj_set_style_text_color(title_lbl, COLOR_TEXT, 0);

    lv_obj_t * clear_btn = lv_obj_create(title_row);
    lv_obj_remove_style_all(clear_btn);
    lv_obj_set_size(clear_btn, 40, 22);
    lv_obj_set_style_bg_color(clear_btn, lv_color_hex(0x1A1A30), 0);
    lv_obj_set_style_bg_color(clear_btn, lv_color_hex(0xFF0055), LV_STATE_PRESSED);
    lv_obj_set_style_radius(clear_btn, 4, 0);
    lv_obj_add_flag(clear_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t * clear_lbl = lv_label_create(clear_btn);
    lv_label_set_text(clear_lbl, "CLEAR");
    lv_obj_set_style_text_color(clear_lbl, COLOR_ACCENT, 0);
    lv_obj_center(clear_lbl);

    lv_obj_t * panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_grow(panel, 1);
    lv_obj_set_style_bg_color(panel, COLOR_PANEL, 0);
    lv_obj_set_style_radius(panel, 14, 0);
    lv_obj_set_style_pad_all(panel, 4, 0);

    hmi.event_list = lv_list_create(panel);
    lv_obj_set_size(hmi.event_list, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(hmi.event_list, COLOR_PANEL, 0);
    lv_obj_set_style_border_width(hmi.event_list, 0, 0);
    lv_obj_set_style_radius(hmi.event_list, 10, 0);
    lv_obj_set_style_pad_all(hmi.event_list, 2, 0);

    const char * events[][3] = {
        { "10:23:15", LV_SYMBOL_OK, "System started - all OK" },
        { "10:23:17", LV_SYMBOL_OK, "Grid sync acquired" },
        { "10:24:01", LV_SYMBOL_OK, "PWM output enabled" },
        { "10:25:30", LV_SYMBOL_WARNING, "Temperature warning: 78\u00B0C" },
        { "10:26:12", LV_SYMBOL_OK, "Fan speed increased" },
        { "10:27:45", LV_SYMBOL_WARNING, "Overcurrent trip at 92A" },
        { "10:28:00", LV_SYMBOL_OK, "Auto-recovery initiated" },
        { "10:28:30", LV_SYMBOL_OK, "System nominal again" },
        { "10:29:15", LV_SYMBOL_WARNING, "Voltage sag detected: 310V" },
        { "10:30:00", LV_SYMBOL_OK, "Caps recharged" },
    };
    lv_color_t event_colors[] = { COLOR_OK, COLOR_OK, COLOR_OK, COLOR_WARN, COLOR_OK, COLOR_ERROR, COLOR_OK, COLOR_OK, COLOR_WARN, COLOR_OK };

    for (uint8_t i = 0; i < 10; i++) {
        lv_obj_t * row = lv_obj_create(hmi.event_list);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, lv_pct(100), 22);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row, 4, 0);
        lv_obj_set_style_pad_hor(row, 4, 0);
        lv_obj_set_style_bg_color(row, (i % 2) ? lv_color_hex(0x0F0F20) : COLOR_PANEL, 0);
        lv_obj_set_style_radius(row, 2, 0);

        lv_obj_t * icon = lv_label_create(row);
        lv_label_set_text(icon, events[i][1]);
        lv_obj_set_style_text_color(icon, event_colors[i], 0);

        lv_obj_t * time_lbl = lv_label_create(row);
        lv_label_set_text(time_lbl, events[i][0]);
        lv_obj_set_style_text_color(time_lbl, COLOR_DIM, 0);

        lv_obj_t * desc = lv_label_create(row);
        lv_label_set_text(desc, events[i][2]);
        lv_obj_set_style_text_color(desc, COLOR_TEXT, 0);
    }
}

void hmi_create_complex(lv_obj_t * parent)
{
    memset(&hmi, 0, sizeof(hmi));
    hmi.current_page = -1;
    lv_obj_set_style_bg_color(parent, COLOR_BG, 0);

    /*=============================================
     * Outer container: column flex, fills screen
     *=============================================*/
    lv_obj_t * cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(cont, 4, 0);
    lv_obj_set_style_pad_row(cont, 4, 0);
    hmi.root_cont = cont;

    /*=============================================
     * Row 0: Header bar (fixed height)
     *=============================================*/
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
    lv_label_set_text(title, LV_SYMBOL_CHARGE "  POWER CONTROL SYSTEM");
    lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);

    lv_obj_t * header_right = lv_obj_create(header);
    lv_obj_set_style_bg_opa(header_right, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header_right, 0, 0);
    lv_obj_set_style_pad_all(header_right, 0, 0);
    lv_obj_remove_flag(header_right, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(header_right, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(header_right, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header_right, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(header_right, 10, 0);

    lv_obj_t * status_hdr = lv_label_create(header_right);
    lv_label_set_text(status_hdr, LV_SYMBOL_BULLET "  OPERATIONAL");
    lv_obj_set_style_text_color(status_hdr, COLOR_OK, 0);

    lv_obj_t * menu_btn = lv_obj_create(header_right);
    lv_obj_remove_style_all(menu_btn);
    lv_obj_set_size(menu_btn, 28, 28);
    lv_obj_set_style_bg_color(menu_btn, COLOR_DIM, 0);
    lv_obj_set_style_bg_color(menu_btn, COLOR_PANEL, LV_STATE_PRESSED);
    lv_obj_set_style_radius(menu_btn, 6, 0);
    lv_obj_add_flag(menu_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(menu_btn, on_menu_click, LV_EVENT_CLICKED, NULL);
    lv_obj_t * menu_icon = lv_label_create(menu_btn);
    lv_label_set_text(menu_icon, LV_SYMBOL_BARS);
    lv_obj_set_style_text_color(menu_icon, COLOR_DIM, 0);
    lv_obj_center(menu_icon);

    /*=============================================
     * Row 2: Tab navigation (fixed height)
     *=============================================*/
    lv_obj_t * tab_row = lv_obj_create(cont);
    lv_obj_remove_style_all(tab_row);
    lv_obj_set_size(tab_row, lv_pct(100), 32);
    lv_obj_set_flex_grow(tab_row, 0);
    lv_obj_set_style_pad_column(tab_row, 4, 0);
    lv_obj_set_flex_flow(tab_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tab_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (int i = 0; i < 3; i++) {
        const char * names[] = { "STATUS", "CHARTS", "EVENTS" };
        lv_event_cb_t cbs[] = { on_tab_click_status, on_tab_click_charts, on_tab_click_events };
        hmi.tab_btn[i] = create_tab_btn(tab_row, cbs[i]);
        hmi.tab_label[i] = lv_label_create(hmi.tab_btn[i]);
        lv_label_set_text(hmi.tab_label[i], names[i]);
        lv_obj_set_style_text_color(hmi.tab_label[i], COLOR_TEXT, 0);
        lv_obj_center(hmi.tab_label[i]);
    }

    /*=============================================
     * Row 4: Page area (flex-grow)
     *=============================================*/
    hmi.page_area = lv_obj_create(cont);
    lv_obj_remove_style_all(hmi.page_area);
    lv_obj_set_size(hmi.page_area, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_grow(hmi.page_area, 1);

    hmi.page_status = lv_obj_create(hmi.page_area);
    lv_obj_remove_style_all(hmi.page_status);
    lv_obj_set_size(hmi.page_status, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(hmi.page_status, 0, 0);
    lv_obj_set_style_pad_row(hmi.page_status, 4, 0);
    create_page_status(hmi.page_status);

    hmi.page_charts = lv_obj_create(hmi.page_area);
    lv_obj_remove_style_all(hmi.page_charts);
    lv_obj_set_size(hmi.page_charts, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(hmi.page_charts, 0, 0);
    lv_obj_set_style_pad_row(hmi.page_charts, 4, 0);
    create_page_charts(hmi.page_charts);

    hmi.page_events = lv_obj_create(hmi.page_area);
    lv_obj_remove_style_all(hmi.page_events);
    lv_obj_set_size(hmi.page_events, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(hmi.page_events, 0, 0);
    create_page_events(hmi.page_events);

    /*=============================================
     * Row 6: Info label (fixed height)
     *=============================================*/
    hmi.info_label = lv_label_create(cont);
    lv_obj_set_size(hmi.info_label, lv_pct(100), 18);
    lv_obj_set_flex_grow(hmi.info_label, 0);
    lv_label_set_text(hmi.info_label, "READY");
    lv_obj_set_style_text_color(hmi.info_label, COLOR_DIM, 0);
    lv_obj_set_style_text_align(hmi.info_label, LV_TEXT_ALIGN_CENTER, 0);

    switch_page(0);

    lv_timer_create(hmi_update_timer, 50, NULL);
}

//============================================================================
// Simplified HMI subset – 3 pages with tabs
//============================================================================

typedef struct {
    lv_obj_t * tab_btn[3];
    lv_obj_t * tab_label[3];
    lv_obj_t * page_area;
    lv_obj_t * page_buttons;
    lv_obj_t * page_chart;
    lv_obj_t * page_gauge;
    int current_page;

    hmi_btn_t * btn_start;
    hmi_btn_t * btn_stop;
    hmi_btn_t * btn_reset;
    lv_obj_t * btn_destroy;
    float destroy_phase;
    bool system_destroyed;

    lv_obj_t * chart;
    lv_chart_series_t * chart_series;

    hmi_gauge_t * gauge;

    float t;
} hmi_subset_t;

static hmi_subset_t hmi_sub;

static void subset_on_start_click(hmi_btn_t * btn, lv_event_t * e)
{
    (void)e;
    hmi_btn_set_state(btn, HMI_BTN_ACTIVE);
    hmi_btn_set_state(hmi_sub.btn_stop, HMI_BTN_IDLE);
    hmi_btn_set_state(hmi_sub.btn_reset, HMI_BTN_IDLE);

    static size_t count = 0;
    count += 1;
}

static void subset_on_stop_click(hmi_btn_t * btn, lv_event_t * e)
{
    (void)e;
    hmi_btn_set_state(btn, HMI_BTN_ACTIVE);
    hmi_btn_set_state(hmi_sub.btn_start, HMI_BTN_IDLE);
    hmi_btn_set_state(hmi_sub.btn_reset, HMI_BTN_IDLE);

    static size_t count = 0;
    count += 1;
}

static void subset_on_reset_click(hmi_btn_t * btn, lv_event_t * e)
{
    (void)e;
    hmi_btn_set_state(btn, HMI_BTN_ACTIVE);
    hmi_btn_set_state(hmi_sub.btn_start, HMI_BTN_IDLE);
    hmi_btn_set_state(hmi_sub.btn_stop, HMI_BTN_IDLE);
}

static void subset_destroy_draw(lv_event_t * e)
{
    lv_layer_t * layer = lv_event_get_layer(e);
    lv_obj_t * obj = lv_event_get_target(e);
    if (layer == NULL || obj == NULL) return;

    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    int32_t h = lv_area_get_height(&coords);
    int32_t cy = coords.y1 + h / 2;

    lv_draw_rect_dsc_t bg;
    lv_draw_rect_dsc_init(&bg);
    bg.bg_color     = lv_color_hex(0x6B0000);
    bg.radius       = 16;
    bg.border_color = lv_color_hex(0xFF0033);
    bg.border_width = 3;
    bg.border_opa   = LV_OPA_80;
    lv_draw_rect(layer, &bg, &coords);

    lv_area_t inner = { coords.x1 + 6, coords.y1 + 6, coords.x2 - 6, coords.y2 - 6 };
    lv_draw_rect_dsc_t ib;
    lv_draw_rect_dsc_init(&ib);
    ib.radius       = 13;
    ib.border_color = lv_color_hex(0xFFAA00);
    ib.border_width = 1;
    ib.border_opa   = LV_OPA_50;
    lv_draw_rect(layer, &ib, &inner);

    float brightness = hmi_pulse_brightness(hmi_sub.destroy_phase);
    lv_opa_t led_opa = 80 + (lv_opa_t)(brightness * 120);

    lv_draw_arc_dsc_t led;
    lv_draw_arc_dsc_init(&led);
    led.color       = lv_color_hex(0xFF0033);
    led.width       = 5;
    led.rounded     = true;
    led.center.x    = coords.x2 - 18;
    led.center.y    = cy;
    led.radius      = 3;
    led.start_angle = 0;
    led.end_angle   = 360;
    led.opa         = led_opa;
    lv_draw_arc(layer, &led);

    lv_draw_rect_dsc_t glow;
    lv_draw_rect_dsc_init(&glow);
    glow.bg_color     = lv_color_hex(0xFF0033);
    glow.radius       = 20;
    glow.bg_opa       = LV_OPA_40 + (lv_opa_t)(brightness * 30);
    lv_area_t glow_a  = coords;
    lv_area_increase(&glow_a, 3, 3);
    lv_draw_rect(layer, &glow, &glow_a);

    lv_draw_label_dsc_t label;
    lv_draw_label_dsc_init(&label);
    label.text  = LV_SYMBOL_WARNING LV_SYMBOL_TRASH " DESTROY " LV_SYMBOL_TRASH LV_SYMBOL_WARNING;
    label.color = lv_color_hex(0xFFFFFF);
    lv_area_t la = { coords.x1 + 6, cy - 8, coords.x2 - 18 - 6, cy + 8 };
    lv_draw_label(layer, &label, &la);
}

static void subset_on_destroy_click(lv_event_t * e)
{
    (void)e;
    if (hmi_sub.system_destroyed) return;
    hmi_sub.system_destroyed = true;
    lv_obj_add_flag(hmi_sub.page_area, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x6B0000), 0);
    lv_obj_t * msg = lv_label_create(scr);
    lv_label_set_text(msg, LV_SYMBOL_WARNING " DESTRUCT " LV_SYMBOL_WARNING);
    lv_obj_set_style_text_color(msg, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(msg, LV_ALIGN_CENTER, 0, 0);
}

static void subset_on_destroy_press(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target(e);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x3A0000), 0);
}

static void subset_on_destroy_release(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target(e);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x6B0000), 0);
}

static void subset_switch_page(int page)
{
    if (page == hmi_sub.current_page) return;

    if (page != 0) lv_obj_add_flag(hmi_sub.page_buttons, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(hmi_sub.page_buttons, LV_OBJ_FLAG_HIDDEN);
    if (page != 1) lv_obj_add_flag(hmi_sub.page_chart, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(hmi_sub.page_chart, LV_OBJ_FLAG_HIDDEN);
    if (page != 2) lv_obj_add_flag(hmi_sub.page_gauge, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(hmi_sub.page_gauge, LV_OBJ_FLAG_HIDDEN);

    if (page == 0) lv_obj_add_flag(hmi_sub.btn_destroy, LV_OBJ_FLAG_CLICKABLE);
    else lv_obj_remove_flag(hmi_sub.btn_destroy, LV_OBJ_FLAG_CLICKABLE);

    for (int i = 0; i < 3; i++) {
        if (i == page) {
            lv_obj_set_style_text_color(hmi_sub.tab_label[i], COLOR_ACCENT, 0);
            lv_obj_set_style_bg_color(hmi_sub.tab_btn[i], lv_color_darken(COLOR_PANEL, 24), 0);
            lv_obj_set_style_shadow_color(hmi_sub.tab_btn[i], COLOR_ACCENT, 0);
            lv_obj_set_style_shadow_width(hmi_sub.tab_btn[i], 8, 0);
            lv_obj_set_style_shadow_opa(hmi_sub.tab_btn[i], LV_OPA_40, 0);
        } else {
            lv_obj_set_style_text_color(hmi_sub.tab_label[i], COLOR_DIM, 0);
            lv_obj_set_style_bg_color(hmi_sub.tab_btn[i], COLOR_PANEL, 0);
            lv_obj_set_style_shadow_width(hmi_sub.tab_btn[i], 0, 0);
        }
    }
    hmi_sub.current_page = page;
}

static void subset_on_tab_0(lv_event_t * e) { (void)e; subset_switch_page(0); }
static void subset_on_tab_1(lv_event_t * e) { (void)e; subset_switch_page(1); }
static void subset_on_tab_2(lv_event_t * e) { (void)e; subset_switch_page(2); }

static lv_obj_t * subset_create_tab_btn(lv_obj_t * parent, lv_event_cb_t cb)
{
    lv_obj_t * btn = lv_obj_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_flex_grow(btn, 1);
    lv_obj_set_height(btn, lv_pct(100));
    lv_obj_set_style_bg_color(btn, COLOR_PANEL, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    return btn;
}

static void subset_update_timer(lv_timer_t * timer)
{
    (void)timer;
    hmi_sub.t += 0.05f;

    if (hmi_sub.chart_series) {
        float val = 50.0f + 40.0f * sinf(hmi_sub.t * 1.2f);
        lv_chart_set_next_value(hmi_sub.chart, hmi_sub.chart_series, (int32_t)val);
        lv_chart_refresh(hmi_sub.chart);
    }

    if (hmi_sub.gauge) {
        float v = 60.0f + 35.0f * sinf(hmi_sub.t * 0.7f);
        hmi_gauge_set_value(hmi_sub.gauge, v);
    }

    if (hmi_sub.btn_destroy) {
        hmi_sub.destroy_phase += 0.04f;
        if (hmi_sub.destroy_phase > 1.0f) hmi_sub.destroy_phase -= 1.0f;
        lv_obj_invalidate(hmi_sub.btn_destroy);
    }
}

void hmi_create(lv_obj_t * parent)
{
    memset(&hmi_sub, 0, sizeof(hmi_sub));
    hmi_sub.current_page = -1;
    lv_obj_set_style_bg_color(parent, COLOR_BG, 0);

    lv_obj_t * cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(cont, 4, 0);
    lv_obj_set_style_pad_row(cont, 4, 0);

    lv_obj_t * tab_row = lv_obj_create(cont);
    lv_obj_remove_style_all(tab_row);
    lv_obj_set_size(tab_row, lv_pct(100), 28);
    lv_obj_set_flex_grow(tab_row, 0);
    lv_obj_set_style_pad_column(tab_row, 4, 0);
    lv_obj_set_flex_flow(tab_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tab_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    const char * names[] = { "BUTTONS", "CHART", "GAUGE" };
    lv_event_cb_t cbs[] = { subset_on_tab_0, subset_on_tab_1, subset_on_tab_2 };
    for (int i = 0; i < 3; i++) {
        hmi_sub.tab_btn[i] = subset_create_tab_btn(tab_row, cbs[i]);
        hmi_sub.tab_label[i] = lv_label_create(hmi_sub.tab_btn[i]);
        lv_label_set_text(hmi_sub.tab_label[i], names[i]);
        lv_obj_set_style_text_color(hmi_sub.tab_label[i], COLOR_TEXT, 0);
        lv_obj_center(hmi_sub.tab_label[i]);
    }

    hmi_sub.page_area = lv_obj_create(cont);
    lv_obj_remove_style_all(hmi_sub.page_area);
    lv_obj_set_size(hmi_sub.page_area, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_grow(hmi_sub.page_area, 1);

    /* Page 0: Four buttons */
    hmi_sub.page_buttons = lv_obj_create(hmi_sub.page_area);
    lv_obj_remove_style_all(hmi_sub.page_buttons);
    lv_obj_set_size(hmi_sub.page_buttons, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(hmi_sub.page_buttons, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(hmi_sub.page_buttons, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(hmi_sub.page_buttons, 6, 0);
    lv_obj_set_style_pad_row(hmi_sub.page_buttons, 6, 0);

    hmi_sub.btn_start = hmi_btn_create(hmi_sub.page_buttons);
    hmi_btn_set_text(hmi_sub.btn_start, "START");
    hmi_btn_set_led_enable(hmi_sub.btn_start, true);
    hmi_btn_set_led_color(hmi_sub.btn_start, COLOR_OK);
    hmi_btn_set_pulse_enable(hmi_sub.btn_start, true);
    hmi_btn_on_click(hmi_sub.btn_start, subset_on_start_click);
    lv_obj_set_size(hmi_btn_get_obj(hmi_sub.btn_start), lv_pct(70), 40);

    hmi_sub.btn_stop = hmi_btn_create(hmi_sub.page_buttons);
    hmi_btn_set_text(hmi_sub.btn_stop, "STOP");
    hmi_btn_set_led_enable(hmi_sub.btn_stop, true);
    hmi_btn_set_led_color(hmi_sub.btn_stop, COLOR_ERROR);
    hmi_btn_on_click(hmi_sub.btn_stop, subset_on_stop_click);
    lv_obj_set_size(hmi_btn_get_obj(hmi_sub.btn_stop), lv_pct(70), 40);

    hmi_sub.btn_reset = hmi_btn_create(hmi_sub.page_buttons);
    hmi_btn_set_text(hmi_sub.btn_reset, "RESET");
    hmi_btn_set_led_enable(hmi_sub.btn_reset, true);
    hmi_btn_set_led_color(hmi_sub.btn_reset, COLOR_WARN);
    hmi_btn_on_click(hmi_sub.btn_reset, subset_on_reset_click);
    lv_obj_set_size(hmi_btn_get_obj(hmi_sub.btn_reset), lv_pct(70), 40);

    hmi_sub.btn_destroy = lv_obj_create(hmi_sub.page_buttons);
    lv_obj_remove_style_all(hmi_sub.btn_destroy);
    lv_obj_set_size(hmi_sub.btn_destroy, lv_pct(70), 40);
    lv_obj_add_event_cb(hmi_sub.btn_destroy, subset_destroy_draw, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_event_cb(hmi_sub.btn_destroy, subset_on_destroy_press, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(hmi_sub.btn_destroy, subset_on_destroy_release, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(hmi_sub.btn_destroy, subset_on_destroy_click, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(hmi_sub.btn_destroy, LV_OBJ_FLAG_CLICKABLE);

    /* Page 1: Single chart */
    hmi_sub.page_chart = lv_obj_create(hmi_sub.page_area);
    lv_obj_remove_style_all(hmi_sub.page_chart);
    lv_obj_set_size(hmi_sub.page_chart, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(hmi_sub.page_chart, 6, 0);

    hmi_sub.chart = lv_chart_create(hmi_sub.page_chart);
    lv_obj_set_size(hmi_sub.chart, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(hmi_sub.chart, COLOR_PANEL, 0);
    lv_obj_set_style_border_width(hmi_sub.chart, 0, 0);
    lv_obj_set_style_radius(hmi_sub.chart, 10, 0);
    lv_obj_set_style_pad_all(hmi_sub.chart, 6, 0);
    lv_chart_set_type(hmi_sub.chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(hmi_sub.chart, 50);
    lv_chart_set_range(hmi_sub.chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_update_mode(hmi_sub.chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_obj_set_style_line_color(hmi_sub.chart, lv_color_hex(0x2A2A4A), LV_PART_MAIN);
    lv_obj_set_style_line_width(hmi_sub.chart, 1, LV_PART_MAIN);
    lv_obj_set_style_line_opa(hmi_sub.chart, LV_OPA_50, LV_PART_MAIN);
    hmi_sub.chart_series = lv_chart_add_series(hmi_sub.chart, COLOR_ACCENT, LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_values(hmi_sub.chart, hmi_sub.chart_series, LV_CHART_POINT_NONE);
    lv_obj_set_style_line_width(hmi_sub.chart, 2, LV_PART_ITEMS);

    /* Page 2: Single gauge */
    hmi_sub.page_gauge = lv_obj_create(hmi_sub.page_area);
    lv_obj_remove_style_all(hmi_sub.page_gauge);
    lv_obj_set_size(hmi_sub.page_gauge, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(hmi_sub.page_gauge, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hmi_sub.page_gauge, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    hmi_sub.gauge = hmi_gauge_create(hmi_sub.page_gauge);
    hmi_gauge_set_style(hmi_sub.gauge, HMI_GAUGE_STYLE_CYAN);
    hmi_gauge_set_title(hmi_sub.gauge, "VALUE");
    hmi_gauge_set_unit(hmi_sub.gauge, "V");
    hmi_gauge_set_range(hmi_sub.gauge, 0.0f, 100.0f);
    hmi_gauge_set_precision(hmi_sub.gauge, 1);
    hmi_gauge_set_tick_count(hmi_sub.gauge, 10);
    hmi_gauge_set_peak_enable(hmi_sub.gauge, true);
    lv_obj_set_size(hmi_gauge_get_obj(hmi_sub.gauge), 180, 180);

    subset_switch_page(0);
    lv_timer_create(subset_update_timer, 50, NULL);
}

//============================================================================
// SVPWM controller screen -- drives the G474RE over the SPI4 slave link
//============================================================================

typedef struct {
    lv_obj_t * root;

    hmi_gauge_t * gauge_freq;
    hmi_gauge_t * gauge_mag;

    hmi_slider_t * slider_freq;
    hmi_slider_t * slider_mag;

    hmi_btn_t * btn_start;
    hmi_btn_t * btn_stop;
    hmi_btn_t * btn_reset;

    lv_obj_t * info_label;
    lv_obj_t * status_label;

    float freq;
    float mag;
    bool running;
} hmi_control_t;

static hmi_control_t ctl;
static volatile bool hmi_ctl_dirty;

/* Accessors: the app (main.c) reads the current control state each SPI
 * exchange, so the frame update and the transceive stay in one thread (same
 * design as the verified h7s78_spi_slave demo). */
float hmi_ctl_get_frequency(void) { return ctl.freq; }
float hmi_ctl_get_magnitude(void) { return ctl.mag; }
bool  hmi_ctl_get_running(void)   { return ctl.running; }

bool hmi_ctl_state_changed(void)
{
    if (hmi_ctl_dirty) {
        hmi_ctl_dirty = false;
        return true;
    }
    return false;
}

static void control_refresh_timer(lv_timer_t * timer)
{
    (void)timer;
    hmi_gauge_set_value(ctl.gauge_freq, ctl.freq);
    hmi_gauge_set_value(ctl.gauge_mag, ctl.mag);
}

static void control_on_freq_change(hmi_slider_t * slider, float value)
{
    (void)slider;
    ctl.freq = value;
    hmi_ctl_dirty = true;
    char buf[48];
    snprintf(buf, sizeof(buf), LV_SYMBOL_SETTINGS " FREQ -> %.1f Hz", value);
    lv_label_set_text(ctl.info_label, buf);
    lv_obj_set_style_text_color(ctl.info_label, COLOR_ACCENT, 0);
}

static void control_on_mag_change(hmi_slider_t * slider, float value)
{
    (void)slider;
    ctl.mag = value;
    hmi_ctl_dirty = true;
    char buf[48];
    snprintf(buf, sizeof(buf), LV_SYMBOL_SETTINGS " MAG -> %.2f V", value);
    lv_label_set_text(ctl.info_label, buf);
    lv_obj_set_style_text_color(ctl.info_label, COLOR_ACCENT, 0);
}

static void control_on_start(hmi_btn_t * btn, lv_event_t * e)
{
    (void)e;
    ctl.running = true;
    hmi_ctl_dirty = true;
    hmi_btn_set_state(btn, HMI_BTN_ACTIVE);
    hmi_btn_set_state(ctl.btn_stop, HMI_BTN_IDLE);
    hmi_btn_set_state(ctl.btn_reset, HMI_BTN_IDLE);
    lv_label_set_text(ctl.status_label, LV_SYMBOL_PLAY " RUNNING");
    lv_obj_set_style_text_color(ctl.status_label, COLOR_OK, 0);
    lv_label_set_text(ctl.info_label, LV_SYMBOL_PLAY " SYNC: start requested");
    lv_obj_set_style_text_color(ctl.info_label, COLOR_OK, 0);
}

static void control_on_stop(hmi_btn_t * btn, lv_event_t * e)
{
    (void)e;
    ctl.running = false;
    hmi_ctl_dirty = true;
    hmi_btn_set_state(btn, HMI_BTN_ACTIVE);
    hmi_btn_set_state(ctl.btn_start, HMI_BTN_IDLE);
    hmi_btn_set_state(ctl.btn_reset, HMI_BTN_IDLE);
    lv_label_set_text(ctl.status_label, LV_SYMBOL_STOP " STOPPED");
    lv_obj_set_style_text_color(ctl.status_label, COLOR_ERROR, 0);
    lv_label_set_text(ctl.info_label, LV_SYMBOL_STOP " SYNC: stop requested");
    lv_obj_set_style_text_color(ctl.info_label, COLOR_ERROR, 0);
}

static void control_on_reset(hmi_btn_t * btn, lv_event_t * e)
{
    (void)e;
    ctl.freq = 60.0f;
    ctl.mag = 4.0f;
    ctl.running = true;
    hmi_ctl_dirty = true;
    hmi_btn_set_state(btn, HMI_BTN_ACTIVE);
    hmi_btn_set_state(ctl.btn_start, HMI_BTN_IDLE);
    hmi_btn_set_state(ctl.btn_stop, HMI_BTN_IDLE);

    hmi_slider_set_value(ctl.slider_freq, ctl.freq);
    hmi_slider_set_value(ctl.slider_mag, ctl.mag);

    lv_label_set_text(ctl.status_label, LV_SYMBOL_REFRESH " RESET 60Hz / 4V");
    lv_obj_set_style_text_color(ctl.status_label, COLOR_WARN, 0);
    lv_label_set_text(ctl.info_label, LV_SYMBOL_REFRESH " Reset reference -> 60 Hz, 4.0 V");
    lv_obj_set_style_text_color(ctl.info_label, COLOR_WARN, 0);
}

static lv_obj_t * control_badge(lv_obj_t * parent, const char * text, lv_color_t color)
{
    lv_obj_t * badge = lv_obj_create(parent);
    lv_obj_remove_style_all(badge);
    lv_obj_set_style_bg_color(badge, COLOR_PANEL, 0);
    lv_obj_set_style_radius(badge, 10, 0);
    lv_obj_set_style_pad_hor(badge, 12, 0);
    lv_obj_set_height(badge, 30);

    lv_obj_t * lbl = lv_label_create(badge);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, color, 0);
    lv_obj_center(lbl);
    return badge;
}

void hmi_create_control(lv_obj_t * parent)
{
    memset(&ctl, 0, sizeof(ctl));
    lv_obj_set_style_bg_color(parent, COLOR_BG, 0);

    lv_obj_t * cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(cont, 8, 0);
    lv_obj_set_style_pad_row(cont, 8, 0);
    ctl.root = cont;

    /* Header */
    lv_obj_t * header = lv_obj_create(cont);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, lv_pct(100), 56);
    lv_obj_set_flex_grow(header, 0);
    lv_obj_set_style_bg_color(header, COLOR_PANEL, 0);
    lv_obj_set_style_radius(header, 12, 0);
    lv_obj_set_style_pad_hor(header, 14, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * title = lv_label_create(header);
    lv_label_set_text(title, LV_SYMBOL_CHARGE "  SVPWM CONTROLLER");
    lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);

    lv_obj_t * header_right = lv_obj_create(header);
    lv_obj_set_style_bg_opa(header_right, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header_right, 0, 0);
    lv_obj_set_style_pad_all(header_right, 0, 0);
    lv_obj_remove_flag(header_right, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(header_right, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header_right, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(header_right, 8, 0);

    ctl.status_label = lv_label_create(header_right);
    lv_label_set_text(ctl.status_label, LV_SYMBOL_STOP " IDLE");
    lv_obj_set_style_text_color(ctl.status_label, COLOR_DIM, 0);

    control_badge(header_right, "SPI4 SLAVE", COLOR_OK);
    control_badge(header_right, "G474RE", COLOR_ACCENT);

    /* Gauges row */
    lv_obj_t * gauge_row = lv_obj_create(cont);
    lv_obj_remove_style_all(gauge_row);
    lv_obj_set_size(gauge_row, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_grow(gauge_row, 1);
    lv_obj_set_flex_flow(gauge_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(gauge_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    ctl.gauge_freq = hmi_gauge_create(gauge_row);
    hmi_gauge_set_style(ctl.gauge_freq, HMI_GAUGE_STYLE_CYAN);
    hmi_gauge_set_title(ctl.gauge_freq, "FREQUENCY");
    hmi_gauge_set_unit(ctl.gauge_freq, "Hz");
    hmi_gauge_set_range(ctl.gauge_freq, 0.0f, 400.0f);
    hmi_gauge_set_precision(ctl.gauge_freq, 1);
    hmi_gauge_set_tick_count(ctl.gauge_freq, 9);
    hmi_gauge_set_value(ctl.gauge_freq, 60.0f);
    lv_obj_set_size(hmi_gauge_get_obj(ctl.gauge_freq), lv_pct(45), lv_pct(100));

    ctl.gauge_mag = hmi_gauge_create(gauge_row);
    hmi_gauge_set_style(ctl.gauge_mag, HMI_GAUGE_STYLE_PINK);
    hmi_gauge_set_title(ctl.gauge_mag, "MAGNITUDE");
    hmi_gauge_set_unit(ctl.gauge_mag, "V");
    hmi_gauge_set_range(ctl.gauge_mag, 0.0f, 5.0f);
    hmi_gauge_set_precision(ctl.gauge_mag, 2);
    hmi_gauge_set_tick_count(ctl.gauge_mag, 6);
    hmi_gauge_set_value(ctl.gauge_mag, 4.0f);
    lv_obj_set_size(hmi_gauge_get_obj(ctl.gauge_mag), lv_pct(45), lv_pct(100));

    /* Control sliders */
    lv_obj_t * slider_row = lv_obj_create(cont);
    lv_obj_remove_style_all(slider_row);
    lv_obj_set_size(slider_row, lv_pct(100), 96);
    lv_obj_set_flex_grow(slider_row, 0);
    lv_obj_set_style_pad_column(slider_row, 8, 0);
    lv_obj_set_flex_flow(slider_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(slider_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    ctl.slider_freq = hmi_slider_create(slider_row);
    hmi_slider_set_title(ctl.slider_freq, "FREQUENCY");
    hmi_slider_set_unit(ctl.slider_freq, "Hz");
    hmi_slider_set_range(ctl.slider_freq, 0.0f, 400.0f);
    hmi_slider_set_precision(ctl.slider_freq, 1);
    hmi_slider_set_color(ctl.slider_freq, COLOR_ACCENT);
    hmi_slider_set_value(ctl.slider_freq, 60.0f);
    hmi_slider_on_change(ctl.slider_freq, control_on_freq_change);
    lv_obj_set_size(hmi_slider_get_obj(ctl.slider_freq), lv_pct(46), lv_pct(100));

    ctl.slider_mag = hmi_slider_create(slider_row);
    hmi_slider_set_title(ctl.slider_mag, "VOLTAGE MAG");
    hmi_slider_set_unit(ctl.slider_mag, "V");
    hmi_slider_set_range(ctl.slider_mag, 0.0f, 5.0f);
    hmi_slider_set_precision(ctl.slider_mag, 2);
    hmi_slider_set_color(ctl.slider_mag, COLOR_ACCENT);
    hmi_slider_set_value(ctl.slider_mag, 4.0f);
    hmi_slider_on_change(ctl.slider_mag, control_on_mag_change);
    lv_obj_set_size(hmi_slider_get_obj(ctl.slider_mag), lv_pct(46), lv_pct(100));

    /* Buttons */
    lv_obj_t * btn_row = lv_obj_create(cont);
    lv_obj_remove_style_all(btn_row);
    lv_obj_set_size(btn_row, lv_pct(100), 64);
    lv_obj_set_flex_grow(btn_row, 0);
    lv_obj_set_style_pad_column(btn_row, 8, 0);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    ctl.btn_start = hmi_btn_create(btn_row);
    hmi_btn_set_text(ctl.btn_start, "START");
    hmi_btn_set_led_enable(ctl.btn_start, true);
    hmi_btn_set_led_color(ctl.btn_start, COLOR_OK);
    hmi_btn_set_pulse_enable(ctl.btn_start, true);
    hmi_btn_on_click(ctl.btn_start, control_on_start);
    lv_obj_set_size(hmi_btn_get_obj(ctl.btn_start), lv_pct(30), lv_pct(90));

    ctl.btn_stop = hmi_btn_create(btn_row);
    hmi_btn_set_text(ctl.btn_stop, "STOP");
    hmi_btn_set_led_enable(ctl.btn_stop, true);
    hmi_btn_set_led_color(ctl.btn_stop, COLOR_ERROR);
    hmi_btn_on_click(ctl.btn_stop, control_on_stop);
    lv_obj_set_size(hmi_btn_get_obj(ctl.btn_stop), lv_pct(30), lv_pct(90));

    ctl.btn_reset = hmi_btn_create(btn_row);
    hmi_btn_set_text(ctl.btn_reset, "RESET");
    hmi_btn_set_led_enable(ctl.btn_reset, true);
    hmi_btn_set_led_color(ctl.btn_reset, COLOR_WARN);
    hmi_btn_on_click(ctl.btn_reset, control_on_reset);
    lv_obj_set_size(hmi_btn_get_obj(ctl.btn_reset), lv_pct(30), lv_pct(90));

    /* Info footer */
    ctl.info_label = lv_label_create(cont);
    lv_obj_set_size(ctl.info_label, lv_pct(100), 24);
    lv_obj_set_flex_grow(ctl.info_label, 0);
    lv_label_set_long_mode(ctl.info_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(ctl.info_label, lv_pct(100));
    lv_label_set_text(ctl.info_label, LV_SYMBOL_DRIVE " Drag sliders to set SVPWM reference, then START");
    lv_obj_set_style_text_color(ctl.info_label, COLOR_DIM, 0);

    ctl.freq = 60.0f;
    ctl.mag = 4.0f;
    ctl.running = false;

    /* Reflect real commanded values on the gauges. */
    lv_timer_create((lv_timer_cb_t)control_refresh_timer, 100, NULL);
}
