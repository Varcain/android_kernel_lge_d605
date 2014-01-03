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
#include <linux/gpio.h>
#include <linux/platform_device.h>

#if defined(CONFIG_KEYBOARD_PP2106)
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

#define LGE_TOUCH_SYNATICS_I2C_SLAVE_ADDR 	0x20

#define GPIO_VOLUME_UP		PM8038_GPIO_PM_TO_SYS(2)
#define GPIO_VOLUME_DOWN	PM8038_GPIO_PM_TO_SYS(3)
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
};

/* Add GPIO keys for 8930 */
static struct gpio_keys_platform_data gpio_keys_8930_pdata = {
	.buttons = keys_8930,
	.nbuttons = 2,
};

static struct platform_device gpio_keys_8930 = {
	.name		= "gpio-keys",
	.id		= -1,
	.dev		= {
		.platform_data  = &gpio_keys_8930_pdata,
	},
};

#ifdef CONFIG_BU52031NVX
#include <linux/mfd/pm8xxx/cradle.h>

static struct pm8xxx_cradle_platform_data cradle_data = {
#if defined(CONFIG_BU52031NVX_POUCHDETECT)
        .pouch_detect_pin = GPIO_POUCH_DETECT,
        .pouch_irq = MSM_GPIO_TO_INT(GPIO_POUCH_DETECT),
#endif
#if defined(CONFIG_BU52031NVX_CARKITDETECT)
        .carkit_detect_pin = GPIO_CARKIT_DETECT,
        .carkit_irq = MSM_GPIO_TO_INT(GPIO_CARKIT_DETECT),
#endif
        .irq_flags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
};
static struct platform_device cradle_device = {
        .name   = PM8XXX_CRADLE_DEV_NAME,
        .id = -1,
        .dev = {
                .platform_data = &cradle_data,
        },
};

