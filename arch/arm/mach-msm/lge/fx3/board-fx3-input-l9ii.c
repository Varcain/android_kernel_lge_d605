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

#ifndef CONFIG_TOUCHSCREEN_MELFAS_MMS134
//////////////////////////////////////////////////////

#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>

#if 0 //defined(CONFIG_KEYBOARD_PP2106)
#include <linux/input/pp2106-keypad.h>
#endif

#include <linux/gpio_keys.h>
#include <linux/delay.h>
#include <mach/board_lge.h>
#include "board-fx3.h"
#include <linux/input/lge_touch_core_l9ii.h>

#define TOUCH_VD33_MAX_UV			2850000
#define TOUCH_VD33_CURR_UA			4230

#define TOUCH_IO_MAX_UV			1800000
#define TOUCH_IO_MIN_UV			1800000

#define MSM_8960_TS_PWR             52
#define MSM_8960_TS_MAKER_ID        68

#define MSM_8930_TS_PWR             52
#define MSM_8930_TS_MAKER_ID        68

#define TS_GPIO_I2C_SDA				16
#define TS_GPIO_I2C_SCL				17
#define TS_GPIO_IRQ					11
//                                                 
#if defined(CONFIG_MACH_LGE_L9II_COMMON)
#define TS_GPIO_RESET				10
#define TS_GPIO_LDO_EN				1
#else
#define TS_GPIO_RESET				52
#endif
//                                                 

//                                                 
#if defined(CONFIG_MACH_LGE_L9II_COMMON)
#define TS_X_MAX             		720
#define TS_Y_MAX             		1280
#else
#define TS_X_MAX             		480
#define TS_Y_MAX             		800
#endif
//                                                 

#define TOUCH_FW_VERSION       		0x01

#define MELFAS_TS_I2C_SLAVE_ADDR 	0x48
#define LGE_TOUCH_SYNATICS_I2C_SLAVE_ADDR 	0x20

//                                                                  
#define GPIO_VOLUME_UP		PM8038_GPIO_PM_TO_SYS(2)
#define GPIO_VOLUME_DOWN	PM8038_GPIO_PM_TO_SYS(3)
#define GPIO_QUICK_MEMO		PM8038_GPIO_PM_TO_SYS(1)
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
		.code = KEY_QUICKMEMO,
		.type = EV_KEY,
		.desc = "quick_memo",
		.gpio = GPIO_QUICK_MEMO,
		.wakeup = 1,
		.active_low = 1,
		.debounce_interval = 15,
	},
};
/* Add GPIO keys for 8930 */
static struct gpio_keys_platform_data gpio_keys_8930_pdata = {
	.buttons = keys_8930,
	.nbuttons = 3,
};

static struct platform_device gpio_keys_8930 = {
	.name		= "gpio-keys",
	.id		= -1,
	.dev		= {
		.platform_data  = &gpio_keys_8930_pdata,
	},
};
//                                               
//                                                                             
#ifdef CONFIG_TOUCHSCREEN_MAX1187X
#include <linux/input/max1187x.h>

