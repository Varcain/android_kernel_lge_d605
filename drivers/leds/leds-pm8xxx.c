/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/workqueue.h>
#include <linux/err.h>

#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pm8xxx/pwm.h>

#include <linux/leds-pm8xxx.h>

#if defined(CONFIG_MACH_MSM8930_LGPS9) || defined(CONFIG_MACH_MSM8930_FX3)
#include <linux/mfd/pm8xxx/pm8038.h>
#include "../../../arch/arm/mach-msm/lge/fx3/board-fx3.h"
#include "../../../arch/arm/mach-msm/include/mach/board_lge.h"
#endif

#define SSBI_REG_ADDR_DRV_KEYPAD	0x48
#define PM8XXX_DRV_KEYPAD_BL_MASK	0xf0
#define PM8XXX_DRV_KEYPAD_BL_SHIFT	0x04

#define SSBI_REG_ADDR_FLASH_DRV0        0x49
#define PM8XXX_DRV_FLASH_MASK           0xf0
#define PM8XXX_DRV_FLASH_SHIFT          0x04

#define SSBI_REG_ADDR_FLASH_DRV1        0xFB

#define SSBI_REG_ADDR_LED_CTRL_BASE	0x131
#define SSBI_REG_ADDR_LED_CTRL(n)	(SSBI_REG_ADDR_LED_CTRL_BASE + (n))
#define PM8XXX_DRV_LED_CTRL_MASK	0xf8
#define PM8XXX_DRV_LED_CTRL_SHIFT	0x03

#define SSBI_REG_ADDR_WLED_CTRL_BASE	0x25A
#define SSBI_REG_ADDR_WLED_CTRL(n)	(SSBI_REG_ADDR_WLED_CTRL_BASE + (n) - 1)

/* wled control registers */
#define WLED_MOD_CTRL_REG		SSBI_REG_ADDR_WLED_CTRL(1)
#define WLED_MAX_CURR_CFG_REG(n)	SSBI_REG_ADDR_WLED_CTRL(n + 2)
#define WLED_BRIGHTNESS_CNTL_REG1(n)	SSBI_REG_ADDR_WLED_CTRL((2 * n) + 5)
#define WLED_BRIGHTNESS_CNTL_REG2(n)	SSBI_REG_ADDR_WLED_CTRL((2 * n) + 6)
#define WLED_SYNC_REG			SSBI_REG_ADDR_WLED_CTRL(11)
#define WLED_OVP_CFG_REG		SSBI_REG_ADDR_WLED_CTRL(13)
#define WLED_BOOST_CFG_REG		SSBI_REG_ADDR_WLED_CTRL(14)
#define WLED_HIGH_POLE_CAP_REG		SSBI_REG_ADDR_WLED_CTRL(16)

#define WLED_STRINGS			0x03
#define WLED_OVP_VAL_MASK		0x30
#define WLED_OVP_VAL_BIT_SHFT		0x04
#define WLED_BOOST_LIMIT_MASK		0xE0
#define WLED_BOOST_LIMIT_BIT_SHFT	0x05
#define WLED_BOOST_OFF			0x00
#define WLED_EN_MASK			0x01
#define WLED_CP_SELECT_MAX		0x03
#define WLED_CP_SELECT_MASK		0x03
#define WLED_DIG_MOD_GEN_MASK		0x70
#define WLED_CS_OUT_MASK		0x0E
#define WLED_CTL_DLY_STEP		200
#define WLED_CTL_DLY_MAX		1400
#define WLED_CTL_DLY_MASK		0xE0
#define WLED_CTL_DLY_BIT_SHFT		0x05
#define WLED_MAX_CURR			25
#define WLED_MAX_CURR_MASK		0x1F
#define WLED_OP_FDBCK_MASK		0x1C
#define WLED_OP_FDBCK_BIT_SHFT		0x02

#define WLED_MAX_LEVEL			255
#define WLED_8_BIT_MASK			0xFF
#define WLED_8_BIT_SHFT			0x08
#define WLED_MAX_DUTY_CYCLE		0xFFF

#define WLED_SYNC_VAL			0x07
#define WLED_SYNC_RESET_VAL		0x00
#define WLED_SYNC_MASK			0xF8

#define ONE_WLED_STRING			1
#define TWO_WLED_STRINGS		2
#define THREE_WLED_STRINGS		3

#define WLED_CABC_ONE_STRING		0x01
#define WLED_CABC_TWO_STRING		0x03
#define WLED_CABC_THREE_STRING		0x07

#define WLED_CABC_SHIFT			3

#define SSBI_REG_ADDR_RGB_CNTL1		0x12D
#define SSBI_REG_ADDR_RGB_CNTL2		0x12E

#define PM8XXX_DRV_RGB_RED_LED		BIT(2)
#define PM8XXX_DRV_RGB_GREEN_LED	BIT(1)
#define PM8XXX_DRV_RGB_BLUE_LED		BIT(0)

#define MAX_FLASH_LED_CURRENT		300
#define MAX_LC_LED_CURRENT		40
#define MAX_KP_BL_LED_CURRENT		300

#define PM8XXX_ID_LED_CURRENT_FACTOR	2  /* Iout = x * 2mA */
#define PM8XXX_ID_FLASH_CURRENT_FACTOR	20 /* Iout = x * 20mA */

#define PM8XXX_FLASH_MODE_DBUS1		1
#define PM8XXX_FLASH_MODE_DBUS2		2
#define PM8XXX_FLASH_MODE_PWM		3

#define MAX_LC_LED_BRIGHTNESS		20
#define MAX_FLASH_BRIGHTNESS		15
#define MAX_KB_LED_BRIGHTNESS		15

#define PM8XXX_LED_OFFSET(id) ((id) - PM8XXX_ID_LED_0)

#define PM8XXX_LED_PWM_FLAGS	(PM_PWM_LUT_LOOP | PM_PWM_LUT_RAMP_UP)

#define UI_MIN_BL		20
#define UI_20_BL	68
#define UI_40_BL	116
#define UI_DEFAULT_BL		149
#define UI_60_BL	162
#define UI_80_BL	208
#define UI_MAX_BL		255

#define LGE_MIN_BL		20
#define LGE_20_BL	27
#define LGE_40_BL	50
#define LGE_DEFAULT_BL		92
#define LGE_60_BL	93
#define LGE_80_BL	159
#define LGE_MAX_BL		255


/*                                                         */
#ifdef CONFIG_LGE_PM8038_KPJT
struct pm8xxx_led_data *red_led = NULL;
struct pm8xxx_led_data *green_led = NULL;
struct pm8xxx_led_data *blue_led = NULL;

#define RED_CHANNEL   5
#define GREEN_CHANNEL 4
#define BLUE_CHANNEL  3

extern void change_led_pattern(int pattern, int pattern_r_on, int pattern_g_on, int pattern_b_on);
extern void make_pwm_led_pattern(int patterns[]);
extern void make_blink_led_pattern(int red, int green, int blue, int delay_on, int delay_off, int on);
extern void pattern_test(int on);

#else
#endif
/*                                                         */

#if defined(CONFIG_MACH_MSM8930_LGPS9) || defined(CONFIG_MACH_MSM8930_FX3)
#define LED_MAP(_version, _kb, _led0, _led1, _led2, _flash_led0, _flash_led1, \
	_wled, _rgb_led_red, _rgb_led_green, _rgb_led_blue, _mpp_kb, _mpp_qwerty)\
	{\
		.version = _version,\
		.supported = _kb << PM8XXX_ID_LED_KB_LIGHT | \
			_led0 << PM8XXX_ID_LED_0 | _led1 << PM8XXX_ID_LED_1 | \
			_led2 << PM8XXX_ID_LED_2  | \
			_flash_led0 << PM8XXX_ID_FLASH_LED_0 | \
			_flash_led1 << PM8XXX_ID_FLASH_LED_1 | \
			_wled << PM8XXX_ID_WLED | \
			_rgb_led_red << PM8XXX_ID_RGB_LED_RED | \
			_rgb_led_green << PM8XXX_ID_RGB_LED_GREEN | \
			_rgb_led_blue << PM8XXX_ID_RGB_LED_BLUE | \
			_mpp_kb << PM8XXX_ID_MPP_KB_LIGHT | \
			_mpp_qwerty << PM8XXX_ID_MPP_QWERTY \
	}
#else
#define LED_MAP(_version, _kb, _led0, _led1, _led2, _flash_led0, _flash_led1, \
	_wled, _rgb_led_red, _rgb_led_green, _rgb_led_blue)\
	{\
		.version = _version,\
		.supported = _kb << PM8XXX_ID_LED_KB_LIGHT | \
			_led0 << PM8XXX_ID_LED_0 | _led1 << PM8XXX_ID_LED_1 | \
			_led2 << PM8XXX_ID_LED_2  | \
			_flash_led0 << PM8XXX_ID_FLASH_LED_0 | \
			_flash_led1 << PM8XXX_ID_FLASH_LED_1 | \
			_wled << PM8XXX_ID_WLED | \
			_rgb_led_red << PM8XXX_ID_RGB_LED_RED | \
			_rgb_led_green << PM8XXX_ID_RGB_LED_GREEN | \
			_rgb_led_blue << PM8XXX_ID_RGB_LED_BLUE | \
	}
