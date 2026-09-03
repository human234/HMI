#include "gauge.h"
#include "common.h"

#include <string.h>
#include <stdio.h>

#define HMI_GAUGE_DEFAULT_WIDTH      120
#define HMI_GAUGE_DEFAULT_HEIGHT     120
#define HMI_GAUGE_DESIGN_RADIUS      55   /* reference radius the style constants were tuned for */
#define HMI_GAUGE_START_ANGLE        135.0f
#define HMI_GAUGE_END_ANGLE          405.0f
#define HMI_GAUGE_ANIM_PERIOD        30

struct hmi_gauge_t {
    lv_obj_t * obj;
    lv_timer_t * timer;

    float value;
    float display_value;
    float minimum;
    float maximum;
    uint16_t tick_count;
    uint8_t precision;
    char title[32];
    char unit[16];

    hmi_gauge_zone_t zones[HMI_GAUGE_MAX_ZONE];
    uint8_t zone_count;

    hmi_gauge_alarm_t alarm;

    bool peak_enable;
    float peak_value;

    bool animation_enable;

    lv_color_t panel_bg;
    lv_color_t panel_border;
    lv_color_t outer_arc;
    lv_color_t inner_arc;
    lv_color_t accent;
    lv_color_t accent2;

    hmi_gauge_style_t style;
    float start_angle;
    float end_angle;
    uint8_t needle_width;
    int16_t needle_offset;
    uint8_t outer_arc_width;
    uint8_t inner_arc_width;
    uint8_t tick_width;
    int16_t tick_inner;
    int16_t tick_outer;
    int16_t radius;   /* actual dial radius, derived from widget size at draw time */
};

/* Scale a length tuned for HMI_GAUGE_DESIGN_RADIUS to the actual radius.
 * Stroke widths grow with the dial but are capped at 2x so they stay crisp. */
static inline int32_t gauge_grow(const hmi_gauge_t * gauge, int32_t base)
{
    int32_t v = (int32_t)(base * gauge->radius / HMI_GAUGE_DESIGN_RADIUS);
    if (v < base) v = base;
    if (v > base * 2) v = base * 2;
    return v;
}

/* Pure proportional scale for lengths (needle length, tails, ...) */
static inline int32_t gauge_len(const hmi_gauge_t * gauge, int32_t base)
{
    return (int32_t)(base * gauge->radius / HMI_GAUGE_DESIGN_RADIUS);
}

static float gauge_value_to_angle(const hmi_gauge_t * gauge, float value)
{
    float ratio = hmi_normalize(value, gauge->minimum, gauge->maximum);
    return hmi_map(ratio, 0.0f, 1.0f, gauge->start_angle, gauge->end_angle);
}

static lv_color_t gauge_needle_color(const hmi_gauge_t * gauge)
{
    switch (gauge->alarm) {
        case HMI_GAUGE_WARNING: return COLOR_WARN;
        case HMI_GAUGE_ERROR:
        case HMI_GAUGE_TRIP:    return COLOR_ERROR;
        default:                return gauge->accent;
    }
}

