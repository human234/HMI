#ifndef CARD_H
#define CARD_H

#include <lvgl.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    IVC2_STATE_OFF     = 0,
    IVC2_STATE_READY   = 1,
    IVC2_STATE_RUNNING = 2,
    IVC2_STATE_WARNING = 3,
    IVC2_STATE_FAULT   = 4
} ivc2_state_t;

typedef enum {
    IVC2_ALARM_NONE     = 0,
    IVC2_ALARM_LOW      = 1,
    IVC2_ALARM_HIGH     = 2,
    IVC2_ALARM_CRITICAL = 3
} ivc2_alarm_t;

typedef struct card_t card_t;

card_t * card_create(lv_obj_t * parent);
void                        card_destroy(card_t * card);

void  card_set_value(card_t * card, float value);
float card_get_value(const card_t * card);

void  card_set_unit(card_t * card, const char * unit);
void  card_set_prefix(card_t * card, const char * prefix);
void  card_set_precision(card_t * card, uint8_t precision);

void  card_set_range(card_t * card, float minimum, float maximum);
void  card_set_alarm_limit(card_t * card, float low, float high);
ivc2_alarm_t card_get_alarm(const card_t * card);

void  card_push_sample(card_t * card, float value);
void  card_enable_sparkline(card_t * card, bool enable);

void  card_set_state(card_t * card, ivc2_state_t state);
void  card_enable_animation(card_t * card, bool enable);

lv_obj_t * card_get_object(const card_t * card);

void value_card_example(void);

#ifdef __cplusplus
}
#endif

#endif /* car_H */
