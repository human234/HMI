#include "card.h"
#include "common.h"

#include <stdio.h>
#include <string.h>

#define IVC2_DEFAULT_WIDTH       180
#define IVC2_DEFAULT_HEIGHT      100
#define IVC2_HISTORY_SIZE        64
#define IVC2_UNIT_SIZE           16
#define IVC2_ANIM_PERIOD         30
#define IVC2_PADDING             10
#define IVC2_RADIUS              12

typedef struct {
    float value;
    float minimum;
    float maximum;
    float low_limit;
    float high_limit;
    uint8_t precision;
    char unit[IVC2_UNIT_SIZE];
    char prefix[IVC2_UNIT_SIZE];
} ivc2_measurement_t;

typedef struct {
    float data[IVC2_HISTORY_SIZE];
    uint16_t index;
    uint16_t count;
} ivc2_history_t;

struct card_t {
    lv_obj_t * object;
    lv_obj_t * title;
    lv_obj_t * val_row;
    lv_obj_t * value;
    lv_obj_t * unit;
    lv_obj_t * state;

    ivc2_measurement_t measurement;
    ivc2_history_t history;
    ivc2_state_t state_mode;
    ivc2_alarm_t alarm;

    bool enable_animation;
    bool enable_sparkline;
};

static lv_color_t ivc2_state_color(ivc2_state_t state)
{
    switch (state) {
        case IVC2_STATE_RUNNING: return COLOR_OK;
        case IVC2_STATE_WARNING: return COLOR_WARN;
        case IVC2_STATE_FAULT:   return COLOR_ERROR;
        case IVC2_STATE_READY:   return COLOR_ACCENT;
        default:                 return COLOR_DIM;
    }
}

static lv_color_t ivc2_alarm_color(ivc2_alarm_t alarm)
{
    switch (alarm) {
        case IVC2_ALARM_LOW:      return COLOR_WARN;
        case IVC2_ALARM_HIGH:
        case IVC2_ALARM_CRITICAL: return COLOR_ERROR;
        default:                  return COLOR_OK;
    }
}

static void ivc2_format_value(card_t * card, char * buffer, size_t size)
{
    snprintf(buffer, size, "%.*f", card->measurement.precision, card->measurement.value);
}

static void ivc2_format_unit(card_t * card, char * buffer, size_t size)
{
    snprintf(buffer, size, "%s%s", card->measurement.prefix, card->measurement.unit);
}

static void ivc2_render_measurement(card_t * card)
{
    char value[64];
    char unit[32];
    ivc2_format_value(card, value, sizeof(value));
    ivc2_format_unit(card, unit, sizeof(unit));
    lv_label_set_text(card->value, value);
    lv_label_set_text(card->unit, unit);
}

static float ivc2_get_range_position(card_t * card)
{
    return hmi_normalize(card->measurement.value, card->measurement.minimum, card->measurement.maximum);
}

static ivc2_alarm_t ivc2_check_alarm(card_t * card)
{
    float v = card->measurement.value;
    if (v < card->measurement.low_limit)  return IVC2_ALARM_LOW;
    if (v > card->measurement.high_limit) return IVC2_ALARM_HIGH;
    return IVC2_ALARM_NONE;
}

static void ivc2_draw_round_bar(lv_layer_t * layer, lv_area_t * area, float percentage, lv_color_t color)
{
    lv_draw_rect_dsc_t bg;
    lv_draw_rect_dsc_init(&bg);
    bg.bg_color = COLOR_DIM;
    bg.radius   = 4;
    lv_draw_rect(layer, &bg, area);

    lv_area_t fill = *area;
    fill.x2 = fill.x1 + (int32_t)((fill.x2 - fill.x1) * percentage);

    lv_draw_rect_dsc_t fg;
    lv_draw_rect_dsc_init(&fg);
    fg.bg_color = color;
    fg.radius   = 4;
    lv_draw_rect(layer, &fg, &fill);
}

