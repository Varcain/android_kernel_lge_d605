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
 */

#ifndef __ARCH_ARM_MACH_MSM_BOARD_MSM8930_H
#define __ARCH_ARM_MACH_MSM_BOARD_MSM8930_H

#define MSM8930_PHASE_2

#include <linux/regulator/msm-gpio-regulator.h>
#include <linux/mfd/pm8xxx/pm8038.h>
#include <linux/i2c.h>
#include <linux/i2c/sx150x.h>
#include <mach/irqs.h>
#include <mach/rpm-regulator.h>
#include <mach/msm_memtypes.h>
#include <mach/msm_rtb.h>
#include <linux/input.h>

#include <linux/i2c/melfas_ts.h>
/*
 * TODO: When physical 8930/PM8038 hardware becomes
 * available, remove this block.
 */
#ifndef MSM8930_PHASE_2
#include <linux/mfd/pm8xxx/pm8921.h>
#define PM8921_GPIO_BASE		NR_GPIO_IRQS
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)	(pm_gpio - 1 + PM8921_GPIO_BASE)
#define PM8921_MPP_BASE			(PM8921_GPIO_BASE + PM8921_NR_GPIOS)
#define PM8921_MPP_PM_TO_SYS(pm_gpio)	(pm_gpio - 1 + PM8921_MPP_BASE)
#endif

/* Macros assume PMIC GPIOs and MPPs start at 1 */
#define PM8038_GPIO_BASE		NR_GPIO_IRQS
#define PM8038_GPIO_PM_TO_SYS(pm_gpio)	(pm_gpio - 1 + PM8038_GPIO_BASE)
#define PM8038_MPP_BASE			(PM8038_GPIO_BASE + PM8038_NR_GPIOS)
#define PM8038_MPP_PM_TO_SYS(pm_gpio)	(pm_gpio - 1 + PM8038_MPP_BASE)
#define PM8038_IRQ_BASE			(NR_MSM_IRQS + NR_GPIO_IRQS)



/*
 * TODO: When physical 8930/PM8038 hardware becomes
 * available, replace this block with 8930/pm8038 regulator
 * declarations.
 */
#ifndef MSM8930_PHASE_2
extern struct pm8xxx_regulator_platform_data
	msm_pm8921_regulator_pdata[] __devinitdata;

extern int msm_pm8921_regulator_pdata_len __devinitdata;

extern struct gpio_regulator_platform_data
	msm_gpio_regulator_pdata[] __devinitdata;

extern struct rpm_regulator_platform_data msm_rpm_regulator_pdata __devinitdata;

#define GPIO_VREG_ID_EXT_5V		0
#define GPIO_VREG_ID_EXT_L2		1
#define GPIO_VREG_ID_EXT_3P3V		2
#endif

extern struct regulator_init_data msm8930_saw_regulator_core0_pdata;
extern struct regulator_init_data msm8930_saw_regulator_core1_pdata;

extern struct pm8xxx_regulator_platform_data
	msm8930_pm8038_regulator_pdata[] __devinitdata;

extern int msm8930_pm8038_regulator_pdata_len __devinitdata;

#define MSM8930_GPIO_VREG_ID_EXT_5V		0
#define MSM8930_GPIO_VREG_ID_EXT_OTG_SW		1

extern struct gpio_regulator_platform_data
	msm8930_gpio_regulator_pdata[] __devinitdata;

extern struct rpm_regulator_platform_data
	msm8930_rpm_regulator_pdata __devinitdata;

#if defined(CONFIG_GPIO_SX150X) || defined(CONFIG_GPIO_SX150X_MODULE)
enum {
	GPIO_EXPANDER_IRQ_BASE = (PM8038_IRQ_BASE + PM8038_NR_IRQS),
	GPIO_EXPANDER_GPIO_BASE = (PM8038_MPP_BASE + PM8038_NR_MPPS),
	/* CAM Expander */
	GPIO_CAM_EXPANDER_BASE = GPIO_EXPANDER_GPIO_BASE,
	GPIO_CAM_GP_STROBE_READY = GPIO_CAM_EXPANDER_BASE,
	GPIO_CAM_GP_AFBUSY,
	GPIO_CAM_GP_STROBE_CE,
	GPIO_CAM_GP_CAM1MP_XCLR,
	GPIO_CAM_GP_CAMIF_RESET_N,
	GPIO_CAM_GP_XMT_FLASH_INT,
	GPIO_CAM_GP_LED_EN1,
	GPIO_CAM_GP_LED_EN2,

};
#endif

