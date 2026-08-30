#include <zephyr/kernel.h>
#include <zephyr/drivers/display.h>
#include "ui.h"

int main(void)
{
    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    create_ui();
    display_blanking_off(display_dev);

    // update ui screen
    while (1) {
        lv_timer_handler();
        k_sleep(K_MSEC(100));
    }
}
