#include <lvgl.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/display.h>
#include "display/lv_display.h"
#include "hmi.h"

int main(void)
{
    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    hmi_create_complex(lv_screen_active());
    display_blanking_off(display_dev);

    // update ui screen
    while (1) {
        lv_timer_handler();
        k_sleep(K_MSEC(100));
    }
}