static void gauge_draw_background(lv_layer_t * layer, const hmi_gauge_t * gauge, lv_area_t * area, int32_t cx, int32_t cy)
{
    lv_draw_rect_dsc_t rect;
    lv_draw_rect_dsc_init(&rect);
    rect.bg_color     = gauge->panel_bg;
    rect.radius       = (gauge->style == HMI_GAUGE_STYLE_PINK) ? 8 : 20;
    rect.border_color = gauge->panel_border;
    rect.border_width = 2;
    rect.border_opa   = LV_OPA_40;
    lv_draw_rect(layer, &rect, area);

    lv_draw_arc_dsc_t ring;
    lv_draw_arc_dsc_init(&ring);
    ring.color       = gauge->accent;
    ring.opa         = LV_OPA_10;
    ring.width       = 2;
    ring.rounded     = true;
    ring.center.x    = cx;
    ring.center.y    = cy;
    ring.radius      = gauge->radius;
    ring.start_angle = gauge->start_angle;
    ring.end_angle   = gauge->end_angle;
    lv_draw_arc(layer, &ring);

    float mid = (gauge->start_angle + gauge->end_angle) / 2.0f;
    lv_point_precise_t top = hmi_polar_to_point(cx, cy, gauge->radius + gauge_grow(gauge, 10), mid);
    lv_point_precise_t bot = hmi_polar_to_point(cx, cy, gauge->radius * 55 / 100, mid + 180.0f);
    lv_draw_line_dsc_t hl;
    lv_draw_line_dsc_init(&hl);
    hl.color = gauge->accent;
    hl.width = 1;
    hl.opa   = LV_OPA_10;
    hl.p1    = top;
    hl.p2    = bot;
    lv_draw_line(layer, &hl);
    lv_point_precise_t top2 = hmi_polar_to_point(cx, cy, gauge->radius + gauge_grow(gauge, 10), mid + 90.0f);
    lv_point_precise_t bot2 = hmi_polar_to_point(cx, cy, gauge->radius * 55 / 100, mid - 90.0f);
    hl.p1 = top2; hl.p2 = bot2;
    lv_draw_line(layer, &hl);

    lv_draw_arc_dsc_init(&ring);
    ring.color       = gauge->accent;
    ring.opa         = LV_OPA_10;
    ring.width       = 1;
    ring.rounded     = true;
    ring.center.x    = cx;
    ring.center.y    = cy;
    ring.radius      = gauge->radius * 73 / 100;
    ring.start_angle = 0;
    ring.end_angle   = 360;
    lv_draw_arc(layer, &ring);
    ring.radius      = gauge->radius * 55 / 100;
    lv_draw_arc(layer, &ring);

    lv_draw_arc_dsc_t stop;
    lv_draw_arc_dsc_init(&stop);
    stop.color       = gauge->accent;
    stop.width       = gauge_grow(gauge, 3);
    stop.rounded     = true;
    stop.opa         = LV_OPA_30;
    stop.center.x    = cx;
    stop.center.y    = cy;
    stop.radius      = gauge->radius + gauge_grow(gauge, 6);
    stop.start_angle = gauge->start_angle - 2;
    stop.end_angle   = gauge->start_angle + 2;
    lv_draw_arc(layer, &stop);
    stop.start_angle = gauge->end_angle - 2;
    stop.end_angle   = gauge->end_angle + 2;
    lv_draw_arc(layer, &stop);

    if (gauge->style == HMI_GAUGE_STYLE_CYAN) {
        lv_draw_arc_dsc_t dot;
        lv_draw_arc_dsc_init(&dot);
        dot.width       = gauge_grow(gauge, 4);
        dot.rounded     = true;
        dot.radius      = gauge_grow(gauge, 4);
        dot.start_angle = 0;
        dot.end_angle   = 360;
        dot.opa         = LV_OPA_60;
        int32_t m = gauge_grow(gauge, 12);
        dot.color = gauge->accent;
        dot.center.x = area->x1 + m; dot.center.y = area->y1 + m;
        lv_draw_arc(layer, &dot);
        dot.color = gauge->accent2;
        dot.center.x = area->x2 - m; dot.center.y = area->y1 + m;
        lv_draw_arc(layer, &dot);
        dot.color = gauge->accent2;
        dot.center.x = area->x1 + m; dot.center.y = area->y2 - m;
        lv_draw_arc(layer, &dot);
        dot.color = gauge->accent;
        dot.center.x = area->x2 - m; dot.center.y = area->y2 - m;
        lv_draw_arc(layer, &dot);
    } else if (gauge->style == HMI_GAUGE_STYLE_PINK) {
        lv_draw_line_dsc_t bk;
        lv_draw_line_dsc_init(&bk);
        bk.color = gauge->accent;
        bk.width = 2;
        bk.opa   = LV_OPA_50;
        int32_t s = gauge_grow(gauge, 8), g = gauge_grow(gauge, 6);
        bk.p1 = (lv_point_precise_t){ area->x1 + g, area->y1 + g + s };
        bk.p2 = (lv_point_precise_t){ area->x1 + g, area->y1 + g };
        lv_draw_line(layer, &bk);
        bk.p1 = (lv_point_precise_t){ area->x1 + g, area->y1 + g };
        bk.p2 = (lv_point_precise_t){ area->x1 + g + s, area->y1 + g };
        lv_draw_line(layer, &bk);
        bk.p1 = (lv_point_precise_t){ area->x2 - g - s, area->y1 + g };
        bk.p2 = (lv_point_precise_t){ area->x2 - g, area->y1 + g };
        lv_draw_line(layer, &bk);
        bk.p1 = (lv_point_precise_t){ area->x2 - g, area->y1 + g };
        bk.p2 = (lv_point_precise_t){ area->x2 - g, area->y1 + g + s };
        lv_draw_line(layer, &bk);
        bk.p1 = (lv_point_precise_t){ area->x1 + g, area->y2 - g - s };
        bk.p2 = (lv_point_precise_t){ area->x1 + g, area->y2 - g };
        lv_draw_line(layer, &bk);
        bk.p1 = (lv_point_precise_t){ area->x1 + g, area->y2 - g };
        bk.p2 = (lv_point_precise_t){ area->x1 + g + s, area->y2 - g };
        lv_draw_line(layer, &bk);
        bk.p1 = (lv_point_precise_t){ area->x2 - g - s, area->y2 - g };
        bk.p2 = (lv_point_precise_t){ area->x2 - g, area->y2 - g };
        lv_draw_line(layer, &bk);
        bk.p1 = (lv_point_precise_t){ area->x2 - g, area->y2 - g };
        bk.p2 = (lv_point_precise_t){ area->x2 - g, area->y2 - g - s };
        lv_draw_line(layer, &bk);
    } else {
        lv_draw_line_dsc_t line;
        lv_draw_line_dsc_init(&line);
        line.color = gauge->accent;
        line.width = 1;
        line.opa   = LV_OPA_20;
        int32_t m = gauge_grow(gauge, 8);
        line.p1 = (lv_point_precise_t){ area->x1 + m, area->y1 + m };
        line.p2 = (lv_point_precise_t){ area->x2 - m, area->y1 + m };
        lv_draw_line(layer, &line);
        line.p1 = (lv_point_precise_t){ area->x2 - m, area->y1 + m };
        line.p2 = (lv_point_precise_t){ area->x2 - m, area->y2 - m };
        lv_draw_line(layer, &line);
        line.p1 = (lv_point_precise_t){ area->x2 - m, area->y2 - m };
        line.p2 = (lv_point_precise_t){ area->x1 + m, area->y2 - m };
        lv_draw_line(layer, &line);
        line.p1 = (lv_point_precise_t){ area->x1 + m, area->y2 - m };
        line.p2 = (lv_point_precise_t){ area->x1 + m, area->y1 + m };
        lv_draw_line(layer, &line);
    }
}

