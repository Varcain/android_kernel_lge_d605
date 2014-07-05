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

#include <linux/interrupt.h>
#include <linux/mfd/pm8xxx/pm8038.h>
#include <linux/mfd/pm8xxx/pm8xxx-adc.h>
#include <linux/msm_ssbi.h>
#include <asm/mach-types.h>
#include <mach/msm_bus_board.h>
#include <mach/restart.h>
#include <mach/socinfo.h>
#include "devices.h"

#include <mach/board_lge.h>
#include "board-fx3.h"

/* Vibrator */
#ifdef CONFIG_LGE_DIRECT_QCOIN_VIBRATOR
#include <linux/mfd/pm8xxx/direct_qcoin.h>
#include "../../../../../drivers/staging/android/timed_output.h"
#endif

#ifdef CONFIG_MACH_MSM8930_FX3
#include <linux/gpio.h>
#endif

extern unsigned int system_rev;

struct pm8xxx_gpio_init {
	unsigned			gpio;
	struct pm_gpio			config;
};

struct pm8xxx_mpp_init {
	unsigned			mpp;
	struct pm8xxx_mpp_config_data	config;
};

#define PM8038_GPIO_INIT(_gpio, _dir, _buf, _val, _pull, _vin, _out_strength, \
			_func, _inv, _disable) \
{ \
	.gpio	= PM8038_GPIO_PM_TO_SYS(_gpio), \
	.config	= { \
		.direction	= _dir, \
		.output_buffer	= _buf, \
		.output_value	= _val, \
		.pull		= _pull, \
		.vin_sel	= _vin, \
		.out_strength	= _out_strength, \
		.function	= _func, \
		.inv_int_pol	= _inv, \
		.disable_pin	= _disable, \
	} \
}

#define PM8038_MPP_INIT(_mpp, _type, _level, _control) \
{ \
	.mpp	= PM8038_MPP_PM_TO_SYS(_mpp), \
	.config	= { \
		.type		= PM8XXX_MPP_TYPE_##_type, \
		.level		= _level, \
		.control	= PM8XXX_MPP_##_control, \
	} \
}

#define PM8038_GPIO_DISABLE(_gpio) \
	PM8038_GPIO_INIT(_gpio, PM_GPIO_DIR_IN, 0, 0, 0, PM8038_GPIO_VIN_L11, \
			 0, 0, 0, 1)

#define PM8038_GPIO_OUTPUT(_gpio, _val) \
	PM8038_GPIO_INIT(_gpio, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, _val, \
			PM_GPIO_PULL_NO, PM8038_GPIO_VIN_L11, \
			PM_GPIO_STRENGTH_HIGH, \
			PM_GPIO_FUNC_NORMAL, 0, 0)

#define PM8038_GPIO_INPUT(_gpio, _pull) \
	PM8038_GPIO_INIT(_gpio, PM_GPIO_DIR_IN, PM_GPIO_OUT_BUF_CMOS, 0, \
			_pull, PM8038_GPIO_VIN_L11, \
			PM_GPIO_STRENGTH_NO, \
			PM_GPIO_FUNC_NORMAL, 0, 0)

#define PM8038_GPIO_OUTPUT_FUNC(_gpio, _val, _func) \
	PM8038_GPIO_INIT(_gpio, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, _val, \
			PM_GPIO_PULL_NO, PM8038_GPIO_VIN_L11, \
			PM_GPIO_STRENGTH_HIGH, \
			_func, 0, 0)

#define PM8038_GPIO_OUTPUT_VIN(_gpio, _val, _vin) \
	PM8038_GPIO_INIT(_gpio, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, _val, \
			PM_GPIO_PULL_NO, _vin, \
			PM_GPIO_STRENGTH_HIGH, \
			PM_GPIO_FUNC_NORMAL, 0, 0)



/* Initial PM8038 GPIO configurations */
static struct pm8xxx_gpio_init pm8038_gpios[] __initdata = {
	/* keys GPIOs */
#if defined(CONFIG_MACH_LGE_FX3_VZW)  || defined(CONFIG_MACH_LGE_FX3Q_TMUS)
	PM8038_GPIO_INPUT(2, PM_GPIO_PULL_UP_30),
	PM8038_GPIO_INPUT(3, PM_GPIO_PULL_UP_30),
#else /*                          */
#ifdef CONFIG_MACH_MSM8930_FX3
	PM8038_GPIO_INPUT(1, PM_GPIO_PULL_UP_30),
	PM8038_GPIO_INPUT(2, PM_GPIO_PULL_UP_30),
	PM8038_GPIO_INPUT(3, PM_GPIO_PULL_UP_30),
#if !(defined(CONFIG_MACH_LGE_F6_TMUS) || defined(CONFIG_MACH_LGE_F6_VDF) || defined(CONFIG_MACH_LGE_F6_ORG ) || defined(CONFIG_MACH_LGE_F6_OPEN) \
	    || defined(CONFIG_MACH_LGE_F6_TMO)) || !defined(CONFIG_MACH_LGE_L9II_COMMON)
	PM8038_GPIO_INPUT(8, PM_GPIO_PULL_UP_30),
#endif
#endif

#endif /*                         */

/*             
                                     
                                      
 */
#if defined( CONFIG_MACH_LGE_F6_TMUS)|| defined(CONFIG_MACH_LGE_F6_VDF) || defined(CONFIG_MACH_LGE_F6_ORG)|| defined(CONFIG_MACH_LGE_F6_OPEN) || defined(CONFIG_MACH_LGE_F6_TMO)
	PM8038_GPIO_INPUT(5, PM_GPIO_PULL_UP_30),
#else
/*              */
	/* haptics gpio */
	PM8038_GPIO_OUTPUT_FUNC(7, 0, PM_GPIO_FUNC_1),
	/* MHL PWR EN */
	PM8038_GPIO_OUTPUT_VIN(5, 1, PM8038_GPIO_VIN_VPH),
#endif
};

#if defined(CONFIG_MACH_LGE_FX3_TMUS) || defined(CONFIG_MACH_LGE_FX3_EUR) \
	|| defined (CONFIG_MACH_LGE_FX3_ATT) || defined(CONFIG_MACH_LGE_FX3_WCDMA_TRF_US)