enum {
	SX150X_CAM,
};

#endif

extern struct sx150x_platform_data msm8930_sx150x_data[];
extern struct msm_camera_board_info msm8930_camera_board_info;
void msm8930_init_cam(void);
void msm8930_init_fb(void);
void msm8930_init_pmic(void);
extern void msm8930_add_vidc_device(void);

/*
 * TODO: When physical 8930/PM8038 hardware becomes
 * available, remove this block or add the config
 * option.
 */
#ifndef MSM8930_PHASE_2
void msm8960_init_pmic(void);
void msm8960_pm8921_gpio_mpp_init(void);
#endif

void msm8930_init_mmc(void);
int msm8930_init_gpiomux(void);
void msm8930_allocate_fb_region(void);
void msm8930_pm8038_gpio_mpp_init(void);
void msm8930_mdp_writeback(struct memtype_reserve *reserve_table);
void __init msm8930_init_gpu(void);

#define PLATFORM_IS_CHARM25() \
	(machine_is_msm8930_cdp() && \
		(socinfo_get_platform_subtype() == 1) \
	)

#ifndef MSM8930_PHASE_2
#define MSM_8960_GSBI1_QUP_I2C_BUS_ID 1
#define MSM_8960_GSBI2_QUP_I2C_BUS_ID 2
#define MSM_8960_GSBI3_QUP_I2C_BUS_ID 3
#define MSM_8960_GSBI4_QUP_I2C_BUS_ID 4
#define MSM_8960_GSBI5_QUP_I2C_BUS_ID 5
#define MSM_8960_GSBI6_QUP_I2C_BUS_ID 6
#define MSM_8960_GSBI7_QUP_I2C_BUS_ID 7
#define MSM_8960_GSBI8_QUP_I2C_BUS_ID 8
#define MSM_8960_GSBI9_QUP_I2C_BUS_ID 9
#define MSM_8960_GSBI10_QUP_I2C_BUS_ID 10
#define MSM_8960_GSBI11_QUP_I2C_BUS_ID 11
#define MSM_8960_GSBI12_QUP_I2C_BUS_ID 12
#else
#define MSM_8930_GSBI3_QUP_I2C_BUS_ID 3
#define MSM_8930_GSBI4_QUP_I2C_BUS_ID 4
#define MSM_8930_GSBI9_QUP_I2C_BUS_ID 0
#define MSM_8930_GSBI10_QUP_I2C_BUS_ID 10
#define MSM_8930_GSBI12_QUP_I2C_BUS_ID 12
#endif

#define I2C_SURF 1
#define I2C_FFA  (1 << 1)
#define I2C_RUMI (1 << 2)
#define I2C_SIM  (1 << 3)
#define I2C_FLUID (1 << 4)
#define I2C_LIQUID (1 << 5)

extern struct msm_rtb_platform_data msm8930_rtb_pdata;
extern struct msm_cache_dump_platform_data msm8930_cache_dump_pdata;
/*
                                             
 */

/*                  */
#define LCD_RESOLUTION_X			800
#define LCD_RESOLUTION_Y			496
#define MSM_FB_WIDTH_MM				56
#define MSM_FB_HEIGHT_MM			94

#define DISABLE_GSBI1_SPI


/*                    */
#define MELFAS_VD33_MAX_UV			2850000
#define MELFAS_VD33_MIN_UV			2850000
#define MELFAS_VD33_CURR_UA			4230

#define MELFAS_IO_MAX_UV			1800000
#define MELFAS_IO_MIN_UV			1800000

#define TS_X_MIN					0
#define TS_X_MAX					480
#define TS_Y_MIN					0
#define TS_Y_MAX					800

#ifdef CONFIG_TOUCHSCREEN_MMS128
#define TS_GPIO_I2C_SDA				16
#define TS_GPIO_I2C_SCL				17
#define TS_GPIO_IRQ					11

#define ATMEL_TS_I2C_SLAVE_ADDR		0x4A
#define MELFAS_TS_I2C_SLAVE_ADDR 	0x48

#define MSM_8960_TS_PWR             52
#define MSM_8960_TS_MAKER_ID        68

#define TOUCH_FW_VERSION  			0x06

