#include "slider.h"
#include "common.h"

#include <string.h>
#include <stdio.h>

#define HMI_SLIDER_DEFAULT_WIDTH      180
#define HMI_SLIDER_DEFAULT_HEIGHT      50

struct hmi_slider_t {
    lv_obj_t * obj;

    float value;
    float display_value;
    float minimum;
    float maximum;
    uint8_t precision;
    char title[32];
    char unit[16];

    lv_color_t color;

    hmi_slider_event_cb_t change_cb;

    bool dragging;
};

static float slider_value_from_x(const hmi_slider_t * slider, int32_t x)
{
    lv_area_t coords;
    lv_obj_get_coords(slider->obj, &coords);
    int32_t w = lv_area_get_width(&coords);
    int32_t h = lv_area_get_height(&coords);
    if (w <= 0) return slider->minimum;

    int32_t track_h = LV_CLAMP(6, h / 7, 12);
    float pad = track_h + 5 + 8;   /* matches thumb_r + 8 in the draw code */
    float range = w - 2.0f * pad;
    if (range <= 0) return slider->minimum;

    float ratio = (x - coords.x1 - pad) / range;
    ratio = hmi_clamp(ratio, 0.0f, 1.0f);

    return slider->minimum + ratio * (slider->maximum - slider->minimum);
}

static void hmi_slider_draw_event(lv_event_t * event)
{
    HMI_DRAW_EVENT_BEGIN(hmi_slider_t, obj, slider);
    int32_t w = lv_area_get_width(&coords);
    int32_t h = lv_area_get_height(&coords);

    float ratio = (slider->display_value - slider->minimum) /
                  (slider->maximum - slider->minimum);
    ratio = hmi_clamp(ratio, 0.0f, 1.0f);

    /* track/thumb geometry scales with the widget */
    int32_t track_h = LV_CLAMP(6, h / 7, 12);
    int32_t thumb_r = track_h + 5;
    int32_t pad = thumb_r + 8;
    int32_t track_y = coords.y2 - track_h - LV_CLAMP(h / 6, 10, 16);

    lv_draw_rect_dsc_t bg;
    lv_draw_rect_dsc_init(&bg);
    bg.bg_color = COLOR_PANEL;
    bg.radius   = 14;
    lv_draw_rect(layer, &bg, &coords);

    int32_t track_x1 = coords.x1 + pad;
    int32_t track_x2 = coords.x2 - pad;
    int32_t track_w = track_x2 - track_x1;
    int32_t fill_w = (int32_t)(track_w * ratio + 0.5f);
    int32_t thumb_center_x = track_x1 + fill_w;

    lv_area_t track_area = { track_x1, track_y, track_x2, track_y + track_h };

    lv_draw_rect_dsc_t track_bg;
    lv_draw_rect_dsc_init(&track_bg);
    track_bg.bg_color = COLOR_BG;
    track_bg.radius   = track_h / 2;
    lv_draw_rect(layer, &track_bg, &track_area);

    lv_area_t fill_area = { track_x1, track_y, track_x1 + fill_w, track_y + track_h };
    lv_draw_rect_dsc_t fill;
    lv_draw_rect_dsc_init(&fill);
    fill.bg_color = slider->color;
    fill.radius   = track_h / 2;
    lv_draw_rect(layer, &fill, &fill_area);

    lv_draw_arc_dsc_t thumb;
    lv_draw_arc_dsc_init(&thumb);
    thumb.color       = COLOR_TEXT;
    thumb.width       = 3;
    thumb.rounded     = true;
    thumb.center.x    = thumb_center_x;
    thumb.center.y    = track_y + track_h / 2;
    thumb.radius      = thumb_r;
    thumb.start_angle = 0;
    thumb.end_angle   = 360;
    lv_draw_arc(layer, &thumb);

    lv_draw_arc_dsc_t thumb_fill;
    lv_draw_arc_dsc_init(&thumb_fill);
    thumb_fill.color       = slider->color;
    thumb_fill.width       = track_h;
    thumb_fill.rounded     = true;
    thumb_fill.center.x    = thumb_center_x;
    thumb_fill.center.y    = track_y + track_h / 2;
    thumb_fill.radius      = track_h / 2;
    thumb_fill.start_angle = 0;
    thumb_fill.end_angle   = 360;
    lv_draw_arc(layer, &thumb_fill);

    char buf[32];
    snprintf(buf, sizeof(buf), "%.*f%s", slider->precision, slider->display_value, slider->unit);
    lv_draw_label_dsc_t lbl;
    lv_draw_label_dsc_init(&lbl);
    lbl.text  = buf;
    lbl.color = COLOR_TEXT;
    /* keep the value label inside the widget when the thumb is at the ends */
    int32_t lbl_w = 84;
    int32_t lbl_x = thumb_center_x - lbl_w / 2;
    if (lbl_x < coords.x1 + 4) lbl_x = coords.x1 + 4;
    if (lbl_x + lbl_w > coords.x2 - 4) lbl_x = coords.x2 - 4 - lbl_w;
    lv_area_t lbl_area = { lbl_x, coords.y1 + 2, lbl_x + lbl_w, coords.y1 + 18 };
    lv_draw_label(layer, &lbl, &lbl_area);

    if (strlen(slider->title) > 0) {
        lbl.text = slider->title;
        lbl.color = COLOR_DIM;
        lv_area_t title_area = { coords.x1 + 6, coords.y1 + 20, coords.x1 + w - 6, coords.y1 + 36 };
        lv_draw_label(layer, &lbl, &title_area);
    }
}