static void gauge_draw_base_arc(lv_layer_t * layer, const hmi_gauge_t * gauge, int32_t cx, int32_t cy)
{
    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init(&arc);
    arc.color       = gauge->outer_arc;
    arc.width       = gauge_grow(gauge, gauge->outer_arc_width);
    arc.rounded     = true;
    arc.center.x    = cx;
    arc.center.y    = cy;
    arc.radius      = gauge->radius;
    arc.start_angle = gauge->start_angle;
    arc.end_angle   = gauge->end_angle;
    lv_draw_arc(layer, &arc);

    lv_draw_arc_dsc_init(&arc);
    arc.color       = gauge->inner_arc;
    arc.width       = gauge_grow(gauge, gauge->inner_arc_width);
    arc.rounded     = true;
    arc.center.x    = cx;
    arc.center.y    = cy;
    arc.radius      = gauge->radius;
    arc.start_angle = gauge->start_angle;
    arc.end_angle   = gauge->end_angle;
    lv_draw_arc(layer, &arc);
}

static void gauge_draw_zones(lv_layer_t * layer, const hmi_gauge_t * gauge, int32_t cx, int32_t cy)
{
    for (uint8_t i = 0; i < gauge->zone_count; i++) {
        lv_draw_arc_dsc_t arc;
        lv_draw_arc_dsc_init(&arc);
        arc.color       = gauge->zones[i].color;
        arc.width       = (gauge->style == HMI_GAUGE_STYLE_CYAN)
                            ? gauge_grow(gauge, 12)
                            : ((gauge->style == HMI_GAUGE_STYLE_PINK) ? gauge_grow(gauge, 18) : gauge_grow(gauge, 16));
        arc.rounded     = true;
        arc.center.x    = cx;
        arc.center.y    = cy;
        arc.radius      = gauge->radius;
        arc.start_angle = gauge_value_to_angle(gauge, gauge->zones[i].min);
        arc.end_angle   = gauge_value_to_angle(gauge, gauge->zones[i].max);
        lv_draw_arc(layer, &arc);
    }
}

