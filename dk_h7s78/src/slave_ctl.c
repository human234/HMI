/*
 * slave_ctl.c -- SVPWM controller command API (slave side, STM32H7S78-DK)
 *
 * Packs electrical-frequency / voltage-magnitude / start-stop commands into
 * the SPI TX frame and asserts the notification GPIO (PD12) so the SPI master
 * (G474RE) knows a new command is ready and clocks the frame over its SPI1
 * link.
 *
 * The frame lives in a static buffer here; the SPI transceive in main.c must
 * point its TX buffer at slave_ctl_frame().
 */

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

/* Repack a full command into the frame and pulse the notify line. */
static void pack_and_notify(uint8_t cmd, const float *freq_hz, const float *mag_v)
{
    ctl_frame[0] = SLAVE_CTL_MAGIC;
    ctl_frame[1] = cmd;

    if (freq_hz) {
        uint16_t f100 = (uint16_t)((*freq_hz) * 100.0f);
        ctl_frame[2] = (uint8_t)(f100 >> 8);
        ctl_frame[3] = (uint8_t)(f100 & 0xFF);
    } else {
        ctl_frame[2] = 0;
        ctl_frame[3] = 0;
    }
    if (mag_v) {
        uint16_t m100 = (uint16_t)((*mag_v) * 100.0f);
        ctl_frame[4] = (uint8_t)(m100 >> 8);
        ctl_frame[5] = (uint8_t)(m100 & 0xFF);
    } else {
        ctl_frame[4] = 0;
        ctl_frame[5] = 0;
    }
    ctl_frame[6] = frame_crc(ctl_frame);
    ctl_frame[7] = SLAVE_CTL_TAIL;

    /* Pulse notify high so the master clocks the frame. */
    gpio_pin_set_dt(&notify, 1);
    gpio_pin_set_dt(&notify, 0);
}

void slave_ctl_set(float freq_hz, float mag_v)
{
    pack_and_notify(SLAVE_CTL_CMD_REF, &freq_hz, &mag_v);
}

void slave_ctl_set_frequency(float freq_hz)
{
    pack_and_notify(SLAVE_CTL_CMD_REF, &freq_hz, NULL);
}

void slave_ctl_set_magnitude(float mag_v)
{
    pack_and_notify(SLAVE_CTL_CMD_REF, NULL, &mag_v);
}

void slave_ctl_start(void)
{
    pack_and_notify(SLAVE_CTL_CMD_START, NULL, NULL);
}

void slave_ctl_stop(void)
{
    pack_and_notify(SLAVE_CTL_CMD_STOP, NULL, NULL);
}

/* Pack a persistent start/stop command together with the reference in one
 * frame. The app sends this on every SPI exchange so the master reliably
 * sees the current running state plus the commanded freq/mag (avoids the
 * transient race where a separate START/STOP pulse is overwritten before
 * the master reads it). */
void slave_ctl_set_state(uint8_t cmd, float freq_hz, float mag_v)
{
    pack_and_notify((cmd == SLAVE_CTL_CMD_START) ? SLAVE_CTL_CMD_START :
                       ((cmd == SLAVE_CTL_CMD_STOP) ? SLAVE_CTL_CMD_STOP :
                        SLAVE_CTL_CMD_REF),
                    &freq_hz, &mag_v);
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
