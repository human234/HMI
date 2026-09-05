#pragma once

/**
 * slave_ctl.h -- SVPWM controller command API (slave side, STM32H7S78-DK)
 *
 * The H7S78-DK acts as an LVGL touchscreen controller AND an SPI slave. The
 * touchscreen drives the G474RE SVPWM controller's electrical frequency and
 * voltage magnitude. Commands are packed into an 8-byte SPI frame in the
 * slave TX buffer; the SPI worker raises the notification GPIO (PD12) only
 * while the slave is armed inside a blocking transceive, so the master
 * (G474RE) clocks the frame over its SPI1 link only when the slave is ready.
 *
 * Frame (matches spi_cmd.c on the master):
 *   [0] magic 0xAA
 *   [1] cmd   0x01 = set reference | 0x02 = start | 0x03 = stop
 *   [2..3] frequency *100 big-endian
 *   [4..5] magnitude *100 big-endian
 *   [6] crc   XOR of [0..5]
 *   [7] tail  0x55
 *
 * Thread-safety: packing only writes the static TX frame and is called from
 * the SPI worker thread; the LVGL UI thread never touches this API.
 */

#include <stdbool.h>
#include <stdint.h>

#define SLAVE_CTL_MAGIC   0xAAu
#define SLAVE_CTL_CMD_REF 0x01u
#define SLAVE_CTL_CMD_START 0x02u
#define SLAVE_CTL_CMD_STOP  0x03u
#define SLAVE_CTL_TAIL    0x55u
#define SLAVE_CTL_LEN     8u

/* Returns pointer to the current 8-byte TX frame (master's read space). */
uint8_t *slave_ctl_frame(void);

/* Init: build the static frame envelope and configure PD12 notify output. */
int slave_ctl_init(void);

/* Pack START/STOP/REF plus frequency (Hz) and magnitude (V) into the TX
 * frame. Pure data operation: does NOT touch the notify line; the caller
 * drives it with slave_ctl_notify(). */
void slave_ctl_pack_state(uint8_t cmd, float freq_hz, float mag_v);

/* Drive the notification line (PD12) to the given level. The SPI worker
 * raises it before each armed transceive and lowers it afterwards, so a new
 * rising edge is available for every (re)transmission attempt. */
void slave_ctl_notify(bool level);