static void gauge_draw_ticks(lv_layer_t * layer, const hmi_gauge_t * gauge, int32_t cx, int32_t cy)
{
    if (gauge->tick_count == 0) return;

    int32_t tick_inner = gauge_len(gauge, gauge->tick_inner);
    int32_t tick_outer = gauge_len(gauge, gauge->tick_outer);

    for (uint16_t i = 0; i <= gauge->tick_count; i++) {
        float ratio = (float)i / (float)gauge->tick_count;
        float angle = hmi_map(ratio, 0.0f, 1.0f, gauge->start_angle, gauge->end_angle);

        if (tick_inner > 0) {
            lv_point_precise_t inner = hmi_polar_to_point(cx, cy, gauge->radius - tick_inner, angle);
            lv_point_precise_t outer = hmi_polar_to_point(cx, cy, gauge->radius, angle);
            lv_draw_line_dsc_t line;
            lv_draw_line_dsc_init(&line);
            line.color = COLOR_TEXT;
            line.width = gauge_grow(gauge, gauge->tick_width);
            line.p1    = inner;
            line.p2    = outer;
            lv_draw_line(layer, &line);
        }
        if (tick_outer > 0) {
            lv_point_precise_t inner = hmi_polar_to_point(cx, cy, gauge->radius, angle);
            lv_point_precise_t outer = hmi_polar_to_point(cx, cy, gauge->radius + tick_outer, angle);
            lv_draw_line_dsc_t line;
            lv_draw_line_dsc_init(&line);
            line.color = COLOR_TEXT;
            line.width = gauge_grow(gauge, gauge->tick_width);
            line.p1    = inner;
            line.p2    = outer;
            lv_draw_line(layer, &line);
        }
    }
}

static void gauge_draw_needle(lv_layer_t * layer, const hmi_gauge_t * gauge, int32_t cx, int32_t cy)
{
    float angle     = gauge_value_to_angle(gauge, gauge->display_value);
    lv_color_t color = gauge_needle_color(gauge);
    int32_t needle_end = gauge->radius - gauge_len(gauge, gauge->needle_offset);

    if (gauge->style == HMI_GAUGE_STYLE_CYAN) {
        lv_point_precise_t end = hmi_polar_to_point(cx, cy, needle_end, angle);
        lv_point_precise_t center = { .x = cx, .y = cy };
        lv_draw_line_dsc_t line;
        lv_draw_line_dsc_init(&line);
        line.color = color;
        line.width = gauge_grow(gauge, gauge->needle_width);
        line.p1    = center;
        line.p2    = end;
        lv_draw_line(layer, &line);
        lv_draw_arc_dsc_t hub;
        lv_draw_arc_dsc_init(&hub);
        hub.color       = color;
        hub.width       = gauge_grow(gauge, 8);
        hub.rounded     = true;
        hub.center.x    = cx;
        hub.center.y    = cy;
        hub.radius      = gauge_grow(gauge, 4);
        hub.start_angle = 0;
        hub.end_angle   = 360;
        lv_draw_arc(layer, &hub);
    } else if (gauge->style == HMI_GAUGE_STYLE_PINK) {
        lv_point_precise_t end = hmi_polar_to_point(cx, cy, needle_end, angle);
        lv_point_precise_t center = { .x = cx, .y = cy };
        lv_draw_line_dsc_t line;
        lv_draw_line_dsc_init(&line);
        line.color = color;
        line.width = gauge_grow(gauge, gauge->needle_width);
        line.round_start = 1;
        line.round_end   = 1;
        line.p1    = center;
        line.p2    = end;
        lv_draw_line(layer, &line);
        lv_draw_arc_dsc_t hub;
        lv_draw_arc_dsc_init(&hub);
        hub.color       = color;
        hub.width       = gauge_grow(gauge, 14);
        hub.rounded     = true;
        hub.center.x    = cx;
        hub.center.y    = cy;
        hub.radius      = gauge_grow(gauge, 7);
        hub.start_angle = 0;
        hub.end_angle   = 360;
        lv_draw_arc(layer, &hub);
        lv_draw_arc_dsc_init(&hub);
        hub.color       = gauge->accent2;
        hub.width       = gauge_grow(gauge, 4);
        hub.rounded     = true;
        hub.center.x    = cx;
        hub.center.y    = cy;
        hub.radius      = gauge_grow(gauge, 3);
        hub.start_angle = 0;
        hub.end_angle   = 360;
        lv_draw_arc(layer, &hub);
    } else {
        lv_point_precise_t end = hmi_polar_to_point(cx, cy, needle_end, angle);
        lv_point_precise_t cw  = hmi_polar_to_point(cx, cy, -gauge_len(gauge, 25), angle);
        lv_point_precise_t center = { .x = cx, .y = cy };
        lv_draw_line_dsc_t line;
        lv_draw_line_dsc_init(&line);
        line.color = color;
        line.width = gauge_grow(gauge, gauge->needle_width);
        line.p1    = cw;
        line.p2    = end;
        line.opa   = LV_OPA_80;
        lv_draw_line(layer, &line);
        lv_draw_line_dsc_init(&line);
        line.color = color;
        line.width = 1;
        line.opa   = LV_OPA_30;
        line.p1    = center;
        line.p2    = cw;
        lv_draw_line(layer, &line);
        lv_draw_arc_dsc_t hub;
        lv_draw_arc_dsc_init(&hub);
        hub.color       = color;
        hub.width       = gauge_grow(gauge, 8);
        hub.rounded     = true;
        hub.center.x    = cx;
        hub.center.y    = cy;
        hub.radius      = gauge_grow(gauge, 4);
        hub.start_angle = 0;
        hub.end_angle   = 360;
        lv_draw_arc(layer, &hub);
        lv_draw_arc_dsc_init(&hub);
        hub.color       = gauge->panel_bg;
        hub.width       = gauge_grow(gauge, 3);
        hub.rounded     = true;
        hub.center.x    = cx;
        hub.center.y    = cy;
        hub.radius      = gauge_grow(gauge, 2);
        hub.start_angle = 0;
        hub.end_angle   = 360;
        lv_draw_arc(layer, &hub);
    }
}