#endif
/**
 * supported_leds - leds supported for each PMIC version
 * @version - version of PMIC
 * @supported - which leds are supported on version
 */

struct supported_leds {
	enum pm8xxx_version version;
	u32 supported;
};


static const struct supported_leds led_map[] = {
#if defined(CONFIG_MACH_MSM8930_LGPS9) || defined(CONFIG_MACH_MSM8930_FX3)
	LED_MAP(PM8XXX_VERSION_8058, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0),
	LED_MAP(PM8XXX_VERSION_8921, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0),
	LED_MAP(PM8XXX_VERSION_8018, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
	LED_MAP(PM8XXX_VERSION_8922, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0),
//                                                                  
#if defined(CONFIG_BACKLIGHT_LM3639)  
	LED_MAP(PM8XXX_VERSION_8038, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1),
#else
	LED_MAP(PM8XXX_VERSION_8038, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1),
#endif
//                                                                   
#else
	LED_MAP(PM8XXX_VERSION_8058, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0),
	LED_MAP(PM8XXX_VERSION_8921, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0),
	LED_MAP(PM8XXX_VERSION_8018, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0),
	LED_MAP(PM8XXX_VERSION_8922, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1),
	LED_MAP(PM8XXX_VERSION_8038, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1),
#endif
};

#if defined(CONFIG_MACH_MSM8930_LGPS9) || defined(CONFIG_MACH_MSM8930_FX3)
static int mpp_level = PM8XXX_MPP_CS_OUT_5MA;
#endif

/**
 * struct pm8xxx_led_data - internal led data structure
 * @led_classdev - led class device
 * @id - led index
 * @work - workqueue for led
 * @lock - to protect the transactions
 * @reg - cached value of led register
 * @pwm_dev - pointer to PWM device if LED is driven using PWM
 * @pwm_channel - PWM channel ID
 * @pwm_period_us - PWM period in micro seconds
 * @pwm_duty_cycles - struct that describes PWM duty cycles info
 */
struct pm8xxx_led_data {
	struct led_classdev	cdev;
	int			id;
	u8			reg;
	u8			wled_mod_ctrl_val;
	struct device		*dev;
	struct work_struct	work;
	struct mutex		lock;
	struct pwm_device	*pwm_dev;
	int			pwm_channel;
	u32			pwm_period_us;
	struct pm8xxx_pwm_duty_cycles *pwm_duty_cycles;
	struct wled_config_data *wled_cfg;
	int			max_current;
};

#if defined(CONFIG_MACH_MSM8930_LGPS9) || defined(CONFIG_MACH_MSM8930_FX3)
static inline int pm8xxx_mpp_config_current_sink(unsigned mpp,
												 unsigned level, unsigned control)
{
	struct pm8xxx_mpp_config_data config = {
		.type = PM8XXX_MPP_TYPE_SINK,
		.level = level,
		.control = control,
	};
	return pm8xxx_mpp_config(mpp, &config);
}
#endif

static void led_kp_set(struct pm8xxx_led_data *led, enum led_brightness value)
{
	int rc;
	u8 level;

	level = (value << PM8XXX_DRV_KEYPAD_BL_SHIFT) &
				 PM8XXX_DRV_KEYPAD_BL_MASK;

	led->reg &= ~PM8XXX_DRV_KEYPAD_BL_MASK;
	led->reg |= level;

	rc = pm8xxx_writeb(led->dev->parent, SSBI_REG_ADDR_DRV_KEYPAD,
								led->reg);
	if (rc < 0)
		dev_err(led->cdev.dev,
			"can't set keypad backlight level rc=%d\n", rc);
}

static void led_lc_set(struct pm8xxx_led_data *led, enum led_brightness value)
{
	int rc, offset;
	u8 level;

	level = (value << PM8XXX_DRV_LED_CTRL_SHIFT) &
				PM8XXX_DRV_LED_CTRL_MASK;

	offset = PM8XXX_LED_OFFSET(led->id);

	led->reg &= ~PM8XXX_DRV_LED_CTRL_MASK;
	led->reg |= level;

	rc = pm8xxx_writeb(led->dev->parent, SSBI_REG_ADDR_LED_CTRL(offset),
								led->reg);
	if (rc)
		dev_err(led->cdev.dev, "can't set (%d) led value rc=%d\n",
				led->id, rc);
}

static void
led_flash_set(struct pm8xxx_led_data *led, enum led_brightness value)
{
	int rc;
	u8 level;
	u16 reg_addr;

	level = (value << PM8XXX_DRV_FLASH_SHIFT) &
				 PM8XXX_DRV_FLASH_MASK;

	led->reg &= ~PM8XXX_DRV_FLASH_MASK;
	led->reg |= level;

	if (led->id == PM8XXX_ID_FLASH_LED_0)
		reg_addr = SSBI_REG_ADDR_FLASH_DRV0;
	else
		reg_addr = SSBI_REG_ADDR_FLASH_DRV1;

	rc = pm8xxx_writeb(led->dev->parent, reg_addr, led->reg);
	if (rc < 0)
		dev_err(led->cdev.dev, "can't set flash led%d level rc=%d\n",
			 led->id, rc);
}

#if defined ( CONFIG_FB_MSM_MIPI_TX11D108VM_R69324A_VIDEO_QHD_PT )	
int debug_lcd_backlight_level[15] = {-1, -1, -1, -1,-1, -1,-1, -1,-1, -1,-1,-1, -1,-1, -1,};
int debug_lcd_backlight_cnt = 0;
#endif

static int
led_wled_set(struct pm8xxx_led_data *led, enum led_brightness value)
{
	int rc, duty;
	u8 val, i, num_wled_strings;
	static int old_value = 0;

#if defined ( CONFIG_FB_MSM_MIPI_TX11D108VM_R69324A_VIDEO_QHD_PT )	
	if (debug_lcd_backlight_cnt < 10){
		debug_lcd_backlight_level[debug_lcd_backlight_cnt] = value;
		debug_lcd_backlight_cnt++;
	}
#endif
	if(old_value == 0 || value == 0)
		dev_err(led->dev->parent, "wled brightness value is %d\n", value);

	old_value = value;
	if ((value < 0) || (val > UI_MAX_BL))
		return  -EINVAL;

	if(value >= UI_MIN_BL && value <= UI_20_BL)
		value = (value - UI_MIN_BL) * (LGE_20_BL - LGE_MIN_BL) / (UI_20_BL - UI_MIN_BL) + LGE_MIN_BL;
	else if(value >UI_20_BL && value <= UI_40_BL)
		value = (value - UI_20_BL) * (LGE_40_BL - LGE_20_BL) / (UI_40_BL - UI_20_BL) + LGE_20_BL;
	else if(value >UI_40_BL && value <= UI_DEFAULT_BL)
		value = (value - UI_40_BL) * (LGE_DEFAULT_BL - LGE_40_BL) / (UI_DEFAULT_BL - UI_40_BL) + LGE_40_BL;
	else if(value >UI_DEFAULT_BL && value <= UI_60_BL)
		value = (value - UI_DEFAULT_BL) * (LGE_60_BL - LGE_DEFAULT_BL) / (UI_60_BL - UI_DEFAULT_BL) + LGE_DEFAULT_BL;
	else if(value >UI_60_BL && value <= UI_80_BL)
		value = (value - UI_60_BL) * (LGE_80_BL - LGE_60_BL) / (UI_80_BL - UI_60_BL) + LGE_60_BL;
	else if(value >UI_80_BL && value <= UI_MAX_BL)
		value = (value - UI_80_BL) * (LGE_MAX_BL - LGE_80_BL) / (UI_MAX_BL - UI_80_BL) + LGE_80_BL;

	if (value > WLED_MAX_LEVEL)
		value = WLED_MAX_LEVEL;

	if (value == 0) {
		rc = pm8xxx_writeb(led->dev->parent, WLED_MOD_CTRL_REG,
				WLED_BOOST_OFF);
		if (rc) {
			dev_err(led->dev->parent, "can't write wled ctrl config"
				" register rc=%d\n", rc);
			return rc;
		}
	} else {
		rc = pm8xxx_writeb(led->dev->parent, WLED_MOD_CTRL_REG,
				led->wled_mod_ctrl_val);
		if (rc) {
			dev_err(led->dev->parent, "can't write wled ctrl config"
				" register rc=%d\n", rc);
			return rc;
		}
	}

	duty = (WLED_MAX_DUTY_CYCLE * value) / WLED_MAX_LEVEL;

	num_wled_strings = led->wled_cfg->num_strings;

	/* program brightness control registers */
	for (i = 0; i < num_wled_strings; i++) {
		rc = pm8xxx_readb(led->dev->parent,
				WLED_BRIGHTNESS_CNTL_REG1(i), &val);
		if (rc) {
			dev_err(led->dev->parent, "can't read wled brightnes ctrl"
				" register1 rc=%d\n", rc);
			return rc;
		}

		val = (val & ~WLED_MAX_CURR_MASK) | (duty >> WLED_8_BIT_SHFT);
		rc = pm8xxx_writeb(led->dev->parent,
				WLED_BRIGHTNESS_CNTL_REG1(i), val);
		if (rc) {
			dev_err(led->dev->parent, "can't write wled brightness ctrl"
				" register1 rc=%d\n", rc);
			return rc;
		}

		val = duty & WLED_8_BIT_MASK;
		rc = pm8xxx_writeb(led->dev->parent,
				WLED_BRIGHTNESS_CNTL_REG2(i), val);
		if (rc) {
			dev_err(led->dev->parent, "can't write wled brightness ctrl"
				" register2 rc=%d\n", rc);
			return rc;
		}
	}
	rc = pm8xxx_readb(led->dev->parent, WLED_SYNC_REG, &val);
	if (rc) {
		dev_err(led->dev->parent,
			"can't read wled sync register rc=%d\n", rc);
		return rc;
	}
	/* sync */
	val &= WLED_SYNC_MASK;
	val |= WLED_SYNC_VAL;
	rc = pm8xxx_writeb(led->dev->parent, WLED_SYNC_REG, val);
	if (rc) {
		dev_err(led->dev->parent,
			"can't read wled sync register rc=%d\n", rc);
		return rc;
	}
	val &= WLED_SYNC_MASK;
	val |= WLED_SYNC_RESET_VAL;
	rc = pm8xxx_writeb(led->dev->parent, WLED_SYNC_REG, val);
	if (rc) {
		dev_err(led->dev->parent,
			"can't read wled sync register rc=%d\n", rc);
		return rc;
	}
	return 0;
}

static void wled_dump_regs(struct pm8xxx_led_data *led)
{
	int i;
	u8 val;

	for (i = 1; i < 17; i++) {
		pm8xxx_readb(led->dev->parent,
				SSBI_REG_ADDR_WLED_CTRL(i), &val);
		pr_debug("WLED_CTRL_%d = 0x%x\n", i, val);
	}
}

static void
led_rgb_write(struct pm8xxx_led_data *led, u16 addr, enum led_brightness value)
{
	int rc;
	u8 val, mask;

	if (led->id != PM8XXX_ID_RGB_LED_BLUE &&
		led->id != PM8XXX_ID_RGB_LED_RED &&
		led->id != PM8XXX_ID_RGB_LED_GREEN)
		return;

	rc = pm8xxx_readb(led->dev->parent, addr, &val);
	if (rc) {
		dev_err(led->cdev.dev, "can't read rgb ctrl register rc=%d\n",
							rc);
		return;
	}

	switch (led->id) {
	case PM8XXX_ID_RGB_LED_RED:
		mask = PM8XXX_DRV_RGB_RED_LED;
		break;
	case PM8XXX_ID_RGB_LED_GREEN:
		mask = PM8XXX_DRV_RGB_GREEN_LED;
		break;
	case PM8XXX_ID_RGB_LED_BLUE:
		mask = PM8XXX_DRV_RGB_BLUE_LED;
		break;
	default:
		return;
	}

	if (value)
		val |= mask;
	else
		val &= ~mask;

	rc = pm8xxx_writeb(led->dev->parent, addr, val);
	if (rc < 0)
		dev_err(led->cdev.dev, "can't set rgb led %d level rc=%d\n",
			 led->id, rc);
}

static void
led_rgb_set(struct pm8xxx_led_data *led, enum led_brightness value)
{
	if (value) {
		led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1, value);
		led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL2, value);
	} else {
		led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL2, value);
		led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1, value);
	}
}