static void ivc2_draw_sparkline(lv_layer_t * layer, card_t * card, lv_area_t * area)
{
    if (card->history.count < 2) return;

    uint16_t cnt = card->history.count;

    /* normalize against the actual window min/max so the trace always
     * fills the graph area regardless of the value magnitude */
    uint16_t first = (card->history.index + IVC2_HISTORY_SIZE - cnt) % IVC2_HISTORY_SIZE;
    float mn = card->history.data[first];
    float mx = mn;
    for (uint16_t i = 1; i < cnt; i++) {
        uint16_t idx = (first + i) % IVC2_HISTORY_SIZE;
        if (card->history.data[idx] < mn) mn = card->history.data[idx];
        if (card->history.data[idx] > mx) mx = card->history.data[idx];
    }
    if (mx <= mn) mx = mn + 1.0f;

    int32_t top = area->y1 + 2;
    int32_t bot = area->y2 - 2;
    int32_t span = bot - top;

    for (uint16_t i = 1; i < cnt; i++) {
        uint16_t prev = (first + i - 1) % IVC2_HISTORY_SIZE;
        uint16_t curr = (first + i)     % IVC2_HISTORY_SIZE;

        lv_point_precise_t p1, p2;
        p1.x = area->x1 + (int32_t)((i - 1) * (area->x2 - area->x1) / (cnt - 1));
        p1.y = bot - (int32_t)((card->history.data[prev] - mn) / (mx - mn) * span);
        p2.x = area->x1 + (int32_t)(i       * (area->x2 - area->x1) / (cnt - 1));
        p2.y = bot - (int32_t)((card->history.data[curr] - mn) / (mx - mn) * span);

        lv_draw_line_dsc_t line;
        lv_draw_line_dsc_init(&line);
        line.color = COLOR_ACCENT;
        line.width = 2;
        line.p1    = p1;
        line.p2    = p2;
        lv_draw_line(layer, &line);
    }
}

static void ivc2_draw_event(lv_event_t * event)
{
    HMI_DRAW_EVENT_BEGIN(card_t, object, card);
    int32_t h = coords.y2 - coords.y1;

    /* range bar pinned to the bottom edge, height scales with the card */
    int32_t bar_h = LV_CLAMP(8, h / 8, 14);
    lv_area_t range_area = {
        .x1 = coords.x1 + IVC2_PADDING,
        .x2 = coords.x2 - IVC2_PADDING,
        .y1 = coords.y2 - IVC2_PADDING - bar_h,
        .y2 = coords.y2 - IVC2_PADDING
    };
    ivc2_draw_round_bar(layer, &range_area, ivc2_get_range_position(card),
                        ivc2_state_color(card->state_mode));

    if (card->enable_sparkline) {
        int32_t graph_h = LV_CLAMP(18, h / 4, 34);
        lv_area_t graph = {
            .x1 = coords.x1 + IVC2_PADDING,
            .x2 = coords.x2 - IVC2_PADDING,
            .y1 = range_area.y1 - 6 - graph_h,
            .y2 = range_area.y1 - 6
        };
        ivc2_draw_sparkline(layer, card, &graph);
    }
}

static void ivc2_history_push(card_t * card, float value)
{
    card->history.data[card->history.index++] = value;
    if (card->history.index >= IVC2_HISTORY_SIZE) card->history.index = 0;
    if (card->history.count < IVC2_HISTORY_SIZE) card->history.count++;
}

static void ivc2_update_alarm(card_t * card)
{
    card->alarm = ivc2_check_alarm(card);
    lv_color_t color = (card->alarm != IVC2_ALARM_NONE)
                     ? ivc2_alarm_color(card->alarm)
                     : ivc2_state_color(card->state_mode);
    lv_obj_set_style_text_color(card->state, color, 0);
}

static const char * ivc2_state_text(ivc2_state_t state)
{
    switch (state) {
        case IVC2_STATE_READY:   return "READY";
        case IVC2_STATE_RUNNING: return "RUNNING";
        case IVC2_STATE_WARNING: return "WARNING";
        case IVC2_STATE_FAULT:   return "FAULT";
        default:                 return "OFF";
    }
}

static void ivc2_render_state(card_t * card)
{
    lv_label_set_text(card->state, ivc2_state_text(card->state_mode));
    lv_obj_set_style_text_color(card->state, ivc2_state_color(card->state_mode), 0);
}

static void ivc2_animation_update(card_t * card)
{
    if (!card->enable_animation) return;

    float brightness = hmi_pulse_brightness(hmi_pulse_phase());
    lv_opa_t opacity = 80 + (lv_opa_t)(brightness * 120);

    if (card->state_mode == IVC2_STATE_RUNNING) {
        lv_obj_set_style_opa(card->state, opacity, 0);
    }
}

