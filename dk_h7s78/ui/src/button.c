#include "button.h"
#include "common.h"

#include <string.h>
#include <stdio.h>

#define HMI_BTN_DEFAULT_WIDTH     120
#define HMI_BTN_DEFAULT_HEIGHT     40
#define HMI_BTN_RADIUS             12
#define HMI_BTN_ANIM_PERIOD        30

struct hmi_btn_t {
    lv_obj_t * obj;
    lv_timer_t * timer;

    char text[32];
    hmi_btn_state_t state;

    bool led_enable;
    lv_color_t led_color;

    bool pulse_enable;

    hmi_btn_event_cb_t click_cb;

    lv_color_t panel_base;
    float pulse_phase;
};

static lv_color_t hmi_btn_state_color(hmi_btn_state_t state)
{
    switch (state) {
        case HMI_BTN_ACTIVE:   return COLOR_ACCENT;
        case HMI_BTN_PRESSED:  return COLOR_ACCENT;
        case HMI_BTN_HOVER:    return COLOR_DIM;
        case HMI_BTN_DISABLED: return COLOR_DIM;
        default:               return COLOR_PANEL;
    }
}

static lv_color_t hmi_btn_text_color(hmi_btn_state_t state)
{
    switch (state) {
        case HMI_BTN_DISABLED: return COLOR_DIM;
        default:               return COLOR_TEXT;
    }
}

static void hmi_btn_draw_event(lv_event_t * event)
{
    HMI_DRAW_EVENT_BEGIN(hmi_btn_t, obj, btn);

    int32_t h = lv_area_get_height(&coords);
    int32_t cy = coords.y1 + h / 2;

    lv_color_t bg = btn->panel_base;
    lv_color_t border = hmi_btn_state_color(btn->state);
    lv_opa_t border_opa = LV_OPA_40;

    if (btn->state == HMI_BTN_PRESSED) {
        bg = lv_color_darken(bg, 32);
        border_opa = LV_OPA_80;
    } else if (btn->state == HMI_BTN_HOVER) {
        bg = lv_color_lighten(bg, 16);
        border_opa = LV_OPA_60;
    } else if (btn->state == HMI_BTN_ACTIVE) {
        if (btn->pulse_enable) {
            float brightness = (sinf(btn->pulse_phase * 6.283185f) + 1.0f) * 0.5f;
            lv_opa_t mix = 16 + (lv_opa_t)(brightness * 40);
            bg = lv_color_mix(COLOR_ACCENT, COLOR_PANEL, mix);
        }
        border_opa = LV_OPA_80;
    } else if (btn->state == HMI_BTN_DISABLED) {
        border_opa = LV_OPA_10;
    }

    lv_draw_rect_dsc_t rect;
    lv_draw_rect_dsc_init(&rect);
    rect.bg_color     = bg;
    rect.radius       = HMI_BTN_RADIUS;
    rect.border_color = border;
    rect.border_width = 2;
    rect.border_opa   = border_opa;
    lv_draw_rect(layer, &rect, &coords);

    if (btn->led_enable) {
        lv_draw_arc_dsc_t led;
        lv_draw_arc_dsc_init(&led);
        led.color       = btn->led_color;
        led.width       = 6;
        led.rounded     = true;
        led.center.x    = coords.x1 + LV_MAX(18, h * 2 / 5);
        led.center.y    = cy;
        led.radius      = 4;
        led.start_angle = 0;
        led.end_angle   = 360;

        if (btn->state == HMI_BTN_ACTIVE && btn->pulse_enable) {
            float brightness = (sinf(btn->pulse_phase * 6.283185f) + 1.0f) * 0.5f;
            led.opa = 80 + (lv_opa_t)(brightness * 120);
        }
        lv_draw_arc(layer, &led);
    }

    lv_draw_label_dsc_t label;
    lv_draw_label_dsc_init(&label);
    label.text  = btn->text;
    label.color = hmi_btn_text_color(btn->state);
    lv_area_t label_area;
    if (btn->led_enable) {
        /* center the text in the space right of the LED */
        int32_t text_x1 = coords.x1 + LV_MAX(18, h * 2 / 5) + 12;
        label_area = (lv_area_t){ text_x1, cy - 12, coords.x2 - 10, cy + 12 };
    } else {
        label.align = LV_TEXT_ALIGN_CENTER;
        label_area = (lv_area_t){ coords.x1 + 8, cy - 12, coords.x2 - 8, cy + 12 };
    }
    lv_draw_label(layer, &label, &label_area);
}