static void hmi_slider_event_cb(lv_event_t * event)
{
    hmi_slider_t * slider = lv_event_get_user_data(event);
    if (slider == NULL) return;

    lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_PRESSED) {
        slider->dragging = true;
        lv_indev_t * indev = lv_event_get_indev(event);
        lv_point_t pt;
        lv_indev_get_point(indev, &pt);
        float val = slider_value_from_x(slider, pt.x);
        slider->value = val;
        slider->display_value = val;
        if (slider->change_cb) slider->change_cb(slider, val);
        lv_obj_invalidate(slider->obj);
    }

    if (code == LV_EVENT_PRESSING) {
        if (!slider->dragging) return;
        lv_indev_t * indev = lv_event_get_indev(event);
        lv_point_t pt;
        lv_indev_get_point(indev, &pt);
        float val = slider_value_from_x(slider, pt.x);
        slider->value = val;
        slider->display_value = val;
        if (slider->change_cb) slider->change_cb(slider, val);
        lv_obj_invalidate(slider->obj);
    }

    if (code == LV_EVENT_RELEASED) {
        slider->dragging = false;
        lv_obj_invalidate(slider->obj);
    }
}

hmi_slider_t * hmi_slider_create(lv_obj_t * parent)
{
    hmi_slider_t * slider = lv_malloc(sizeof(hmi_slider_t));
    if (slider == NULL) return NULL;

    memset(slider, 0, sizeof(hmi_slider_t));

    slider->minimum  = 0.0f;
    slider->maximum  = 100.0f;
    slider->precision = 1;
    slider->color     = COLOR_ACCENT;
    strcpy(slider->title, "SLIDER");
    slider->unit[0] = '\0';

    slider->obj = lv_obj_create(parent);
    lv_obj_remove_style_all(slider->obj);
    lv_obj_set_size(slider->obj, HMI_SLIDER_DEFAULT_WIDTH, HMI_SLIDER_DEFAULT_HEIGHT);
    lv_obj_add_flag(slider->obj, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_event_cb(slider->obj, hmi_slider_draw_event, LV_EVENT_DRAW_MAIN, slider);
    lv_obj_add_event_cb(slider->obj, hmi_slider_event_cb, LV_EVENT_ALL, slider);

    return slider;
}

void hmi_slider_delete(hmi_slider_t * slider)
{
    if (slider == NULL) return;
    lv_obj_delete(slider->obj);
    lv_free(slider);
}

void hmi_slider_set_value(hmi_slider_t * slider, float value)
{
    if (slider == NULL) return;
    slider->value = hmi_clamp(value, slider->minimum, slider->maximum);
    slider->display_value = slider->value;
    lv_obj_invalidate(slider->obj);
}

float hmi_slider_get_value(const hmi_slider_t * slider)
{
    if (slider == NULL) return 0.0f;
    return slider->value;
}

void hmi_slider_set_range(hmi_slider_t * slider, float min, float max)
{
    if (slider == NULL) return;
    slider->minimum = min;
    slider->maximum = max;
    slider->value = hmi_clamp(slider->value, min, max);
    slider->display_value = slider->value;
    lv_obj_invalidate(slider->obj);
}

void hmi_slider_set_precision(hmi_slider_t * slider, uint8_t precision)
{
    if (slider == NULL) return;
    slider->precision = precision;
    lv_obj_invalidate(slider->obj);
}

void hmi_slider_set_unit(hmi_slider_t * slider, const char * unit)
{
    if (slider == NULL || unit == NULL) return;
    strncpy(slider->unit, unit, sizeof(slider->unit) - 1);
    slider->unit[sizeof(slider->unit) - 1] = '\0';
    lv_obj_invalidate(slider->obj);
}

void hmi_slider_set_title(hmi_slider_t * slider, const char * title)
{
    if (slider == NULL || title == NULL) return;
    strncpy(slider->title, title, sizeof(slider->title) - 1);
    slider->title[sizeof(slider->title) - 1] = '\0';
    lv_obj_invalidate(slider->obj);
}

void hmi_slider_set_color(hmi_slider_t * slider, lv_color_t color)
{
    if (slider == NULL) return;
    slider->color = color;
    lv_obj_invalidate(slider->obj);
}

void hmi_slider_on_change(hmi_slider_t * slider, hmi_slider_event_cb_t cb)
{
    if (slider == NULL) return;
    slider->change_cb = cb;
}

lv_obj_t * hmi_slider_get_obj(const hmi_slider_t * slider)
{
    if (slider == NULL) return NULL;
    return slider->obj;
}

#if 1

typedef struct {
    hmi_slider_t * slider;
    lv_obj_t * value_label;
} slider_demo_t;

static slider_demo_t demo_slider;

static void on_slider_change(hmi_slider_t * slider, float value)
{
    (void)slider;
    char buf[32];
    snprintf(buf, sizeof(buf), "Value: %.1f", value);
    lv_label_set_text(demo_slider.value_label, buf);
}

void slider_example(void)
{
    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);

    lv_obj_t * cont = lv_obj_create(scr);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(cont, 20, 0);

    lv_obj_t * title = lv_label_create(cont);
    lv_label_set_text(title, "INDUSTRIAL SLIDER");
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0);

    demo_slider.slider = hmi_slider_create(cont);
    hmi_slider_set_title(demo_slider.slider, "SETPOINT");
    hmi_slider_set_unit(demo_slider.slider, "Hz");
    hmi_slider_set_range(demo_slider.slider, 0.0f, 100.0f);
    hmi_slider_set_precision(demo_slider.slider, 1);
    hmi_slider_set_color(demo_slider.slider, COLOR_OK);
    hmi_slider_on_change(demo_slider.slider, on_slider_change);
    lv_obj_set_size(hmi_slider_get_obj(demo_slider.slider), lv_pct(80), 70);

    demo_slider.value_label = lv_label_create(cont);
    lv_label_set_text(demo_slider.value_label, "Value: 0.0");
    lv_obj_set_style_text_color(demo_slider.value_label, COLOR_DIM, 0);
}

#endif