static struct pm8xxx_gpio_init pm8038_gpio_config__sleep_clk[1] __initdata = {
	/* Sleep clock setting for BCM4330, REV_A only */
	PM8038_GPIO_OUTPUT_FUNC(8, 1, PM_GPIO_FUNC_1)
};
#endif

#ifdef CONFIG_MACH_MSM8930_FX3
static int msm8930_nc_gpios[] __initdata = {
	LGE_MSM8930_NC_GPIO_INITDATA
};

static int pm8038_nc_gpios[] __initdata = {
	LGE_PM8038_NC_GPIO_INITDATA
};

static struct pm_gpio pm_nc_gpio_config = {
	.direction		= PM_GPIO_DIR_IN,
	.output_buffer	= PM_GPIO_OUT_BUF_CMOS,
	.output_value	= 0,
	.pull			= PM_GPIO_PULL_DN,
	.vin_sel		= PM8038_GPIO_VIN_L11,
	.out_strength	= PM_GPIO_STRENGTH_NO,
	.function		= PM_GPIO_FUNC_NORMAL,
	.inv_int_pol	= 0,
	.disable_pin	= 0,
};
#endif

/* Initial PM8038 MPP configurations */
static struct pm8xxx_mpp_init pm8038_mpps[] __initdata = {
};


void __init msm8930_pm8038_gpio_mpp_init(void)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(pm8038_gpios); i++) {
		rc = pm8xxx_gpio_config(pm8038_gpios[i].gpio,
					&pm8038_gpios[i].config);
		if (rc) {
			pr_err("%s: pm8xxx_gpio_config: rc=%d\n", __func__, rc);
			break;
		}
	}

	/* Initial MPP configuration. */
	for (i = 0; i < ARRAY_SIZE(pm8038_mpps); i++) {
		rc = pm8xxx_mpp_config(pm8038_mpps[i].mpp,
					&pm8038_mpps[i].config);
		if (rc) {
			pr_err("%s: pm8xxx_mpp_config: rc=%d\n", __func__, rc);
			break;
		}
	}
#if defined(CONFIG_MACH_LGE_FX3_TMUS) || defined(CONFIG_MACH_LGE_FX3_EUR) \
	|| defined (CONFIG_MACH_LGE_FX3_ATT)|| defined(CONFIG_MACH_LGE_FX3_WCDMA_TRF_US)
	if(system_rev == HW_REV_A)
	{
		pm8xxx_gpio_config(pm8038_gpio_config__sleep_clk[0].gpio,
					&pm8038_gpio_config__sleep_clk[0].config);
	}
#endif

/*                                                      
                                  
 */