static void hmi_btn_event_cb(lv_event_t * event)
{
    hmi_btn_t * btn = lv_event_get_user_data(event);
    if (btn == NULL) return;

    lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_CLICKED) {
        if (btn->state == HMI_BTN_DISABLED) return;
        if (btn->click_cb) btn->click_cb(btn, event);
    }

    if (code == LV_EVENT_PRESSED) {
        if (btn->state != HMI_BTN_DISABLED) hmi_btn_set_state(btn, HMI_BTN_PRESSED);
    }

    if (code == LV_EVENT_RELEASED) {
        if (btn->state == HMI_BTN_PRESSED) {
            if (btn->state == HMI_BTN_DISABLED) return;
            hmi_btn_set_state(btn, btn->state == HMI_BTN_ACTIVE ? HMI_BTN_ACTIVE : HMI_BTN_IDLE);
        }
    }

    if (code == LV_EVENT_HOVER_OVER) {
        if (btn->state != HMI_BTN_DISABLED && btn->state != HMI_BTN_PRESSED) {
            hmi_btn_set_state(btn, HMI_BTN_HOVER);
        }
    }

    if (code == LV_EVENT_HOVER_LEAVE) {
        if (btn->state == HMI_BTN_HOVER) {
            hmi_btn_set_state(btn, btn->state == HMI_BTN_ACTIVE ? HMI_BTN_ACTIVE : HMI_BTN_IDLE);
        }
    }
}

static void hmi_btn_animation(lv_timer_t * timer)
{
    hmi_btn_t * btn = lv_timer_get_user_data(timer);
    if (btn == NULL) return;

    if (btn->pulse_enable && btn->state == HMI_BTN_ACTIVE) {
        btn->pulse_phase += 0.05f;
        if (btn->pulse_phase > 1.0f) btn->pulse_phase -= 1.0f;
        lv_obj_invalidate(btn->obj);
    }
}

hmi_btn_t * hmi_btn_create(lv_obj_t * parent)
{
    hmi_btn_t * btn = lv_malloc(sizeof(hmi_btn_t));
    if (btn == NULL) return NULL;

    memset(btn, 0, sizeof(hmi_btn_t));

    btn->state       = HMI_BTN_IDLE;
    btn->led_color   = COLOR_OK;
    btn->panel_base  = COLOR_PANEL;
    strcpy(btn->text, "BUTTON");

    btn->obj = lv_obj_create(parent);
    lv_obj_remove_style_all(btn->obj);
    lv_obj_set_size(btn->obj, HMI_BTN_DEFAULT_WIDTH, HMI_BTN_DEFAULT_HEIGHT);

    lv_obj_add_event_cb(btn->obj, hmi_btn_draw_event, LV_EVENT_DRAW_MAIN, btn);
    lv_obj_add_event_cb(btn->obj, hmi_btn_event_cb, LV_EVENT_ALL, btn);

    lv_obj_add_flag(btn->obj, LV_OBJ_FLAG_CLICKABLE);

    btn->timer = lv_timer_create(hmi_btn_animation, HMI_BTN_ANIM_PERIOD, btn);

    return btn;
}

void hmi_btn_delete(hmi_btn_t * btn)
{
    if (btn == NULL) return;
    if (btn->timer) lv_timer_delete(btn->timer);
    lv_obj_delete(btn->obj);
    lv_free(btn);
}

void hmi_btn_set_text(hmi_btn_t * btn, const char * text)
{
    if (btn == NULL || text == NULL) return;
    strncpy(btn->text, text, sizeof(btn->text) - 1);
    btn->text[sizeof(btn->text) - 1] = '\0';
    lv_obj_invalidate(btn->obj);
}

void hmi_btn_set_state(hmi_btn_t * btn, hmi_btn_state_t state)
{
    if (btn == NULL) return;
    btn->state = state;
    lv_obj_invalidate(btn->obj);
}

hmi_btn_state_t hmi_btn_get_state(const hmi_btn_t * btn)
{
    if (btn == NULL) return HMI_BTN_IDLE;
    return btn->state;
}

void hmi_btn_set_led_enable(hmi_btn_t * btn, bool enable)
{
    if (btn == NULL) return;
    btn->led_enable = enable;
    lv_obj_invalidate(btn->obj);
}

void hmi_btn_set_led_color(hmi_btn_t * btn, lv_color_t color)
{
    if (btn == NULL) return;
    btn->led_color = color;
    lv_obj_invalidate(btn->obj);
}

