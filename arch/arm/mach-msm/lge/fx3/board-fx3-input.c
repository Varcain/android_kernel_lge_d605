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
#include <linux/input/lge_touch_core.h>

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
#define TS_GPIO_RESET				52

#define TS_X_MAX             		480
#define TS_Y_MAX             		800

#define TOUCH_FW_VERSION       		0x01

#define MELFAS_TS_I2C_SLAVE_ADDR 	0x48
#define LGE_TOUCH_SYNATICS_I2C_SLAVE_ADDR 	0x20

//                                                                  
#define GPIO_VOLUME_UP		PM8038_GPIO_PM_TO_SYS(2)
#define GPIO_VOLUME_DOWN	PM8038_GPIO_PM_TO_SYS(3)
#define GPIO_HOMEPAGE		PM8038_GPIO_PM_TO_SYS(1)
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

static struct touch_device_caps touch_caps = {
	.button_support 			= 1,
	.button_margin				= 10,
	.number_of_button 			= 2,    
	.button_name				= {KEY_BACK,KEY_MENU},
	.is_width_supported 		= 1,
	.is_pressure_supported 		= 1,
	.is_id_supported			= 1,
	.max_width 					= 15,
	.max_pressure 				= 0xFF,
	.max_id						= 5,
	.lcd_x						= TS_X_MAX,
	.lcd_y						= TS_Y_MAX,
	.x_max						= 960,
	.y_max						= 1600,	
};

static struct touch_operation_role touch_role = {
	.operation_mode 		= INTERRUPT_MODE,
	.report_mode			= REDUCED_REPORT_MODE, //CONTINUOUS_REPORT_MODE, 
	.key_type				= TOUCH_HARD_KEY,
	.delta_pos_threshold 	= 1,//0,
	.orientation 			= 1,
	.report_period			= 10000000,
	.booting_delay 			= 200,//400, /*100 -> 400 synaptice recommand time*/
	.reset_delay			= 20,
	.suspend_pwr			= POWER_OFF,
	.jitter_filter_enable	= 1,
	.jitter_curr_ratio		= 26,
	.ghost_finger_solution_enable = 0,
	.accuracy_filter_enable = 1,
	.irqflags 				= IRQF_TRIGGER_FALLING,
};

static struct touch_platform_data  lge_synaptics_ts_data = {
	.int_pin	= TS_GPIO_IRQ,
	.reset_pin	= TS_GPIO_RESET,
	.maker		= "Synaptics",
	.fw_version = "E011",
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
void __init lge_add_input_devices(void)
{
	int rc = 0;

	gpio_tlmm_config(GPIO_CFG(TS_GPIO_IRQ, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(TS_GPIO_I2C_SDA, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(TS_GPIO_I2C_SCL, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

	gpio_tlmm_config(GPIO_CFG(TS_GPIO_RESET, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	rc = gpio_request(MSM_8930_TS_MAKER_ID, "TOUCH_PANEL_MAKERID");
		
	if (unlikely(rc < 0))
		pr_err("%s not able to get gpio\n", __func__);
		
	/*	maker_id set 0 - normal , maker_id set 1 - dummy pattern */
	gpio_direction_input(MSM_8930_TS_MAKER_ID);

	ts_set_vreg(1, false);
	
	i2c_register_board_info(MSM_8930_GSBI3_QUP_I2C_BUS_ID,
		(&msm_i2c_synaptics_ts_info[0]), 1);

#if defined(CONFIG_MACH_LGE_FX3_TMUS)
    if(gpio_get_value(MSM_8930_TS_MAKER_ID)) {
		strncpy(lge_synaptics_ts_data.fw_version, "E001", sizeof(lge_synaptics_ts_data.fw_version));
       printk("%s :lge_synaptics_ts_data.fw_version = E001 by Touch Pannel[%s]\n", __func__,lge_synaptics_ts_data.fw_version);
	   }
	else
		printk("%s :MSM_8930_TS_MAKER_ID is low\n", __func__);
#endif//	

//                                                                  
#if defined(CONFIG_MACH_LGE_FX3_SPCS)
    if(system_rev < HW_REV_B) {
        keys_8930[1].gpio = PM8038_GPIO_PM_TO_SYS(8);
        printk("%s : VOL_DOWN pin is changed to PM_GPIO(8) for SPCS Rev.A\n", __func__);
    }
#endif
    platform_device_register(&gpio_keys_8930);
//                                               

}

#endif // CONFIG_TOUCHSCREEN_MELFAS_MMS134
