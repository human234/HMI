#ifndef HMI_H
#define HMI_H

#include <stdbool.h>
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

void hmi_create(lv_obj_t * parent);
void hmi_create_complex(lv_obj_t * parent);
void hmi_create_control(lv_obj_t * parent);

/* Control-screen state accessors. The app reads these each SPI exchange so
 * the SVPWM frame update and the slave transceive run in the same thread. */
float hmi_ctl_get_frequency(void);
float hmi_ctl_get_magnitude(void);
bool  hmi_ctl_get_running(void);

#ifdef __cplusplus
}
#endif

#endif /* HMI_H */