static void gauge_draw_peak(lv_layer_t * layer, const hmi_gauge_t * gauge, int32_t cx, int32_t cy)
{
    if (!gauge->peak_enable) return;

    /* place the marker just inside the scale ring so it never clips
     * the widget bounds on large dials */
    float angle = gauge_value_to_angle(gauge, gauge->peak_value);
    int32_t pr = gauge->radius - gauge_len(gauge, gauge->tick_outer) - gauge_grow(gauge, 6);
    if (pr < gauge->radius / 2) pr = gauge->radius / 2;
    lv_point_precise_t point = hmi_polar_to_point(cx, cy, pr, angle);

    lv_draw_arc_dsc_t marker;
    lv_draw_arc_dsc_init(&marker);
    marker.color       = COLOR_WARN;
    marker.width       = gauge_grow(gauge, 8);
    marker.rounded     = true;
    marker.center.x    = (int32_t)point.x;
    marker.center.y    = (int32_t)point.y;
    marker.radius      = gauge_grow(gauge, 4);
    marker.start_angle = 0;
    marker.end_angle   = 360;
    lv_draw_arc(layer, &marker);
}

static void gauge_draw_text(lv_layer_t * layer, const hmi_gauge_t * gauge, int32_t cx, int32_t cy)
{
    char buffer[32];
    lv_area_t area;
    lv_draw_label_dsc_t label;
    int32_t r = gauge->radius;

    /* outer edge of the scale ring: zone arc half-width + outer ticks */
    int32_t ring_out = r + gauge_grow(gauge, gauge->outer_arc_width) / 2
                        + gauge_len(gauge, gauge->tick_outer) + 6;

    snprintf(buffer, sizeof(buffer), "%.*f", gauge->precision, gauge->display_value);

    lv_draw_label_dsc_init(&label);
    label.align = LV_TEXT_ALIGN_CENTER;
    label.text  = buffer;
    label.color = gauge->accent;
    /* value sits in the lower part of the dial, clear of the hub */
    int32_t vy = cy + r * 35 / 100;
    area = (lv_area_t){ cx - r * 65 / 100, vy - 11, cx + r * 65 / 100, vy + 13 };
    lv_draw_label(layer, &label, &area);
    label.text  = gauge->unit;
    label.color = COLOR_DIM;
    area.y1 = vy + 16; area.y2 = vy + 30;
    lv_draw_label(layer, &label, &area);

    /* title floats above the dial with an underline */
    label.text = gauge->title;
    area.y2 = cy - ring_out - 4;
    area.y1 = area.y2 - 16;
    lv_draw_label(layer, &label, &area);
    lv_draw_line_dsc_t ul;
    lv_draw_line_dsc_init(&ul);
    ul.color = gauge->accent;
    ul.width = 2;
    ul.p1 = (lv_point_precise_t){ .x = cx - r * 35 / 100, .y = area.y2 + 3 };
    ul.p2 = (lv_point_precise_t){ .x = cx + r * 35 / 100, .y = area.y2 + 3 };
    lv_draw_line(layer, &ul);
}

