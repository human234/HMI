#ifndef HMI_H
#define HMI_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

void hmi_create(lv_obj_t * parent);
void hmi_create_complex(lv_obj_t * parent);

#ifdef __cplusplus
}
#endif

#endif /* HMI_H */