#ifdef CONFIG_MACH_MSM8930_FX3
	for (i = 0; i < ARRAY_SIZE(msm8930_nc_gpios); i++) {
		gpio_tlmm_config(GPIO_CFG(msm8930_nc_gpios[i], 0,
			GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		if (rc) {
			pr_err("%s: msm8930_nc_gpio_config: rc=%d\n", __func__, rc);
			break;
		}
	}

	for (i = 0; i < ARRAY_SIZE(pm8038_nc_gpios); i++) {
		pm8xxx_gpio_config(PM8038_GPIO_PM_TO_SYS(pm8038_nc_gpios[i]),
					&pm_nc_gpio_config);
		if (rc) {
			pr_err("%s: pm8038_nc_gpio_config: rc=%d\n", __func__, rc);
			break;
		}
	}
#endif
}

static struct pm8xxx_adc_amux pm8038_adc_channels_data[] = {
	{"vcoin", CHANNEL_VCOIN, CHAN_PATH_SCALING2, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"vbat", CHANNEL_VBAT, CHAN_PATH_SCALING2, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"dcin", CHANNEL_DCIN, CHAN_PATH_SCALING4, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"ichg", CHANNEL_ICHG, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"vph_pwr", CHANNEL_VPH_PWR, CHAN_PATH_SCALING2, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"ibat", CHANNEL_IBAT, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"batt_therm", CHANNEL_BATT_THERM, CHAN_PATH_SCALING1, AMUX_RSV2,
		ADC_DECIMATION_TYPE2, ADC_SCALE_BATT_THERM},
	{"batt_id", CHANNEL_BATT_ID, CHAN_PATH_SCALING1, AMUX_RSV2,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"usbin", CHANNEL_USBIN, CHAN_PATH_SCALING3, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"pmic_therm", CHANNEL_DIE_TEMP, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_PMIC_THERM},
	{"625mv", CHANNEL_625MV, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"125v", CHANNEL_125V, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"chg_temp", CHANNEL_CHG_TEMP, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
#ifdef CONFIG_LGE_PM_TEMP_SENSOR
	/*                                               
                                                           
                                                      */
	{"pcb_therm", ADC_MPP_1_AMUX4, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
#else /* QCT original */
	{"pa_therm1", ADC_MPP_1_AMUX4, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_PA_THERM},
	/*              */
#endif /*                           */
	{"xo_therm", CHANNEL_MUXOFF, CHAN_PATH_SCALING1, AMUX_RSV0,
		ADC_DECIMATION_TYPE2, ADC_SCALE_XOTHERM},
	{"pa_therm0", ADC_MPP_1_AMUX3, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_PA_THERM},
	{"usb_id", ADC_MPP_1_AMUX6, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
};

static struct pm8xxx_adc_properties pm8038_adc_data = {
	.adc_vdd_reference	= 1800, /* milli-voltage for this adc */
	.bitresolution		= 15,
	.bipolar                = 0,
};

static struct pm8xxx_adc_platform_data pm8038_adc_pdata = {
	.adc_channel            = pm8038_adc_channels_data,
	.adc_num_board_channel  = ARRAY_SIZE(pm8038_adc_channels_data),
	.adc_prop               = &pm8038_adc_data,
	.adc_mpp_base		= PM8038_MPP_PM_TO_SYS(1),
};

static struct pm8xxx_irq_platform_data pm8xxx_irq_pdata __devinitdata = {
	.irq_base		= PM8038_IRQ_BASE,
	.devirq			= MSM_GPIO_TO_INT(104),
	.irq_trigger_flag	= IRQF_TRIGGER_LOW,
};

static struct pm8xxx_gpio_platform_data pm8xxx_gpio_pdata __devinitdata = {
	.gpio_base	= PM8038_GPIO_PM_TO_SYS(1),
};

static struct pm8xxx_mpp_platform_data pm8xxx_mpp_pdata __devinitdata = {
	.mpp_base	= PM8038_MPP_PM_TO_SYS(1),
};

static struct pm8xxx_rtc_platform_data pm8xxx_rtc_pdata __devinitdata = {
	.rtc_write_enable	= false,
	.rtc_alarm_powerup	= false,
};

static struct pm8xxx_pwrkey_platform_data pm8xxx_pwrkey_pdata = {
	.pull_up		= 1,
	.kpd_trigger_delay_us	= 15625,
	.wakeup			= 1,
};

static int pm8921_therm_mitigation[] = {
	IBAT_CURRENT,
	700,
	600,
	325,
};

/*                                                                         */
#ifdef CONFIG_LGE_PM_435V_BATT
#define MAX_VOLTAGE_MV		4350
#else
#define MAX_VOLTAGE_MV		4200
#endif

#if defined(CONFIG_MACH_LGE_FX3_SPCS) || defined(CONFIG_MACH_LGE_FX3_TMUS)
#define CHG_TERM_MA		130

#else 

#if defined(CONFIG_MACH_LGE_L9II_COMMON)
#define CHG_TERM_MA		100
#else
#define CHG_TERM_MA		100
#endif

#endif

static struct pm8921_charger_platform_data pm8921_chg_pdata __devinitdata = {
#ifdef CONFIG_LGE_PM_435V_BATT
	//.safety_time			= 480,
	.update_time			= 60000,
	.max_voltage			= MAX_VOLTAGE_MV,
	.min_voltage			= 2700,
#else
	//.safety_time		= 180,
	.update_time		= 60000,
	.max_voltage		= MAX_VOLTAGE_MV,
	.min_voltage		= 3200,
#endif
#ifndef CONFIG_LGE_PM
	.uvd_thresh_voltage	= 4050,
#endif /* QCT ORIGIN */
	.alarm_low_mv		= 3400,
	.alarm_high_mv		= 4280,
#if defined(CONFIG_MACH_MSM8960_D1LV)
	.resume_voltage_delta	= 70,

#elif defined(CONFIG_MACH_LGE_L9II_COMMON)
	.resume_voltage_delta	= 70,
	.resume_charge_percent	= 99,
#else
	.resume_voltage_delta	= 100,
	.resume_charge_percent	= 99,
#endif
	.term_current		= CHG_TERM_MA,
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	.cool_temp			= INT_MIN,	/* from 10, */
	.warm_temp			= INT_MIN,	/* from 40, */
	.cold_thr		= 1,
	/*           
                                    
                                
 */
	.thermal_mitigation_method = IUSB_NORMAL_METHOD,
	/*                               */
#else
#ifdef CONFIG_LGE_PM_435V_BATT
	.cool_temp			= 0,	/* from 10 */
	.warm_temp			= 45,	/* from 40 */
#else
	.cool_temp		= 10,
	.warm_temp		= 40,
#endif
#endif
	.temp_check_period	= 1,
#ifdef CONFIG_LGE_PM_435V_BATT
	.max_bat_chg_current	= IBAT_CURRENT,
#else
	.max_bat_chg_current	= 1100,
#endif
	.cool_bat_chg_current	= 350,
	.warm_bat_chg_current	= 350,
	.cool_bat_voltage	= 4100,
	.warm_bat_voltage	= 4100,
	.thermal_mitigation	= pm8921_therm_mitigation,
	.thermal_levels		= ARRAY_SIZE(pm8921_therm_mitigation),
	.led_src_config		= LED_SRC_VPH_PWR,
#ifndef CONFIG_LGE_PM_435V_BATT
	.rconn_mohm		= 18,
#endif
/*
                      
                               
                                        
      
*/
#ifdef CONFIG_LGE_PM_435V_BATT
#ifdef CONFIG_LGE_PM_TRKLCHG_IN_KERNEL
#ifdef CONFIG_MACH_LGE_FX3Q_TMUS
	.weak_voltage       = 3000,
#else
	.weak_voltage		= 3200,
#endif
	.trkl_current		= 50,
#else
	.weak_voltage		= 2900,
	.weak_current		= 325,
#endif
#endif
#ifdef CONFIG_MACH_MSM8930_FX3
#if defined(CONFIG_MACH_LGE_F6_TMUS) || defined(CONFIG_MACH_LGE_F6_VDF)
	.vin_min        = 4300,
#else
	.vin_min		= 4400,
#endif
	.aicl			= true,
#endif /* CONFIG_MACH_MSM8930_FX3 */
#ifdef CONFIG_LGE_PM_BOOST_IC
	.boost_byp_sw_gpio = 56,
	.boost_byp_thr = 3600,
#endif /*                        */
#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_F6_TMUS)
	.stop_chg_upon_expiry = 1,
#endif
};

#if defined (CONFIG_MACH_LGE_F6_TMUS)|| defined(CONFIG_MACH_LGE_F6_VDF) || defined(CONFIG_MACH_LGE_F6_ORG)|| defined(CONFIG_MACH_LGE_F6_OPEN) || defined(CONFIG_MACH_LGE_F6_TMO)
#define PM8038_WLED_MAX_CURRENT		16
#elif defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS)
#define PM8038_WLED_MAX_CURRENT		20
#else
#define PM8038_WLED_MAX_CURRENT		19
#endif
#define PM8XXX_LED_PWM_PERIOD		1000
#define PM8XXX_LED_PWM_DUTY_MS		20
/*                                                         */
#ifdef CONFIG_LGE_PM8038_KPJT
#define PM8XXX_LED_PWM_PAUSE_LO		2000
#define PM8038_RGB_LED_MAX_CURRENT	12
#else
#define PM8038_RGB_LED_MAX_CURRENT	6
#endif
/*                                                         */

/*                                                         */
#ifdef CONFIG_LGE_PM8038_KPJT
static struct led_info pm8038_led_info[] = {
	[0] = {
		.name			= "wled",
		.default_trigger	= "bkl_trigger",
	},
	[1] = {
		.name			= "led:rgb_red",
	//	.default_trigger	= "battery-charging",
	},
	[2] = {
		.name			= "led:rgb_green",
	},
	[3] = {
		.name			= "led:rgb_blue",
	},
	[4] = {
		.name			= "button-backlight",
	},
	[5] = {
		.name			= "keyboard-backlight",
	},
};
#else
//=========================================================================
#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS) 
static struct led_info pm8038_led_info[] = {
	[0] = {
		.name			= "wled",
		.default_trigger	= "bkl_trigger",
	},
	[1] = {
		.name			= "red",
	},
	[2] = {
		.name			= "green",
	},
	[3] = {
		.name			= "button-backlight",
	},
	[4] = {
		.name			= "keyboard-backlight",
	},
};
#else /*                          */
#if defined(CONFIG_MACH_LGE_FX3_SPCS) || defined (CONFIG_MACH_LGE_FX3_MPCS) || defined (CONFIG_MACH_LGE_FX3_TMUS) || defined(CONFIG_MACH_LGE_F6_TMUS) || \
	defined(CONFIG_MACH_LGE_FX3_SPCSTRF) || defined(CONFIG_MACH_LGE_FX3_CRK) || defined(CONFIG_MACH_LGE_FX3_WCDMA_TRF_US) || \
	defined(CONFIG_MACH_LGE_L9II_COMMON)|| defined(CONFIG_MACH_LGE_F6_VDF) || \
    defined(CONFIG_MACH_LGE_F6_ORG) || defined(CONFIG_MACH_LGE_F6_OPEN)|| defined(CONFIG_MACH_LGE_F6_TMO)
static struct led_info pm8038_led_info[] = {
	[0] = {
		.name			= "wled",
		.default_trigger	= "bkl_trigger",
	},
	[1] = {
		.name			= "button-backlight",
	},
	[2] = {
		.name			= "led:rgb_green",
	},
#ifdef CONFIG_MACH_MSM8930_FX3
	[3] = {
		.name			= "led:rgb_red",
	},
	[4] = {
		.name			= "qwerty-backlight",
	},
#endif
};
#else
static struct led_info pm8038_led_info[] = {
	[0] = {
		.name			= "wled",
		.default_trigger	= "bkl_trigger",
	},
	[1] = {
		.name			= "led:rgb_red",
		.default_trigger	= "battery-charging",
	},
	[2] = {
		.name			= "led:rgb_green",
	},
#ifdef CONFIG_MACH_MSM8930_FX3
	[3] = {
		.name			= "button-backlight",
	},
	[4] = {
		.name			= "qwerty-backlight",
	},
#else
	[3] = {
		.name			= "led:rgb_blue",
	},
#endif
};

#endif
#endif /*                         */
//=========================================================================

#endif
/*                                                         */

static struct led_platform_data pm8038_led_core_pdata = {
	.num_leds = ARRAY_SIZE(pm8038_led_info),
	.leds = pm8038_led_info,
};

static struct wled_config_data wled_cfg = {
	.dig_mod_gen_en = true,
	.cs_out_en = true,
	.ctrl_delay_us = 0,
	.op_fdbck = true,
	.ovp_val = WLED_OVP_32V,
	.boost_curr_lim = WLED_CURR_LIMIT_525mA,
	.num_strings = 2,
};

/*                                                         */
#ifdef CONFIG_LGE_PM8038_KPJT
/*  duty pcts array is...
 *  0......62         63      64    65    66       67     68     69    70       71      72     73    74       75   76   77    78   
 *	  LUT            START, LENGTH, DUTY, PAUSE   START, LENGTH, DUTY, PAUSE    START, LENGTH, DUTY, PAUSE    FLAG FLAG FLAG PERIOD  
 *   TABLE                       [RED]                      [GREEN]                       [BLUE]               RED  GRN  BLE  ALL       
 */


//Default (working-pattern)
static int pm8038_leds_pwm_duty_pcts0[79] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,
        0,0,0,0,
        0,0,0,0,
        0,0,0,2000
};

//#1 ID_POWER_ON (RG)
static int pm8038_leds_pwm_duty_pcts1[79] = {
    66,70,80,85,87,85,80,77,73,70,67,63,60,57,53,50,47,43,40,37,33,30,27,23,20,17,13,10,7,3,0,100,99,97,95,93,80,60,40,30,26,24,22,19,15,13,10,9,8,7,6,5,4,3,3,2,1,0,0,0,0,0,0,
        0,31,32,0,
        31,31,32,0,
        62,1,32,0,
        5,5,5,2000
};

//#2 ID_LCD_ON (RGB)
static int pm8038_leds_pwm_duty_pcts2[79] = {
    100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,99,98,95,90,85,79,73,65,58,50,42,35,27,21,15,10,5,2,1,0,0,0,0,0,0,0,0,0,0,
        0,54,19,0,
        0,54,19,0,
        0,54,19,0,
        2,2,2,2000
};

//#3 ID_CHARGING (R)
static int pm8038_leds_pwm_duty_pcts3[79] = {
    0,2,4,7,9,11,13,16,18,20,22,24,27,29,31,33,36,38,40,42,44,47,49,51,53,56,58,60,62,64,67,69,71,73,76,78,80,82,84,87,89,91,93,96,98,100,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,46,64,0,
        0,46,64,0,
        0,46,64,0,
        7,7,7,2000
/*    0,0,1,3,6,9,13,17,22,27,32,38,44,50,56,62,68,73,78,83,87,91,94,97,99,100,100,100,100,100,100,100,100,99,97,94,91,87,83,78,73,68,62,56,50,44,38,32,27,22,17,13,9,6,3,1,0,0,0,0,0,0,0,
        0,58,100,0,
        0,58,100,0,
        0,58,100,0,
        3,3,3,2000*/
};

//#4 ID_CHARGING_FULL (G) 
#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS) 
static int pm8038_leds_pwm_duty_pcts4[79] = {
	25,25,25,25,25,25,25,25,25,25,
	25,25,25,25,25,25,25,25,25,25,
	25,25,25,25,25,25,25,25,25,25,
	25,25,25,25,25,25,25,25,25,25,
	25,25,25,25,25,25,25,25,25,25,
	25,25,25,25,25,25,25,25,25,25,
	25,25,25,
        0,63,30,0,
        0,63,30,0,
        0,63,30,0,
        3,3,3,2000
};
#else /*                          */
static int pm8038_leds_pwm_duty_pcts4[79] = {
    50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,50,
        0,63,30,0,
        0,63,30,0,
        0,63,30,0,
        3,3,3,2000
};
#endif /*                         */

//#5 ID_CALENDAR_REMIND (GB)
static int pm8038_leds_pwm_duty_pcts5[79] = {
    100,99,98,97,95,93,90,88,84,81,77,73,68,64,59,54,48,43,37,31,25,19,13,6,0,0,0,0,0,0,0,0,0,0,0,100,99,98,97,95,93,90,88,84,81,77,73,68,64,59,54,48,43,37,31,25,19,13,6,0,0,0,0,
        0,60,12,0,
        0,60,12,0,
        0,60,12,0,
        2,2,2,2000
};

//#6 ID_POWER_OFF (RG)
static int pm8038_leds_pwm_duty_pcts6[79] = {
    66,70,80,85,87,85,80,77,73,70,67,63,60,57,53,50,47,43,40,37,33,30,27,23,20,17,13,10,7,3,0,100,99,97,95,93,80,60,40,30,26,24,22,19,15,13,10,9,8,7,6,5,4,3,3,2,1,0,0,0,0,0,0,
        0,31,32,0,
        31,31,32,0,
        62,1,32,0,
        5,5,5,2000
};

//#7 ID_MISSED_NOTI (G)
#if defined(CONFIG_MACH_LGE_FX3_SPCS)
static int pm8038_leds_pwm_duty_pcts7[79] = {
    100,99,98,97,95,93,90,88,84,81,77,73,68,64,59,54,48,43,37,31,25,19,13,6,0,0,0,0,0,0,0,0,0,0,0,100,99,98,97,95,93,90,88,84,81,77,73,68,64,59,54,48,43,37,31,25,19,13,6,0,0,0,0,
        0,60,12,11280,
        0,60,12,11280,
        0,60,12,11280,
        19,19,19,2000
};
#else
static int pm8038_leds_pwm_duty_pcts7[79] = {
    100,99,98,97,95,93,90,88,84,81,77,73,68,64,59,54,48,43,37,31,25,19,13,6,0,0,0,0,0,0,0,0,0,0,0,100,99,98,97,95,93,90,88,84,81,77,73,68,64,59,54,48,43,37,31,25,19,13,6,0,0,0,0,
        0,60,12,3280,
        0,60,12,3280,
        0,60,12,3280,
        19,19,19,2000
};
#endif

//#8 ID_ALARM (RG)
static int pm8038_leds_pwm_duty_pcts8[79] = {
    100,100,100,100,98,96,90,84,83,81,80,77,73,69,64,60,55,50,45,40,34,29,23,17,12,6,0,0,0,0,0,0,0,0,0,3,5,8,10,13,18,23,25,27,29,31,33,34,35,30,26,23,21,20,19,10,8,5,1,0,0,0,0,
        0,32,36,8,
        31,32,36,8,
        31,32,36,8,
        19,19,19,2000
/*    100,99,98,97,96,94,92,89,87,84,80,77,73,69,64,60,55,50,45,40,34,29,23,17,12,6,0,0,0,0,7,15,25,30,35,40,47,51,56,62,69,71,73,74,75,67,56,45,38,29,20,11,2,1,0,0,0,0,0,0,0,0,0,
        0,29,40,0,
        29,29,40,0,
        29,29,40,0,
        3,3,3,2000*/
};

//#9 ID_CALL_01 (RGB)
static int pm8038_leds_pwm_duty_pcts9[79] = {
    0,10,20,30,0,30,40,50,40,30,0,0,0,0,0,0,10,10,5,0,0,14,29,43,57,71,86,100,92,83,75,67,58,50,42,33,25,17,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,20,64,0,
        20,20,64,0,
        40,20,64,0,
        3,3,3,2000
/*    0,9,9,0,0,0,0,0,21,42,63,42,21,0,0,0,0,0,0,0,0,24,47,54,70,80,80,80,80,80,80,76,75,73,59,47,35,22,10,0,0,0,0,34,67,44,22,10,5,0,0,0,0,9,16,33,40,33,16,9,0,0,0,
        0,20,64,0,
        20,20,64,0,
        40,20,64,0,
        3,3,3,2000*/
/*    0,22,44,66,89,90,93,95,97,100,100,100,99,99,98,98,97,96,95,95,94,92,91,90,88,85,82,80,77,74,69,65,60,55,50,45,34,22,11,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,40,32,0,
        0,40,32,0,
        0,40,32,0,
        3,3,3,2000*/
};

//#10 ID_CALL_02 (RGB)
static int pm8038_leds_pwm_duty_pcts10[79] = {
    0,44,89,93,97,100,99,98,97,95,92,90,85,80,74,65,55,45,22,0,0,18,36,37,39,40,40,39,39,38,37,36,34,32,30,26,22,18,9,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,20,64,0,
        20,20,64,0,
        20,20,64,0,
        3,3,3,2000
};

//#11 ID_CALL_03 (RGB)
static int pm8038_leds_pwm_duty_pcts11[79] = {
    10,44,58,87,80,73,66,44,22,0,0,0,0,0,16,33,49,47,45,30,15,0,0,19,39,58,72,86,100,99,99,99,97,95,77,59,39,20,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,63,76,88,83,79,74,82,90,60,30,
        0,21,64,0,
        21,21,64,0,
        42,21,64,0,
        3,3,3,2000
};

//#12 ID_VOLUME_UP (RB)
static int pm8038_leds_pwm_duty_pcts12[79] = {
    100,100,99,98,96,94,91,88,84,81,76,72,67,63,58,53,47,42,37,33,28,24,19,16,12,9,6,4,2,1,0,0,71,70,70,69,68,66,64,62,60,57,54,51,48,44,41,37,34,30,26,23,20,17,14,11,9,6,4,3,2,1,0,
        0,31,26,0,
        31,1,26,0,
        32,31,26,0,
        2,2,2,2000
};

//#13 ID_VOLUME_DOWN (GB)
static int pm8038_leds_pwm_duty_pcts13[79] = {
    0,49,49,49,48,47,46,45,43,42,40,38,36,33,31,28,26,23,21,19,16,14,12,10,8,6,4,3,2,1,1,0,100,100,99,98,96,94,91,88,84,81,76,72,67,63,58,53,47,42,37,33,28,24,19,16,12,9,6,4,2,1,0,
        0,1,26,0,
        1,31,26,0,
        32,31,26,0,
        2,2,2,2000
};

//#14 ID_FAVORITE_MISSED_NOTI (RGB)
static int pm8038_leds_pwm_duty_pcts14[79] = {
    100,100,100,100,100,92,84,76,68,60,52,44,36,28,20,100,100,100,100,100,90,80,70,60,50,40,30,20,10,0,40,40,40,40,40,37,34,30,27,24,21,18,14,11,8,40,40,40,40,40,36,32,28,24,20,16,12,8,4,0,0,0,0,
        0,30,47,2590,
        30,30,47,2590,
        30,30,47,2590,
        19,19,19,2000
};

//empty patterns
static int pm8038_leds_pwm_duty_pcts15[79] = {
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,2000
};

static int pm8038_leds_pwm_duty_pcts16[79] = {
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,2000
};

static int pm8038_leds_pwm_duty_pcts17[79] = {
		0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0 ,0,0,0,0,0,0,0,0,0,0 ,0,0,0,0,0,0,0,0,0,0 ,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,2000
};

#else
//=========================================================================
#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3_SPCS) || defined(CONFIG_MACH_LGE_FX3_MPCS) || defined(CONFIG_MACH_LGE_FX3_TMUS) || defined(CONFIG_MACH_LGE_F6_TMUS) \
	|| defined(CONFIG_MACH_LGE_FX3_SPCSTRF) || defined(CONFIG_MACH_LGE_FX3_CRK) || defined(CONFIG_MACH_LGE_FX3_WCDMA_TRF_US) \
	|| defined(CONFIG_MACH_LGE_F6_VDF) || defined(CONFIG_MACH_LGE_FX3Q_TMUS) || defined(CONFIG_MACH_LGE_F6_ORG)|| defined(CONFIG_MACH_LGE_F6_OPEN) || defined(CONFIG_MACH_LGE_F6_TMO)
static int pm8038_led0_pwm_duty_pcts[56] = {
			100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
			100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
			100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
			100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
			100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
			100 ,100, 100, 100, 100, 100	
};
#else
static int pm8038_led0_pwm_duty_pcts[56] = {
		1, 4, 8, 12, 16, 20, 24, 28, 32, 36,
		40, 44, 46, 52, 56, 60, 64, 68, 72, 76,
		80, 84, 88, 92, 96, 100, 100, 100, 98, 95,
		92, 88, 84, 82, 78, 74, 70, 66, 62, 58,
		58, 54, 50, 48, 42, 38, 34, 30, 26, 22,
		14, 10, 6, 4, 1
};
#endif
//=========================================================================
#endif
/*                                                         */




/*
 * Note: There is a bug in LPG module that results in incorrect
 * behavior of pattern when LUT index 0 is used. So effectively
 * there are 63 usable LUT entries.
 */

/*                                                         */
#ifdef CONFIG_LGE_PM8038_KPJT
static struct pm8xxx_pwm_duty_cycles pm8038_leds_pwm_duty_cycles = {
	.duty_pcts0 = (int *)&pm8038_leds_pwm_duty_pcts0,
	.duty_pcts1 = (int *)&pm8038_leds_pwm_duty_pcts1,
	.duty_pcts2 = (int *)&pm8038_leds_pwm_duty_pcts2,
	.duty_pcts3 = (int *)&pm8038_leds_pwm_duty_pcts3,
	.duty_pcts4 = (int *)&pm8038_leds_pwm_duty_pcts4,
	.duty_pcts5 = (int *)&pm8038_leds_pwm_duty_pcts5,
	.duty_pcts6 = (int *)&pm8038_leds_pwm_duty_pcts6,
	.duty_pcts7 = (int *)&pm8038_leds_pwm_duty_pcts7,
	.duty_pcts8 = (int *)&pm8038_leds_pwm_duty_pcts8,
	.duty_pcts9 = (int *)&pm8038_leds_pwm_duty_pcts9,
	.duty_pcts10 = (int *)&pm8038_leds_pwm_duty_pcts10,
	.duty_pcts11 = (int *)&pm8038_leds_pwm_duty_pcts11,
	.duty_pcts12 = (int *)&pm8038_leds_pwm_duty_pcts12,
	.duty_pcts13 = (int *)&pm8038_leds_pwm_duty_pcts13,
	.duty_pcts14 = (int *)&pm8038_leds_pwm_duty_pcts14,
	.duty_pcts15 = (int *)&pm8038_leds_pwm_duty_pcts15,
	.duty_pcts16 = (int *)&pm8038_leds_pwm_duty_pcts16,
	.duty_pcts17 = (int *)&pm8038_leds_pwm_duty_pcts17,

	.num_duty_pcts = ARRAY_SIZE(pm8038_leds_pwm_duty_pcts0),
	.duty_ms = PM8XXX_LED_PWM_DUTY_MS,
	.start_idx = 1,
	.pause_lo = PM8XXX_LED_PWM_PAUSE_LO,
};

static struct pm8xxx_led_config pm8038_led_configs[] = {
	[0] = {
		.id = PM8XXX_ID_WLED,
		.mode = PM8XXX_LED_MODE_MANUAL,
		.max_current = PM8038_WLED_MAX_CURRENT,
		.default_state = 0,
		.wled_cfg = &wled_cfg,
	},
	[1] = {
		.id = PM8XXX_ID_RGB_LED_RED,
		.mode = PM8XXX_LED_MODE_PWM1,
		.max_current = PM8038_RGB_LED_MAX_CURRENT,
		.pwm_channel = 5,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
		.pwm_duty_cycles = &pm8038_leds_pwm_duty_cycles,
	},
	[2] = {
		.id = PM8XXX_ID_RGB_LED_GREEN,
		.mode = PM8XXX_LED_MODE_PWM1,
		.max_current = PM8038_RGB_LED_MAX_CURRENT,
		.pwm_channel = 4,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
		.pwm_duty_cycles = &pm8038_leds_pwm_duty_cycles,
	},
	[3] = {
		.id = PM8XXX_ID_RGB_LED_BLUE,
		.mode = PM8XXX_LED_MODE_PWM1,
		.max_current = PM8038_RGB_LED_MAX_CURRENT,
		.pwm_channel = 3,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
		.pwm_duty_cycles = &pm8038_leds_pwm_duty_cycles,
	},
	[4] = {
		.id = PM8XXX_ID_MPP_KB_LIGHT,
		.mode = PM8XXX_LED_MODE_MANUAL,
		.max_current = PM8038_WLED_MAX_CURRENT,
	},
	[5] = {
		.id = PM8XXX_ID_MPP_QWERTY,
		.mode = PM8XXX_LED_MODE_MANUAL,
		.max_current = PM8038_WLED_MAX_CURRENT,
	},
};
#else
//=========================================================================
static struct pm8xxx_pwm_duty_cycles pm8038_led0_pwm_duty_cycles = {
	.duty_pcts = (int *)&pm8038_led0_pwm_duty_pcts,
	.num_duty_pcts = ARRAY_SIZE(pm8038_led0_pwm_duty_pcts),
	.duty_ms = PM8XXX_LED_PWM_DUTY_MS,
	.start_idx = 1,
};

static struct pm8xxx_led_config pm8038_led_configs[] = {
	[0] = {
		.id = PM8XXX_ID_WLED,
		.mode = PM8XXX_LED_MODE_MANUAL,
		.max_current = PM8038_WLED_MAX_CURRENT,
		.default_state = 0,
		.wled_cfg = &wled_cfg,
	},
	[1] = {
		.id = PM8XXX_ID_RGB_LED_RED,
		.mode = PM8XXX_LED_MODE_PWM1,
		.max_current = PM8038_RGB_LED_MAX_CURRENT,
		.pwm_channel = 5,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
		.pwm_duty_cycles = &pm8038_led0_pwm_duty_cycles,
	},
	[2] = {
		.id = PM8XXX_ID_RGB_LED_GREEN,
		.mode = PM8XXX_LED_MODE_PWM1,
		.max_current = PM8038_RGB_LED_MAX_CURRENT,
		.pwm_channel = 4,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
		.pwm_duty_cycles = &pm8038_led0_pwm_duty_cycles,
	},
#ifdef CONFIG_MACH_MSM8930_FX3
	[3] = {
		.id = PM8XXX_ID_MPP_KB_LIGHT,
		.mode = PM8XXX_LED_MODE_MANUAL,
		.max_current = PM8038_WLED_MAX_CURRENT,
	},
	[4] = {
		.id = PM8XXX_ID_MPP_QWERTY,
		.mode = PM8XXX_LED_MODE_MANUAL,
		.max_current = PM8038_WLED_MAX_CURRENT,
	},
#else
	[3] = {
		.id = PM8XXX_ID_RGB_LED_BLUE,
		.mode = PM8XXX_LED_MODE_PWM1,
		.max_current = PM8038_RGB_LED_MAX_CURRENT,
		.pwm_channel = 3,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
		.pwm_duty_cycles = &pm8038_led0_pwm_duty_cycles,
	},
#endif
};

//=========================================================================

#endif
/*                                                         */

static struct pm8xxx_led_platform_data pm8xxx_leds_pdata = {
	.led_core = &pm8038_led_core_pdata,
	.configs = pm8038_led_configs,
	.num_configs = ARRAY_SIZE(pm8038_led_configs),
};

static struct pm8xxx_ccadc_platform_data pm8xxx_ccadc_pdata = {
	.r_sense_uohm		= 10000,
	.calib_delay_ms		= 600000,
};

static struct pm8xxx_misc_platform_data pm8xxx_misc_pdata = {
	.priority		= 0,
};

/*
 *	0x254=0xC8 (Threshold=110, preamp bias=01)
 *	0x255=0xC1 (Hold=110, max attn=0000, mute=1)
 *	0x256=0xB0 (decay=101, attack=10, delay=0)
 */

static struct pm8xxx_spk_platform_data pm8xxx_spk_pdata = {
	.spk_add_enable		= false,
	.cd_ng_threshold	= 0x6,
	.cd_nf_preamp_bias	= 0x1,
	.cd_ng_hold		= 0x6,
	.cd_ng_max_atten	= 0x0,
	.noise_mute		= 1,
	.cd_ng_decay_rate	= 0x5,
	.cd_ng_attack_rate	= 0x2,
	.cd_delay		= 0x0,
};

static struct pm8921_bms_platform_data pm8921_bms_pdata __devinitdata = {
	.battery_type			= BATT_UNKNOWN,
/*                                                  
                                                
                                                      
                                                            
                                                                                                           
*/
#ifdef CONFIG_MACH_LGE_L9II_OPEN_EU_REV_A
	.r_sense_uohm			= 33000,
#else	
	.r_sense_uohm			= 10000,
#endif
/*                                                  
                                                
                                                      
                                                            
                                                                                                           
*/
#if defined(CONFIG_MACH_LGE_FX3_VZW)
	.v_cutoff			= 3300,
#elif defined(CONFIG_LGE_PM_BOOST_IC)
	.v_cutoff			= 3000,
#else
	.v_cutoff			= 3200,
#endif
	.max_voltage_uv			= MAX_VOLTAGE_MV * 1000,
#if defined(CONFIG_MACH_LGE_F6_TMUS) || defined(CONFIG_MACH_LGE_F6_VDF) || defined(CONFIG_MACH_LGE_FX3Q_TMUS)
	.shutdown_soc_valid_limit	= 10,
#else
	.shutdown_soc_valid_limit	= 20,
#endif
	.adjust_soc_low_threshold	= 25,
	.chg_term_ua			= CHG_TERM_MA * 1000,
	.rconn_mohm			= 18,
	.normal_voltage_calc_ms		= 20000,
	.low_voltage_calc_ms		= 1000,
/*[START] add more variable as below  from JB MR2 */
	.alarm_low_mv			= 3400,
	.alarm_high_mv			= 4280,  /* QCT Original is 4000 */
	.high_ocv_correction_limit_uv	= 50,
	.low_ocv_correction_limit_uv	= 100,
	.hold_soc_est			= 3,
	.enable_fcc_learning		= 1,
	.min_fcc_learning_soc		= 20,
	.min_fcc_ocv_pc			= 30,
	.min_fcc_learning_samples	= 5,
/*[END] add more variable as below  from JB MR2 */

};

#ifdef CONFIG_PMIC8XXX_VIBRATOR
static struct pm8xxx_vibrator_platform_data pm8xxx_vibrator_pdata = {
    .initial_vibrate_ms = 500,
    /*                                          
                                                                          
                                        
     */
    .max_timeout_ms = 30000,
    .min_timeout_ms = VIB_MIN_TIMEOUT_MS,
    .min_stop_ms = VIB_MIN_STOP_MS,
    .level_mV = VIB_LEVEL_MV,
    .overdrive_ms = VIB_OVERDRIVE_MS,
    .overdrive_range_ms = VIB_OVERDRIVE_RANGE_MS,
};
#endif

/* Vibrator */
#ifdef CONFIG_LGE_DIRECT_QCOIN_VIBRATOR
static int qcoin_power_set(struct device *dev, int level)
{
	int rc;
	u8 val;
	printk("[%s] Motor Level is %d\n", __func__, level);
	if (level > 0) {
		rc = pm8xxx_readb(dev, VIB_DRV, &val);
		if (rc < 0) {
			printk("[%s] Vibrator read error on pmic\n", __func__);
			return rc;
		}
		val |= ((level << VIB_DRV_SEL_SHIFT) & VIB_DRV_SEL_MASK);
		rc = pm8xxx_writeb(dev, VIB_DRV, val);
		if (rc < 0) {
			printk("[%s] Vibrator write error on pmic\n", __func__);
			return rc;
		}
	} else {
		pm8xxx_readb(dev, VIB_DRV, &val);
		val &= ~VIB_DRV_SEL_MASK;
		rc = pm8xxx_writeb(dev, VIB_DRV, val);
		if (rc < 0) {
			printk("[%s] Vibrator write error on pmic\n", __func__);
			return rc;
		}
	}
	return 0;
}

static struct direct_qcoin_platform_data pm8xxx_qcoin_pdata = {
	.enable_status = 0,
	.amp = MOTOR_AMP,
	.power_set = qcoin_power_set,
	.high_max = VIB_HIGH_MAX,
	.low_max = VIB_LOW_MAX,
	.low_min = VIB_LOW_MIN,
};
#endif /*                                  */

static struct pm8038_platform_data pm8038_platform_data __devinitdata = {
	.irq_pdata		= &pm8xxx_irq_pdata,
	.gpio_pdata		= &pm8xxx_gpio_pdata,
	.mpp_pdata		= &pm8xxx_mpp_pdata,
	.rtc_pdata              = &pm8xxx_rtc_pdata,
	.pwrkey_pdata		= &pm8xxx_pwrkey_pdata,
	.misc_pdata		= &pm8xxx_misc_pdata,
	.regulator_pdatas	= msm8930_pm8038_regulator_pdata,
	.charger_pdata		= &pm8921_chg_pdata,
	.bms_pdata		= &pm8921_bms_pdata,
	.adc_pdata		= &pm8038_adc_pdata,
	.leds_pdata		= &pm8xxx_leds_pdata,
	.ccadc_pdata		= &pm8xxx_ccadc_pdata,
	.spk_pdata		= &pm8xxx_spk_pdata,
#ifdef CONFIG_PMIC8XXX_VIBRATOR
    .vibrator_pdata     =   &pm8xxx_vibrator_pdata,
#endif
#ifdef CONFIG_LGE_DIRECT_QCOIN_VIBRATOR
	.pm8xxx_qcoin_pdata     = &pm8xxx_qcoin_pdata,
#endif
};

static struct msm_ssbi_platform_data msm8930_ssbi_pm8038_pdata __devinitdata = {
	.controller_type = MSM_SBI_CTRL_PMIC_ARBITER,
	.slave	= {
		.name			= "pm8038-core",
		.platform_data		= &pm8038_platform_data,
	},
};

void __init msm8930_init_pmic(void)
{
	{
		/* PM8038 configuration */
		pmic_reset_irq = PM8038_IRQ_BASE + PM8038_RESOUT_IRQ;
		msm8960_device_ssbi_pmic.dev.platform_data =
					&msm8930_ssbi_pm8038_pdata;
		pm8038_platform_data.num_regulators
			= msm8930_pm8038_regulator_pdata_len;
		if (machine_is_msm8930_mtp())
			pm8921_bms_pdata.battery_type = BATT_PALLADIUM;
		else if (machine_is_msm8930_cdp())
			pm8921_chg_pdata.has_dc_supply = true;
	}

/* Vibrator, Only in case of DC motor */
#if defined(CONFIG_LGE_DIRECT_QCOIN_VIBRATOR) && defined(CONFIG_MACH_MSM8930_FX3)
		pm8xxx_qcoin_pdata.high_max = 31;
		pm8xxx_qcoin_pdata.low_max = 31;
		pm8xxx_qcoin_pdata.low_min = 31;
        //pm8921_chg_pdata.has_dc_supply = true;

#endif /*                                  */
}