void hmi_btn_set_pulse_enable(hmi_btn_t * btn, bool enable)
{
    if (btn == NULL) return;
    btn->pulse_enable = enable;
}

void hmi_btn_on_click(hmi_btn_t * btn, hmi_btn_event_cb_t cb)
{
    if (btn == NULL) return;
    btn->click_cb = cb;
}

lv_obj_t * hmi_btn_get_obj(const hmi_btn_t * btn)
{
    if (btn == NULL) return NULL;
    return btn->obj;
}

#if 1

#include <math.h>

typedef struct {
    hmi_btn_t * btn;
    lv_obj_t * status_label;
} btn_demo_t;

static btn_demo_t demo_start;
static btn_demo_t demo_stop;
static btn_demo_t demo_reset;
static bool system_running = false;

static void on_start_click(hmi_btn_t * btn, lv_event_t * event)
{
    (void)event;
    system_running = true;
    hmi_btn_set_state(btn, HMI_BTN_ACTIVE);
    hmi_btn_set_state(demo_stop.btn, HMI_BTN_IDLE);
    hmi_btn_set_state(demo_reset.btn, HMI_BTN_IDLE);
    lv_label_set_text(demo_start.status_label, "RUNNING");
    lv_obj_set_style_text_color(demo_start.status_label, COLOR_OK, 0);
}

static void on_stop_click(hmi_btn_t * btn, lv_event_t * event)
{
    (void)event;
    system_running = false;
    hmi_btn_set_state(btn, HMI_BTN_ACTIVE);
    hmi_btn_set_state(demo_start.btn, HMI_BTN_IDLE);
    hmi_btn_set_state(demo_reset.btn, HMI_BTN_IDLE);
    lv_label_set_text(demo_start.status_label, "STOPPED");
    lv_obj_set_style_text_color(demo_start.status_label, COLOR_WARN, 0);
}

static void on_reset_click(hmi_btn_t * btn, lv_event_t * event)
{
    (void)event;
    system_running = false;
    hmi_btn_set_state(demo_start.btn, HMI_BTN_IDLE);
    hmi_btn_set_state(demo_stop.btn, HMI_BTN_IDLE);
    hmi_btn_set_state(btn, HMI_BTN_ACTIVE);
    lv_label_set_text(demo_start.status_label, "RESET");
    lv_obj_set_style_text_color(demo_start.status_label, COLOR_ACCENT, 0);
}

void button_example(void)
{
    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);

    lv_obj_t * cont = lv_obj_create(scr);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));

    lv_obj_t * label = lv_label_create(cont);
    lv_label_set_text(label, "INDUSTRIAL CONTROL");
    lv_obj_set_style_text_color(label, COLOR_DIM, 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_t * btn_row = lv_obj_create(cont);
    lv_obj_remove_style_all(btn_row);
    lv_obj_set_size(btn_row, lv_pct(100), 80);
    lv_obj_align(btn_row, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    demo_start.btn = hmi_btn_create(btn_row);
    hmi_btn_set_text(demo_start.btn, "START");
    hmi_btn_set_led_enable(demo_start.btn, true);
    hmi_btn_set_led_color(demo_start.btn, COLOR_OK);
    hmi_btn_set_pulse_enable(demo_start.btn, true);
    hmi_btn_on_click(demo_start.btn, on_start_click);

    demo_stop.btn = hmi_btn_create(btn_row);
    hmi_btn_set_text(demo_stop.btn, "STOP");
    hmi_btn_set_led_enable(demo_stop.btn, true);
    hmi_btn_set_led_color(demo_stop.btn, COLOR_WARN);
    hmi_btn_on_click(demo_stop.btn, on_stop_click);

    demo_reset.btn = hmi_btn_create(btn_row);
    hmi_btn_set_text(demo_reset.btn, "RESET");
    hmi_btn_set_led_enable(demo_reset.btn, true);
    hmi_btn_set_led_color(demo_reset.btn, COLOR_ACCENT);
    hmi_btn_on_click(demo_reset.btn, on_reset_click);

    demo_start.status_label = lv_label_create(cont);
    lv_label_set_text(demo_start.status_label, "READY");
    lv_obj_set_style_text_color(demo_start.status_label, COLOR_DIM, 0);
    lv_obj_align(demo_start.status_label, LV_ALIGN_BOTTOM_MID, 0, -40);
}

#endif
