#pragma once

/**
 * slave_ctl.h -- SVPWM controller command API (slave side, STM32H7S78-DK)
 *
 * The H7S78-DK acts as an LVGL touchscreen controller AND an SPI slave. The
 * touchscreen drives the G474RE SVPWM controller's electrical frequency and
 * voltage magnitude. Commands set via this API are packed into an 8-byte SPI
 * frame in the slave TX buffer, and the notification GPIO (PD12) is asserted
 * so the master (G474RE) detects a rising edge and clocks the frame over its
 * SPI1 link.
 *
 * Frame (matches spi_cmd.c on the master):
 *   [0] magic 0xAA
 *   [1] cmd   0x01 = set reference | 0x02 = start | 0x03 = stop
 *   [2..3] frequency *100 big-endian
 *   [4..5] magnitude *100 big-endian
 *   [6] crc   XOR of [0..5]
 *   [7] tail  0x55
 *
 * Thread-safety: the pack/set/start/stop helpers are safe to call from the
 * LVGL UI thread; the SPI slave loop reads slave_ctl_frame() just before each
 * (blocking) transceive so it always shifts out the most recent committed
 * frame.
 */

#include <stdint.h>

#define SLAVE_CTL_MAGIC  0xAAu
#define SLAVE_CTL_CMD_REF  0x01u
#define SLAVE_CTL_CMD_START 0x02u
#define SLAVE_CTL_CMD_STOP  0x03u
#define SLAVE_CTL_TAIL   0x55u
#define SLAVE_CTL_LEN    8u

/* Returns pointer to the current 8-byte TX frame (master's read space). */
uint8_t *slave_ctl_frame(void);

/* Init: build the static frame envelope and configure PD12 notify output. */
int slave_ctl_init(void);

/* Set electrical frequency (Hz) and voltage magnitude (V).  Updates the frame
 * and asserts the notification line so the master reads the new reference. */
void slave_ctl_set(float freq_hz, float mag_v);

/* Raw accessors: set frequency only, magnitude only. */
void slave_ctl_set_frequency(float freq_hz);
void slave_ctl_set_magnitude(float mag_v);

/* Start / stop the SVPWM controller on the master. */
void slave_ctl_start(void);
void slave_ctl_stop(void);

/* Pack START (or STOP/REF) with the reference in one persistent frame, sent
 * every SPI exchange. cmd is one of SLAVE_CTL_CMD_START / _STOP / _REF. */
void slave_ctl_set_state(uint8_t cmd, float freq_hz, float mag_v);

/* Convenience macros matching the controller.js naming:
 *   ctl_set_speed(Hz), ctl_set_mag(V). */
#define ctl_set_speed(freq_hz)  slave_ctl_set_frequency((freq_hz))
#define ctl_set_mag(mag_v)      slave_ctl_set_magnitude((mag_v))