/* lge/include/board_lge.h
 * Copyright (C) 2010 LGE Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

platform.team@lge.com	2011.01

 */
#ifndef __ASM_ARCH_MSM_BOARD_LGE_H
#define __ASM_ARCH_MSM_BOARD_LGE_H
#include <linux/types.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/rfkill.h>
#include <linux/platform_device.h>
#include <asm/setup.h>

/*                                                    */
enum {
  EVB1         = 0,
  EVB2,
  LGE_REV_A,
  LGE_REV_B,
  LGE_REV_C,
  LGE_REV_D,
  LGE_REV_E,
  LGE_REV_F,
  LGE_REV_G,
  LGE_REV_10,
  LGE_REV_11,
  LGE_REV_12,
  LGE_REV_13,
  LGE_REV_14,
  LGE_REV_15,
  LGE_REV_16,
  LGE_REV_17,
  LGE_REV_18,
  LGE_REV_19,	  
  
  LGE_REV_TOT_NUM,
};

extern int lge_bd_rev;

/*                                      */
enum {
  LGE_I_BOARD_ATNT = 0,
  LGE_I_BOARD_DCM,
  LGE_I_BOARD_SKT,
  LGE_I_BOARD_VZW,
  LGE_I_BOARD_LGU,
  LGE_I_BOARD_MAX,
};

extern int lge_bd_target;
/*                                    */

#ifdef CONFIG_LGE_CHARGER_VOLTAGE_CURRENT_SCENARIO
enum {
		BATT_DS2704,
		BATT_ISL6296,
};

extern int lge_battery_info;
#endif

#ifdef CONFIG_LGE_SENSOR
#define SENSOR_POWER_OFF_K3DH	0x1
#define SENSOR_POWER_OFF_K3G	0x2
#define SENSOR_POWER_OFF_AMI306	0x4
#define SENSOR_POWER_OFF_VALID	0x7

enum {
	SENSOR_TYPE_ACCELEROMETER = 0, 	//K3DH
	SENSOR_TYPE_DCOMPASS,		//AMI306
	SENSOR_TYPE_GYROSCOPE,		//K3G
};
#endif

struct gpio_i2c_pin {
	unsigned int sda_pin;
	unsigned int scl_pin;
	unsigned int reset_pin;
	unsigned int irq_pin;
};

struct k3dh_acc_platform_data {
	int poll_interval;
	int min_interval;

	u8 g_range;

	u8 axis_map_x;
	u8 axis_map_y;
	u8 axis_map_z;

	u8 negate_x;
	u8 negate_y;
	u8 negate_z;

	int (*init)(void);
	void (*exit)(void);
	int (*power_on)(int);
	int (*power_off)(int);
	int gpio_int1;
	int gpio_int2;

};

struct k3g_platform_data {
	u8 fs_range;
	u8 axis_map_x;
	u8 axis_map_y;
	u8 axis_map_z;
	u8 negate_x;
	u8 negate_y;
	u8 negate_z;
	int (*init)(void);
	void (*exit)(void);
	int (*power_on)(int);
	int (*power_off)(int);
};

struct ami306_platform_data {
	int (*init)(void);
	void (*exit)(void);	
	int (*power_on)(int);
	int (*power_off)(int);

	int fdata_mDir;
	int fdata_sign_x;
	int fdata_sign_y;
	int fdata_sign_z;
	int fdata_order0;
	int fdata_order1;
	int fdata_order2;
};

//                                                                                                      
#if !defined(CONFIG_MACH_LGE_L9II_COMMON)
//                                                                             
/* android vibrator platform data */
struct android_vibrator_platform_data {
	int enable_status;
	int (*power_set)(int enable);           /* LDO Power Set Function */
	int (*pwm_set)(int enable, int gain);           /* PWM Set Function */
	int (*ic_enable_set)(int enable);       /* Motor IC Set Function */
	int (*vibrator_init)(void);
};
//                                                                                                      
#endif
//                                                                             

//                                                      
struct bluetooth_platform_data {
	int (*bluetooth_power)(int on);
	int (*bluetooth_toggle_radio)(void *data, bool blocked);
};

struct bluesleep_platform_data {
	int bluetooth_port_num;
};
//                                                     

/* atcmd virtual keyboard platform data */
struct atcmd_virtual_platform_data {
	unsigned int keypad_row;
	unsigned int keypad_col;
	unsigned char *keycode;
};

/*                                                                  */
/* mhl platform data */
struct mhl_platform_data {
  unsigned int is_support;
  unsigned int interrupt_pin;
  unsigned int reset_pin;
  unsigned int select_pin;
  unsigned int wakeup_pin;
  int (*power)(int on);
  int (*power_config)(void);
};
/*                                                                 */

/* implement in devices_lge.c */

/*                                               */
#if defined(CONFIG_LGE_HIDDEN_RESET_PATCH)
extern int hidden_reset_enable;
extern int on_hidden_reset;
void *lge_get_fb_addr(void);
void *lge_get_fb_copy_virt_addr(void);
void *lge_get_fb_copy_phys_addr(void);
#endif
#endif