static void ivc2_timer_callback(lv_timer_t * timer)
{
    card_t * card = lv_timer_get_user_data(timer);
    if (card == NULL) return;

    ivc2_update_alarm(card);
    ivc2_animation_update(card);
    lv_obj_invalidate(card->object);
}

static lv_obj_t * ivc2_create_label(lv_obj_t * parent)
{
    lv_obj_t * obj = lv_label_create(parent);
    lv_label_set_text(obj, "");
    return obj;
}

static void ivc2_create_children(card_t * card)
{
    card->title = ivc2_create_label(card->object);

    /* value + unit live in a content-sized flex row so the unit always
     * follows the value regardless of card width */
    card->val_row = lv_obj_create(card->object);
    lv_obj_remove_style_all(card->val_row);
    lv_obj_clear_flag(card->val_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(card->val_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(card->val_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card->val_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(card->val_row, 4, 0);

    card->value = ivc2_create_label(card->val_row);
    card->unit  = ivc2_create_label(card->val_row);
    card->state = ivc2_create_label(card->object);

    lv_obj_set_style_text_color(card->title, COLOR_DIM, 0);
    lv_obj_set_style_text_color(card->value, COLOR_TEXT, 0);
    lv_obj_set_style_text_color(card->unit,  COLOR_DIM, 0);
    lv_obj_set_style_text_color(card->state, COLOR_OK, 0);

    lv_obj_align(card->title, LV_ALIGN_TOP_LEFT, IVC2_PADDING, IVC2_PADDING - 4);
    lv_obj_align(card->val_row, LV_ALIGN_TOP_MID, 0, IVC2_PADDING + 14);
    lv_obj_align(card->state, LV_ALIGN_TOP_RIGHT, -IVC2_PADDING, IVC2_PADDING - 4);
}

card_t * card_create(lv_obj_t * parent)
{
    if (parent == NULL) return NULL;

    card_t * card = lv_malloc(sizeof(card_t));
    if (card == NULL) return NULL;

    memset(card, 0, sizeof(card_t));

    card->measurement.maximum   = 100.0f;
    card->measurement.precision = 1;
    card->state_mode            = IVC2_STATE_OFF;
    card->enable_animation      = true;

    card->object = lv_obj_create(parent);
    lv_obj_remove_style_all(card->object);
    lv_obj_set_size(card->object, IVC2_DEFAULT_WIDTH, IVC2_DEFAULT_HEIGHT);

    lv_obj_set_style_bg_color(card->object, COLOR_PANEL, 0);
    lv_obj_set_style_radius(card->object, IVC2_RADIUS, 0);
    lv_obj_set_style_shadow_width(card->object, 12, 0);
    lv_obj_set_style_shadow_color(card->object, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(card->object, LV_OPA_40, 0);

    ivc2_create_children(card);

    lv_obj_add_event_cb(card->object, ivc2_draw_event, LV_EVENT_DRAW_MAIN, card);
    lv_timer_create(ivc2_timer_callback, IVC2_ANIM_PERIOD, card);

    ivc2_render_measurement(card);
    ivc2_render_state(card);

    return card;
}

void card_destroy(card_t * card)
{
    if (card == NULL) return;
    if (card->object) lv_obj_delete(card->object);
    lv_free(card);
}

void card_set_value(card_t * card, float value)
{
    if (card == NULL) return;
    card->measurement.value = value;
    ivc2_render_measurement(card);
}

float card_get_value(const card_t * card)
{
    if (card == NULL) return 0.0f;
    return card->measurement.value;
}

void card_set_unit(card_t * card, const char * unit)
{
    if (card == NULL || unit == NULL) return;
    strncpy(card->measurement.unit, unit, IVC2_UNIT_SIZE - 1);
    card->measurement.unit[IVC2_UNIT_SIZE - 1] = '\0';
    ivc2_render_measurement(card);
}

void card_set_prefix(card_t * card, const char * prefix)
{
    if (card == NULL || prefix == NULL) return;
    strncpy(card->measurement.prefix, prefix, IVC2_UNIT_SIZE - 1);
    card->measurement.prefix[IVC2_UNIT_SIZE - 1] = '\0';
    ivc2_render_measurement(card);
}

void card_set_precision(card_t * card, uint8_t precision)
{
    if (card == NULL) return;
    card->measurement.precision = precision;
    ivc2_render_measurement(card);
}

void card_set_range(card_t * card, float minimum, float maximum)
{
    if (card == NULL) return;
    card->measurement.minimum = minimum;
    card->measurement.maximum = maximum;
}

void card_set_alarm_limit(card_t * card, float low, float high)
{
    if (card == NULL) return;
    card->measurement.low_limit  = low;
    card->measurement.high_limit = high;
}

ivc2_alarm_t card_get_alarm(const card_t * card)
{
    if (card == NULL) return IVC2_ALARM_NONE;
    return card->alarm;
}

void card_push_sample(card_t * card, float value)
{
    if (card == NULL) return;
    ivc2_history_push(card, value);
    lv_obj_invalidate(card->object);
}

void card_enable_sparkline(card_t * card, bool enable)
{
    if (card == NULL) return;
    card->enable_sparkline = enable;
}

void card_set_state(card_t * card, ivc2_state_t state)
{
    if (card == NULL) return;
    card->state_mode = state;
    ivc2_render_state(card);
}

void card_enable_animation(card_t * card, bool enable)
{
    if (card == NULL) return;
    card->enable_animation = enable;
}

lv_obj_t * card_get_object(const card_t * card)
{
    if (card == NULL) return NULL;
    return card->object;
}

#if 1

#include <math.h>

typedef struct {
    card_t * card;
    float phase;
    float center;
    float amplitude;
    float speed;
    float history_value;
} card_demo_t;

static card_demo_t demo_v;
static card_demo_t demo_a;
static card_demo_t demo_hz;

static void card_demo_timer(lv_timer_t * timer)
{
    (void)timer;
    float t = lv_tick_get() / 1000.0f;

    demo_v.history_value = demo_v.center + demo_v.amplitude * sinf(demo_v.phase + t * demo_v.speed);
    demo_a.history_value = demo_a.center + demo_a.amplitude * sinf(demo_a.phase + t * demo_a.speed);
    demo_hz.history_value = demo_hz.center + demo_hz.amplitude * sinf(demo_hz.phase + t * demo_hz.speed);

    card_set_value(demo_v.card, demo_v.history_value);
    card_set_value(demo_a.card, demo_a.history_value);
    card_set_value(demo_hz.card, demo_hz.history_value);

    card_push_sample(demo_v.card, demo_v.history_value);
    card_push_sample(demo_a.card, demo_a.history_value);
    card_push_sample(demo_hz.card, demo_hz.history_value);
}

void value_card_example(void)
{
    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);

    lv_obj_t * cont = lv_obj_create(scr);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    demo_v = (card_demo_t){
        .center = 400.0f, .amplitude = 80.0f, .phase = 0.0f, .speed = 0.5f,
        .card = card_create(cont)
    };
    card_set_unit(demo_v.card, "V");
    card_set_value(demo_v.card, 400.0f);
    card_set_range(demo_v.card, 250.0f, 480.0f);
    card_set_alarm_limit(demo_v.card, 300.0f, 430.0f);
    card_set_state(demo_v.card, IVC2_STATE_RUNNING);
    card_enable_sparkline(demo_v.card, true);

    demo_a = (card_demo_t){
        .center = 50.0f, .amplitude = 30.0f, .phase = 2.0f, .speed = 0.7f,
        .card = card_create(cont)
    };
    card_set_unit(demo_a.card, "A");
    card_set_value(demo_a.card, 50.0f);
    card_set_range(demo_a.card, 0.0f, 100.0f);
    card_set_alarm_limit(demo_a.card, 20.0f, 80.0f);
    card_set_state(demo_a.card, IVC2_STATE_RUNNING);
    card_enable_sparkline(demo_a.card, true);

    demo_hz = (card_demo_t){
        .center = 50.0f, .amplitude = 10.0f, .phase = 4.0f, .speed = 0.3f,
        .card = card_create(cont)
    };
    card_set_unit(demo_hz.card, "Hz");
    card_set_value(demo_hz.card, 50.0f);
    card_set_range(demo_hz.card, 0.0f, 100.0f);
    card_set_alarm_limit(demo_hz.card, 45.0f, 55.0f);
    card_set_state(demo_hz.card, IVC2_STATE_RUNNING);
    card_enable_sparkline(demo_hz.card, true);

    lv_timer_create(card_demo_timer, 50, NULL);
}

#endif