static void cradle_init_gpio(void) {
#if defined(CONFIG_BU52031NVX_POUCHDETECT)
	gpio_request(cradle_data.pouch_detect_pin,"pouch_detect_pin");
	gpio_tlmm_config(GPIO_CFG(cradle_data.pouch_detect_pin,
		0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
#endif
#if defined(CONFIG_BU52031NVX_CARKITDETECT)
	gpio_request(cradle_data.carkit_detect_pin,"carkit_detect_pin");
	gpio_tlmm_config(GPIO_CFG(cradle_data.carkit_detect_pin,
		0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
#endif
}
#endif

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

static void hallic_init_gpio(void) {
	gpio_request(s5712ACDL1_pdata.irq_pin,"hallic_irq");
	gpio_tlmm_config(GPIO_CFG(s5712ACDL1_pdata.irq_pin,
		0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
}
#endif

#if defined(CONFIG_KEYBOARD_PP2106)
static unsigned short pp2106_keycode[PP2106_KEYPAD_ROW][PP2106_KEYPAD_COL] = {
	{ KEY_RIGHTALT, KEY_Q_BACK, KEY_Q_HOME, KEY_Q_MENU, KEY_F1, KEY_ENTER, KEY_LEFTALT, 0 },
	{ KEY_N, KEY_M, KEY_COMMA, KEY_DOT, KEY_SPACE, KEY_SPACE, 0, 0 },
	{ KEY_L, KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B, KEY_RIGHT, 0 },
	{ KEY_D, KEY_F, KEY_G, KEY_H, KEY_J, KEY_K, KEY_LEFT, 0 },
	{ KEY_U, KEY_I, KEY_O, KEY_P, KEY_A, KEY_S, KEY_DOWN, 0 },
	{ KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_UP, 0 },
	{ KEY_7, KEY_8, KEY_9, KEY_0, KEY_BACKSPACE, KEY_SEARCH, KEY_LEFTSHIFT, 0 },
	{ KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, 0, 0 },
};

static struct pp2106_keypad_platform_data pp2106_pdata = {
	.keypad_row = PP2106_KEYPAD_ROW,
	.keypad_col = PP2106_KEYPAD_COL,
	.keycode = (unsigned char *)pp2106_keycode,
	.reset_pin = 67,
	.irq_pin = 69,
	.irq_num = MSM_GPIO_TO_INT(69),
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

extern unsigned int system_rev;

static unsigned short pp2106_keycode_revC[PP2106_KEYPAD_ROW][PP2106_KEYPAD_COL] = {
	{ KEY_RIGHTALT, KEY_Q_BACK, KEY_Q_HOME, KEY_Q_MENU, KEY_F1, KEY_ENTER, KEY_LEFTALT, 0 },
	{ KEY_N, KEY_M, KEY_COMMA, KEY_DOT, 0, 0, KEY_SPACE, 0 },
	{ KEY_L, KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B, KEY_RIGHT, 0 },
	{ KEY_D, KEY_F, KEY_G, KEY_H, KEY_J, KEY_K, KEY_LEFT, 0 },
	{ KEY_U, KEY_I, KEY_O, KEY_P, KEY_A, KEY_S, KEY_DOWN, 0 },
	{ KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_UP, 0 },
	{ KEY_7, KEY_8, KEY_9, KEY_0, KEY_BACKSPACE, KEY_SEARCH, KEY_LEFTSHIFT, 0 },
	{ KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, 0, 0 },
};

static struct pp2106_keypad_platform_data pp2106_pdata_revC = {
	.keypad_row = PP2106_KEYPAD_ROW,
	.keypad_col = PP2106_KEYPAD_COL,
	.keycode = (unsigned char *)pp2106_keycode_revC,
	.reset_pin = 67,
	.irq_pin = 69,
	.irq_num = MSM_GPIO_TO_INT(69),
	.sda_pin = 8,
	.scl_pin = 9,
};

static struct platform_device pp2106_keypad_device_revC = {
	.name = "pp2106-keypad",
	.id = 0,
	.dev = {
		.platform_data = &pp2106_pdata_revC,
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

/*                    */
extern unsigned int system_rev;
static bool touch_req = false;
int vdd_set_on_off(int on)
{
	int rc = -EINVAL;

	int touch_ldo_en = 1;
	pr_info("%s: %d, touch_req = [%d]\n", __func__, on,touch_req);

	if(!touch_req){
		rc = gpio_request(touch_ldo_en, "touch_ldo");
		gpio_direction_output(touch_ldo_en, 1);
		touch_req = true;
	}
	gpio_set_value(touch_ldo_en, on);
	mdelay(5);

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
	.number_of_button 			= 4,
	.button_name				= {KEY_BACK,KEY_HOMEPAGE,KEY_RECENT,KEY_MENU},
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
	.report_mode			= REDUCED_REPORT_MODE,//CONTINUOUS_REPORT_MODE,
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
	.fw_version = "E004",
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

	platform_device_register(&gpio_keys_8930);

#if defined(CONFIG_KEYBOARD_PP2106)
	pp2106_init_gpio();
#if defined(CONFIG_MACH_LGE_FX3Q_TMUS)
	if (system_rev >= HW_REV_A) {
		platform_device_register(&pp2106_keypad_device_revC);
	} else {
		platform_device_register(&pp2106_keypad_device);
	}
#else /*                            */
	if (system_rev >= HW_REV_C) {
		platform_device_register(&pp2106_keypad_device_revC);
	} else {
		platform_device_register(&pp2106_keypad_device);
	}
#endif /*                           */
#endif

#if defined(CONFIG_HALLIC_S5712ACDL1)
	hallic_init_gpio();
    platform_add_devices(hallic_input_devices, ARRAY_SIZE(hallic_input_devices));
#endif

#ifdef CONFIG_BU52031NVX
	cradle_init_gpio();
	platform_device_register(&cradle_device);
#endif
}
