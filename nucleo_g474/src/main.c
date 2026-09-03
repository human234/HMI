#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "control.h"
#include "spi_cmd.h"

/*
 * g474_svpwm -- 3-phase inverter drive via SVPWM on TIM1.
 *
 * Transplants the STM32H743ZI SVPWM (control.c) to the NUCLEO-G474RE board,
 * with timer constants/registers corrected to match the verified G474RETEST
 * project. See control.c for the exact register values.
 */
int main(void)
{
    init_tim1_pwm();
    init_tim1_irq();
    spi_cmd_start();

    return 0;
}
