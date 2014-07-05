/*
  * Copyright (C) 2011 LGE, Inc.
  *
  * Author: Sungwoo Cho <sungwoo.cho@lge.com>
  *
  * This software is licensed under the terms of the GNU General
  * License version 2, as published by the Free Software Foundation,
  * may be copied, distributed, and modified under those terms.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
  * GNU General Public License for more details.
  */

#ifdef CONFIG_ANDROID_SW_IRRC
 
#include <linux/types.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <mach/gpiomux.h>

#include <mach/board_lge.h>

#include "devices.h"
#include <linux/android_irrc.h>
#include <linux/i2c.h>

#define GPIO_IRRC_PWM               3

static int irrc_init(void)
{
	int rc;

	/* gpio init */
	printk("[IRRC][%s] \n", __func__);
	rc = gpio_request(GPIO_IRRC_PWM, "irrc_pwm");
	gpio_direction_output(GPIO_IRRC_PWM, 1);
	if (unlikely(rc < 0))
		ERR_MSG("not able to get gpio\n");

	return 0;
}

static struct android_irrc_platform_data irrc_data = {
	.enable_status = 0,
	.irrc_init = irrc_init,
};

static struct platform_device android_irrc_device = {
	.name   = "android-irrc",
	.id = -1,
	.dev = {
		.platform_data = &irrc_data,
	},
};

void __init lge_add_misc_devices(void)
{
	printk("[IRRC][%s] \n", __func__);

	platform_device_register(&android_irrc_device);
}
#endif