static int pm8xxx_led_pwm_work(struct pm8xxx_led_data *led)
{
	int duty_us;
	int rc = 0;

	if (led->pwm_duty_cycles == NULL) {
		duty_us = (led->pwm_period_us * led->cdev.brightness) /
								LED_FULL;
		rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
		if (led->cdev.brightness) {
			led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				led->cdev.brightness);
			rc = pwm_enable(led->pwm_dev);
		} else {
			pwm_disable(led->pwm_dev);
			led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				led->cdev.brightness);
		}
	} else {
		if (led->cdev.brightness)
			led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				led->cdev.brightness);
		rc = pm8xxx_pwm_lut_enable(led->pwm_dev, led->cdev.brightness);
		if (!led->cdev.brightness)
			led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				led->cdev.brightness);
	}

	return rc;
}

static void __pm8xxx_led_work(struct pm8xxx_led_data *led,
					enum led_brightness level)
{
	int rc;
#if defined(CONFIG_MACH_MSM8930_LGPS9) || defined(CONFIG_MACH_MSM8930_FX3)
	int mpp_num;
#endif

	mutex_lock(&led->lock);

	switch (led->id) {
	case PM8XXX_ID_LED_KB_LIGHT:
		led_kp_set(led, level);
		break;
	case PM8XXX_ID_LED_0:
	case PM8XXX_ID_LED_1:
	case PM8XXX_ID_LED_2:
		led_lc_set(led, level);
		break;
	case PM8XXX_ID_FLASH_LED_0:
	case PM8XXX_ID_FLASH_LED_1:
		led_flash_set(led, level);
		break;
	case PM8XXX_ID_WLED:
		rc = led_wled_set(led, level);
		if (rc < 0)
			pr_err("wled brightness set failed %d\n", rc);
		break;
	case PM8XXX_ID_RGB_LED_RED:
	case PM8XXX_ID_RGB_LED_GREEN:
	case PM8XXX_ID_RGB_LED_BLUE:
		led_rgb_set(led, level);
		break;
#if defined(CONFIG_MACH_MSM8930_LGPS9) || defined(CONFIG_MACH_MSM8930_FX3)
	case PM8XXX_ID_MPP_KB_LIGHT:
		mpp_num = PM8038_MPP_PM_TO_SYS(4);
		pm8xxx_mpp_config_current_sink(mpp_num, mpp_level, level ? 1 : 0);
		break;
	case PM8XXX_ID_MPP_QWERTY:
		mpp_num = PM8038_MPP_PM_TO_SYS(3);
		pm8xxx_mpp_config_current_sink(mpp_num, mpp_level, level ? 1 : 0);
		break;
#endif
	default:
		dev_err(led->cdev.dev, "unknown led id %d", led->id);
		break;
	}

	mutex_unlock(&led->lock);
}

static void pm8xxx_led_work(struct work_struct *work)
{
	int rc;

	struct pm8xxx_led_data *led = container_of(work,
					 struct pm8xxx_led_data, work);

	if (led->pwm_dev == NULL) {
		__pm8xxx_led_work(led, led->cdev.brightness);
	} else {
		rc = pm8xxx_led_pwm_work(led);
		if (rc)
			pr_err("could not configure PWM mode for LED:%d\n",
								led->id);
	}
}

static void pm8xxx_led_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	struct	pm8xxx_led_data *led;

	led = container_of(led_cdev, struct pm8xxx_led_data, cdev);

	if (value < LED_OFF || value > led->cdev.max_brightness) {
		dev_err(led->cdev.dev, "Invalid brightness value exceeds");
		return;
	}
#if defined(CONFIG_MACH_MSM8930_LGPS9) || defined(CONFIG_MACH_MSM8930_FX3)
	if (led->id == PM8XXX_ID_MPP_KB_LIGHT
			|| led->id == PM8XXX_ID_MPP_QWERTY) {
			if (value > PM8XXX_MPP_CS_OUT_5MA)
				mpp_level = PM8XXX_MPP_CS_OUT_5MA;
			else
				mpp_level = value;
	}
#endif

	led->cdev.brightness = value;
	schedule_work(&led->work);
}