#endif


/*                     */
#ifdef CONFIG_HALLIC_S5712ACDL1
#define GPIO_S5712ACDL1_IRQ         35
#endif


/*                       */
#ifdef CONFIG_LGE_DIRECT_QCOIN_VIBRATOR
#define MOTOR_AMP					128
#define VIB_DRV						0x4A
#define VIB_DRV_SEL_MASK			0xf8
#define VIB_DRV_SEL_SHIFT			0x03
#define VIB_DRV_EN_MANUAL_MASK		0xfc
#define VIB_DRV_LOGIC_SHIFT			0x2
#define VIB_HIGH_MAX            	24    /* for Short Impact */
#define VIB_LOW_MAX             	24    /* Max level for Normal Scene */
#define VIB_LOW_MIN             	20    /* Min level for Normal Scene */
#endif

/*                      */
#define GPIO_EAR_SENSE_N			90
#define GPIO_EAR_MIC_EN				47
#define GPIO_EARPOL_DETECT			48
#define GPIO_EAR_KEY_INT			55

/*                        */
#define GPIO_EXT_BOOST_EN PM8038_GPIO_PM_TO_SYS(3)

#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
enum {
	DISCHG_BATT_TEMP_OVER_60,
	DISCHG_BATT_TEMP_57_60,
	DISCHG_BATT_TEMP_UNDER_57,
	CHG_BATT_TEMP_LEVEL_1, // OVER_55
	CHG_BATT_TEMP_LEVEL_2, // 46_55
	CHG_BATT_TEMP_LEVEL_3, // 42_45
	CHG_BATT_TEMP_LEVEL_4, // 0_41
	CHG_BATT_TEMP_LEVEL_5, // M10_0 
	CHG_BATT_TEMP_LEVEL_6, // UNDER_M10
};

enum {
	DISCHG_BATT_NORMAL_STATE,
	DISCHG_BATT_WARNING_STATE,
	DISCHG_BATT_POWEROFF_STATE,
	CHG_BATT_NORMAL_STATE,
	CHG_BATT_DC_CURRENT_STATE,
	CHG_BATT_WARNING_STATE,
	CHG_BATT_STOP_CHARGING_STATE,
};
#endif



/*                               */
#ifdef CONFIG_LGE_PM
/* For cable detection */
/* Ref resistance value = 200K */
#define ADC_NO_INIT_CABLE   0
#define ADC_CABLE_MHL_1K    50000
#define ADC_CABLE_U_28P7K   200000
#define ADC_CABLE_28P7K     300000
#define ADC_CABLE_56K       490000
#define ADC_CABLE_100K      650000
#define ADC_CABLE_130K      780000
#define ADC_CABLE_180K      875000
#define ADC_CABLE_200K      920000
#define ADC_CABLE_220K      988000
#define ADC_CABLE_270K      1077000
#define ADC_CABLE_330K      1294000
#define ADC_CABLE_620K      1418000
#define ADC_CABLE_910K      1600000
#define ADC_CABLE_NONE      1800000

#define C_NO_INIT_TA_MA     0
#define C_MHL_1K_TA_MA      500
#define C_U_28P7K_TA_MA     500
#define C_28P7K_TA_MA       500
#define C_56K_TA_MA         1500 /* it will be changed in future */
#define C_100K_TA_MA        500
#define C_130K_TA_MA        1500
#define C_180K_TA_MA        900
#define C_200K_TA_MA        700
#define C_220K_TA_MA        700
#define C_270K_TA_MA        700
#define C_330K_TA_MA        500
#define C_620K_TA_MA        500
#define C_910K_TA_MA        1500//[ORG]500
#define C_NONE_TA_MA        700

#define C_NO_INIT_USB_MA    0
#define C_MHL_1K_USB_MA     500
#define C_U_28P7K_USB_MA    500
#define C_28P7K_USB_MA      500
#define C_56K_USB_MA        1500 /* it will be changed in future */
#define C_100K_USB_MA       500
#define C_130K_USB_MA       1500
#define C_180K_USB_MA       500
#define C_200K_USB_MA       500
#define C_220K_USB_MA       500
#define C_270K_USB_MA       500
#define C_330K_USB_MA       500
#define C_620K_USB_MA       500
#define C_910K_USB_MA       1500//[ORG]500
#define C_NONE_USB_MA       500
#endif

