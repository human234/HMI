/*
 * slave_ctl.c -- SVPWM controller command API (slave side, STM32H7S78-DK)
 *
 * Packs electrical-frequency / voltage-magnitude / start-stop commands into
 * the SPI TX frame. The notification GPIO (PD12) is driven as a level by
 * slave_ctl_notify(); the SPI worker raises it only while the slave is
 * actually armed inside a blocking transceive, so the G474RE master clocks
 * the frame over its SPI1 link only when the slave is ready. On a missed
 * exchange the worker simply raises a fresh edge and retries.
 *
 * The frame lives in a static buffer here; the SPI transceive in main.c must
 * point its TX buffer at slave_ctl_frame().
 */

#include <stdbool.h>
#include <string.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>

#include "slave_ctl.h"

#define NOTIFY_NODE   DT_NODELABEL(gpiod)
#define NOTIFY_PIN    12u   /* PD12: slave -> master PC7 */

static uint8_t ctl_frame[SLAVE_CTL_LEN];

static const struct gpio_dt_spec notify = {
    .port = DEVICE_DT_GET(NOTIFY_NODE),
    .pin = NOTIFY_PIN,
    .dt_flags = GPIO_OUTPUT | GPIO_ACTIVE_HIGH,
};

static uint8_t frame_crc(const uint8_t *f)
{
    uint8_t c = 0;
    for (int i = 0; i < 6; i++) {
        c ^= f[i];
    }
    return c;
}

/* Pack a full command into the frame (does not touch the notify line). */
void slave_ctl_pack_state(uint8_t cmd, float freq_hz, float mag_v)
{
    ctl_frame[0] = SLAVE_CTL_MAGIC;

    if (cmd == SLAVE_CTL_CMD_START) {
        ctl_frame[1] = SLAVE_CTL_CMD_START;
    } else if (cmd == SLAVE_CTL_CMD_STOP) {
        ctl_frame[1] = SLAVE_CTL_CMD_STOP;
    } else {
        ctl_frame[1] = SLAVE_CTL_CMD_REF;
    }

    uint16_t f100 = (uint16_t)(freq_hz * 100.0f);
    ctl_frame[2] = (uint8_t)(f100 >> 8);
    ctl_frame[3] = (uint8_t)(f100 & 0xFF);

    uint16_t m100 = (uint16_t)(mag_v * 100.0f);
    ctl_frame[4] = (uint8_t)(m100 >> 8);
    ctl_frame[5] = (uint8_t)(m100 & 0xFF);

    ctl_frame[6] = frame_crc(ctl_frame);
    ctl_frame[7] = SLAVE_CTL_TAIL;
}

void slave_ctl_notify(bool level)
{
    gpio_pin_set_dt(&notify, level ? 1 : 0);
}

uint8_t *slave_ctl_frame(void)
{
    return ctl_frame;
}

int slave_ctl_init(void)
{
    ctl_frame[0] = SLAVE_CTL_MAGIC;
    ctl_frame[1] = SLAVE_CTL_CMD_REF;
    ctl_frame[2] = 0;
    ctl_frame[3] = 0;
    ctl_frame[4] = 0;
    ctl_frame[5] = 0;
    ctl_frame[6] = frame_crc(ctl_frame);
    ctl_frame[7] = SLAVE_CTL_TAIL;

    if (!device_is_ready(notify.port)) {
        return -ENODEV;
    }
    if (gpio_pin_configure_dt(&notify, GPIO_OUTPUT | GPIO_ACTIVE_HIGH) != 0) {
        return -EIO;
    }
    gpio_pin_set_dt(&notify, 0);

    return 0;
}