static void hmi_gauge_draw_event(lv_event_t * event)
{
    HMI_DRAW_EVENT_BEGIN(hmi_gauge_t, obj, gauge);
    int32_t cx = (coords.x1 + coords.x2) / 2;
    int32_t cy = (coords.y1 + coords.y2) / 2;

    int32_t w = coords.x2 - coords.x1;
    int32_t h = coords.y2 - coords.y1;
    /* derive the dial radius from the widget size; margin keeps ticks,
     * peak marker and title inside the bounds */
    int32_t r = LV_MIN(w, h) / 2 - 20;
    if (r < 30) r = 30;
    gauge->radius = r;

    gauge_draw_background(layer, gauge, &coords, cx, cy);
    gauge_draw_zones(layer, gauge, cx, cy);
    gauge_draw_base_arc(layer, gauge, cx, cy);
    gauge_draw_ticks(layer, gauge, cx, cy);
    gauge_draw_needle(layer, gauge, cx, cy);
    gauge_draw_peak(layer, gauge, cx, cy);
    gauge_draw_text(layer, gauge, cx, cy);
}

static void hmi_gauge_animation(lv_timer_t * timer)
{
    hmi_gauge_t * gauge = lv_timer_get_user_data(timer);
    if (gauge == NULL) return;

    if (gauge->animation_enable) {
        gauge->display_value += (gauge->value - gauge->display_value) * 0.08f;
    } else {
        gauge->display_value = gauge->value;
    }

    if (gauge->peak_enable && gauge->value > gauge->peak_value) {
        gauge->peak_value = gauge->value;
    }

    lv_obj_invalidate(gauge->obj);
}

hmi_gauge_t * hmi_gauge_create(lv_obj_t * parent)
{
    hmi_gauge_t * gauge = lv_malloc(sizeof(hmi_gauge_t));
    if (gauge == NULL) return NULL;

    memset(gauge, 0, sizeof(hmi_gauge_t));

    gauge->minimum          = 0.0f;
    gauge->maximum          = 100.0f;
    gauge->tick_count       = 10;
    gauge->precision        = 1;
    gauge->animation_enable = true;
    gauge->alarm            = HMI_GAUGE_OK;
    strcpy(gauge->title, "VALUE");
    gauge->unit[0] = '\0';

    gauge->obj = lv_obj_create(parent);
    lv_obj_remove_style_all(gauge->obj);
    lv_obj_set_size(gauge->obj, HMI_GAUGE_DEFAULT_WIDTH, HMI_GAUGE_DEFAULT_HEIGHT);

    hmi_gauge_set_style(gauge, HMI_GAUGE_STYLE_CYAN);

    lv_obj_add_event_cb(gauge->obj, hmi_gauge_draw_event, LV_EVENT_DRAW_MAIN, gauge);

    gauge->timer = lv_timer_create(hmi_gauge_animation, HMI_GAUGE_ANIM_PERIOD, gauge);

    return gauge;
}

void hmi_gauge_set_style(hmi_gauge_t * gauge, hmi_gauge_style_t style)
{
    if (gauge == NULL) return;
    gauge->style = style;

    gauge->panel_bg     = lv_color_hex(0x16162A);
    gauge->panel_border = lv_color_hex(0x2A2A4A);
    gauge->outer_arc    = lv_color_hex(0x1A1A3A);
    gauge->inner_arc    = lv_color_hex(0x1A1A30);
    gauge->accent       = lv_color_hex(0x00E5FF);
    gauge->accent2      = lv_color_hex(0xFF00AA);

    switch (style) {
        case HMI_GAUGE_STYLE_CYAN:
            gauge->start_angle  = 135.0f;
            gauge->end_angle    = 405.0f;
            gauge->needle_width = 4;
            gauge->needle_offset = 22;
            gauge->outer_arc_width = 18;
            gauge->inner_arc_width = 10;
            gauge->tick_width   = 1;
            gauge->tick_inner   = 0;
            gauge->tick_outer   = 8;
            break;
        case HMI_GAUGE_STYLE_PINK:
            gauge->start_angle  = 0.0f;
            gauge->end_angle    = 360.0f;
            gauge->needle_width = 7;
            gauge->needle_offset = 42;
            gauge->outer_arc_width = 24;
            gauge->inner_arc_width = 16;
            gauge->tick_width   = 2;
            gauge->tick_inner   = 6;
            gauge->tick_outer   = 0;
            break;
        case HMI_GAUGE_STYLE_AMBER:
            gauge->start_angle  = 180.0f;
            gauge->end_angle    = 360.0f;
            gauge->needle_width = 5;
            gauge->needle_offset = 16;
            gauge->outer_arc_width = 20;
            gauge->inner_arc_width = 12;
            gauge->tick_width   = 1;
            gauge->tick_inner   = 6;
            gauge->tick_outer   = 6;
            break;
    }
}