static int pm8xxx_set_led_mode_and_max_brightness(struct pm8xxx_led_data *led,
		enum pm8xxx_led_modes led_mode, int max_current)
{
	switch (led->id) {
	case PM8XXX_ID_LED_0:
	case PM8XXX_ID_LED_1:
	case PM8XXX_ID_LED_2:
		led->cdev.max_brightness = max_current /
						PM8XXX_ID_LED_CURRENT_FACTOR;
		if (led->cdev.max_brightness > MAX_LC_LED_BRIGHTNESS)
			led->cdev.max_brightness = MAX_LC_LED_BRIGHTNESS;
		led->reg = led_mode;
		break;
	case PM8XXX_ID_LED_KB_LIGHT:
	case PM8XXX_ID_FLASH_LED_0:
	case PM8XXX_ID_FLASH_LED_1:
		led->cdev.max_brightness = max_current /
						PM8XXX_ID_FLASH_CURRENT_FACTOR;
		if (led->cdev.max_brightness > MAX_FLASH_BRIGHTNESS)
			led->cdev.max_brightness = MAX_FLASH_BRIGHTNESS;

		switch (led_mode) {
		case PM8XXX_LED_MODE_PWM1:
		case PM8XXX_LED_MODE_PWM2:
		case PM8XXX_LED_MODE_PWM3:
			led->reg = PM8XXX_FLASH_MODE_PWM;
			break;
		case PM8XXX_LED_MODE_DTEST1:
			led->reg = PM8XXX_FLASH_MODE_DBUS1;
			break;
		case PM8XXX_LED_MODE_DTEST2:
			led->reg = PM8XXX_FLASH_MODE_DBUS2;
			break;
		default:
			led->reg = PM8XXX_LED_MODE_MANUAL;
			break;
		}
		break;
	case PM8XXX_ID_WLED:
		led->cdev.max_brightness = WLED_MAX_LEVEL;
		break;
	case PM8XXX_ID_RGB_LED_RED:
	case PM8XXX_ID_RGB_LED_GREEN:
	case PM8XXX_ID_RGB_LED_BLUE:
		led->cdev.max_brightness = LED_FULL;
		break;
#if defined(CONFIG_MACH_MSM8930_LGPS9) || defined(CONFIG_MACH_MSM8930_FX3)
	case PM8XXX_ID_MPP_KB_LIGHT:
	case PM8XXX_ID_MPP_QWERTY:
		led->cdev.max_brightness = LED_FULL;
		break;
#endif
	default:
		dev_err(led->cdev.dev, "LED Id is invalid");
		return -EINVAL;
	}

	return 0;
}

static enum led_brightness pm8xxx_led_get(struct led_classdev *led_cdev)
{
	struct pm8xxx_led_data *led;

	led = container_of(led_cdev, struct pm8xxx_led_data, cdev);

	return led->cdev.brightness;
}

static int __devinit init_wled(struct pm8xxx_led_data *led)
{
	int rc, i;
	u8 val, num_wled_strings;

	num_wled_strings = led->wled_cfg->num_strings;

	/* program over voltage protection threshold */
	if (led->wled_cfg->ovp_val > WLED_OVP_37V) {
		dev_err(led->dev->parent, "Invalid ovp value");
		return -EINVAL;
	}
#if defined(CONFIG_MACH_LGE) /*to remove WLED audible noise */
#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS) || defined (CONFIG_FB_MSM_MIPI_TX11D108VM_R69324A_VIDEO_QHD_PT)/*f6 models, At UI Brightness 0%, the screen is blinking*/
	val = 0x00; // 800kHz
#else
	val = 0x10;
#endif
	rc = pm8xxx_writeb(led->dev->parent, 0x265, val);
	if (rc) {
		dev_err(led->dev->parent, "can't write wled ctrl12 config"
			" register rc=%d\n", rc);
		return rc;
	}

	val = 0x12;

	rc = pm8xxx_writeb(led->dev->parent, WLED_OVP_CFG_REG, val);
	if (rc) {
		dev_err(led->dev->parent, "can't write wled ovp config"
			" register rc=%d\n", rc);
		return rc;
	}

	val = 0xff;

	rc = pm8xxx_writeb(led->dev->parent, 0x268, val);
	if (rc) {
		dev_err(led->dev->parent, "can't write wled ctrl15 config"
			" register rc=%d\n", rc);
		return rc;
	}

#else

	rc = pm8xxx_readb(led->dev->parent, WLED_OVP_CFG_REG, &val);
	if (rc) {
		dev_err(led->dev->parent, "can't read wled ovp config"
			" register rc=%d\n", rc);
		return rc;
	}

	val = (val & ~WLED_OVP_VAL_MASK) |
		(led->wled_cfg->ovp_val << WLED_OVP_VAL_BIT_SHFT);

	rc = pm8xxx_writeb(led->dev->parent, WLED_OVP_CFG_REG, val);
	if (rc) {
		dev_err(led->dev->parent, "can't write wled ovp config"
			" register rc=%d\n", rc);
		return rc;
	}
#endif

	/* program current boost limit and output feedback*/
	if (led->wled_cfg->boost_curr_lim > WLED_CURR_LIMIT_1680mA) {
		dev_err(led->dev->parent, "Invalid boost current limit");
		return -EINVAL;
	}

	rc = pm8xxx_readb(led->dev->parent, WLED_BOOST_CFG_REG, &val);
	if (rc) {
		dev_err(led->dev->parent, "can't read wled boost config"
			" register rc=%d\n", rc);
		return rc;
	}

	val = (val & ~WLED_BOOST_LIMIT_MASK) |
		(led->wled_cfg->boost_curr_lim << WLED_BOOST_LIMIT_BIT_SHFT);

	val = (val & ~WLED_OP_FDBCK_MASK) |
		(led->wled_cfg->op_fdbck << WLED_OP_FDBCK_BIT_SHFT);

	rc = pm8xxx_writeb(led->dev->parent, WLED_BOOST_CFG_REG, val);
	if (rc) {
		dev_err(led->dev->parent, "can't write wled boost config"
			" register rc=%d\n", rc);
		return rc;
	}

	/* program high pole capacitance */
	if (led->wled_cfg->cp_select > WLED_CP_SELECT_MAX) {
		dev_err(led->dev->parent, "Invalid pole capacitance");
		return -EINVAL;
	}

	rc = pm8xxx_readb(led->dev->parent, WLED_HIGH_POLE_CAP_REG, &val);
	if (rc) {
		dev_err(led->dev->parent, "can't read wled high pole"
			" capacitance register rc=%d\n", rc);
		return rc;
	}

	val = (val & ~WLED_CP_SELECT_MASK) | led->wled_cfg->cp_select;

	rc = pm8xxx_writeb(led->dev->parent, WLED_HIGH_POLE_CAP_REG, val);
	if (rc) {
		dev_err(led->dev->parent, "can't write wled high pole"
			" capacitance register rc=%d\n", rc);
		return rc;
	}

	/* program activation delay and maximum current */
	for (i = 0; i < num_wled_strings; i++) {
		rc = pm8xxx_readb(led->dev->parent,
				WLED_MAX_CURR_CFG_REG(i), &val);
		if (rc) {
			dev_err(led->dev->parent, "can't read wled max current"
				" config register rc=%d\n", rc);
			return rc;
		}

		if ((led->wled_cfg->ctrl_delay_us % WLED_CTL_DLY_STEP) ||
			(led->wled_cfg->ctrl_delay_us > WLED_CTL_DLY_MAX)) {
			dev_err(led->dev->parent, "Invalid control delay\n");
			return rc;
		}

		val = val / WLED_CTL_DLY_STEP;
		val = (val & ~WLED_CTL_DLY_MASK) |
			(led->wled_cfg->ctrl_delay_us << WLED_CTL_DLY_BIT_SHFT);

		if ((led->max_current > WLED_MAX_CURR)) {
			dev_err(led->dev->parent, "Invalid max current\n");
			return -EINVAL;
		}

		val = (val & ~WLED_MAX_CURR_MASK) | led->max_current;

		rc = pm8xxx_writeb(led->dev->parent,
				WLED_MAX_CURR_CFG_REG(i), val);
		if (rc) {
			dev_err(led->dev->parent, "can't write wled max current"
				" config register rc=%d\n", rc);
			return rc;
		}
	}

	if (led->wled_cfg->cabc_en) {
		rc = pm8xxx_readb(led->dev->parent, WLED_SYNC_REG, &val);
		if (rc) {
			dev_err(led->dev->parent,
				"can't read cabc register rc=%d\n", rc);
			return rc;
		}

		switch (num_wled_strings) {
		case ONE_WLED_STRING:
			val |= (WLED_CABC_ONE_STRING << WLED_CABC_SHIFT);
			break;
		case TWO_WLED_STRINGS:
			val |= (WLED_CABC_TWO_STRING << WLED_CABC_SHIFT);
			break;
		case THREE_WLED_STRINGS:
			val |= (WLED_CABC_THREE_STRING << WLED_CABC_SHIFT);
			break;
		default:
			break;
		}

		rc = pm8xxx_writeb(led->dev->parent, WLED_SYNC_REG, val);
		if (rc) {
			dev_err(led->dev->parent,
				"can't write to enable cabc rc=%d\n", rc);
			return rc;
		}
	}

	/* program digital module generator, cs out and enable the module */
	rc = pm8xxx_readb(led->dev->parent, WLED_MOD_CTRL_REG, &val);
	if (rc) {
		dev_err(led->dev->parent, "can't read wled module ctrl"
			" register rc=%d\n", rc);
		return rc;
	}

	if (led->wled_cfg->dig_mod_gen_en)
		val |= WLED_DIG_MOD_GEN_MASK;

	if (led->wled_cfg->cs_out_en)
		val |= WLED_CS_OUT_MASK;

	val |= WLED_EN_MASK;

	rc = pm8xxx_writeb(led->dev->parent, WLED_MOD_CTRL_REG, val);
	if (rc) {
		dev_err(led->dev->parent, "can't write wled module ctrl"
			" register rc=%d\n", rc);
		return rc;
	}
	led->wled_mod_ctrl_val = val;

	/* dump wled registers */
	wled_dump_regs(led);

	return 0;
}

