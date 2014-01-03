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

#define GPIO_IRRC_PWM               37

/*
static struct gpiomux_setting irrc_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting irrc_active_cfg_gpio37 = {
	.func = GPIOMUX_FUNC_5,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct msm_gpiomux_config gpio37_irrc_configs[] = {
	{
		.gpio = GPIO_IRRC_PWM,
		.settings = {
			[GPIOMUX_ACTIVE]    = &irrc_active_cfg_gpio37,
			[GPIOMUX_SUSPENDED] = &irrc_suspend_cfg,
		},
	},
};
*/

static int irrc_init(void)
{
	int rc;

	/* GPIO function setting */
//	msm_gpiomux_install(gpio37_irrc_configs,
//			ARRAY_SIZE(gpio37_irrc_configs));

	/* gpio init */
	printk("[suker][IRRC][%s] \n", __func__);
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
	printk("[suker][IRRC][%s] \n", __func__);

	platform_device_register(&android_irrc_device);
}
#endif
