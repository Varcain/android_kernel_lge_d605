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

#ifndef __ARCH_ARM_MACH_MSM_BOARD_LGE_H
#define __ARCH_ARM_MACH_MSM_BOARD_LGE_H

/*
                            
 */

/*                           */

/* HALL-IC(s5712ACDL1) platform data */
#ifdef CONFIG_HALLIC_S5712ACDL1
struct s5712ACDL1_platform_data {
	unsigned int irq_pin;
	unsigned int prohibit_time;
};
#endif

/*                              */

#ifdef CONFIG_ANDROID_RAM_CONSOLE
#define LGE_RAM_CONSOLE_SIZE (124 * SZ_1K * 2)
#endif
#ifdef CONFIG_LGE_HANDLE_PANIC
#define LGE_CRASH_LOG_SIZE (4*SZ_1K)
#endif


typedef enum {
	HW_REV_EVB1 = 0,
	HW_REV_EVB2,
	HW_REV_A,
	HW_REV_B,
	HW_REV_C,
	HW_REV_D,
	HW_REV_E,
	HW_REV_F,
	HW_REV_G,
	HW_REV_1_0,
	HW_REV_1_1,
	HW_REV_1_2,
	HW_REV_MAX
} hw_rev_type;

hw_rev_type lge_get_board_revno(void);

typedef enum {
	NO_INIT_CABLE = 0,
	CABLE_MHL_1K,
	CABLE_U_28P7K,
	CABLE_28P7K,
	CABLE_56K,
	CABLE_100K,
	CABLE_130K,
	CABLE_180K,
	CABLE_200K,
	CABLE_220K,
	CABLE_270K,
	CABLE_330K,
	CABLE_620K,
	CABLE_910K,
	CABLE_NONE
} acc_cable_type;

struct chg_cable_info {
	acc_cable_type cable_type;
	unsigned ta_ma;
	unsigned usb_ma;
/*           
                            
                                
 */
	int adc;
	int threshould;
};

int lge_pm_get_cable_info(struct chg_cable_info *);
void lge_pm_read_cable_info(void);
acc_cable_type lge_pm_get_cable_type(void);
unsigned lge_pm_get_ta_current(void);
unsigned lge_pm_get_usb_current(void);
#ifdef CONFIG_LGE_PM_TRKLCHG_IN_KERNEL
int is_battery_low_in_lk(void);
#endif

/* PROXIMITY platform data */
struct prox_platform_data {
	uint32_t version;
	int irq_num;
	int (*power)(int onoff);
};

/*                                                                        */
#ifdef CONFIG_LGE_PM
struct pseudo_batt_info_type {
	int mode;
	int id;
	int therm;
	int temp;
	int volt;
	int capacity;
	int charging;
};
#endif
/*                                        */

/*                                                                     */
#if defined(CONFIG_MACH_LGE_L9II_COMMON)
struct off_batt_info_type {
	int mode;
	
};
#endif

/*                                                                   */
#define AGC_COMPRESIION_RATE        0
#define AGC_OUTPUT_LIMITER_DISABLE  1
#define AGC_FIXED_GAIN          14

/*                 */
#ifdef CONFIG_I2C
struct i2c_registry {
	u8                     machs;
	int                    bus;
	struct i2c_board_info *info;
	int                    len;
};
typedef void (gpio_i2c_init_func_t)(int bus_num);

void __init lge_add_msm_i2c_device(struct i2c_registry *device);
void __init lge_add_gpio_i2c_device(gpio_i2c_init_func_t *init_func);

void __init lge_add_i2c_devices(void);
void __init lge_add_lcd_devices(void);
void __init lge_add_input_devices(void);
void __init lge_add_usb_devices(void);
#endif
void __init lge_add_camera_devices(void);
void __init lge_add_sensor_devices(void);
void __init lge_add_sound_devices(void);

#ifdef CONFIG_SWITCH_FSA8008
bool lge_get_board_usembhc(void);
#endif
#if defined(CONFIG_LGE_NFC)	
void __init lge_add_nfc_devices(void);
#endif

#ifdef CONFIG_LGE_HANDLE_PANIC
void __init lge_add_panic_handler_devices(void);
int lge_get_magic_for_subsystem(void);
void lge_set_magic_for_subsystem(const char* subsys_name);
#endif

#if defined(CONFIG_LGE_PM) && defined(CONFIG_DEBUG_FS)
/*                                               
                                                 */
int gpio_debug_init(void);
void gpio_debug_print(void);
#else
static inline int gpio_debug_init(void){ return 0; }
static inline void gpio_debug_print(void) { retur; }
/*              */
#endif

#ifdef CONFIG_LGE_HIDDEN_RESET
extern int hreset_enable;
extern int on_hidden_reset;

int lge_get_fb_phys_info(unsigned long *start, unsigned long *size);
void *lge_get_hreset_fb_phys_addr(void);
#endif

#ifdef CONFIG_LGE_PM_BATTERY_ID_CHECKER
enum {
	BATT_DS2704_N = 17,
	BATT_DS2704_L = 32,
	BATT_ISL6296_N = 73,
	BATT_ISL6296_L = 94,
	BATT_DS2704_C = 48,
	BATT_ISL6296_C = 105,
};
extern int lge_battery_info;
#endif


#ifdef CONFIG_LGE_KCAL
struct kcal_platform_data {
	int (*set_values)(int r, int g, int b);
	int (*get_values)(int *r, int *g, int *b);
	int (*refresh_display)(void);
};
#endif

enum lge_boot_mode_type {
	LGE_BOOT_MODE_NORMAL = 0,
	LGE_BOOT_MODE_CHARGER,
	LGE_BOOT_MODE_CHARGERLOGO,
	LGE_BOOT_MODE_FACTORY,
	LGE_BOOT_MODE_FACTORY2,
	LGE_BOOT_MODE_PIFBOOT,
	LGE_BOOT_MODE_MINIOS    /*                          */
};

#ifdef CONFIG_SWITCH_FSA8008
struct fsa8008_platform_data {
	const char *switch_name;            /* switch device name */
	const char *keypad_name;			/* keypad device name */

	unsigned int key_code;				/* key code for hook */

	unsigned int gpio_detect;	/* DET : to detect jack inserted or not */
	unsigned int gpio_mic_en;	/* EN : to enable mic */
	unsigned int gpio_jpole;	/* JPOLE : 3pole or 4pole */
	unsigned int gpio_key;		/* S/E button */

	void (*set_headset_mic_bias)(int enable); /* callback function which is initialized while probing */

	unsigned int latency_for_detection; /* latency for pole (3 or 4)detection (in ms) */
};
#endif

extern unsigned int system_rev;

#ifdef CONFIG_ANDROID_RAM_CONSOLE
void __init lge_add_ramconsole_devices(void);
#endif

/*                            */
enum lge_boot_mode_type lge_get_boot_mode(void);
/*                            */

#if defined(CONFIG_MACH_LGE_FX3_SPCS) && defined(CONFIG_LGE_USB_DIAG_DISABLE)
int lge_get_emergency_status(void);
#endif

#endif /*                                 */
#ifdef CONFIG_LGE_QFPROM_INTERFACE
void __init lge_add_qfprom_devices(void);
#endif