static int __devinit get_init_value(struct pm8xxx_led_data *led, u8 *val)
{
	int rc, offset;
	u16 addr;

	switch (led->id) {
	case PM8XXX_ID_LED_KB_LIGHT:
		addr = SSBI_REG_ADDR_DRV_KEYPAD;
		break;
	case PM8XXX_ID_LED_0:
	case PM8XXX_ID_LED_1:
	case PM8XXX_ID_LED_2:
		offset = PM8XXX_LED_OFFSET(led->id);
		addr = SSBI_REG_ADDR_LED_CTRL(offset);
		break;
	case PM8XXX_ID_FLASH_LED_0:
		addr = SSBI_REG_ADDR_FLASH_DRV0;
		break;
	case PM8XXX_ID_FLASH_LED_1:
		addr = SSBI_REG_ADDR_FLASH_DRV1;
		break;
	case PM8XXX_ID_WLED:
		rc = init_wled(led);
		if (rc)
			dev_err(led->cdev.dev, "can't initialize wled rc=%d\n",
								rc);
		return rc;
	case PM8XXX_ID_RGB_LED_RED:
	case PM8XXX_ID_RGB_LED_GREEN:
	case PM8XXX_ID_RGB_LED_BLUE:
		addr = SSBI_REG_ADDR_RGB_CNTL1;
		break;
#if defined(CONFIG_MACH_MSM8930_LGPS9) || defined(CONFIG_MACH_MSM8930_FX3)
	case PM8XXX_ID_MPP_KB_LIGHT:
	case PM8XXX_ID_MPP_QWERTY:
		addr = 0; /* temp add for unintialized warning */
		break;
#endif
	default:
		dev_err(led->cdev.dev, "unknown led id %d", led->id);
		return -EINVAL;
	}

	rc = pm8xxx_readb(led->dev->parent, addr, val);
	if (rc)
		dev_err(led->cdev.dev, "can't get led(%d) level rc=%d\n",
							led->id, rc);

	return rc;
}
/*                                                         */
#ifdef CONFIG_LGE_PM8038_KPJT
void make_pwm_led_pattern(int patterns[])
{
	int i=0;
	int *duty_pcts_red = NULL;
	int *duty_pcts_green = NULL;
	int *duty_pcts_blue =NULL;

	// 1. set all leds brightness to 0
	red_led->cdev.brightness = 0;
	green_led->cdev.brightness = 0;
	blue_led->cdev.brightness = 0;

	// 2. run work-function, as brightness 0, all led turn off
	pm8xxx_led_pwm_work(red_led);
	pm8xxx_led_pwm_work(green_led);
	pm8xxx_led_pwm_work(blue_led);

		duty_pcts_red   = patterns; //red_led->pwm_duty_cycles->duty_pcts1;
		duty_pcts_green = patterns; //green_led->pwm_duty_cycles->duty_pcts1;
		duty_pcts_blue  = patterns; //blue_led->pwm_duty_cycles->duty_pcts1;


	printk("[PMIC K-PJT] LUT is \n");
	for(i=0;i<79;i++){
		printk("%d ",duty_pcts_red[i]);
	}
	printk("\n");


	// 4. lut disable, so we can edit LUT table after done this.
	pm8xxx_pwm_lut_enable( red_led->pwm_dev, 0 );
	pm8xxx_pwm_lut_enable( green_led->pwm_dev, 0 );
	pm8xxx_pwm_lut_enable( blue_led->pwm_dev, 0 );

	// 5. lut config(red led).
	pm8xxx_pwm_lut_config(
	red_led->pwm_dev,
	duty_pcts_red[78],
	&duty_pcts_red[duty_pcts_red[63]],
	duty_pcts_red[65],
	duty_pcts_red[63]+1,
	duty_pcts_red[64],
	0,
	duty_pcts_red[66],
	duty_pcts_red[75]
	);
	// 6. lut config(green led).
	pm8xxx_pwm_lut_config(
	green_led->pwm_dev,
	duty_pcts_green[78],
	&duty_pcts_green[duty_pcts_green[67]],
	duty_pcts_green[69],
	duty_pcts_green[67]+1,
	duty_pcts_green[68],
	0,
	duty_pcts_green[70],
	duty_pcts_green[76]
	);
	// 7. lut config(blue led).
	pm8xxx_pwm_lut_config(
	blue_led->pwm_dev,
	duty_pcts_blue[78],
	&duty_pcts_blue[duty_pcts_blue[71]],
	duty_pcts_blue[73],
	duty_pcts_blue[71]+1,
	duty_pcts_blue[72],
	0,
	duty_pcts_blue[74],
	duty_pcts_blue[77]
	);

	// 8. lut enable, so we can run led after done this.
	pm8xxx_pwm_lut_enable( red_led->pwm_dev, 1 );
	pm8xxx_pwm_lut_enable( green_led->pwm_dev, 1 );
	pm8xxx_pwm_lut_enable( blue_led->pwm_dev, 1 );
}