void hmi_gauge_delete(hmi_gauge_t * gauge)
{
    if (gauge == NULL) return;
    if (gauge->timer) lv_timer_delete(gauge->timer);
    lv_obj_delete(gauge->obj);
    lv_free(gauge);
}

void hmi_gauge_set_value(hmi_gauge_t * gauge, float value)
{
    if (gauge == NULL) return;
    gauge->value = hmi_clamp(value, gauge->minimum, gauge->maximum);
}

float hmi_gauge_get_value(const hmi_gauge_t * gauge)
{
    if (gauge == NULL) return 0.0f;
    return gauge->value;
}

void hmi_gauge_set_range(hmi_gauge_t * gauge, float min, float max)
{
    if (gauge == NULL) return;
    gauge->minimum = min;
    gauge->maximum = max;
    gauge->value   = hmi_clamp(gauge->value, min, max);
}

void hmi_gauge_set_precision(hmi_gauge_t * gauge, uint8_t precision)
{
    if (gauge == NULL) return;
    gauge->precision = precision;
}

void hmi_gauge_set_tick_count(hmi_gauge_t * gauge, uint16_t count)
{
    if (gauge == NULL) return;
    gauge->tick_count = count;
}

void hmi_gauge_set_title(hmi_gauge_t * gauge, const char * title)
{
    if (gauge == NULL || title == NULL) return;
    strncpy(gauge->title, title, sizeof(gauge->title) - 1);
    gauge->title[sizeof(gauge->title) - 1] = '\0';
}

void hmi_gauge_set_unit(hmi_gauge_t * gauge, const char * unit)
{
    if (gauge == NULL || unit == NULL) return;
    strncpy(gauge->unit, unit, sizeof(gauge->unit) - 1);
    gauge->unit[sizeof(gauge->unit) - 1] = '\0';
}

bool hmi_gauge_add_zone(hmi_gauge_t * gauge, float min, float max, lv_color_t color)
{
    if (gauge == NULL) return false;
    if (gauge->zone_count >= HMI_GAUGE_MAX_ZONE) return false;

    gauge->zones[gauge->zone_count] = (hmi_gauge_zone_t){ .min = min, .max = max, .color = color };
    gauge->zone_count++;
    return true;
}

void hmi_gauge_clear_zones(hmi_gauge_t * gauge)
{
    if (gauge == NULL) return;
    gauge->zone_count = 0;
}

void hmi_gauge_set_alarm(hmi_gauge_t * gauge, hmi_gauge_alarm_t alarm)
{
    if (gauge == NULL) return;
    gauge->alarm = alarm;
}

hmi_gauge_alarm_t hmi_gauge_get_alarm(const hmi_gauge_t * gauge)
{
    if (gauge == NULL) return HMI_GAUGE_OK;
    return gauge->alarm;
}

void hmi_gauge_set_peak_enable(hmi_gauge_t * gauge, bool enable)
{
    if (gauge == NULL) return;
    gauge->peak_enable = enable;
}

void hmi_gauge_reset_peak(hmi_gauge_t * gauge)
{
    if (gauge == NULL) return;
    gauge->peak_value = gauge->minimum;
}

void hmi_gauge_set_animation_enable(hmi_gauge_t * gauge, bool enable)
{
    if (gauge == NULL) return;
    gauge->animation_enable = enable;
    if (!enable) gauge->display_value = gauge->value;
}

lv_obj_t * hmi_gauge_get_obj(const hmi_gauge_t * gauge)
{
    if (gauge == NULL) return NULL;
    return gauge->obj;
}

#if 1

typedef struct {
    hmi_gauge_t * gauge;
    float phase;
    float center;
    float amplitude;
    float speed;
} gauge_demo_t;

