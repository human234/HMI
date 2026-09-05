#include <string.h>
#include <lvgl.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/display.h>
#include <zephyr/device.h>
#include "display/lv_display.h"
#include "hmi.h"
#include "slave_ctl.h"

#define SPI_DEV_NODE DT_NODELABEL(spi4)

#define SPI_FREQ_HZ 1000000U

#define LVGL_TICK_MS 5u

static const struct device *spi_dev;

static uint8_t rx_buf[SLAVE_CTL_LEN];

static struct spi_buf tx_spi_buf;
static struct spi_buf rx_spi_buf;
static const struct spi_buf_set rx = { &rx_spi_buf, 1 };
static const struct spi_buf_set tx = { &tx_spi_buf, 1 };

/* The blocking SPI slave transceive runs on its own thread so a pending
 * exchange can never stall the LVGL renderer/input on the main thread. */
#define SPI_STACK_SIZE 2048u
#define SPI_PRIO       5u
K_THREAD_STACK_DEFINE(spi_stack, SPI_STACK_SIZE);
static struct k_thread spi_thread_data;

/* Slave, 8-bit word, MSB first, Motorola mode 0. Software slave-select
 * (SSM=1): the peripheral is always selected and each full-duplex burst
 * (clocked by the G474 master) shifts the latest TX frame onto MISO. */
static const struct spi_config spi_cfg = {
    .frequency = SPI_FREQ_HZ,
    .operation = SPI_OP_MODE_SLAVE | SPI_WORD_SET(8) |
                 SPI_TRANSFER_MSB,
    .slave = 0,
    .cs = { .gpio = { .port = NULL, .pin = 0, .dt_flags = 0 } },
};

/* Dedicated SPI slave worker: an always-listening loop that keeps the slave
 * armed inside a blocking transceive nearly 100% of the time. When the UI
 * state changes it packs the new frame and raises PD12 while the slave is
 * actually armed; the G474 master is edge-driven and clocks the frame, and
 * the transceive returns 0 only on a completed exchange. If the master never
 * clocks (frame lost to the arm race), the interrupt-mode transceive times
 * out (~264 ms), PD12 is lowered, and the change stays "dirty" so the next
 * iteration raises a fresh edge and retries -- the link self-heals instead
 * of wedging forever.
 *
 * Reads the frame right before each exchange so it always carries the latest
 * committed state (matches the header's documented "SPI slave loop"). */
static void spi_thread_fn(void *a, void *b, void *c)
{
    ARG_UNUSED(a);
    ARG_UNUSED(b);
    ARG_UNUSED(c);

    bool last_run = false;
    float last_freq = 0.0f;
    float last_mag = 0.0f;

    while (1) {
        bool run = hmi_ctl_get_running();
        float freq = hmi_ctl_get_frequency();
        float mag = hmi_ctl_get_magnitude();

        bool dirty = (run != last_run) || (freq != last_freq) ||
                     (mag != last_mag);

        if (dirty) {
            slave_ctl_pack_state(run ? SLAVE_CTL_CMD_START : SLAVE_CTL_CMD_STOP,
                                 freq, mag);
            slave_ctl_notify(true);
        }

        memset(rx_buf, 0, sizeof(rx_buf));
        rx_spi_buf.buf = rx_buf;
        rx_spi_buf.len = sizeof(rx_buf);
        tx_spi_buf.buf = slave_ctl_frame();
        tx_spi_buf.len = SLAVE_CTL_LEN;

        int ret = spi_transceive(spi_dev, &spi_cfg, &tx, &rx);

        slave_ctl_notify(false);

        if (ret == 0) {
            last_run = run;
            last_freq = freq;
            last_mag = mag;
        } else if (ret != -ETIMEDOUT) {
            printk("slave spi_transceive error: %d\n", ret);
        }
    }
}

int main(void)
{
    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

    spi_dev = DEVICE_DT_GET(SPI_DEV_NODE);
    if (!device_is_ready(spi_dev)) {
        printk("SPI4 slave not ready\n");
        return -1;
    }

    if (slave_ctl_init() != 0) {
        printk("slave_ctl init failed\n");
        return -1;
    }

    hmi_create_control(lv_screen_active());
    display_blanking_off(display_dev);

    k_thread_create(&spi_thread_data, spi_stack, SPI_STACK_SIZE,
                    spi_thread_fn, NULL, NULL, NULL, SPI_PRIO, 0, K_NO_WAIT);

    printk("SVPWM touchscreen controller ready (SPI4 slave + LVGL UI)\n");

    while (1) {
        lv_timer_handler();
        k_sleep(K_MSEC(LVGL_TICK_MS));
    }
    return 0;
}
