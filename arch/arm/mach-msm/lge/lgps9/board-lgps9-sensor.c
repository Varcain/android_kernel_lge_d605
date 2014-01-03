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

#include <linux/delay.h>
#include <mach/board_lge.h>
#include "board-lgps9.h"


#define SENSOR_VDD	2850000
#define SENSOR_IO 	1800000


/* acceleration platform data */
struct acceleration_platform_data {
	uint32_t version;
	int irq_num;
	int (*power)(unsigned char onoff);
};

/* ecompass platform data */
struct ecom_platform_data {
	uint32_t version;
	int irq_num;
	int (*power)(unsigned char onoff);
};

/* PROXIMITY platform data */
struct prox_platform_data {
	uint32_t version;
	int irq_num;
	int (*power)(unsigned char onoff);
};

int sensor_power(int on)
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
		SENSOR_VDD, SENSOR_VDD);

	vreg_lvs2 = regulator_get(NULL, "8038_lvs2");	/* 1P8_LVS2_TOUCH */
	if (IS_ERR(vreg_lvs2)) {
		pr_err("%s: regulator get of touch_1p8 failed (%ld)\n"
			, __func__, PTR_ERR(vreg_lvs2));
		rc = PTR_ERR(vreg_lvs2);
		return rc;
	}
	rc = regulator_set_voltage(vreg_lvs2,
		SENSOR_IO, SENSOR_IO);

	rc = regulator_enable(vreg_lvs2);
#if 0
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
#endif
	return rc;
}

static struct acceleration_platform_data bma250_pdata = {
	.irq_num = 46,
};

static struct ecom_platform_data bmm050_pdata = {
	.irq_num = 46,
};


static struct prox_platform_data apds9190_pdata = {
	.irq_num = 49,
};

static struct i2c_board_info bmm050_i2c_bdinfo[] = {
	[0] = {
		I2C_BOARD_INFO("bmm050", 0x10),
		.platform_data = &bmm050_pdata,
	},
};


static struct i2c_board_info bma250_i2c_bdinfo[] = {
	[0] = {
		I2C_BOARD_INFO("bma250", 0x18),
		.platform_data = &bma250_pdata,
	},
};

static struct i2c_board_info apds9190_i2c_bdinfo[] = {
	[0] = {
		I2C_BOARD_INFO("apds9190", 0x39),
		.platform_data = &apds9190_pdata,
	},
};


void __init lge_add_sensor_devices(void)
{
	gpio_tlmm_config(GPIO_CFG(44, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(45, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	/* Sensor power on */
	sensor_power(1);

	i2c_register_board_info(MSM_8930_GSBI12_QUP_I2C_BUS_ID,(&bma250_i2c_bdinfo[0]), 1);
	i2c_register_board_info(MSM_8930_GSBI12_QUP_I2C_BUS_ID,(&bmm050_i2c_bdinfo[0]), 1);
	i2c_register_board_info(MSM_8930_GSBI12_QUP_I2C_BUS_ID,(&apds9190_i2c_bdinfo[0]), 1);
}
