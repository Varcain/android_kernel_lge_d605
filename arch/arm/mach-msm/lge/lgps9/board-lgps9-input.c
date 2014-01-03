/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/input.h>
#include <mach/gpio.h>
#include <linux/platform_device.h>
#include <linux/input/pp2106-keypad.h>

#include <linux/delay.h>
#include <mach/board_lge.h>
#include "board-lgps9.h"

#if defined(CONFIG_HALLIC_S5712ACDL1)
static struct s5712ACDL1_platform_data s5712ACDL1_pdata = {
	.irq_pin = GPIO_S5712ACDL1_IRQ,
	.prohibit_time = 100,
};

static struct platform_device hallic_device = {
	.name = "hall-ic",
	.id = -1,
	.dev = {
		.platform_data = &s5712ACDL1_pdata,
	},
};

static struct platform_device *hallic_input_devices[] __initdata = {
	&hallic_device,
};
#endif

#ifdef CONFIG_KEYBOARD_PP2106
static unsigned short pp2106_keycode[PP2106_KEYPAD_ROW][PP2106_KEYPAD_COL] = {
	{ 0, 0, KEY_1, KEY_Q, KEY_A, KEY_LEFTSHIFT, KEY_LEFTALT, 0 },
	{ 0, 0, KEY_2, KEY_W, KEY_S, KEY_Z, KEY_SEARCH, 0 },
	{ 0, 0, KEY_3, KEY_E, 0, KEY_X, 0, KEY_VOLUMEUP },
	{ 0, 0, KEY_4, KEY_R, KEY_D, KEY_C, KEY_EMAIL, KEY_VOLUMEDOWN },
	{ 0, 0, KEY_5, KEY_T, KEY_F, KEY_V, KEY_SPACE, 0 },
	{ 0, 0, KEY_6, KEY_Y, KEY_G, KEY_B, 0, 0 },
	{ 0, 0, KEY_7, KEY_U, KEY_H, KEY_N, KEY_ENTER, KEY_DOT },
	{ 0, 0, KEY_8, KEY_I, KEY_J, KEY_M, KEY_LEFT, KEY_BACKSPACE },
};

static struct pp2106_keypad_platform_data pp2106_pdata = {
	.keypad_row = PP2106_KEYPAD_ROW,
	.keypad_col = PP2106_KEYPAD_COL,
	.keycode = (unsigned char *)pp2106_keycode,
	.reset_pin = 33,
	.irq_pin = 43,
	.irq_num = MSM_GPIO_TO_INT(43),
	.sda_pin = 8,
	.scl_pin = 9,
};

static struct platform_device pp2106_keypad_device = {
	.name = "pp2106-keypad",
	.id = 0,
	.dev = {
		.platform_data = &pp2106_pdata,
	},
};