// start hosoeng.han
// static int max1187x_power(int on)
extern unsigned int system_rev;
static bool touch_req = false;
int max1187x_power(int on)
// end hoseong.han
{
	int rc = -EINVAL;
	static struct regulator *vreg_touch_3v;
	static struct regulator *vreg_touch_1p8;

	vreg_touch_3v = regulator_get(NULL, "8038_l8");
	if (IS_ERR(vreg_touch_3v)) {
		pr_err("could not get vreg_touch_3v (%ld)\n",
			PTR_ERR(vreg_touch_3v));
		rc = PTR_ERR(vreg_touch_3v);
		return rc;
	}

	rc = regulator_set_voltage(vreg_touch_3v, 3000000, 3000000);

	gpio_request(TS_GPIO_IRQ , "touch_int");
	gpio_request(TS_GPIO_I2C_SDA , "touch_sda");
	gpio_request(TS_GPIO_I2C_SCL , "touch_scl");

	if (system_rev > HW_REV_A)
	{
                if(!touch_req)
                {
                        gpio_request(TS_GPIO_LDO_EN, "touch_ldo");
                        gpio_direction_output(TS_GPIO_LDO_EN, 1);
                        touch_req = true;
                }
	}
	else
	{
		if (!vreg_touch_1p8) {
			vreg_touch_1p8 = regulator_get(NULL, "8038_l13");
			if (IS_ERR(vreg_touch_1p8)) {
				pr_err("could not get vreg_touch_1p8 (%ld)\n",
					PTR_ERR(vreg_touch_1p8));
				rc = PTR_ERR(vreg_touch_1p8);
				return rc;
			}
		}

		rc = regulator_set_voltage(vreg_touch_1p8, 1800000, 1800000);
	}

	if (on) {
		rc = gpio_tlmm_config(GPIO_CFG(TS_GPIO_IRQ, 0, GPIO_CFG_INPUT,
			GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		gpio_direction_input(TS_GPIO_IRQ);
		gpio_direction_output(TS_GPIO_I2C_SDA, 1);
		gpio_direction_output(TS_GPIO_I2C_SCL, 1);

                if (system_rev > HW_REV_A)
                        gpio_set_value(TS_GPIO_LDO_EN, 1);
                else
                        rc = regulator_enable(vreg_touch_1p8);
		rc = regulator_enable(vreg_touch_3v);
		msleep(5);
		gpio_set_value(TS_GPIO_RESET, 1);
	} else {
		rc = gpio_tlmm_config(GPIO_CFG(TS_GPIO_IRQ, 0, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		gpio_direction_output(TS_GPIO_IRQ, 0);
		gpio_direction_output(TS_GPIO_I2C_SDA, 0);

		gpio_set_value(TS_GPIO_RESET, 0);
		msleep(5);
                if (system_rev > HW_REV_A)
                        gpio_set_value(TS_GPIO_LDO_EN, 0);
                else
                        rc = regulator_disable(vreg_touch_1p8);
		rc = regulator_disable(vreg_touch_3v);

	}

	gpio_free(TS_GPIO_IRQ);
	gpio_free(TS_GPIO_I2C_SDA);
	gpio_free(TS_GPIO_I2C_SCL);

	return rc;
}

struct max1187x_pdata max1187x_platdata = {
	.gpio_tirq  = TS_GPIO_IRQ,
	.num_fw_mappings = 3,// Hanz_touch 2013-04-15 2,
	.fw_mapping[0] = {
		.config_id = 0x0CFD,
		.chip_id = 0x00,  //                                                                  
		.filename = "max11871.bin",
		.filesize = 0x8000, //  0xC000,
		.file_codesize = 0x8000 // 0xC000
	},
	.fw_mapping[1] = {
		.config_id = 0x0CFD,
		.chip_id = 0x57,
		.filename = "max11871.bin",
		.filesize = 0x8000, //  0xC000,
		.file_codesize = 0x8000 // 0xC000
	},
	.fw_mapping[2] = {
		.config_id = 0x0CFD,
		.chip_id = 0x57,
		.filename = "max11871.bin",
		.filesize = 0x8000, //  0xC000,
		.file_codesize = 0x8000 // 0xC000
	},
	.defaults_allow = 1,
	.default_config_id = 0x0CFD,
	.default_chip_id = 0x57,
	//.i2c_words = MAX_WORDS_REPORT,
	.i2c_words = 128,
	.coordinate_settings = MAX1187X_SWAP_XY,
	.panel_margin_xl = 0,
	.lcd_x = 718,// 720,
	.panel_margin_xh = 0,
	.panel_margin_yl = 0,
	.lcd_y = 1279,// 1280,
	.panel_margin_yh = 0,
	.num_sensor_x = 32,
	.num_sensor_y = 18,
	.button_code0 = KEY_BACK,
	.button_code1 = KEY_HOME,
	.button_code2 = KEY_MENU,
	.button_code3 = KEY_RESERVED,
	.linux_touch_protocol = 1,
	.max1187x_report_mode = 1,
	.enable_touch_wakeup = 0,
	.enable_pressure_shaping = 1,
	.enable_fast_calculation = 1,
	.enable_fw_download = 1,
// start hoseong.han
	.power_func = &max1187x_power,
// end hoseong.han
};

static struct i2c_board_info max1187x_ts_info[] = {
	{
		I2C_BOARD_INFO(MAX1187X_NAME, 0x48),
		.platform_data = &max1187x_platdata,
		.irq = MSM_GPIO_TO_INT(TS_GPIO_IRQ),
	}
};
#else // Hanz_touch 2013-03-20
/*                    */
extern unsigned int system_rev;
static bool touch_req = false;
int vdd_set_on_off(int on)
{
	int rc = -EINVAL;
#if defined(CONFIG_MACH_LGE_FX3_SPCS) || defined(CONFIG_MACH_LGE_FX3_SPCSTRF)
	if (system_rev < HW_REV_B)
	{
		int sensor_ldo_en = 43;
		static struct regulator *vreg_l10, *vreg_l11;

		gpio_request(sensor_ldo_en, "sensor_ldo");
		gpio_direction_output(sensor_ldo_en, 1);
		gpio_set_value(sensor_ldo_en, 1);
		mdelay(5);

		vreg_l10 = regulator_get(NULL, "8038_l10");
		if (IS_ERR(vreg_l10)) 
		{
			pr_err("%s: regulator get of touch_VDD failed (%ld)\n"
				, __func__, PTR_ERR(vreg_l10));
			rc = PTR_ERR(vreg_l10);
			return rc;
		}
		rc = regulator_set_voltage(vreg_l10,
			TOUCH_VD33_MAX_UV, TOUCH_VD33_MAX_UV);

		vreg_l11 = regulator_get(NULL, "8038_l11");
		if (IS_ERR(vreg_l11)) 
		{
			pr_err("%s: regulator get of touch_VIO failed (%ld)\n"
				, __func__, PTR_ERR(vreg_l11));
			rc = PTR_ERR(vreg_l11);
			return rc;
		}
		rc = regulator_set_voltage(vreg_l11,
			TOUCH_IO_MIN_UV, TOUCH_IO_MAX_UV);

		rc = regulator_enable(vreg_l11);
		rc = regulator_enable(vreg_l10);
	}
	else
	{
		int touch_ldo_en = 1;
		pr_info("%s: %d, touch_req = [%d]\n", __func__, on,touch_req);

		if(!touch_req){
			rc = gpio_request(touch_ldo_en, "touch_ldo");
			gpio_direction_output(touch_ldo_en, 1);
			touch_req = true;
		}
		gpio_set_value(touch_ldo_en, on);
		mdelay(5);
	}
#else		
	int touch_ldo_en = 1;
	pr_info("%s: %d, touch_req = [%d]\n", __func__, on,touch_req);

	if(!touch_req){
		rc = gpio_request(touch_ldo_en, "touch_ldo");
		gpio_direction_output(touch_ldo_en, 1);
		touch_req = true;
	}
	gpio_set_value(touch_ldo_en, on);
	mdelay(5);
#endif//
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
static struct touch_power_module touch_pwr = {
	.use_regulator	= 0,
	.vdd			= "8038_l10",
	.vdd_voltage	= TOUCH_VD33_MAX_UV,
	.vio			= "8038_l11",
	.vio_voltage	= TOUCH_IO_MAX_UV,
	.power			= vdd_set_on_off,
};

struct touch_device_caps touch_caps = {
	.button_support 		= 1,
	.number_of_button 		= 3,
	.button_name 			= {KEY_BACK,KEY_HOME,KEY_MENU},
	.button_margin 			= 10,
	.is_width_supported 	= 1,
	.is_pressure_supported 	= 1,
	.is_id_supported		= 1,
	.max_width 				= 15,
	.max_pressure 			= 0xFF,
	.max_id					= 10,
	.lcd_x					= 720,
	.lcd_y					= 1280,
	.x_max					= 1440,//1100,
	.y_max					= 2780,//1900,

};

struct touch_operation_role	touch_role = {
	.operation_mode 		= INTERRUPT_MODE,
	.key_type			= TOUCH_SOFT_KEY, /* rev.a : hard_key, rev.b : soft_key */
	.report_mode			= 0,
	.delta_pos_threshold 	= 0,
	.orientation 			= 0,
	.booting_delay 			= 200,
	.reset_delay			= 20,
	.report_period			= 10000000, 	/* 12.5 msec -> 10.0 msec(X3) */
	.suspend_pwr			= POWER_OFF,
	.resume_pwr				= POWER_ON,
	.jitter_filter_enable	= 1,
	.jitter_curr_ratio		= 28,
	.accuracy_filter_enable	= 1,
	.irqflags 				= IRQF_TRIGGER_FALLING,
};

static struct touch_platform_data  lge_synaptics_ts_data = {
	.int_pin	= TS_GPIO_IRQ,
	.reset_pin	= TS_GPIO_RESET,
	.maker		= "Synaptics",
	.caps		= &touch_caps,
	.role		= &touch_role,
	.pwr		= &touch_pwr,
};

static struct i2c_board_info msm_i2c_synaptics_ts_info[] = {
       [0] = {
               I2C_BOARD_INFO(LGE_TOUCH_NAME,LGE_TOUCH_SYNATICS_I2C_SLAVE_ADDR),
               .platform_data = &lge_synaptics_ts_data,
               .irq = MSM_GPIO_TO_INT(TS_GPIO_IRQ),
       },
};
/*                    */
#endif 
//                                              

void __init lge_add_input_devices(void)
{
//                                                         

	gpio_tlmm_config(GPIO_CFG(TS_GPIO_IRQ, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(TS_GPIO_I2C_SDA, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(TS_GPIO_I2C_SCL, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

	gpio_tlmm_config(GPIO_CFG(TS_GPIO_RESET, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
#if defined(CONFIG_MACH_LGE_L9II_COMMON)
	if (system_rev > HW_REV_A)
		gpio_tlmm_config(GPIO_CFG(TS_GPIO_LDO_EN, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
#endif
//                                                                                                             
		
//                                                                  
//                                                                                             
		
	/*	maker_id set 0 - normal , maker_id set 1 - dummy pattern */
//                                                                                         

//                                                                             
#ifdef CONFIG_TOUCHSCREEN_MAX1187X	
	i2c_register_board_info(MSM_8930_GSBI3_QUP_I2C_BUS_ID,
		(&max1187x_ts_info[0]), 1);
#else
	ts_set_vreg(1, false);
	
	i2c_register_board_info(MSM_8930_GSBI3_QUP_I2C_BUS_ID,
		(&msm_i2c_synaptics_ts_info[0]), 1);
#endif
//                                              

    platform_device_register(&gpio_keys_8930);

}

#endif // CONFIG_TOUCHSCREEN_MELFAS_MMS134