void make_blink_led_pattern(int red, int green, int blue, int delay_on, int delay_off, int on){
#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS) 
	int *duty_pcts_red = NULL;

	int count_on =0;
	int count_off =0;
	int duty_ms = 0;

	int i,j = 0;

	// 1. set all leds brightness to 0
	red_led->cdev.brightness = 0;

	// 2. run work-function, as brightness 0, all led turn off
	pm8xxx_led_pwm_work(red_led);

	if(on) {
		// 3. change LUT structure.
		// and we use duty_pcts17 for blink led.
		duty_pcts_red   = red_led->pwm_duty_cycles->duty_pcts17;

		//4. calculate count_on/count_off/duty_ms
		count_on= 63*delay_on/(delay_on+delay_off);
		count_off=63-count_on;
	    duty_ms = (delay_on+delay_off)/63;

		if(delay_on>100000){
			count_on= 63;
			count_off=0;
		    duty_ms = 100;
		}

		printk("[PMIC-KPJT] RED=%d, GREEN=%d, BLUE=%d, ON=%d,OFF=%d,ENABLE=%d\n",red,green,blue,delay_on,delay_off,on);
		printk("[PMIC-KPJT] COUNT_ON=%d, COUNT_OFF=%d, DUTY_MS=%d\n",count_on,count_off,duty_ms);

		//5. make blink LUT Table
		//RED
		for(i=0;i<63;i++){
			if(j<count_on){
				duty_pcts_red[i]=red;
				j++;
			} else {
				duty_pcts_red[i]=0;
			}
		}

		printk("[PMIC K-PJT] BLINK ONLY LED LUT is \n");
		for(i=0;i<63;i++){
			printk("%d ",duty_pcts_red[i]);
		}
		printk("\n");


		// 6. lut disable, so we can edit LUT table after done this.
		pm8xxx_pwm_lut_enable( red_led->pwm_dev, 0 );

		// 7. lut config(red led).
		/**
		 * pm8xxx_pwm_lut_config - change a PWM device configuration to use LUT
		 * @pwm: the PWM device
		 * @period_us: period in microseconds
		 * @duty_pct: arrary of duty cycles in percent, like 20, 50.
		 * @duty_time_ms: time for each duty cycle in milliseconds
		 * @start_idx: start index in lookup table from 0 to MAX-1
		 * @idx_len: number of index
		 * @pause_lo: pause time in milliseconds at low index
		 * @pause_hi: pause time in milliseconds at high index
		 * @flags: control flags
		 */
		pm8xxx_pwm_lut_config(
		red_led->pwm_dev,
		2000,					//period_us
		&duty_pcts_red[0],		//duty_pct
		duty_ms,				//duty_time_ms
		1,						//start_idx
		63,						//idx_len
		0,						//pause_lo
		0,						//pause_hi
		3						//flags
		);

		// 10. lut enable, so we can run led after done this.
		pm8xxx_pwm_lut_enable( red_led->pwm_dev, 1 );

		// 11. set all leds brightness to 255
		red_led->cdev.brightness = 255;

		// 12. run work-function, as brightness 255, all led turn on
		pm8xxx_led_pwm_work(red_led);
	} else {
		//led already turned off.
		//So what to do? kk.
	}

#else
	int *duty_pcts_red = NULL;
	int *duty_pcts_green = NULL;
	int *duty_pcts_blue =NULL;
	
	int count_on =0;
	int count_off =0;
	int duty_ms = 0;

	int i,j = 0;

	// 1. set all leds brightness to 0
	red_led->cdev.brightness = 0;
	green_led->cdev.brightness = 0;
	blue_led->cdev.brightness = 0;

	// 2. run work-function, as brightness 0, all led turn off
	pm8xxx_led_pwm_work(red_led);
	pm8xxx_led_pwm_work(green_led);
	pm8xxx_led_pwm_work(blue_led);

if(on){
	// 3. change LUT structure.
	// and we use duty_pcts17 for blink led.
	duty_pcts_red   = red_led->pwm_duty_cycles->duty_pcts17;
	duty_pcts_green = green_led->pwm_duty_cycles->duty_pcts17;
	duty_pcts_blue  = blue_led->pwm_duty_cycles->duty_pcts17;

	//4. calculate count_on/count_off/duty_ms
	count_on= 21*delay_on/(delay_on+delay_off);
	count_off=21-count_on;
    duty_ms = (delay_on+delay_off)/21;

	if(delay_on>100000){
	count_on= 21;
	count_off=0;
    duty_ms = 100;

	}


	printk("[PMIC-KPJT] RED=%d, GREEN=%d, BLUE=%d, ON=%d,OFF=%d,ENABLE=%d\n",red,green,blue,delay_on,delay_off,on);
	printk("[PMIC-KPJT] COUNT_ON=%d, COUNT_OFF=%d, DUTY_MS=%d\n",count_on,count_off,duty_ms);

	//5. make blink LUT Table
	//RED
	for(i=0;i<21;i++){
		if(j<count_on){
			duty_pcts_red[i]=red;
			j++;
		}else{
			duty_pcts_red[i]=0;
		}
	}
	j=0;
	//GREEN
	for(i=21;i<42;i++){
		if(j<count_on){
			duty_pcts_green[i]=green;
			j++;
		}else{
			duty_pcts_green[i]=0;
		}
	}
	j=0;
	//BLUE
	for(i=42;i<63;i++){
		if(j<count_on){
			duty_pcts_blue[i]=blue;
			j++;
		}else{
			duty_pcts_blue[i]=0;
		}
	}
	j=0;
	printk("[PMIC K-PJT] BLINK ONLY LED LUT is \n");
	for(i=0;i<63;i++){
		printk("%d ",duty_pcts_red[i]);
	}
	printk("\n");


	// 6. lut disable, so we can edit LUT table after done this.
	pm8xxx_pwm_lut_enable( red_led->pwm_dev, 0 );
	pm8xxx_pwm_lut_enable( green_led->pwm_dev, 0 );
	pm8xxx_pwm_lut_enable( blue_led->pwm_dev, 0 );

	// 7. lut config(red led).
	/**
	 * pm8xxx_pwm_lut_config - change a PWM device configuration to use LUT
	 * @pwm: the PWM device
	 * @period_us: period in microseconds
	 * @duty_pct: arrary of duty cycles in percent, like 20, 50.
	 * @duty_time_ms: time for each duty cycle in milliseconds
	 * @start_idx: start index in lookup table from 0 to MAX-1
	 * @idx_len: number of index
	 * @pause_lo: pause time in milliseconds at low index
	 * @pause_hi: pause time in milliseconds at high index
	 * @flags: control flags
	 */
	pm8xxx_pwm_lut_config(
	red_led->pwm_dev,
	2000,					//period_us
	&duty_pcts_red[0],		//duty_pct
	duty_ms,				//duty_time_ms
	1,						//start_idx
	21,						//idx_len
	0,						//pause_lo
	0,						//pause_hi
	3						//flags
	);
	// 8. lut config(green led).
	pm8xxx_pwm_lut_config(
	green_led->pwm_dev,
	2000,					//period_us
	&duty_pcts_red[21],		//duty_pct
	duty_ms,				//duty_time_ms
	22,						//start_idx
	21,						//idx_len
	0,						//pause_lo
	0,						//pause_hi
	3						//flags
	);
	// 9. lut config(blue led).
	pm8xxx_pwm_lut_config(
	blue_led->pwm_dev,
	2000,					//period_us
	&duty_pcts_red[42],		//duty_pct
	duty_ms,				//duty_time_ms
	43,						//start_idx
	21,						//idx_len
	0,						//pause_lo
	0,						//pause_hi
	3						//flags
	);



	// 10. lut enable, so we can run led after done this.
	pm8xxx_pwm_lut_enable( red_led->pwm_dev, 1 );
	pm8xxx_pwm_lut_enable( green_led->pwm_dev, 1 );
	pm8xxx_pwm_lut_enable( blue_led->pwm_dev, 1 );

	// 11. set all leds brightness to 255
	red_led->cdev.brightness = 255;
	green_led->cdev.brightness = 255;
	blue_led->cdev.brightness = 255;

	// 12. run work-function, as brightness 255, all led turn on
	pm8xxx_led_pwm_work(red_led);
	pm8xxx_led_pwm_work(green_led);
	pm8xxx_led_pwm_work(blue_led);
}else{
	//led already turned off.
	//So what to do? kk.
}
#endif
}

