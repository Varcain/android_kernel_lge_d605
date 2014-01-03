/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/module.h>
#include "msm_camera_eeprom.h"
#include "msm_camera_i2c.h"

DEFINE_MUTEX(s5k4e1_eeprom_mutex);
static struct msm_eeprom_ctrl_t s5k4e1_eeprom_t;

static const struct i2c_device_id s5k4e1_eeprom_i2c_id[] = {
	{"s5k4e1_eeprom", (kernel_ulong_t)&s5k4e1_eeprom_t},
	{ }
};

static struct i2c_driver s5k4e1_eeprom_i2c_driver = {
	.id_table = s5k4e1_eeprom_i2c_id,
	.probe  = msm_eeprom_i2c_probe,
	.remove = __exit_p(s5k4e1_eeprom_i2c_remove),
	.driver = {
		.name = "s5k4e1_eeprom",
	},
};

static int __init s5k4e1_eeprom_i2c_add_driver(void)
{
	int rc = 0;
	rc = i2c_add_driver(s5k4e1_eeprom_t.i2c_driver);
	return rc;
}

static struct v4l2_subdev_core_ops s5k4e1_eeprom_subdev_core_ops = {
	.ioctl = msm_eeprom_subdev_ioctl,
};

static struct v4l2_subdev_ops s5k4e1_eeprom_subdev_ops = {
	.core = &s5k4e1_eeprom_subdev_core_ops,
};

uint8_t s5k4e1_wbcalib_data[6];
struct msm_calib_wb s5k4e1_wb_data;

static struct msm_camera_eeprom_info_t s5k4e1_calib_supp_info = {
	{FALSE, 0, 0, 1},
	{TRUE, 6, 0, 1024},
	{FALSE, 0, 0, 1},
	{FALSE, 0, 0, 1},
};

static struct msm_camera_eeprom_read_t s5k4e1_eeprom_read_tbl[] = {
	{0x60, &s5k4e1_wbcalib_data[0], 6, 1},
};


static struct msm_camera_eeprom_data_t s5k4e1_eeprom_data_tbl[] = {
	{&s5k4e1_wb_data, sizeof(struct msm_calib_wb)},
};

static void s5k4e1_format_wbdata(void)
{
	s5k4e1_wb_data.r_over_g = (uint16_t)(s5k4e1_wbcalib_data[0] << 8) |
		s5k4e1_wbcalib_data[1];
	s5k4e1_wb_data.b_over_g = (uint16_t)(s5k4e1_wbcalib_data[2] << 8) |
		s5k4e1_wbcalib_data[3];
	s5k4e1_wb_data.gr_over_gb = (uint16_t)(s5k4e1_wbcalib_data[4] << 8) |
		s5k4e1_wbcalib_data[5];
}

void s5k4e1_format_calibrationdata(void)
{
	s5k4e1_format_wbdata();
}
static struct msm_eeprom_ctrl_t s5k4e1_eeprom_t = {
	.i2c_driver = &s5k4e1_eeprom_i2c_driver,
	.i2c_addr = 0x28,
	.eeprom_v4l2_subdev_ops = &s5k4e1_eeprom_subdev_ops,

	.i2c_client = {
		.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	},

	.eeprom_mutex = &s5k4e1_eeprom_mutex,

	.func_tbl = {
		.eeprom_init = NULL,
		.eeprom_release = NULL,
		.eeprom_get_info = msm_camera_eeprom_get_info,
		.eeprom_get_data = msm_camera_eeprom_get_data,
		.eeprom_set_dev_addr = NULL,
		.eeprom_format_data = s5k4e1_format_calibrationdata,
	},
	.info = &s5k4e1_calib_supp_info,
	.info_size = sizeof(struct msm_camera_eeprom_info_t),
	.read_tbl = s5k4e1_eeprom_read_tbl,
	.read_tbl_size = ARRAY_SIZE(s5k4e1_eeprom_read_tbl),
	.data_tbl = s5k4e1_eeprom_data_tbl,
	.data_tbl_size = ARRAY_SIZE(s5k4e1_eeprom_data_tbl),
};

subsys_initcall(s5k4e1_eeprom_i2c_add_driver);
MODULE_DESCRIPTION("S5K4E1 EEPROM");
MODULE_LICENSE("GPL v2");
