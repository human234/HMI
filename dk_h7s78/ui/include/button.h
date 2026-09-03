#ifndef BUTTON_H
#define BUTTON_H

#include <lvgl.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HMI_BTN_IDLE     = 0,
    HMI_BTN_HOVER    = 1,
    HMI_BTN_PRESSED  = 2,
    HMI_BTN_ACTIVE   = 3,
    HMI_BTN_DISABLED = 4
} hmi_btn_state_t;

typedef struct hmi_btn_t hmi_btn_t;

typedef void (*hmi_btn_event_cb_t)(hmi_btn_t * btn, lv_event_t * event);

hmi_btn_t * hmi_btn_create(lv_obj_t * parent);
void        hmi_btn_delete(hmi_btn_t * btn);

void        hmi_btn_set_text(hmi_btn_t * btn, const char * text);
void        hmi_btn_set_state(hmi_btn_t * btn, hmi_btn_state_t state);
hmi_btn_state_t hmi_btn_get_state(const hmi_btn_t * btn);

void        hmi_btn_set_led_enable(hmi_btn_t * btn, bool enable);
void        hmi_btn_set_led_color(hmi_btn_t * btn, lv_color_t color);

void        hmi_btn_set_pulse_enable(hmi_btn_t * btn, bool enable);

void        hmi_btn_on_click(hmi_btn_t * btn, hmi_btn_event_cb_t cb);

lv_obj_t *  hmi_btn_get_obj(const hmi_btn_t * btn);

void button_example(void);

#ifdef __cplusplus
}
#endif

#endif /* BUTTON_H */
