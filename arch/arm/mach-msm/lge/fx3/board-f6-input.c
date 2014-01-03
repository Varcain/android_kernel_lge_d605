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
#ifdef CONFIG_TOUCHSCREEN_MELFAS_MMS134

#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>

#include <linux/gpio_keys.h>
#include <linux/delay.h>
#include <mach/board_lge.h>
#include "board-fx3.h"

#define TOUCH_VD33_MAX_UV			2900000
#define TOUCH_VD33_CURR_UA			4230

#define TOUCH_IO_MAX_UV			1800000
#define TOUCH_IO_MIN_UV			1800000

#define TS_GPIO_I2C_SDA				16
#define TS_GPIO_I2C_SCL				17
#define TS_GPIO_IRQ					11
#define TS_GPIO_RESET				52
#define TS_GPIO_TOUCH_LDO			1
#define TS_GPIO_SENSOR_LDO			43
#define MSM_8930_TS_MAKER_ID		68


#define TOUCH_FW_VERSION       		0x01

#define MELFAS_TS_I2C_SLAVE_ADDR 	0x48

/*                                                
                                                
                                                
 */
#define GPIO_HOMEPAGE		PM8038_GPIO_PM_TO_SYS(1)
#define GPIO_VOLUME_UP		PM8038_GPIO_PM_TO_SYS(3)
#define GPIO_VOLUME_DOWN	PM8038_GPIO_PM_TO_SYS(5)
#define GPIO_QUICKMEMO		PM8038_GPIO_PM_TO_SYS(2)
/*#define GPIO_HOMEPAGE		PM8038_GPIO_PM_TO_SYS(1)
#define GPIO_VOLUME_UP		PM8038_GPIO_PM_TO_SYS(2)
#define GPIO_VOLUME_DOWN	PM8038_GPIO_PM_TO_SYS(3)
#define GPIO_QUICKMEMO		PM8038_GPIO_PM_TO_SYS(5)*/

static struct gpio_keys_button keys_8930[] = {
	{
		.code = KEY_VOLUMEUP,
		.type = EV_KEY,
		.desc = "volume_up",
		.gpio = GPIO_VOLUME_UP,
		.wakeup = 1,
		.active_low = 1,
		.debounce_interval = 15,
	},
	{
		.code = KEY_VOLUMEDOWN,
		.type = EV_KEY,
		.desc = "volume_down",
		.gpio = GPIO_VOLUME_DOWN,
		.wakeup = 1,
		.active_low = 1,
		.debounce_interval = 15,
	},
	{
		.code = KEY_HOMEPAGE,
		.type = EV_KEY,
		.desc = "homepage",
		.gpio = GPIO_HOMEPAGE,
		.wakeup = 1,
		.active_low = 1,
		.debounce_interval = 15,
	},	
	{
		.code = KEY_QUICKMEMO,
		.type = EV_KEY,
		.desc = "quickmemo",
		.gpio = GPIO_QUICKMEMO,
		.wakeup = 1,
		.active_low = 1,
		.debounce_interval = 15,
	},
};
/* Add GPIO keys for 8930 */
static struct gpio_keys_platform_data gpio_keys_8930_pdata = {
	.buttons = keys_8930,
	.nbuttons = 4,
};

static struct platform_device gpio_keys_8930 = {
	.name		= "gpio-keys",
	.id		= -1,
	.dev		= {
		.platform_data  = &gpio_keys_8930_pdata,
	},
};
//                                               


/*                                                                    */
bool dummy_touch = false;
static bool touch_req = false;
int vdd_set_on_off(int on)
{
	int rc = -EINVAL;

	if(!touch_req){
		/* Rev.A need enable SENSOR_LDO(1.8V) */
		rc = gpio_request(TS_GPIO_SENSOR_LDO, "sensor_ldo");
		if(rc < 0) {
	        pr_err("%s: sensor_ldo(%d) request fail\n", __func__,TS_GPIO_SENSOR_LDO);
	        return rc;
	    }
		gpio_direction_output(TS_GPIO_SENSOR_LDO, 1);   
		
		rc = gpio_request(TS_GPIO_TOUCH_LDO, "touch_ldo");
		if(rc < 0) {
	        pr_err("%s: touch_ldo(%d) request fail\n", __func__, TS_GPIO_TOUCH_LDO);
	        return rc;
	    }
		gpio_direction_output(TS_GPIO_TOUCH_LDO, 1);
		mdelay(20);

		touch_req = true;
	}
	
	gpio_direction_output(TS_GPIO_TOUCH_LDO, on);
	mdelay(20);
	
	gpio_set_value(TS_GPIO_TOUCH_LDO,on);
	mdelay(20);		

	return rc;	
}
int ts_set_vreg(int on, bool log_en)
{
	int rc = -EINVAL;

	if (log_en)
		printk(KERN_INFO "[Touch D] %s: power %s\n",
			__func__, on ? "On" : "Off");

	rc = vdd_set_on_off(on);
	msleep(30);
	return rc;
}

/* CONFIG_TOUCHSCREEN_MMS134S */
static struct melfas_tsi_platform_data melfas_ts_pdata = {
	.gpio_scl = TS_GPIO_I2C_SCL,
	.gpio_sda = TS_GPIO_I2C_SDA,
	.gpio_ldo = TS_GPIO_TOUCH_LDO,
	.i2c_int_gpio = TS_GPIO_IRQ,
	.power_enable = ts_set_vreg,
	.ic_booting_delay		= 400,		/* ms */
	.report_period			= 12500000, 	/* 12.5 msec */
	.num_of_finger			= 5,
	.num_of_button			= 2,
	.button[0]				= KEY_BACK,
	.button[1]				= KEY_MENU,
	.x_max					= 540,//= 556,
	.y_max					= 960,
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
};

void __init lge_add_input_devices(void)
{
	int rc = 0;
	gpio_tlmm_config(GPIO_CFG(TS_GPIO_IRQ, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(TS_GPIO_I2C_SDA, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(TS_GPIO_I2C_SCL, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	
	rc = gpio_request(MSM_8930_TS_MAKER_ID, "TOUCH_PANEL_MAKERID");
	
	if (unlikely(rc < 0))
		pr_err("%s not able to get gpio\n", __func__);
	

	/*  maker_id set 0 - normal , maker_id set 1 - dummy pattern */
	gpio_direction_input(MSM_8930_TS_MAKER_ID);
	
    ts_set_vreg(1, false);
	i2c_register_board_info(MSM_8930_GSBI3_QUP_I2C_BUS_ID,
			(&melfas_touch_panel_i2c_bdinfo[0]), 1);	

//   if(system_rev == HW_REV_A)
        platform_device_register(&gpio_keys_8930); 
//   else if(system_rev > HW_REV_A)
//        platform_device_register(&gpio_keys_8930_temp);
}
/*                                                                    */

#endif // CONFIG_TOUCHSCREEN_MELFAS_MMS134