void pattern_test(int pattern_on)
{

		red_led->cdev.brightness = 0;
		green_led->cdev.brightness = 0;
		blue_led->cdev.brightness = 0;

		pm8xxx_led_pwm_work(red_led);
		pm8xxx_led_pwm_work(green_led);
		pm8xxx_led_pwm_work(blue_led);
		if(pattern_on==1){
			printk("[PMIC K-PJT] test pattern red is ON!! \n");
			red_led->cdev.brightness = 255;
			green_led->cdev.brightness = 0;
			blue_led->cdev.brightness = 0;

			pm8xxx_led_pwm_work(red_led);
			pm8xxx_led_pwm_work(green_led);
			pm8xxx_led_pwm_work(blue_led);
		}else if(pattern_on==2){
			printk("[PMIC K-PJT] test pattern green is ON!! \n");
			red_led->cdev.brightness = 0;
			green_led->cdev.brightness = 255;
			blue_led->cdev.brightness = 0;
			pm8xxx_led_pwm_work(red_led);
			pm8xxx_led_pwm_work(green_led);
			pm8xxx_led_pwm_work(blue_led);
		}else if(pattern_on==3){
			printk("[PMIC K-PJT] test pattern blue is ON!! \n");
			red_led->cdev.brightness = 0;
			green_led->cdev.brightness = 0;
			blue_led->cdev.brightness = 255;
			pm8xxx_led_pwm_work(red_led);
			pm8xxx_led_pwm_work(green_led);
			pm8xxx_led_pwm_work(blue_led);
		}else if(pattern_on==4){
			printk("[PMIC K-PJT] test pattern red+green is ON!! \n");
			red_led->cdev.brightness = 255;
			green_led->cdev.brightness = 255;
			blue_led->cdev.brightness = 0;
			pm8xxx_led_pwm_work(red_led);
			pm8xxx_led_pwm_work(green_led);
			pm8xxx_led_pwm_work(blue_led);
		}else if(pattern_on==5){
			printk("[PMIC K-PJT] test pattern red+blue is ON!! \n");
			red_led->cdev.brightness = 255;
			green_led->cdev.brightness = 0;
			blue_led->cdev.brightness = 255;
			pm8xxx_led_pwm_work(red_led);
			pm8xxx_led_pwm_work(green_led);
			pm8xxx_led_pwm_work(blue_led);
		}else if(pattern_on==6){
			printk("[PMIC K-PJT] test pattern green+blue is ON!! \n");
			red_led->cdev.brightness = 0;
			green_led->cdev.brightness = 255;
			blue_led->cdev.brightness = 255;
			pm8xxx_led_pwm_work(red_led);
			pm8xxx_led_pwm_work(green_led);
			pm8xxx_led_pwm_work(blue_led);
		}else if(pattern_on==7){
			printk("[PMIC K-PJT] test pattern red+green+blue is ON!! \n");
			red_led->cdev.brightness = 255;
			green_led->cdev.brightness = 255;
			blue_led->cdev.brightness = 255;
			pm8xxx_led_pwm_work(red_led);
			pm8xxx_led_pwm_work(green_led);
			pm8xxx_led_pwm_work(blue_led);
		}else{
			printk("[PMIC K-PJT] test pattern red+green+blue is OFF!! \n");
			red_led->cdev.brightness = 0;
			green_led->cdev.brightness = 0;
			blue_led->cdev.brightness = 0;
			pm8xxx_led_pwm_work(red_led);
			pm8xxx_led_pwm_work(green_led);
			pm8xxx_led_pwm_work(blue_led);
		}

}
void change_led_pattern (int pattern, int pattern_r_on, int pattern_g_on, int pattern_b_on )
{

    int i =0;

	int *duty_pcts_red = NULL;
	int *duty_pcts_green = NULL;
	int *duty_pcts_blue =NULL;
	
	// 1. set all leds brightness to 0
	red_led->cdev.brightness = 0;
	green_led->cdev.brightness = 0;
	blue_led->cdev.brightness = 0;

	// 2. run work-function, as brightness 0, all led turn off
	pm8xxx_led_pwm_work(red_led);
	pm8xxx_led_pwm_work(green_led);
	pm8xxx_led_pwm_work(blue_led);

	if(pattern_r_on|pattern_g_on|pattern_b_on){
	// 3. change LUT structure in platform device.
	switch (pattern) {

	case 1:
		duty_pcts_red   = red_led->pwm_duty_cycles->duty_pcts1;
		duty_pcts_green = green_led->pwm_duty_cycles->duty_pcts1;
		duty_pcts_blue  = blue_led->pwm_duty_cycles->duty_pcts1;
		break;

	case 2:
		duty_pcts_red   = red_led->pwm_duty_cycles->duty_pcts2;
		duty_pcts_green = green_led->pwm_duty_cycles->duty_pcts2;
		duty_pcts_blue  = blue_led->pwm_duty_cycles->duty_pcts2;
		break;

	case 3:
		duty_pcts_red   = red_led->pwm_duty_cycles->duty_pcts3;
		duty_pcts_green = green_led->pwm_duty_cycles->duty_pcts3;
		duty_pcts_blue  = blue_led->pwm_duty_cycles->duty_pcts3;
		break;

	case 4:
		duty_pcts_red   = red_led->pwm_duty_cycles->duty_pcts4;
		duty_pcts_green = green_led->pwm_duty_cycles->duty_pcts4;
		duty_pcts_blue  = blue_led->pwm_duty_cycles->duty_pcts4;
		break;

	case 5:
		duty_pcts_red   = red_led->pwm_duty_cycles->duty_pcts5;
		duty_pcts_green = green_led->pwm_duty_cycles->duty_pcts5;
		duty_pcts_blue  = blue_led->pwm_duty_cycles->duty_pcts5;
		break;

	case 6:
		duty_pcts_red   = red_led->pwm_duty_cycles->duty_pcts6;
		duty_pcts_green = green_led->pwm_duty_cycles->duty_pcts6;
		duty_pcts_blue  = blue_led->pwm_duty_cycles->duty_pcts6;
		break;

	case 7:
		duty_pcts_red   = red_led->pwm_duty_cycles->duty_pcts7;
		duty_pcts_green = green_led->pwm_duty_cycles->duty_pcts7;
		duty_pcts_blue  = blue_led->pwm_duty_cycles->duty_pcts7;
		break;

	case 8:
		duty_pcts_red   = red_led->pwm_duty_cycles->duty_pcts8;
		duty_pcts_green = green_led->pwm_duty_cycles->duty_pcts8;
		duty_pcts_blue  = blue_led->pwm_duty_cycles->duty_pcts8;
		break;

	case 9:
		duty_pcts_red   = red_led->pwm_duty_cycles->duty_pcts9;
		duty_pcts_green = green_led->pwm_duty_cycles->duty_pcts9;
		duty_pcts_blue  = blue_led->pwm_duty_cycles->duty_pcts9;
		break;

	case 10:
		duty_pcts_red   = red_led->pwm_duty_cycles->duty_pcts10;
		duty_pcts_green = green_led->pwm_duty_cycles->duty_pcts10;
		duty_pcts_blue  = blue_led->pwm_duty_cycles->duty_pcts10;
		break;

	case 11:
		duty_pcts_red   = red_led->pwm_duty_cycles->duty_pcts11;
		duty_pcts_green = green_led->pwm_duty_cycles->duty_pcts11;
		duty_pcts_blue  = blue_led->pwm_duty_cycles->duty_pcts11;
		break;

	case 12:
		duty_pcts_red   = red_led->pwm_duty_cycles->duty_pcts12;
		duty_pcts_green = green_led->pwm_duty_cycles->duty_pcts12;
		duty_pcts_blue  = blue_led->pwm_duty_cycles->duty_pcts12;
		break;

	case 13:
		duty_pcts_red   = red_led->pwm_duty_cycles->duty_pcts13;
		duty_pcts_green = green_led->pwm_duty_cycles->duty_pcts13;
		duty_pcts_blue  = blue_led->pwm_duty_cycles->duty_pcts13;
		break;

	case 14:
		duty_pcts_red   = red_led->pwm_duty_cycles->duty_pcts14;
		duty_pcts_green = green_led->pwm_duty_cycles->duty_pcts14;
		duty_pcts_blue  = blue_led->pwm_duty_cycles->duty_pcts14;
		break;

	case 15:
		duty_pcts_red   = red_led->pwm_duty_cycles->duty_pcts15;
		duty_pcts_green = green_led->pwm_duty_cycles->duty_pcts15;
		duty_pcts_blue  = blue_led->pwm_duty_cycles->duty_pcts15;
		break;

	case 16:
		duty_pcts_red   = red_led->pwm_duty_cycles->duty_pcts16;
		duty_pcts_green = green_led->pwm_duty_cycles->duty_pcts16;
		duty_pcts_blue  = blue_led->pwm_duty_cycles->duty_pcts16;
		break;

	default:
		return;
	}


	printk("[PMIC K-PJT] LUT is \n");
	for(i=0;i<79;i++){
		printk("%d ",duty_pcts_red[i]);
	}
	printk("\n");


	// 4. lut disable, so we can edit LUT table after done this.
	pm8xxx_pwm_lut_enable( red_led->pwm_dev, 0 );
	pm8xxx_pwm_lut_enable( green_led->pwm_dev, 0 );
	pm8xxx_pwm_lut_enable( blue_led->pwm_dev, 0 );

	// 5. lut config(red led).
	pm8xxx_pwm_lut_config(
	red_led->pwm_dev,
	duty_pcts_red[78],
	&duty_pcts_red[duty_pcts_red[63]],
	duty_pcts_red[65],
	duty_pcts_red[63]+1,
	duty_pcts_red[64],
	0,
	duty_pcts_red[66],
	duty_pcts_red[75]
	);
	// 6. lut config(green led).
	pm8xxx_pwm_lut_config(
	green_led->pwm_dev,
	duty_pcts_green[78],
	&duty_pcts_green[duty_pcts_green[67]],
	duty_pcts_green[69],
	duty_pcts_green[67]+1,
	duty_pcts_green[68],
	0,
	duty_pcts_green[70],
	duty_pcts_green[76]
	);
	// 7. lut config(blue led).
	pm8xxx_pwm_lut_config(
	blue_led->pwm_dev,
	duty_pcts_blue[78],
	&duty_pcts_blue[duty_pcts_blue[71]],
	duty_pcts_blue[73],
	duty_pcts_blue[71]+1,
	duty_pcts_blue[72],
	0,
	duty_pcts_blue[74],
	duty_pcts_blue[77]
	);

	// 8. lut enable, so we can run led after done this.
	pm8xxx_pwm_lut_enable( red_led->pwm_dev, 1 );
	pm8xxx_pwm_lut_enable( green_led->pwm_dev, 1 );
	pm8xxx_pwm_lut_enable( blue_led->pwm_dev, 1 );


	// 9. set leds brightness to 255
	if(pattern_r_on)
	{
	red_led->cdev.brightness = 255;
	}
	if(pattern_g_on)
	{
	green_led->cdev.brightness = 255;
	}
	if(pattern_b_on)
	{
	blue_led->cdev.brightness = 255;
	}


	// 10. run work-function, as brightness 255, all led turn on
	pm8xxx_led_pwm_work(red_led);
	pm8xxx_led_pwm_work(green_led);
	pm8xxx_led_pwm_work(blue_led);
	}else{
	//led already turned off.
	//So what to do? kk.
	}



}

#else
#endif
/*                                                         */