static void pp2106_init_gpio(void) {
	gpio_tlmm_config(GPIO_CFG(pp2106_pdata.reset_pin,
		0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(pp2106_pdata.irq_pin,
		0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(pp2106_pdata.sda_pin,
		0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(pp2106_pdata.scl_pin,
		0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
}
#endif

//                  
#if defined(CONFIG_TOUCHSCREEN_MMS128)
int vdd_set_on_off(int on)
{
	int rc = -EINVAL;
	static struct regulator *vreg_l9, *vreg_lvs2;
	

	vreg_l9 = regulator_get(NULL, "8038_l9");	/* 3P0_L9_TOUCH */
	if (IS_ERR(vreg_l9)) {
		pr_err("%s: regulator get of touch_3p0 failed (%ld)\n"
			, __func__, PTR_ERR(vreg_l9));
		rc = PTR_ERR(vreg_l9);
		return rc;
	}
	rc = regulator_set_voltage(vreg_l9,
		MELFAS_VD33_MIN_UV, MELFAS_VD33_MAX_UV);

	vreg_lvs2 = regulator_get(NULL, "8038_lvs2");	/* 1P8_LVS2_TOUCH */
	if (IS_ERR(vreg_lvs2)) {
		pr_err("%s: regulator get of touch_1p8 failed (%ld)\n"
			, __func__, PTR_ERR(vreg_lvs2));
		rc = PTR_ERR(vreg_lvs2);
		return rc;
	}
	rc = regulator_set_voltage(vreg_lvs2,
		MELFAS_IO_MIN_UV, MELFAS_IO_MAX_UV);

	rc = regulator_enable(vreg_lvs2);
#ifndef Andrew_log
		pr_err("%s: touch regulator_get_voltage (%d)\n"
			, __func__, regulator_get_voltage(vreg_l9));
#endif

	if (on) {
		if (regulator_is_enabled(vreg_l9) <= 0) {
			rc = regulator_set_optimum_mode(vreg_l9, MELFAS_VD33_CURR_UA);
			rc = regulator_enable(vreg_l9);
		}
	} else {
		if (regulator_is_enabled(vreg_l9) > 0) {
			rc = regulator_set_optimum_mode(vreg_l9, 0);
			rc = regulator_force_disable(vreg_l9);
		}
	}
	return rc;
}

int ts_set_vreg(int on, bool log_en)
{
	int rc = -EINVAL;

	if (log_en)
		printk(KERN_INFO "[Touch D] %s: power %s\n",
			__func__, on ? "On" : "Off");

#if defined(CONFIG_USING_INNOTEK_PANEL_4_3)
	rc = ts_set_l9(on);
#else
	rc = vdd_set_on_off(on);
#endif
	msleep(30);
	return rc;
}

#endif

static struct melfas_tsi_platform_data melfas_ts_pdata = {
	.gpio_scl = TS_GPIO_I2C_SCL,
	.gpio_sda = TS_GPIO_I2C_SDA,
	.gpio_ldo = MSM_8960_TS_PWR,
	.i2c_int_gpio = TS_GPIO_IRQ,
	.power_enable = ts_set_vreg,
	.ic_booting_delay		= 400,		/* ms */
	.report_period			= 12500000, 	/* 12.5 msec */
	.num_of_finger			= 10,
	.num_of_button			= 4,
	.button[0]				= KEY_BACK,
	.button[1]				= KEY_HOMEPAGE,
	.button[2]				= KEY_MENU,
	.button[3]				= KEY_SEARCH,
	.x_max					= TS_X_MAX,
	.y_max					= TS_Y_MAX,
	.fw_ver					= TOUCH_FW_VERSION,
	.palm_threshold			= 0,
	.delta_pos_threshold	= 0,
};

static struct melfas_tsi_platform_data melfas_ts_pdata_vzw = {
	.gpio_scl = TS_GPIO_I2C_SCL,
	.gpio_sda = TS_GPIO_I2C_SDA,
	.gpio_ldo = MSM_8960_TS_PWR,
	.i2c_int_gpio = TS_GPIO_IRQ,
	.power_enable = ts_set_vreg,
	.ic_booting_delay		= 400,		/* ms */
	.report_period			= 12500000, 	/* 12.5 msec */
	.num_of_finger			= 10,
	.num_of_button			= 3,
	.button[0]				= KEY_BACK,
	.button[1]				= KEY_HOMEPAGE,
	.button[2]				= KEY_MENU,
	.x_max					= TS_X_MAX,
	.y_max					= TS_Y_MAX,
	.fw_ver					= TOUCH_FW_VERSION,
	.palm_threshold			= 0,
	.delta_pos_threshold	= 0,
};

static struct i2c_board_info melfas_touch_panel_i2c_bdinfo[] = {
	[0] = {
			I2C_BOARD_INFO(MELFAS_TS_NAME, MELFAS_TS_I2C_SLAVE_ADDR),
			.platform_data = &melfas_ts_pdata,
			.irq = MSM_GPIO_TO_INT(TS_GPIO_IRQ),
	},
	[1] = {
			I2C_BOARD_INFO(MELFAS_TS_NAME, MELFAS_TS_I2C_SLAVE_ADDR),
			.platform_data = &melfas_ts_pdata_vzw,
			.irq = MSM_GPIO_TO_INT(TS_GPIO_IRQ),
	},
};

void __init lge_add_input_devices(void)
{
	int rc;

	gpio_tlmm_config(GPIO_CFG(TS_GPIO_IRQ, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(TS_GPIO_I2C_SDA, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(TS_GPIO_I2C_SCL, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

#if defined(CONFIG_TOUCHSCREEN_ATMEL_MAXT224E) ||\
	defined(CONFIG_TOUCHSCREEN_MMS136) ||\
	defined(CONFIG_TOUCHSCREEN_MMS128)

	rc = gpio_request(MSM_8960_TS_MAKER_ID, "TOUCH_PANEL_MAKERID");

	if (unlikely(rc < 0))
		pr_err("%s not able to get gpio\n", __func__);

	/*  maker_id set 0 - normal , maker_id set 1 - dummy pattern */
	gpio_direction_input(MSM_8960_TS_MAKER_ID);

    ts_set_vreg(1, false);
		
//                                         
		i2c_register_board_info(MSM_8930_GSBI3_QUP_I2C_BUS_ID,
			(&melfas_touch_panel_i2c_bdinfo[0]), 1);
//	} else {
//		i2c_register_board_info(MSM_8930_GSBI3_QUP_I2C_BUS_ID,
//			(&melfas_touch_panel_i2c_bdinfo[1]), 1);
//	}
#endif

#ifdef CONFIG_KEYBOARD_PP2106
	pp2106_init_gpio();
	platform_device_register(&pp2106_keypad_device);
#endif

#if defined(CONFIG_HALLIC_S5712ACDL1)
    platform_add_devices(hallic_input_devices, ARRAY_SIZE(hallic_input_devices));
#endif
}