static gauge_demo_t demo_voltage;
static gauge_demo_t demo_current;
static gauge_demo_t demo_frequency;

static void gauge_demo_timer(lv_timer_t * timer)
{
    (void)timer;
    float t = lv_tick_get() / 1000.0f;

    hmi_gauge_set_value(demo_voltage.gauge,
        demo_voltage.center + demo_voltage.amplitude * sinf(demo_voltage.phase + t * demo_voltage.speed));

    hmi_gauge_set_value(demo_current.gauge,
        demo_current.center + demo_current.amplitude * sinf(demo_current.phase + t * demo_current.speed));

    hmi_gauge_set_value(demo_frequency.gauge,
        demo_frequency.center + demo_frequency.amplitude * sinf(demo_frequency.phase + t * demo_frequency.speed));
}

void gauge_example(void)
{
    lv_obj_t * scr = lv_screen_active();

    lv_obj_set_style_bg_color(scr, lv_color_hex(0xE8ECF0), 0);

    lv_obj_t * cont = lv_obj_create(scr);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    demo_voltage = (gauge_demo_t){
        .center    = 400.0f,
        .amplitude = 80.0f,
        .phase     = 0.0f,
        .speed     = 0.5f,
        .gauge     = hmi_gauge_create(cont)
    };
    hmi_gauge_set_title(demo_voltage.gauge, "VOLTAGE");
    hmi_gauge_set_unit(demo_voltage.gauge, "V");
    hmi_gauge_set_range(demo_voltage.gauge, 250.0f, 480.0f);
    hmi_gauge_set_precision(demo_voltage.gauge, 1);
    hmi_gauge_set_tick_count(demo_voltage.gauge, 8);
    hmi_gauge_set_peak_enable(demo_voltage.gauge, true);
    hmi_gauge_add_zone(demo_voltage.gauge, 250.0f, 300.0f, COLOR_ERROR);
    hmi_gauge_add_zone(demo_voltage.gauge, 300.0f, 350.0f, COLOR_WARN);
    hmi_gauge_add_zone(demo_voltage.gauge, 350.0f, 430.0f, COLOR_OK);
    hmi_gauge_add_zone(demo_voltage.gauge, 430.0f, 480.0f, COLOR_ERROR);

    demo_current = (gauge_demo_t){
        .center    = 50.0f,
        .amplitude = 30.0f,
        .phase     = 2.0f,
        .speed     = 0.7f,
        .gauge     = hmi_gauge_create(cont)
    };
    hmi_gauge_set_title(demo_current.gauge, "CURRENT");
    hmi_gauge_set_unit(demo_current.gauge, "A");
    hmi_gauge_set_range(demo_current.gauge, 0.0f, 100.0f);
    hmi_gauge_set_precision(demo_current.gauge, 1);
    hmi_gauge_set_tick_count(demo_current.gauge, 10);
    hmi_gauge_set_peak_enable(demo_current.gauge, true);
    hmi_gauge_add_zone(demo_current.gauge, 0.0f, 30.0f, COLOR_OK);
    hmi_gauge_add_zone(demo_current.gauge, 30.0f, 60.0f, COLOR_WARN);
    hmi_gauge_add_zone(demo_current.gauge, 60.0f, 100.0f, COLOR_ERROR);

    demo_frequency = (gauge_demo_t){
        .center    = 50.0f,
        .amplitude = 10.0f,
        .phase     = 4.0f,
        .speed     = 0.3f,
        .gauge     = hmi_gauge_create(cont)
    };
    hmi_gauge_set_title(demo_frequency.gauge, "FREQUENCY");
    hmi_gauge_set_unit(demo_frequency.gauge, "Hz");
    hmi_gauge_set_range(demo_frequency.gauge, 0.0f, 100.0f);
    hmi_gauge_set_precision(demo_frequency.gauge, 2);
    hmi_gauge_set_tick_count(demo_frequency.gauge, 10);
    hmi_gauge_add_zone(demo_frequency.gauge, 0.0f, 45.0f, COLOR_ERROR);
    hmi_gauge_add_zone(demo_frequency.gauge, 45.0f, 55.0f, COLOR_OK);
    hmi_gauge_add_zone(demo_frequency.gauge, 55.0f, 100.0f, COLOR_ERROR);

    lv_timer_create(gauge_demo_timer, 50, NULL);
}

#endif