static int pm8xxx_led_pwm_configure(struct pm8xxx_led_data *led)
{
/*                                                         */
#ifdef CONFIG_LGE_PM8038_KPJT
	int start_idx, idx_len, duty_us, pause_lo, rc;
#else
	int start_idx, idx_len, duty_us, rc;
#endif
/*                                                         */
	led->pwm_dev = pwm_request(led->pwm_channel,
					led->cdev.name);

/*                                                         */ 
#ifdef CONFIG_LGE_PM8038_KPJT
	if ( led->pwm_channel ==  RED_CHANNEL ){
		red_led = led ;
	}else if(led->pwm_channel ==  GREEN_CHANNEL) {
		green_led = led ;
	}else if(led->pwm_channel ==  BLUE_CHANNEL){
		blue_led = led ;
	}
#else
#endif
/*                                                         */ 



	if (IS_ERR_OR_NULL(led->pwm_dev)) {
		pr_err("could not acquire PWM Channel %d, "
			"error %ld\n", led->pwm_channel,
			PTR_ERR(led->pwm_dev));
		led->pwm_dev = NULL;
		return -ENODEV;
	}

	if (led->pwm_duty_cycles != NULL) {
		start_idx = led->pwm_duty_cycles->start_idx;
		idx_len = led->pwm_duty_cycles->num_duty_pcts;

		if (idx_len >= PM_PWM_LUT_SIZE && start_idx) {
			pr_err("Wrong LUT size or index\n");
			return -EINVAL;
		}
		if ((start_idx + idx_len) > PM_PWM_LUT_SIZE) {
			pr_err("Exceed LUT limit\n");
			return -EINVAL;
		}
/*                                                         */ 
#ifdef CONFIG_LGE_PM8038_KPJT
#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS) 
		pause_lo = led->pwm_duty_cycles->pause_lo;
		rc = pm8xxx_pwm_lut_config(led->pwm_dev, led->pwm_period_us,
				led->pwm_duty_cycles->duty_pcts0,
				led->pwm_duty_cycles->duty_ms,
				start_idx, idx_len, pause_lo, 0,
				PM8XXX_LED_PWM_FLAGS);
#else
		pause_lo = led->pwm_duty_cycles->pause_lo;
		rc = pm8xxx_pwm_lut_config(led->pwm_dev, led->pwm_period_us,
				led->pwm_duty_cycles->duty_pcts1,
				led->pwm_duty_cycles->duty_ms,
				start_idx, idx_len, pause_lo, 0,
				PM8XXX_LED_PWM_FLAGS);
#endif
#else
		rc = pm8xxx_pwm_lut_config(led->pwm_dev, led->pwm_period_us,
				led->pwm_duty_cycles->duty_pcts,
				led->pwm_duty_cycles->duty_ms,
				start_idx, idx_len, 0, 0,
				PM8XXX_LED_PWM_FLAGS);
#endif
/*                                                         */ 
	} else {
		duty_us = led->pwm_period_us;
		rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
	}

	return rc;
}


static int __devinit pm8xxx_led_probe(struct platform_device *pdev)
{
	const struct pm8xxx_led_platform_data *pdata = pdev->dev.platform_data;
	const struct led_platform_data *pcore_data;
	struct led_info *curr_led;
	struct pm8xxx_led_data *led, *led_dat;
	struct pm8xxx_led_config *led_cfg;
	enum pm8xxx_version version;
	bool found = false;
	int rc, i, j;

	if (pdata == NULL) {
		dev_err(&pdev->dev, "platform data not supplied\n");
		return -EINVAL;
	}

	pcore_data = pdata->led_core;

	if (pcore_data->num_leds != pdata->num_configs) {
		dev_err(&pdev->dev, "#no. of led configs and #no. of led"
				"entries are not equal\n");
		return -EINVAL;
	}

	led = kcalloc(pcore_data->num_leds, sizeof(*led), GFP_KERNEL);
	if (led == NULL) {
		dev_err(&pdev->dev, "failed to alloc memory\n");
		return -ENOMEM;
	}

	for (i = 0; i < pcore_data->num_leds; i++) {
		curr_led	= &pcore_data->leds[i];
		led_dat		= &led[i];
		led_cfg		= &pdata->configs[i];

		led_dat->id     = led_cfg->id;
		led_dat->pwm_channel = led_cfg->pwm_channel;
		led_dat->pwm_period_us = led_cfg->pwm_period_us;
		led_dat->pwm_duty_cycles = led_cfg->pwm_duty_cycles;
		led_dat->wled_cfg = led_cfg->wled_cfg;
		led_dat->max_current = led_cfg->max_current;

		if (!((led_dat->id >= PM8XXX_ID_LED_KB_LIGHT) &&
				(led_dat->id < PM8XXX_ID_MAX))) {
			dev_err(&pdev->dev, "invalid LED ID(%d) specified\n",
				led_dat->id);
			rc = -EINVAL;
			goto fail_id_check;

		}

		found = false;
		version = pm8xxx_get_version(pdev->dev.parent);
		for (j = 0; j < ARRAY_SIZE(led_map); j++) {
			if (version == led_map[j].version
			&& (led_map[j].supported & (1 << led_dat->id))) {
				found = true;
				break;
			}
		}

		if (!found) {
			dev_err(&pdev->dev, "invalid LED ID(%d) specified\n",
				led_dat->id);
			rc = -EINVAL;
			goto fail_id_check;
		}

		led_dat->cdev.name		= curr_led->name;
		led_dat->cdev.default_trigger   = curr_led->default_trigger;
		led_dat->cdev.brightness_set    = pm8xxx_led_set;
		led_dat->cdev.brightness_get    = pm8xxx_led_get;
		led_dat->cdev.brightness	= LED_OFF;
		led_dat->cdev.flags		= curr_led->flags;
		led_dat->dev			= &pdev->dev;

		rc =  get_init_value(led_dat, &led_dat->reg);
		if (rc < 0)
			goto fail_id_check;

		rc = pm8xxx_set_led_mode_and_max_brightness(led_dat,
					led_cfg->mode, led_cfg->max_current);
		if (rc < 0)
			goto fail_id_check;

		mutex_init(&led_dat->lock);
		INIT_WORK(&led_dat->work, pm8xxx_led_work);

		rc = led_classdev_register(&pdev->dev, &led_dat->cdev);
		if (rc) {
			dev_err(&pdev->dev, "unable to register led %d,rc=%d\n",
						 led_dat->id, rc);
			goto fail_id_check;
		}

		/* configure default state */
		if (led_cfg->default_state)
			led->cdev.brightness = led_dat->cdev.max_brightness;
		else
			led->cdev.brightness = LED_OFF;

		if (led_cfg->mode != PM8XXX_LED_MODE_MANUAL) {
			if (led_dat->id == PM8XXX_ID_RGB_LED_RED ||
				led_dat->id == PM8XXX_ID_RGB_LED_GREEN ||
				led_dat->id == PM8XXX_ID_RGB_LED_BLUE)
				__pm8xxx_led_work(led_dat, 0);
			else
				__pm8xxx_led_work(led_dat,
					led_dat->cdev.max_brightness);

			if (led_dat->pwm_channel != -1) {
				led_dat->cdev.max_brightness = LED_FULL;
				rc = pm8xxx_led_pwm_configure(led_dat);
				if (rc) {
					dev_err(&pdev->dev, "failed to "
					"configure LED, error: %d\n", rc);
					goto fail_id_check;
				}
			schedule_work(&led->work);
			}
		} else {
			__pm8xxx_led_work(led_dat, led->cdev.brightness);
		}
	}
/*                                                         */ 
#ifdef CONFIG_LGE_PM8038_KPJT
	rc = led_pattern_sysfs_register();
	if (rc) {
		dev_err(&pdev->dev, "unable to register pattern_sysfs_register\n");
		goto fail_id_check;
	}
#else
#endif
/*                                                         */ 
	platform_set_drvdata(pdev, led);

#ifdef CONFIG_LGE_PM8038_KPJT
#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS) 
	/* skip booting led*/
#else
    if(lge_get_boot_mode() != LGE_BOOT_MODE_MINIOS)
        change_led_pattern(1,1,1,0);
#endif
#endif

	return 0;

fail_id_check:
	if (i > 0) {
		for (i = i - 1; i >= 0; i--) {
			mutex_destroy(&led[i].lock);
			led_classdev_unregister(&led[i].cdev);
			if (led[i].pwm_dev != NULL)
				pwm_free(led[i].pwm_dev);
		}
	}
	kfree(led);
	return rc;
}

static int __devexit pm8xxx_led_remove(struct platform_device *pdev)
{
	int i;
	const struct led_platform_data *pdata =
				pdev->dev.platform_data;
	struct pm8xxx_led_data *led = platform_get_drvdata(pdev);

	for (i = 0; i < pdata->num_leds; i++) {
		cancel_work_sync(&led[i].work);
		mutex_destroy(&led[i].lock);
		led_classdev_unregister(&led[i].cdev);
		if (led[i].pwm_dev != NULL)
			pwm_free(led[i].pwm_dev);
	}

	kfree(led);

	return 0;
}

static struct platform_driver pm8xxx_led_driver = {
	.probe		= pm8xxx_led_probe,
	.remove		= __devexit_p(pm8xxx_led_remove),
	.driver		= {
		.name	= PM8XXX_LEDS_DEV_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init pm8xxx_led_init(void)
{
	return platform_driver_register(&pm8xxx_led_driver);
}
subsys_initcall(pm8xxx_led_init);

static void __exit pm8xxx_led_exit(void)
{
	platform_driver_unregister(&pm8xxx_led_driver);
}
module_exit(pm8xxx_led_exit);

MODULE_DESCRIPTION("PM8XXX LEDs driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:pm8xxx-led");
