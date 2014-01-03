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
#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/mfd/pm8xxx/pm8921-charger.h>
#include <linux/mfd/pm8xxx/pm8921-bms.h>
#include <linux/mfd/pm8xxx/pm8xxx-adc.h>
#include <linux/mfd/pm8xxx/ccadc.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/workqueue.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/mfd/pm8xxx/batt-alarm.h>
#include <linux/ratelimit.h>

#include <mach/msm_xo.h>
#include <mach/msm_hsusb.h>

#ifdef CONFIG_BATTERY_MAX17043
#include <linux/max17043_fuelgauge.h>
#endif

/* [START] sungsookim */
#if defined (CONFIG_LGE_PM) || defined (CONFIG_LGE_PM_BATTERY_ID_CHECKER)
#include <mach/board_lge.h>
#endif
/* [END] */


#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
#include <linux/reboot.h>
/*           
                                   
                                   
 */
#include "../../../arch/arm/mach-msm/lge/fx3/board-fx3.h"
#endif

#ifdef CONFIG_MACH_LGE
#include <linux/gpio.h>
#endif
#define CHG_BUCK_CLOCK_CTRL	0x14

#define PBL_ACCESS1		0x04
#define PBL_ACCESS2		0x05
#define SYS_CONFIG_1		0x06
#define SYS_CONFIG_2		0x07
#define CHG_CNTRL		0x204
#define CHG_IBAT_MAX		0x205
#define CHG_TEST		0x206
#define CHG_BUCK_CTRL_TEST1	0x207
#define CHG_BUCK_CTRL_TEST2	0x208
#define CHG_BUCK_CTRL_TEST3	0x209
#define COMPARATOR_OVERRIDE	0x20A
#define PSI_TXRX_SAMPLE_DATA_0	0x20B
#define PSI_TXRX_SAMPLE_DATA_1	0x20C
#define PSI_TXRX_SAMPLE_DATA_2	0x20D
#define PSI_TXRX_SAMPLE_DATA_3	0x20E
#define PSI_CONFIG_STATUS	0x20F
#define CHG_IBAT_SAFE		0x210
#define CHG_ITRICKLE		0x211
#define CHG_CNTRL_2		0x212
#define CHG_VBAT_DET		0x213
#define CHG_VTRICKLE		0x214
#define CHG_ITERM		0x215
#define CHG_CNTRL_3		0x216
#define CHG_VIN_MIN		0x217
#define CHG_TWDOG		0x218
#define CHG_TTRKL_MAX		0x219
#define CHG_TEMP_THRESH		0x21A
#define CHG_TCHG_MAX		0x21B
#define USB_OVP_CONTROL		0x21C
#define DC_OVP_CONTROL		0x21D
#define USB_OVP_TEST		0x21E
#define DC_OVP_TEST		0x21F
#define CHG_VDD_MAX		0x220
#define CHG_VDD_SAFE		0x221
#define CHG_VBAT_BOOT_THRESH	0x222
#define USB_OVP_TRIM		0x355
#define BUCK_CONTROL_TRIM1	0x356
#define BUCK_CONTROL_TRIM2	0x357
#define BUCK_CONTROL_TRIM3	0x358
#define BUCK_CONTROL_TRIM4	0x359
#define CHG_DEFAULTS_TRIM	0x35A
#define CHG_ITRIM		0x35B
#define CHG_TTRIM		0x35C
#define CHG_COMP_OVR		0x20A
#define IUSB_FINE_RES		0x2B6
#define OVP_USB_UVD		0x2B7

/* check EOC every 10 seconds */
#define EOC_CHECK_PERIOD_MS	10000
/* check for USB unplug every 200 msecs */
#define UNPLUG_CHECK_WAIT_PERIOD_MS 200
#if defined(CONFIG_MACH_MSM8930_FX3) && !(defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS))
#define UNPLUG_CHECK_RAMP_MS 500
#else
#define UNPLUG_CHECK_RAMP_MS 25
#endif
#define USB_TRIM_ENTRIES 16
#if defined(CONFIG_MACH_LGE_FX3_VZW)  || defined(CONFIG_MACH_LGE_FX3Q_TMUS) || defined(CONFIG_MACH_LGE_F6_TMUS)|| defined(CONFIG_MACH_LGE_F6_VDF)
#define MONITOR_BATT_TEMP_PERIOD_MS		60000
#else
#define MONITOR_BATT_TEMP_PERIOD_MS		1000
#endif
#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS) || defined(CONFIG_MACH_LGE_F6_TMUS)|| defined(CONFIG_MACH_LGE_F6_VDF)
#define MONITOR_BATT_TEMP_START_MS		25
#else
#define MONITOR_BATT_TEMP_START_MS		500
#endif
/*                                                */
#ifdef CONFIG_LGE_PM
#define LT_CABLE_56K                6
#define LT_CABLE_130K               7
#define LT_CABLE_910K		    11
#define NO_BATT_QHSUSB_CHG_PORT_DCP 22
#include "../../arch/arm/mach-msm/smd_private.h"
#endif
/*                                                */

#ifdef CONFIG_LGE_PM_BOOST_IC
#ifdef CONFIG_MACH_LGE_FX3Q_TMUS
#define BOOST_MONITOR_MS	10000
#define VBAT_AVG_SAMPLES	5
#define BOOST_ENABLE_THRESHOLD 3600
#else
#define BOOST_MONITOR_MS	1000
#define VBAT_AVG_SAMPLES	10 /*number of VBAT averaging unit*/
#endif
#endif /*                        */

#ifdef CONFIG_LGE_PM_TRKLCHG_IN_KERNEL
#define PWR_ON_EVENT_USB_CHG		0x20
#define PWR_ON_EVENT_WALL_CHG		0x40
#endif
#ifdef CONFIG_LGE_PM_VZW_FAST_CHG
#define VZW_FAST_CHG_CHK_CNT	2
#endif

enum chg_fsm_state {
	FSM_STATE_OFF_0 = 0,
	FSM_STATE_BATFETDET_START_12 = 12,
	FSM_STATE_BATFETDET_END_16 = 16,
	FSM_STATE_ON_CHG_HIGHI_1 = 1,
	FSM_STATE_ATC_2A = 2,
	FSM_STATE_ATC_2B = 18,
	FSM_STATE_ON_BAT_3 = 3,
	FSM_STATE_ATC_FAIL_4 = 4 ,
	FSM_STATE_DELAY_5 = 5,
	FSM_STATE_ON_CHG_AND_BAT_6 = 6,
	FSM_STATE_FAST_CHG_7 = 7,
	FSM_STATE_TRKL_CHG_8 = 8,
	FSM_STATE_CHG_FAIL_9 = 9,
	FSM_STATE_EOC_10 = 10,
	FSM_STATE_ON_CHG_VREGOK_11 = 11,
	FSM_STATE_ATC_PAUSE_13 = 13,
	FSM_STATE_FAST_CHG_PAUSE_14 = 14,
	FSM_STATE_TRKL_CHG_PAUSE_15 = 15,
	FSM_STATE_START_BOOT = 20,
	FSM_STATE_FLCB_VREGOK = 21,
	FSM_STATE_FLCB = 22,
};

struct fsm_state_to_batt_status {
	enum chg_fsm_state	fsm_state;
	int			batt_state;
};

static int pm8921_battery_gauge_alarm_notify(struct notifier_block *nb,
					  unsigned long status, void *unused);

static struct notifier_block alarm_notifier = {
	.notifier_call = pm8921_battery_gauge_alarm_notify,
};

static struct fsm_state_to_batt_status map[] = {
	{FSM_STATE_OFF_0, POWER_SUPPLY_STATUS_UNKNOWN},
	{FSM_STATE_BATFETDET_START_12, POWER_SUPPLY_STATUS_UNKNOWN},
	{FSM_STATE_BATFETDET_END_16, POWER_SUPPLY_STATUS_UNKNOWN},
	/*
	 * for CHG_HIGHI_1 report NOT_CHARGING if battery missing,
	 * too hot/cold, charger too hot
	 */
	{FSM_STATE_ON_CHG_HIGHI_1, POWER_SUPPLY_STATUS_FULL},
	{FSM_STATE_ATC_2A, POWER_SUPPLY_STATUS_CHARGING},
	{FSM_STATE_ATC_2B, POWER_SUPPLY_STATUS_CHARGING},
	{FSM_STATE_ON_BAT_3, POWER_SUPPLY_STATUS_DISCHARGING},
	{FSM_STATE_ATC_FAIL_4, POWER_SUPPLY_STATUS_DISCHARGING},
	{FSM_STATE_DELAY_5, POWER_SUPPLY_STATUS_UNKNOWN },
	{FSM_STATE_ON_CHG_AND_BAT_6, POWER_SUPPLY_STATUS_CHARGING},
	{FSM_STATE_FAST_CHG_7, POWER_SUPPLY_STATUS_CHARGING},
	{FSM_STATE_TRKL_CHG_8, POWER_SUPPLY_STATUS_CHARGING},
	{FSM_STATE_CHG_FAIL_9, POWER_SUPPLY_STATUS_DISCHARGING},
	{FSM_STATE_EOC_10, POWER_SUPPLY_STATUS_FULL},
	{FSM_STATE_ON_CHG_VREGOK_11, POWER_SUPPLY_STATUS_NOT_CHARGING},
	{FSM_STATE_ATC_PAUSE_13, POWER_SUPPLY_STATUS_NOT_CHARGING},
	{FSM_STATE_FAST_CHG_PAUSE_14, POWER_SUPPLY_STATUS_NOT_CHARGING},
	{FSM_STATE_TRKL_CHG_PAUSE_15, POWER_SUPPLY_STATUS_NOT_CHARGING},
	{FSM_STATE_START_BOOT, POWER_SUPPLY_STATUS_NOT_CHARGING},
	{FSM_STATE_FLCB_VREGOK, POWER_SUPPLY_STATUS_NOT_CHARGING},
	{FSM_STATE_FLCB, POWER_SUPPLY_STATUS_NOT_CHARGING},
};

enum chg_regulation_loop {
	VDD_LOOP = BIT(3),
	BAT_CURRENT_LOOP = BIT(2),
	INPUT_CURRENT_LOOP = BIT(1),
	INPUT_VOLTAGE_LOOP = BIT(0),
	CHG_ALL_LOOPS = VDD_LOOP | BAT_CURRENT_LOOP
			| INPUT_CURRENT_LOOP | INPUT_VOLTAGE_LOOP,
};

enum pmic_chg_interrupts {
	USBIN_VALID_IRQ = 0,
	USBIN_OV_IRQ,
	BATT_INSERTED_IRQ,
	VBATDET_LOW_IRQ,
	USBIN_UV_IRQ,
	VBAT_OV_IRQ,
	CHGWDOG_IRQ,
	VCP_IRQ,
	ATCDONE_IRQ,
	ATCFAIL_IRQ,
	CHGDONE_IRQ,
	CHGFAIL_IRQ,
	CHGSTATE_IRQ,
	LOOP_CHANGE_IRQ,
	FASTCHG_IRQ,
	TRKLCHG_IRQ,
	BATT_REMOVED_IRQ,
	BATTTEMP_HOT_IRQ,
	CHGHOT_IRQ,
	BATTTEMP_COLD_IRQ,
	CHG_GONE_IRQ,
#ifndef CONFIG_LGE_PM
	BAT_TEMP_OK_IRQ,
#endif
	COARSE_DET_LOW_IRQ,
	VDD_LOOP_IRQ,
	VREG_OV_IRQ,
	VBATDET_IRQ,
	BATFET_IRQ,
	PSI_IRQ,
	DCIN_VALID_IRQ,
	DCIN_OV_IRQ,
	DCIN_UV_IRQ,
	PM_CHG_MAX_INTS,
};

struct bms_notify {
	int			is_battery_full;
	int			is_charging;
	struct	work_struct	work;
};

/**
 * struct pm8921_chg_chip -device information
 * @dev:			device pointer to access the parent
 * @usb_present:		present status of usb
 * @dc_present:			present status of dc
 * @usb_charger_current:	usb current to charge the battery with used when
 *				the usb path is enabled or charging is resumed
 * @safety_time:		max time for which charging will happen
 * @update_time:		how frequently the userland needs to be updated
 * @max_voltage_mv:		the max volts the batt should be charged up to
 * @min_voltage_mv:		the min battery voltage before turning the FETon
 * @uvd_voltage_mv:		(PM8917 only) the falling UVD threshold voltage
 * @alarm_low_mv:		the battery alarm voltage low
 * @alarm_high_mv:		the battery alarm voltage high
 * @cool_temp_dc:		the cool temp threshold in deciCelcius
 * @warm_temp_dc:		the warm temp threshold in deciCelcius
 * @resume_voltage_delta:	the voltage delta from vdd max at which the
 *				battery should resume charging
 * @term_current:		The charging based term current
 *
 */
struct pm8921_chg_chip {
	struct device			*dev;
	unsigned int			usb_present;
	unsigned int			dc_present;
	unsigned int			usb_charger_current;
	unsigned int			max_bat_chg_current;
	unsigned int			pmic_chg_irq[PM_CHG_MAX_INTS];
	unsigned int			safety_time;
	unsigned int			ttrkl_time;
	unsigned int			update_time;
	unsigned int			curr_voltage_mv;
	unsigned int			max_voltage_mv;
	unsigned int			min_voltage_mv;
	unsigned int			uvd_voltage_mv;
	unsigned int			alarm_low_mv;
	unsigned int			alarm_high_mv;
	int				cool_temp_dc;
	int				warm_temp_dc;
	unsigned int			temp_check_period;
	unsigned int			cool_bat_chg_current;
	unsigned int			warm_bat_chg_current;
	unsigned int			cool_bat_voltage;
	unsigned int			warm_bat_voltage;
	unsigned int			is_bat_cool;
	unsigned int			is_bat_warm;
	unsigned int			resume_voltage_delta;
	int				resume_charge_percent;
	unsigned int			term_current;
	unsigned int			vbat_channel;
	unsigned int			batt_temp_channel;
	unsigned int			batt_id_channel;
	struct power_supply		usb_psy;
	struct power_supply		dc_psy;
	struct power_supply		*ext_psy;
	struct power_supply		batt_psy;
	struct dentry			*dent;
	struct bms_notify		bms_notify;
	int				*usb_trim_table;
	bool				keep_btm_on_suspend;
	bool				ext_charging;
	bool				ext_charge_done;
	bool				iusb_fine_res;
#ifndef CONFIG_LGE_PM
	bool				dc_unplug_check;
#endif
	bool				disable_hw_clock_switching;
	DECLARE_BITMAP(enabled_irqs, PM_CHG_MAX_INTS);
	struct work_struct		battery_id_valid_work;
	int64_t				batt_id_min;
	int64_t				batt_id_max;
	int				trkl_voltage;
	int				weak_voltage;
	int				trkl_current;
	int				weak_current;
	int				vin_min;
	unsigned int			*thermal_mitigation;
	int				thermal_levels;
	struct delayed_work		update_heartbeat_work;
	struct delayed_work		eoc_work;
	struct delayed_work		unplug_check_work;
	struct delayed_work		vin_collapse_check_work;
#if defined(CONFIG_LGE_PM) && !defined(CONFIG_BATTERY_MAX17043)
	struct wake_lock		batt_alarm_wake_up;
#endif

#ifdef CONFIG_LGE_PM
	struct wake_lock		increase_usb_ma_value_wake_lock;
	struct delayed_work		increase_usb_ma_value_work;
#endif
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	struct delayed_work		monitor_batt_temp_work;
	struct wake_lock                monitor_batt_temp_wake_lock;
/*           
                                    
                                
*/
	xo_mitigation_way thermal_mitigation_method;
/*                               */
#endif
	struct wake_lock		eoc_wake_lock;
	enum pm8921_chg_cold_thr	cold_thr;
	enum pm8921_chg_hot_thr		hot_thr;
	int				rconn_mohm;
/*
                      
                  
                     
      
*/
	enum pm8921_chg_led_src_config	led_src_config;
	bool				host_mode;
#ifndef CONFIG_LGE_PM
	bool				has_dc_supply;
#endif
	u8				active_path;
	int				recent_reported_soc;
#ifdef CONFIG_MACH_MSM8930_FX3
	bool				aicl;
#endif
#ifdef CONFIG_LGE_PM_BOOST_IC
	/*                                             
                                                           */
	unsigned int boost_byp_sw_gpio;
	unsigned int boost_byp_thr;
	bool boost_byp_sw_enable;
	struct delayed_work monitor_boost_work;
	/*              */
#endif /*                        */
	int				stop_chg_upon_expiry;
	bool				eoc_check;
};

/* user space parameter to limit usb current */
static unsigned int usb_max_current;
/*
 * usb_target_ma is used for wall charger
 * adaptive input current limiting only. Use
 * pm_iusbmax_get() to get current maximum usb current setting.
 */
static int usb_target_ma;
/*
 * below variable is for debugging in chg_update_charging_state
 * ibat_max_ma will be setted in pm_chg_ibatmax_set();
 */
static int ibat_max_ma;
/*                                          
                                                               
                                                                     
                                  
 */
#ifdef CONFIG_LGE_PM_VZW_FAST_CHG
static int chg_state_check_count = 0;
static int is_finished_chg_state_check = 0;
static bool is_finished_first_rampup = false;
static int stepdown_check_before_first_rampup = 0;
extern bool usb_connected_flag;
int vzw_fast_chg_ma = 0;
EXPORT_SYMBOL(vzw_fast_chg_ma);
#endif
#ifdef CONFIG_MACH_LGE_FX3Q_TMUS
bool boost_enable_flag = false;
#endif
static int charging_disabled;
static int thermal_mitigation;

/* Battery alarm for the model using BMS */

#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
static unsigned int last_stop_charging;
static unsigned int chg_batt_temp_state;
static unsigned int chg_batt_temp_state_prev;
/*
                               
                                                                         
 */
static int pseudo_ui_charging;
/*                                                                     */
static int pre_xo_mitigation;
/*                          */

/*           
                            
                                
*/
static bool xo_mitigation_stop_chg = false;
static int g_batt_temp = 0;
#endif

/* [START] sungsookim */
#ifdef CONFIG_LGE_PM
static struct pseudo_batt_info_type pseudo_batt_info = {
	.mode = 0,
};
#if defined(CONFIG_MACH_LGE_L9II_COMMON)

/*                                                                     */
static struct off_batt_info_type off_batt_info = {
	.mode = 0,
};

/*                                                                   */
#endif


static int block_charging_state = 1; /* 1 : charing, 0 : block charging */
#ifdef CONFIG_MACH_LGE_FX3_VZW
static int stop_chg_upon_expiry_state = 1;
#endif
#endif
/* [END] */

#ifdef CONFIG_LGE_PM_VZW_FAST_CHG
enum vzw_chg_state {
	VZW_NO_CHARGER = 0,
	VZW_NORMAL_CHARGING,
	VZW_NOT_CHARGING,
	VZW_UNDER_CURRENT_CHARGING,
	VZW_USB_DRIVER_UNINSTALLED,
};
static enum vzw_chg_state chg_state = VZW_NO_CHARGER;
#endif

static struct pm8921_chg_chip *the_chip;

static struct pm8xxx_adc_arb_btm_param btm_config;
/*                                                              */
#ifdef CONFIG_LGE_PM
int lge_power_test_flag = 0;
#endif
/*                          */

/*                                                                        */
#ifdef CONFIG_LGE_PM
static int init_chg_current;
#endif
/*                                                                      */
/*                                                                 */
#ifdef CONFIG_LGE_PM
static int ac_online = 0;
static int usb_online = 0;
#endif
/*                                        */

/*                                  
                                                                                                                 */
#if defined(CONFIG_MACH_MSM8960_D1LU) || defined(CONFIG_MACH_MSM8960_D1LSK) || defined(CONFIG_MACH_MSM8960_D1LKT) || defined(CONFIG_MACH_MSM8960_L_DCM) || defined(CONFIG_MACH_MSM8960_L0)
int batt_health = POWER_SUPPLY_HEALTH_GOOD;
#endif
/*                                  */

/* Battery alarm for the model using BMS  */
#if defined(CONFIG_LGE_PM) && !defined(CONFIG_BATTERY_MAX17043)
static int batt_alarm_disabled = 1;
#endif

#ifdef CONFIG_LGE_PM
struct wake_lock uevent_wake_lock;
static void decrease_usb_ma_value(int *value);
#endif

/*                                          
                                                                                  
                                                                               */
static int is_battery_valid(struct pm8921_chg_chip *chip);
/* END : donghyuk79.park 2013-04.23 */

/*                                                          */
#ifdef CONFIG_LGE_PM
int pm8921_charger_is_ta_connected(void)
{
	if (the_chip == NULL)
		return 0;
	return ac_online;
}
EXPORT_SYMBOL(pm8921_charger_is_ta_connected);
#endif
/*                          */
#ifdef CONFIG_LGE_PM
bool is_factory_cable(void)
{
	int result = 0;
	static acc_cable_type old_cable_type = CABLE_NONE;
	acc_cable_type cable_type = lge_pm_get_cable_type();

#ifdef CONFIG_MACH_MSM8930_FX3
	if (the_chip == NULL) {
		pr_err("called before init\n");
		return 0;
	}
#endif
	if ((cable_type == CABLE_56K ||
		cable_type == CABLE_130K ||
		cable_type == CABLE_910K)) {
#ifdef CONFIG_MACH_MSM8930_FX3
		the_chip->aicl = false;
#endif

		result = 1;
	}
	else {
#ifdef CONFIG_MACH_MSM8930_FX3
		the_chip->aicl = true;
#endif
		result = 0;
	}

	if (cable_type != old_cable_type) {
		char buf[32];
		switch (cable_type) {
			case NO_INIT_CABLE:
				strcpy(buf, "NO_INIT_CABLE");
				break;

			case CABLE_56K:
				strcpy(buf, "CABLE_56K");
				break;

			case CABLE_130K:
				strcpy(buf, "CABLE_130K");
				break;

			case CABLE_910K:
				strcpy(buf, "CABLE_910K");
				break;

			case CABLE_NONE:
				strcpy(buf, "CABLE_NONE");
				break;

			default:
				sprintf(buf, "return %d", cable_type);
				break;
		}

		printk(KERN_INFO "[POWER] %s(): %s\n", __func__, buf);
	}

	old_cable_type = cable_type;
	return result;
}
EXPORT_SYMBOL(is_factory_cable);
#endif
#ifdef CONFIG_LGE_PM
void pm8921_charger_force_update_batt_psy(void)
{
	struct pm8921_chg_chip *chip;

	if (!the_chip) {
		pr_err("called before init\n");
		return;
	}

	chip = the_chip;

	power_supply_changed(&chip->batt_psy);
}
EXPORT_SYMBOL_GPL(pm8921_charger_force_update_batt_psy);
#endif

/* Battery alarm for the model using BMS */
#if defined(CONFIG_LGE_PM) && !defined(CONFIG_BATTERY_MAX17043)
int get_bms_battery_uvolts(void)
{
	int rc;
	struct pm8xxx_adc_chan_result result;

	rc = pm8xxx_adc_read(CHANNEL_VBAT, &result);
	if (rc) {
		pr_err("error reading adc channel = %d, rc = %d\n",
					CHANNEL_VBAT, rc);
		return rc;
	}
	pr_debug("mvolts phy = %lld meas = 0x%llx\n", result.physical,
						result.measurement);
	return (int)result.physical;
}

/*                                      
                                                         
                                                                                                      
                                  
 */
#define BATTERY_UPPER_THRESHOLD	4350
#define BATTERY_LOW_THRESHOLD	3600
#endif

static int pm_chg_masked_write(struct pm8921_chg_chip *chip, u16 addr,
							u8 mask, u8 val)
{
	int rc;
	u8 reg;

	if (!chip->dev->parent) {
		pr_err("called before init, chip->dev->parent\n");
		return -EINVAL;
	}

	rc = pm8xxx_readb(chip->dev->parent, addr, &reg);
	if (rc) {
		pr_err("pm8xxx_readb failed: addr=%03X, rc=%d\n", addr, rc);
		return rc;
	}
	reg &= ~mask;
	reg |= val & mask;
	rc = pm8xxx_writeb(chip->dev->parent, addr, reg);
	if (rc) {
		pr_err("pm8xxx_writeb failed: addr=%03X, rc=%d\n", addr, rc);
		return rc;
	}
	return 0;
}

static int pm_chg_get_rt_status(struct pm8921_chg_chip *chip, int irq_id)
{
	return pm8xxx_read_irq_stat(chip->dev->parent,
					chip->pmic_chg_irq[irq_id]);
}

/* Treat OverVoltage/UnderVoltage as source missing */
static int is_usb_chg_plugged_in(struct pm8921_chg_chip *chip)
{
	return pm_chg_get_rt_status(chip, USBIN_VALID_IRQ);
}

/* Treat OverVoltage/UnderVoltage as source missing */
static int is_dc_chg_plugged_in(struct pm8921_chg_chip *chip)
{
	return pm_chg_get_rt_status(chip, DCIN_VALID_IRQ);
}

static int is_batfet_closed(struct pm8921_chg_chip *chip)
{
	return pm_chg_get_rt_status(chip, BATFET_IRQ);
}
#define CAPTURE_FSM_STATE_CMD	0xC2
#define READ_BANK_7		0x70
#define READ_BANK_4		0x40
static int pm_chg_get_fsm_state(struct pm8921_chg_chip *chip)
{
	u8 temp;
	int err, ret = 0;

	temp = CAPTURE_FSM_STATE_CMD;
	err = pm8xxx_writeb(chip->dev->parent, CHG_TEST, temp);
	if (err) {
		pr_err("Error %d writing %d to addr %d\n", err, temp, CHG_TEST);
		return err;
	}

	temp = READ_BANK_7;
	err = pm8xxx_writeb(chip->dev->parent, CHG_TEST, temp);
	if (err) {
		pr_err("Error %d writing %d to addr %d\n", err, temp, CHG_TEST);
		return err;
	}

	err = pm8xxx_readb(chip->dev->parent, CHG_TEST, &temp);
	if (err) {
		pr_err("pm8xxx_readb fail: addr=%03X, rc=%d\n", CHG_TEST, err);
		return err;
	}
	/* get the lower 4 bits */
	ret = temp & 0xF;

	temp = READ_BANK_4;
	err = pm8xxx_writeb(chip->dev->parent, CHG_TEST, temp);
	if (err) {
		pr_err("Error %d writing %d to addr %d\n", err, temp, CHG_TEST);
		return err;
	}

	err = pm8xxx_readb(chip->dev->parent, CHG_TEST, &temp);
	if (err) {
		pr_err("pm8xxx_readb fail: addr=%03X, rc=%d\n", CHG_TEST, err);
		return err;
	}
	/* get the upper 1 bit */
	ret |= (temp & 0x1) << 4;
	return  ret;
}

#define READ_BANK_6		0x60
static int pm_chg_get_regulation_loop(struct pm8921_chg_chip *chip)
{
	u8 temp;
	int err;

	temp = READ_BANK_6;
	err = pm8xxx_writeb(chip->dev->parent, CHG_TEST, temp);
	if (err) {
		pr_err("Error %d writing %d to addr %d\n", err, temp, CHG_TEST);
		return err;
	}

	err = pm8xxx_readb(chip->dev->parent, CHG_TEST, &temp);
	if (err) {
		pr_err("pm8xxx_readb fail: addr=%03X, rc=%d\n", CHG_TEST, err);
		return err;
	}

	/* return the lower 4 bits */
	return temp & CHG_ALL_LOOPS;
}

#define CHG_USB_SUSPEND_BIT  BIT(2)
static int pm_chg_usb_suspend_enable(struct pm8921_chg_chip *chip, int enable)
{
	return pm_chg_masked_write(chip, CHG_CNTRL_3, CHG_USB_SUSPEND_BIT,
			enable ? CHG_USB_SUSPEND_BIT : 0);
}

#define CHG_EN_BIT	BIT(7)
static int pm_chg_auto_enable(struct pm8921_chg_chip *chip, int enable)
{
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	static int enable_local = -EINVAL;

/*                                          
                                                                                  
                                                                               */
#ifdef CONFIG_LGE_PM_BATTERY_ID_CHECKER
	if(is_battery_valid(chip) == 0){
		enable = 0;
	}
#endif
/* END : donghyuk79.park 2013-04.23 */	
	if (enable == enable_local)
		return 0;
	enable_local = enable;
#endif
	return pm_chg_masked_write(chip, CHG_CNTRL_3, CHG_EN_BIT,
				enable ? CHG_EN_BIT : 0);
}

#define CHG_FAILED_CLEAR	BIT(0)
#define ATC_FAILED_CLEAR	BIT(1)
static int pm_chg_failed_clear(struct pm8921_chg_chip *chip, int clear)
{
	int rc;

	rc = pm_chg_masked_write(chip, CHG_CNTRL_3, ATC_FAILED_CLEAR,
				clear ? ATC_FAILED_CLEAR : 0);
	rc |= pm_chg_masked_write(chip, CHG_CNTRL_3, CHG_FAILED_CLEAR,
				clear ? CHG_FAILED_CLEAR : 0);
	return rc;
}

#define CHG_CHARGE_DIS_BIT	BIT(1)
static int pm_chg_charge_dis(struct pm8921_chg_chip *chip, int disable)
{
	return pm_chg_masked_write(chip, CHG_CNTRL, CHG_CHARGE_DIS_BIT,
				disable ? CHG_CHARGE_DIS_BIT : 0);
}

static int pm_is_chg_charge_dis(struct pm8921_chg_chip *chip)
{
	u8 temp;

	pm8xxx_readb(chip->dev->parent, CHG_CNTRL, &temp);
	return  temp & CHG_CHARGE_DIS_BIT;
}
#define PM8921_CHG_V_MIN_MV	3240
#define PM8921_CHG_V_STEP_MV	20
#define PM8921_CHG_V_STEP_10MV_OFFSET_BIT	BIT(7)
#define PM8921_CHG_VDDMAX_MAX	4500
#define PM8921_CHG_VDDMAX_MIN	3400
#define PM8921_CHG_V_MASK	0x7F
static int __pm_chg_vddmax_set(struct pm8921_chg_chip *chip, int voltage)
{
	int remainder;
	u8 temp = 0;

	if (voltage < PM8921_CHG_VDDMAX_MIN
			|| voltage > PM8921_CHG_VDDMAX_MAX) {
		pr_err("bad mV=%d asked to set\n", voltage);
		return -EINVAL;
	}

	temp = (voltage - PM8921_CHG_V_MIN_MV) / PM8921_CHG_V_STEP_MV;

	remainder = voltage % 20;
	if (remainder >= 10) {
		temp |= PM8921_CHG_V_STEP_10MV_OFFSET_BIT;
	}

	pr_debug("voltage=%d setting %02x\n", voltage, temp);
	return pm8xxx_writeb(chip->dev->parent, CHG_VDD_MAX, temp);
}

static int pm_chg_vddmax_get(struct pm8921_chg_chip *chip, int *voltage)
{
	u8 temp;
	int rc;

	rc = pm8xxx_readb(chip->dev->parent, CHG_VDD_MAX, &temp);
	if (rc) {
		pr_err("rc = %d while reading vdd max\n", rc);
		*voltage = 0;
		return rc;
	}
	*voltage = (int)(temp & PM8921_CHG_V_MASK) * PM8921_CHG_V_STEP_MV
							+ PM8921_CHG_V_MIN_MV;
	if (temp & PM8921_CHG_V_STEP_10MV_OFFSET_BIT)
		*voltage =  *voltage + 10;
	return 0;
}

static int pm_chg_vddmax_set(struct pm8921_chg_chip *chip, int voltage)
{
	int current_mv, ret, steps, i;
	bool increase;

	ret = 0;

	if (voltage < PM8921_CHG_VDDMAX_MIN
		|| voltage > PM8921_CHG_VDDMAX_MAX) {
		pr_err("bad mV=%d asked to set\n", voltage);
		return -EINVAL;
	}

	ret = pm_chg_vddmax_get(chip, &current_mv);
	if (ret) {
		pr_err("Failed to read vddmax rc=%d\n", ret);
		return -EINVAL;
	}
	if (current_mv == voltage)
		return 0;

	/* Only change in increments when USB is present */
	if (is_usb_chg_plugged_in(chip)) {
		if (current_mv < voltage) {
			steps = (voltage - current_mv) / PM8921_CHG_V_STEP_MV;
			increase = true;
		} else {
			steps = (current_mv - voltage) / PM8921_CHG_V_STEP_MV;
			increase = false;
		}
		for (i = 0; i < steps; i++) {
			if (increase)
				current_mv += PM8921_CHG_V_STEP_MV;
			else
				current_mv -= PM8921_CHG_V_STEP_MV;
			ret |= __pm_chg_vddmax_set(chip, current_mv);
		}
	}
	ret |= __pm_chg_vddmax_set(chip, voltage);
	return ret;
}

#define PM8921_CHG_VDDSAFE_MIN	3400
#define PM8921_CHG_VDDSAFE_MAX	4500
static int pm_chg_vddsafe_set(struct pm8921_chg_chip *chip, int voltage)
{
	u8 temp;

	if (voltage < PM8921_CHG_VDDSAFE_MIN
			|| voltage > PM8921_CHG_VDDSAFE_MAX) {
		pr_err("bad mV=%d asked to set\n", voltage);
		return -EINVAL;
	}
	temp = (voltage - PM8921_CHG_V_MIN_MV) / PM8921_CHG_V_STEP_MV;
	pr_debug("voltage=%d setting %02x\n", voltage, temp);
	return pm_chg_masked_write(chip, CHG_VDD_SAFE, PM8921_CHG_V_MASK, temp);
}

#define PM8921_CHG_VBATDET_MIN	3240
#define PM8921_CHG_VBATDET_MAX	5780
static int pm_chg_vbatdet_set(struct pm8921_chg_chip *chip, int voltage)
{
	u8 temp;

	if (voltage < PM8921_CHG_VBATDET_MIN
			|| voltage > PM8921_CHG_VBATDET_MAX) {
		pr_err("bad mV=%d asked to set\n", voltage);
		return -EINVAL;
	}
	temp = (voltage - PM8921_CHG_V_MIN_MV) / PM8921_CHG_V_STEP_MV;
	pr_debug("voltage=%d setting %02x\n", voltage, temp);
	return pm_chg_masked_write(chip, CHG_VBAT_DET, PM8921_CHG_V_MASK, temp);
}

#define PM8921_CHG_VINMIN_MIN_MV	3800
#define PM8921_CHG_VINMIN_STEP_MV	100
#define PM8921_CHG_VINMIN_USABLE_MAX	6500
#define PM8921_CHG_VINMIN_USABLE_MIN	4300
#define PM8921_CHG_VINMIN_MASK		0x1F
static int pm_chg_vinmin_set(struct pm8921_chg_chip *chip, int voltage)
{
	u8 temp;

	if (voltage < PM8921_CHG_VINMIN_USABLE_MIN
			|| voltage > PM8921_CHG_VINMIN_USABLE_MAX) {
		pr_err("bad mV=%d asked to set\n", voltage);
		return -EINVAL;
	}
	temp = (voltage - PM8921_CHG_VINMIN_MIN_MV) / PM8921_CHG_VINMIN_STEP_MV;
	pr_debug("voltage=%d setting %02x\n", voltage, temp);
	return pm_chg_masked_write(chip, CHG_VIN_MIN, PM8921_CHG_VINMIN_MASK,
									temp);
}

static int pm_chg_vinmin_get(struct pm8921_chg_chip *chip)
{
	u8 temp;
	int rc, voltage_mv;

	rc = pm8xxx_readb(chip->dev->parent, CHG_VIN_MIN, &temp);
	temp &= PM8921_CHG_VINMIN_MASK;

	voltage_mv = PM8921_CHG_VINMIN_MIN_MV +
			(int)temp * PM8921_CHG_VINMIN_STEP_MV;

	return voltage_mv;
}

#define PM8917_USB_UVD_MIN_MV	3850
#define PM8917_USB_UVD_MAX_MV	4350
#define PM8917_USB_UVD_STEP_MV	100
#define PM8917_USB_UVD_MASK	0x7
static int pm_chg_uvd_threshold_set(struct pm8921_chg_chip *chip, int thresh_mv)
{
	u8 temp;

	if (thresh_mv < PM8917_USB_UVD_MIN_MV
			|| thresh_mv > PM8917_USB_UVD_MAX_MV) {
		pr_err("bad mV=%d asked to set\n", thresh_mv);
		return -EINVAL;
	}
	temp = (thresh_mv - PM8917_USB_UVD_MIN_MV) / PM8917_USB_UVD_STEP_MV;
	return pm_chg_masked_write(chip, OVP_USB_UVD,
				PM8917_USB_UVD_MASK, temp);
}

#define PM8921_CHG_IBATMAX_MIN	325
#define PM8921_CHG_IBATMAX_MAX	2000
#define PM8921_CHG_I_MIN_MA	225
#define PM8921_CHG_I_STEP_MA	50
#define PM8921_CHG_I_MASK	0x3F
#ifdef CONFIG_LGE_PM
#define PM8921_CHG_I_DECREASE_CHARGING_CURRENT	400	//mA
#define DECREASE_CHARGING_VOLTAGE		4000	//mV
#endif
static int pm_chg_ibatmax_set(struct pm8921_chg_chip *chip, int chg_current)
{
	u8 temp;

#ifdef CONFIG_LGE_PM
	pr_info("[POWER] Set ibat_max_ma & ibatmax %dmA\n", chg_current);	
	ibat_max_ma = chg_current;
#endif

	if (chg_current < PM8921_CHG_IBATMAX_MIN
			|| chg_current > PM8921_CHG_IBATMAX_MAX) {
		pr_err("bad mA=%d asked to set\n", chg_current);
		return -EINVAL;
	}
	temp = (chg_current - PM8921_CHG_I_MIN_MA) / PM8921_CHG_I_STEP_MA;
	return pm_chg_masked_write(chip, CHG_IBAT_MAX, PM8921_CHG_I_MASK, temp);
}

static int pm_chg_ibatmax_get(struct pm8921_chg_chip *chip, int *chg_current)
{
	u8 temp;
	int rc;

	rc = pm8xxx_readb(chip->dev->parent, CHG_IBAT_MAX, &temp);

	if(rc){
		pr_err("err=%d reading CHG_IBAT_MAX\n", rc);
		*chg_current = 0;
		return rc;
	}

	temp &= PM8921_CHG_I_MASK;

	*chg_current = (int)temp* PM8921_CHG_I_STEP_MA + PM8921_CHG_I_MIN_MA;
	return 0;
}


#define PM8921_CHG_IBATSAFE_MIN	225
#define PM8921_CHG_IBATSAFE_MAX	3375
static int pm_chg_ibatsafe_set(struct pm8921_chg_chip *chip, int chg_current)
{
	u8 temp;

	if (chg_current < PM8921_CHG_IBATSAFE_MIN
			|| chg_current > PM8921_CHG_IBATSAFE_MAX) {
		pr_err("bad mA=%d asked to set\n", chg_current);
		return -EINVAL;
	}
	temp = (chg_current - PM8921_CHG_I_MIN_MA) / PM8921_CHG_I_STEP_MA;
	return pm_chg_masked_write(chip, CHG_IBAT_SAFE,
						PM8921_CHG_I_MASK, temp);
}

#define PM8921_CHG_ITERM_MIN_MA		50
#define PM8921_CHG_ITERM_MAX_MA		200
#define PM8921_CHG_ITERM_STEP_MA	10
#define PM8921_CHG_ITERM_MASK		0xF
static int pm_chg_iterm_set(struct pm8921_chg_chip *chip, int chg_current)
{
	u8 temp;

	if (chg_current < PM8921_CHG_ITERM_MIN_MA
			|| chg_current > PM8921_CHG_ITERM_MAX_MA) {
		pr_err("bad mA=%d asked to set\n", chg_current);
		return -EINVAL;
	}

	temp = (chg_current - PM8921_CHG_ITERM_MIN_MA)
				/ PM8921_CHG_ITERM_STEP_MA;
	return pm_chg_masked_write(chip, CHG_ITERM, PM8921_CHG_ITERM_MASK,
					 temp);
}

static int pm_chg_iterm_get(struct pm8921_chg_chip *chip, int *chg_current)
{
	u8 temp;
	int rc;

	rc = pm8xxx_readb(chip->dev->parent, CHG_ITERM, &temp);
	if (rc) {
		pr_err("err=%d reading CHG_ITEM\n", rc);
		*chg_current = 0;
		return rc;
	}
	temp &= PM8921_CHG_ITERM_MASK;
	*chg_current = (int)temp * PM8921_CHG_ITERM_STEP_MA
					+ PM8921_CHG_ITERM_MIN_MA;
	return 0;
}

struct usb_ma_limit_entry {
	int	usb_ma;
	u8	value;
};

/* USB Trim tables */
static int usb_trim_8038_table[USB_TRIM_ENTRIES] = {
	0x0,
	0x0,
	-0x9,
	0x0,
	-0xD,
	0x0,
	-0x10,
	-0x11,
	0x0,
	0x0,
	-0x25,
	0x0,
	-0x28,
	0x0,
	-0x32,
	0x0
};

static int usb_trim_8917_table[USB_TRIM_ENTRIES] = {
	0x0,
	0x0,
	0xA,
	0xC,
	0x10,
	0x10,
	0x13,
	0x14,
	0x13,
	0x3,
	0x1A,
	0x1D,
	0x1D,
	0x21,
	0x24,
	0x26
};

/* Maximum USB  setting table */
static struct usb_ma_limit_entry usb_ma_table[] = {
	{100, 0x0},
	{200, 0x1},
	{500, 0x2},
	{600, 0x3},
	{700, 0x4},
	{800, 0x5},
	{850, 0x6},
	{900, 0x8},
	{950, 0x7},
	{1000, 0x9},
	{1100, 0xA},
	{1200, 0xB},
	{1300, 0xC},
	{1400, 0xD},
	{1500, 0xE},
	{1600, 0xF},
};

#define REG_SBI_CONFIG			0x04F
#define PAGE3_ENABLE_MASK		0x6
#define USB_OVP_TRIM_MASK		0x3F
#define USB_OVP_TRIM_PM8917_MASK	0x7F
#define USB_OVP_TRIM_MIN		0x00
#define REG_USB_OVP_TRIM_ORIG_LSB	0x10A
#define REG_USB_OVP_TRIM_ORIG_MSB	0x09C
#define REG_USB_OVP_TRIM_PM8917		0x2B5
#define REG_USB_OVP_TRIM_PM8917_BIT	BIT(0)
static int pm_chg_usb_trim(struct pm8921_chg_chip *chip, int index)
{
	u8 temp, sbi_config, msb, lsb, mask;
	s8 trim;
	int rc = 0;
	static u8 usb_trim_reg_orig = 0xFF;

	/* No trim data for PM8921 */
	if (!chip->usb_trim_table)
		return 0;

	if (usb_trim_reg_orig == 0xFF) {
		rc = pm8xxx_readb(chip->dev->parent,
				REG_USB_OVP_TRIM_ORIG_MSB, &msb);
		if (rc) {
			pr_err("error = %d reading sbi config reg\n", rc);
			return rc;
		}

		rc = pm8xxx_readb(chip->dev->parent,
				REG_USB_OVP_TRIM_ORIG_LSB, &lsb);
		if (rc) {
			pr_err("error = %d reading sbi config reg\n", rc);
			return rc;
		}

		msb = msb >> 5;
		lsb = lsb >> 5;
		usb_trim_reg_orig = msb << 3 | lsb;

		if (pm8xxx_get_version(chip->dev->parent)
				== PM8XXX_VERSION_8917) {
			rc = pm8xxx_readb(chip->dev->parent,
					REG_USB_OVP_TRIM_PM8917, &msb);
			if (rc) {
				pr_err("error = %d reading config reg\n", rc);
				return rc;
			}

			msb = msb & REG_USB_OVP_TRIM_PM8917_BIT;
			usb_trim_reg_orig |= msb << 6;
		}
	}

	/* use the original trim value */
	trim = usb_trim_reg_orig;

	trim += chip->usb_trim_table[index];
	if (trim < 0)
		trim = 0;
#ifndef CONFIG_LGE_PM
	pr_err("trim_orig %d write 0x%x index=%d value 0x%x to USB_OVP_TRIM\n",
		usb_trim_reg_orig, trim, index, chip->usb_trim_table[index]);
#endif

	rc = pm8xxx_readb(chip->dev->parent, REG_SBI_CONFIG, &sbi_config);
	if (rc) {
		pr_err("error = %d reading sbi config reg\n", rc);
		return rc;
	}

	temp = sbi_config | PAGE3_ENABLE_MASK;
	rc = pm8xxx_writeb(chip->dev->parent, REG_SBI_CONFIG, temp);
	if (rc) {
		pr_err("error = %d writing sbi config reg\n", rc);
		return rc;
	}

	mask = USB_OVP_TRIM_MASK;

	if (pm8xxx_get_version(chip->dev->parent) == PM8XXX_VERSION_8917)
		mask = USB_OVP_TRIM_PM8917_MASK;

	rc = pm_chg_masked_write(chip, USB_OVP_TRIM, mask, trim);
	if (rc) {
		pr_err("error = %d writing USB_OVP_TRIM\n", rc);
		return rc;
	}

	rc = pm8xxx_writeb(chip->dev->parent, REG_SBI_CONFIG, sbi_config);
	if (rc) {
		pr_err("error = %d writing sbi config reg\n", rc);
		return rc;
	}
	return rc;
}

#define PM8921_CHG_IUSB_MASK 0x1C
#define PM8921_CHG_IUSB_SHIFT 2
#define PM8921_CHG_IUSB_MAX  7
#define PM8921_CHG_IUSB_MIN  0
#define PM8917_IUSB_FINE_RES BIT(0)
static int pm_chg_iusbmax_set(struct pm8921_chg_chip *chip, int index)
{
	u8 temp, fineres, reg_val;
	int rc;

#ifdef CONFIG_LGE_PM
	pr_info("[POWER] Set iusb %dmA\n", usb_ma_table[index].usb_ma);
#endif
	reg_val = usb_ma_table[index].value >> 1;
	fineres = PM8917_IUSB_FINE_RES & usb_ma_table[index].value;

	if (reg_val < PM8921_CHG_IUSB_MIN || reg_val > PM8921_CHG_IUSB_MAX) {
		pr_err("bad mA=%d asked to set\n", reg_val);
		return -EINVAL;
	}
	temp = reg_val << PM8921_CHG_IUSB_SHIFT;

	/* IUSB_FINE_RES */
	if (chip->iusb_fine_res) {
		/* Clear IUSB_FINE_RES bit to avoid overshoot */
		rc = pm_chg_masked_write(chip, IUSB_FINE_RES,
			PM8917_IUSB_FINE_RES, 0);

		rc |= pm_chg_masked_write(chip, PBL_ACCESS2,
			PM8921_CHG_IUSB_MASK, temp);

		if (rc) {
			pr_err("Failed to write PBL_ACCESS2 rc=%d\n", rc);
			return rc;
		}

		if (fineres) {
			rc = pm_chg_masked_write(chip, IUSB_FINE_RES,
				PM8917_IUSB_FINE_RES, fineres);
			if (rc) {
				pr_err("Failed to write ISUB_FINE_RES rc=%d\n",
					rc);
				return rc;
			}
		}
	} else {
		rc = pm_chg_masked_write(chip, PBL_ACCESS2,
			PM8921_CHG_IUSB_MASK, temp);
		if (rc) {
			pr_err("Failed to write PBL_ACCESS2 rc=%d\n", rc);
			return rc;
		}
	}

	rc = pm_chg_usb_trim(chip, index);
	if (rc)
			pr_err("unable to set usb trim rc = %d\n", rc);

	return rc;
}

static int pm_chg_iusbmax_get(struct pm8921_chg_chip *chip, int *mA)
{
	u8 temp, fineres;
	int rc, i;

	fineres = 0;
	*mA = 0;
	rc = pm8xxx_readb(chip->dev->parent, PBL_ACCESS2, &temp);
	if (rc) {
		pr_err("err=%d reading PBL_ACCESS2\n", rc);
		return rc;
	}

	if (chip->iusb_fine_res) {
		rc = pm8xxx_readb(chip->dev->parent, IUSB_FINE_RES, &fineres);
		if (rc) {
			pr_err("err=%d reading IUSB_FINE_RES\n", rc);
			return rc;
		}
	}
	temp &= PM8921_CHG_IUSB_MASK;
	temp = temp >> PM8921_CHG_IUSB_SHIFT;

	temp = (temp << 1) | (fineres & PM8917_IUSB_FINE_RES);
	for (i = ARRAY_SIZE(usb_ma_table) - 1; i >= 0; i--) {
		if (usb_ma_table[i].value == temp)
			break;
	}

	*mA = usb_ma_table[i].usb_ma;

	return rc;
}

#define PM8921_CHG_WD_MASK 0x1F
static int pm_chg_disable_wd(struct pm8921_chg_chip *chip)
{
	/* writing 0 to the wd timer disables it */
	return pm_chg_masked_write(chip, CHG_TWDOG, PM8921_CHG_WD_MASK, 0);
}

#define PM8921_CHG_TCHG_MASK	0x7F
#define PM8921_CHG_TCHG_MIN	4
#define PM8921_CHG_TCHG_MAX	512
#define PM8921_CHG_TCHG_STEP	4
static int pm_chg_tchg_max_set(struct pm8921_chg_chip *chip, int minutes)
{
	u8 temp;

	if (minutes < PM8921_CHG_TCHG_MIN || minutes > PM8921_CHG_TCHG_MAX) {
		pr_err("bad max minutes =%d asked to set\n", minutes);
		return -EINVAL;
	}

	temp = (minutes - 1)/PM8921_CHG_TCHG_STEP;
	return pm_chg_masked_write(chip, CHG_TCHG_MAX, PM8921_CHG_TCHG_MASK,
					 temp);
}

#define PM8921_CHG_TTRKL_MASK	0x3F
#define PM8921_CHG_TTRKL_MIN	1
#define PM8921_CHG_TTRKL_MAX	64
static int pm_chg_ttrkl_max_set(struct pm8921_chg_chip *chip, int minutes)
{
	u8 temp;

	if (minutes < PM8921_CHG_TTRKL_MIN || minutes > PM8921_CHG_TTRKL_MAX) {
		pr_err("bad max minutes =%d asked to set\n", minutes);
		return -EINVAL;
	}

	temp = minutes - 1;
	return pm_chg_masked_write(chip, CHG_TTRKL_MAX, PM8921_CHG_TTRKL_MASK,
					 temp);
}

#define PM8921_CHG_VTRKL_MIN_MV		2050
#define PM8921_CHG_VTRKL_MAX_MV		2800
#define PM8921_CHG_VTRKL_STEP_MV	50
#define PM8921_CHG_VTRKL_SHIFT		4
#define PM8921_CHG_VTRKL_MASK		0xF0
static int pm_chg_vtrkl_low_set(struct pm8921_chg_chip *chip, int millivolts)
{
	u8 temp;

	if (millivolts < PM8921_CHG_VTRKL_MIN_MV
			|| millivolts > PM8921_CHG_VTRKL_MAX_MV) {
		pr_err("bad voltage = %dmV asked to set\n", millivolts);
		return -EINVAL;
	}

	temp = (millivolts - PM8921_CHG_VTRKL_MIN_MV)/PM8921_CHG_VTRKL_STEP_MV;
	temp = temp << PM8921_CHG_VTRKL_SHIFT;
	return pm_chg_masked_write(chip, CHG_VTRICKLE, PM8921_CHG_VTRKL_MASK,
					 temp);
}

#define PM8921_CHG_VWEAK_MIN_MV		2100
#define PM8921_CHG_VWEAK_MAX_MV		3600
#define PM8921_CHG_VWEAK_STEP_MV	100
#define PM8921_CHG_VWEAK_MASK		0x0F
static int pm_chg_vweak_set(struct pm8921_chg_chip *chip, int millivolts)
{
	u8 temp;

	if (millivolts < PM8921_CHG_VWEAK_MIN_MV
			|| millivolts > PM8921_CHG_VWEAK_MAX_MV) {
		pr_err("bad voltage = %dmV asked to set\n", millivolts);
		return -EINVAL;
	}

	temp = (millivolts - PM8921_CHG_VWEAK_MIN_MV)/PM8921_CHG_VWEAK_STEP_MV;
	return pm_chg_masked_write(chip, CHG_VTRICKLE, PM8921_CHG_VWEAK_MASK,
					 temp);
}

#define PM8921_CHG_ITRKL_MIN_MA		50
#define PM8921_CHG_ITRKL_MAX_MA		200
#define PM8921_CHG_ITRKL_MASK		0x0F
#define PM8921_CHG_ITRKL_STEP_MA	10
static int pm_chg_itrkl_set(struct pm8921_chg_chip *chip, int milliamps)
{
	u8 temp;

	if (milliamps < PM8921_CHG_ITRKL_MIN_MA
		|| milliamps > PM8921_CHG_ITRKL_MAX_MA) {
		pr_err("bad current = %dmA asked to set\n", milliamps);
		return -EINVAL;
	}

	temp = (milliamps - PM8921_CHG_ITRKL_MIN_MA)/PM8921_CHG_ITRKL_STEP_MA;

	return pm_chg_masked_write(chip, CHG_ITRICKLE, PM8921_CHG_ITRKL_MASK,
					 temp);
}

#define PM8921_CHG_IWEAK_MIN_MA		325
#define PM8921_CHG_IWEAK_MAX_MA		525
#define PM8921_CHG_IWEAK_SHIFT		7
#define PM8921_CHG_IWEAK_MASK		0x80
static int pm_chg_iweak_set(struct pm8921_chg_chip *chip, int milliamps)
{
	u8 temp;

	if (milliamps < PM8921_CHG_IWEAK_MIN_MA
		|| milliamps > PM8921_CHG_IWEAK_MAX_MA) {
		pr_err("bad current = %dmA asked to set\n", milliamps);
		return -EINVAL;
	}

	if (milliamps < PM8921_CHG_IWEAK_MAX_MA)
		temp = 0;
	else
		temp = 1;

	temp = temp << PM8921_CHG_IWEAK_SHIFT;
	return pm_chg_masked_write(chip, CHG_ITRICKLE, PM8921_CHG_IWEAK_MASK,
					 temp);
}

#define PM8921_CHG_BATT_TEMP_THR_COLD	BIT(1)
#define PM8921_CHG_BATT_TEMP_THR_COLD_SHIFT	1
static int pm_chg_batt_cold_temp_config(struct pm8921_chg_chip *chip,
					enum pm8921_chg_cold_thr cold_thr)
{
	u8 temp;

	temp = cold_thr << PM8921_CHG_BATT_TEMP_THR_COLD_SHIFT;
	temp = temp & PM8921_CHG_BATT_TEMP_THR_COLD;
	return pm_chg_masked_write(chip, CHG_CNTRL_2,
					PM8921_CHG_BATT_TEMP_THR_COLD,
					 temp);
}

#define PM8921_CHG_BATT_TEMP_THR_HOT		BIT(0)
#define PM8921_CHG_BATT_TEMP_THR_HOT_SHIFT	0
static int pm_chg_batt_hot_temp_config(struct pm8921_chg_chip *chip,
					enum pm8921_chg_hot_thr hot_thr)
{
	u8 temp;

	temp = hot_thr << PM8921_CHG_BATT_TEMP_THR_HOT_SHIFT;
	temp = temp & PM8921_CHG_BATT_TEMP_THR_HOT;
	return pm_chg_masked_write(chip, CHG_CNTRL_2,
					PM8921_CHG_BATT_TEMP_THR_HOT,
					 temp);
}

#define PM8921_CHG_LED_SRC_CONFIG_SHIFT	4
#define PM8921_CHG_LED_SRC_CONFIG_MASK	0x30
static int pm_chg_led_src_config(struct pm8921_chg_chip *chip,
				enum pm8921_chg_led_src_config led_src_config)
{
	u8 temp;

	if (led_src_config < LED_SRC_GND ||
			led_src_config > LED_SRC_BYPASS)
		return -EINVAL;

	if (led_src_config == LED_SRC_BYPASS)
		return 0;

	temp = led_src_config << PM8921_CHG_LED_SRC_CONFIG_SHIFT;

	return pm_chg_masked_write(chip, CHG_CNTRL_3,
					PM8921_CHG_LED_SRC_CONFIG_MASK, temp);
}
#if 0
static void disable_input_voltage_regulation(struct pm8921_chg_chip *chip)
{
	u8 temp;
	int rc;

	rc = pm8xxx_writeb(chip->dev->parent, CHG_BUCK_CTRL_TEST3, 0x70);
	if (rc) {
		pr_err("Failed to write 0x70 to CTRL_TEST3 rc = %d\n", rc);
		return;
	}
	rc = pm8xxx_readb(chip->dev->parent, CHG_BUCK_CTRL_TEST3, &temp);
	if (rc) {
		pr_err("Failed to read CTRL_TEST3 rc = %d\n", rc);
		return;
	}
	/* set the input voltage disable bit and the write bit */
	temp |= 0x81;
	rc = pm8xxx_writeb(chip->dev->parent, CHG_BUCK_CTRL_TEST3, temp);
	if (rc) {
		pr_err("Failed to write 0x%x to CTRL_TEST3 rc=%d\n", temp, rc);
		return;
	}
}
#endif
static void enable_input_voltage_regulation(struct pm8921_chg_chip *chip)
{
	u8 temp;
	int rc;

	rc = pm8xxx_writeb(chip->dev->parent, CHG_BUCK_CTRL_TEST3, 0x70);
	if (rc) {
		pr_err("Failed to write 0x70 to CTRL_TEST3 rc = %d\n", rc);
		return;
	}
	rc = pm8xxx_readb(chip->dev->parent, CHG_BUCK_CTRL_TEST3, &temp);
	if (rc) {
		pr_err("Failed to read CTRL_TEST3 rc = %d\n", rc);
		return;
	}
	/* unset the input voltage disable bit */
	temp &= 0xFE;
	/* set the write bit */
	temp |= 0x80;
	rc = pm8xxx_writeb(chip->dev->parent, CHG_BUCK_CTRL_TEST3, temp);
	if (rc) {
		pr_err("Failed to write 0x%x to CTRL_TEST3 rc=%d\n", temp, rc);
		return;
	}
}

#ifndef CONFIG_LGE_PM_BATTERY_ID_CHECKER
static int64_t read_battery_id(struct pm8921_chg_chip *chip)
{
	int rc;
	struct pm8xxx_adc_chan_result result;

	rc = pm8xxx_adc_read(chip->batt_id_channel, &result);
	if (rc) {
		pr_err("error reading batt id channel = %d, rc = %d\n",
					chip->vbat_channel, rc);
		return rc;
	}
	pr_debug("batt_id phy = %lld meas = 0x%llx\n", result.physical,
						result.measurement);
	return result.physical;
}
#endif

static int is_battery_valid(struct pm8921_chg_chip *chip)
{
#ifndef CONFIG_LGE_PM_BATTERY_ID_CHECKER
	int64_t rc;
#endif

#ifdef CONFIG_LGE_PM
	if(pseudo_batt_info.mode == 1)
		return 1;
	else if (is_factory_cable())
		return 1;
#endif

#ifdef CONFIG_LGE_PM_BATTERY_ID_CHECKER
	if (lge_battery_info == BATT_DS2704_N || lge_battery_info == BATT_DS2704_L ||
		lge_battery_info == BATT_ISL6296_N || lge_battery_info == BATT_ISL6296_L ||
		lge_battery_info == BATT_DS2704_C || lge_battery_info == BATT_ISL6296_C)
		return 1;
	else
		return 0;
#else
	if (chip->batt_id_min == 0 && chip->batt_id_max == 0)
		return 1;

	rc = read_battery_id(chip);
	if (rc < 0) {
		pr_err("error reading batt id channel = %d, rc = %lld\n",
					chip->vbat_channel, rc);
		/* assume battery id is valid when adc error happens */
		return 1;
	}

	if (rc < chip->batt_id_min || rc > chip->batt_id_max) {
		pr_err("batt_id phy =%lld is not valid\n", rc);
		return 0;
	}
	return 1;
#endif
}

static void check_battery_valid(struct pm8921_chg_chip *chip)
{
	if (is_battery_valid(chip) == 0) {
		pr_err("batt_id not valid, disbling charging\n");
		pm_chg_auto_enable(chip, 0);
	} else {
		pm_chg_auto_enable(chip, !charging_disabled);
	}
}

static void battery_id_valid(struct work_struct *work)
{
	struct pm8921_chg_chip *chip = container_of(work,
				struct pm8921_chg_chip, battery_id_valid_work);

	check_battery_valid(chip);
}

static void pm8921_chg_enable_irq(struct pm8921_chg_chip *chip, int interrupt)
{
	if (!__test_and_set_bit(interrupt, chip->enabled_irqs)) {
		dev_dbg(chip->dev, "%d\n", chip->pmic_chg_irq[interrupt]);
		enable_irq(chip->pmic_chg_irq[interrupt]);
	}
}

static void pm8921_chg_disable_irq(struct pm8921_chg_chip *chip, int interrupt)
{
	if (__test_and_clear_bit(interrupt, chip->enabled_irqs)) {
		dev_dbg(chip->dev, "%d\n", chip->pmic_chg_irq[interrupt]);
		disable_irq_nosync(chip->pmic_chg_irq[interrupt]);
	}
}

static int pm8921_chg_is_enabled(struct pm8921_chg_chip *chip, int interrupt)
{
	return test_bit(interrupt, chip->enabled_irqs);
}

static bool is_ext_charging(struct pm8921_chg_chip *chip)
{
	union power_supply_propval ret = {0,};

	if (!chip->ext_psy)
		return false;
	if (chip->ext_psy->get_property(chip->ext_psy,
					POWER_SUPPLY_PROP_CHARGE_TYPE, &ret))
		return false;
	if (ret.intval > POWER_SUPPLY_CHARGE_TYPE_NONE)
		return ret.intval;

	return false;
}

static bool is_ext_trickle_charging(struct pm8921_chg_chip *chip)
{
	union power_supply_propval ret = {0,};

	if (!chip->ext_psy)
		return false;
	if (chip->ext_psy->get_property(chip->ext_psy,
					POWER_SUPPLY_PROP_CHARGE_TYPE, &ret))
		return false;
	if (ret.intval == POWER_SUPPLY_CHARGE_TYPE_TRICKLE)
		return true;

	return false;
}

static int is_battery_charging(int fsm_state)
{
	if (is_ext_charging(the_chip))
		return 1;

	switch (fsm_state) {
	case FSM_STATE_ATC_2A:
	case FSM_STATE_ATC_2B:
	case FSM_STATE_ON_CHG_AND_BAT_6:
	case FSM_STATE_FAST_CHG_7:
	case FSM_STATE_TRKL_CHG_8:
		return 1;
	}
	return 0;
}

static void bms_notify(struct work_struct *work)
{
	struct bms_notify *n = container_of(work, struct bms_notify, work);

	if (n->is_charging) {
		pm8921_bms_charging_began();
	} else {
		pm8921_bms_charging_end(n->is_battery_full);
		n->is_battery_full = 0;
	}
}

static void bms_notify_check(struct pm8921_chg_chip *chip)
{
	int fsm_state, new_is_charging;

	fsm_state = pm_chg_get_fsm_state(chip);
	new_is_charging = is_battery_charging(fsm_state);

	if (chip->bms_notify.is_charging ^ new_is_charging) {
		chip->bms_notify.is_charging = new_is_charging;
		schedule_work(&(chip->bms_notify.work));
	}
}

static enum power_supply_property pm_power_props_usb[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_SCOPE,
};

static enum power_supply_property pm_power_props_mains[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
};

static char *pm_power_supplied_to[] = {
	"battery",
};

#define USB_WALL_THRESHOLD_MA	500
static int pm_power_get_property_mains(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	/* Check if called before init */
	if (!the_chip)
		return -EINVAL;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = 0;
		if (charging_disabled)
			return 0;

#ifndef CONFIG_LGE_PM
		if (the_chip->has_dc_supply) {
			val->intval = 1;
			return 0;
		}
#endif
		if (pm_is_chg_charge_dis(the_chip)) {
			val->intval = 0;
			return 0;
		}

		if (the_chip->dc_present) {
			val->intval = 1;
			return 0;
		}

/*                                                                 */
#ifdef CONFIG_LGE_PM
		if(!is_usb_chg_plugged_in(the_chip))
			val->intval = 0;
		else
			val->intval =ac_online;
#else
		/* USB with max current greater than 500 mA connected */
		if (usb_target_ma > USB_WALL_THRESHOLD_MA)
			val->intval = is_usb_chg_plugged_in(the_chip);
#endif
/*                                        */

			return 0;

		break;
	default:
		return -EINVAL;
	}
	return 0;
}

/*                                                                 */
#ifdef CONFIG_LGE_PM
static int pm_power_set_property_mains(struct power_supply *psy,
				  enum power_supply_property psp,
				  const union power_supply_propval *val)
{
	/* Check if called before init */
	if (!the_chip)
		return -EINVAL;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		ac_online = val->intval;
		return 1;
	default:
		return -EINVAL;
	}
	return 0;
}
#endif
/*                                        */


static int switch_usb_to_charge_mode(struct pm8921_chg_chip *chip)
{
	int rc;

	if (!chip->host_mode)
		return 0;

	/* enable usbin valid comparator and remove force usb ovp fet off */
	rc = pm8xxx_writeb(chip->dev->parent, USB_OVP_TEST, 0xB2);
	if (rc < 0) {
		pr_err("Failed to write 0xB2 to USB_OVP_TEST rc = %d\n", rc);
		return rc;
	}

	chip->host_mode = 0;

	return 0;
}

static int switch_usb_to_host_mode(struct pm8921_chg_chip *chip)
{
	int rc;

	if (chip->host_mode)
		return 0;

	/* disable usbin valid comparator and force usb ovp fet off */
	rc = pm8xxx_writeb(chip->dev->parent, USB_OVP_TEST, 0xB3);
	if (rc < 0) {
		pr_err("Failed to write 0xB3 to USB_OVP_TEST rc = %d\n", rc);
		return rc;
	}

	chip->host_mode = 1;

	return 0;
}

static int pm_power_set_property_usb(struct power_supply *psy,
				  enum power_supply_property psp,
				  const union power_supply_propval *val)
{
	/* Check if called before init */
	if (!the_chip)
		return -EINVAL;

	switch (psp) {
/*                                                                 */
#ifdef CONFIG_LGE_PM
	case POWER_SUPPLY_PROP_ONLINE:
		usb_online = val->intval;
		return 1;
#endif
/*                                        */
	case POWER_SUPPLY_PROP_SCOPE:
		if (val->intval == POWER_SUPPLY_SCOPE_SYSTEM)
			return switch_usb_to_host_mode(the_chip);
		if (val->intval == POWER_SUPPLY_SCOPE_DEVICE)
			return switch_usb_to_charge_mode(the_chip);
		else
			return -EINVAL;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		return pm8921_set_usb_power_supply_type(val->intval);
	default:
		return -EINVAL;
	}
	return 0;
}

static int pm_power_get_property_usb(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	int current_max;

	/* Check if called before init */
	if (!the_chip)
		return -EINVAL;

	switch (psp) {
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (pm_is_chg_charge_dis(the_chip)) {
			val->intval = 0;
		} else {
			pm_chg_iusbmax_get(the_chip, &current_max);
			val->intval = current_max;
		}
		break;
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = 0;
		if (charging_disabled)
			return 0;

		/*
		 * if drawing any current from usb is disabled behave
		 * as if no usb cable is connected
		 */
		if (pm_is_chg_charge_dis(the_chip))
			return 0;

		/* USB charging */
/*                                                                 */
#ifdef CONFIG_LGE_PM
		if(!is_usb_chg_plugged_in(the_chip))
			val->intval = 0;
		else
			val->intval = usb_online;
#else
		if (usb_target_ma < USB_WALL_THRESHOLD_MA)
			val->intval = is_usb_chg_plugged_in(the_chip);
		else
		    return 0;
#endif
/*                                        */
		break;

	case POWER_SUPPLY_PROP_SCOPE:
		if (the_chip->host_mode)
			val->intval = POWER_SUPPLY_SCOPE_SYSTEM;
		else
			val->intval = POWER_SUPPLY_SCOPE_DEVICE;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static enum power_supply_property msm_batt_power_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_ENERGY_FULL,
/*                                                                      */
#ifdef CONFIG_LGE_PM
	POWER_SUPPLY_PROP_PSEUDO_BATT,
	POWER_SUPPLY_PROP_BLOCK_CHARGING,
	POWER_SUPPLY_PROP_EXT_PWR_CHECK,
#ifdef CONFIG_MACH_LGE_FX3_VZW
	POWER_SUPPLY_PROP_STOP_CHG_UPON_EXPIRY,
#endif
#endif
/* [END] */
#ifdef CONFIG_LGE_PM_BATTERY_ID_CHECKER
	POWER_SUPPLY_PROP_BATTERY_ID_CHECK,
#endif
#ifdef CONFIG_LGE_PM_VZW_FAST_CHG
	POWER_SUPPLY_PROP_VZW_CHG_STATE,
#endif
#if defined(CONFIG_MACH_LGE_L9II_COMMON)

/*                                                                     */
	POWER_SUPPLY_PROP_OFF_BATT,
/*                                                                   */
#endif
};

static int get_prop_battery_uvolts(struct pm8921_chg_chip *chip)
{
	int rc;
	struct pm8xxx_adc_chan_result result;

	rc = pm8xxx_adc_read(chip->vbat_channel, &result);
	if (rc) {
		pr_err("error reading adc channel = %d, rc = %d\n",
					chip->vbat_channel, rc);
		return rc;
	}
	pr_debug("mvolts phy = %lld meas = 0x%llx\n", result.physical,
						result.measurement);
	return (int)result.physical;
}

static unsigned int voltage_based_capacity(struct pm8921_chg_chip *chip)
{
	unsigned int current_voltage_uv = get_prop_battery_uvolts(chip);
	unsigned int current_voltage_mv = current_voltage_uv / 1000;
	unsigned int low_voltage = chip->min_voltage_mv;
	unsigned int high_voltage = chip->max_voltage_mv;

	if (current_voltage_mv <= low_voltage)
		return 0;
	else if (current_voltage_mv >= high_voltage)
		return 100;
	else
		return (current_voltage_mv - low_voltage) * 100
		    / (high_voltage - low_voltage);
}

static int get_prop_batt_present(struct pm8921_chg_chip *chip)
{
#ifdef CONFIG_LGE_PM
	if(pseudo_batt_info.mode == 1)
		return 1;
	else if (is_factory_cable())
		return 1;
#endif

/*                                          
                                                                                  
                                                                               */
#ifdef CONFIG_LGE_PM_BATTERY_ID_CHECKER
	if ((lge_battery_info == BATT_DS2704_N || lge_battery_info == BATT_DS2704_L ||
		lge_battery_info == BATT_ISL6296_N || lge_battery_info == BATT_ISL6296_L ||
		lge_battery_info == BATT_DS2704_C || lge_battery_info == BATT_ISL6296_C ||
	    lge_battery_info == BATT_UNKNOWN) &&
		pm_chg_get_rt_status(chip, BATT_INSERTED_IRQ))
		return 1;
	else
		return 0;
#else
	return pm_chg_get_rt_status(chip, BATT_INSERTED_IRQ);
#endif
/* END : donghyuk79.park 2013-04.23 */
}

static int get_prop_batt_capacity(struct pm8921_chg_chip *chip)
{
	int percent_soc;

	if (!get_prop_batt_present(chip))
		percent_soc = voltage_based_capacity(chip);
	else
		percent_soc = pm8921_bms_get_percent_charge();

	if (percent_soc == -ENXIO)
#ifdef CONFIG_LGE_PM
	{
		pr_debug("[POWER] get_prop_batt_capacity, pm8921-charger, percent_soc == -ENXIO \n");
#endif
		percent_soc = voltage_based_capacity(chip);
#ifdef CONFIG_LGE_PM
	}else{
		pr_debug("[POWER] get_prop_batt_capacity, pm8921-charger, percent_soc == %d \n", percent_soc);
	
	}
#endif
#ifdef CONFIG_LGE_PM
	// Remove useless log
#else
	if (percent_soc <= 10)
		pr_warn_ratelimited("low battery charge = %d%%\n",
						percent_soc);
#endif

	if (chip->recent_reported_soc == (chip->resume_charge_percent + 1)
			&& percent_soc == chip->resume_charge_percent) {
		pr_debug("soc fell below %d. charging enabled.\n",
						chip->resume_charge_percent);
		if (chip->is_bat_warm)
			pr_warn_ratelimited("battery is warm = %d, do not resume charging at %d%%.\n",
					chip->is_bat_warm,
					chip->resume_charge_percent);
		else if (chip->is_bat_cool)
			pr_warn_ratelimited("battery is cool = %d, do not resume charging at %d%%.\n",
					chip->is_bat_cool,
					chip->resume_charge_percent);
		else{
			if (!the_chip) {
				pr_err("called before init\n");
				pm_chg_vbatdet_set(chip, PM8921_CHG_VBATDET_MAX);
			}else{
				pm_chg_vbatdet_set(the_chip, PM8921_CHG_VBATDET_MAX);
			}
		}
	}

	chip->recent_reported_soc = percent_soc;
	return percent_soc;
}

static int get_prop_batt_current_max(struct pm8921_chg_chip *chip)
{
	int rbatt, ibatt_ua, vbatt_uv, ocv_uv;
	int imax_ma;
	int rc;
	pr_debug("[BMS-POWER] before pm8921_bms_get_rbatt\n");
	rbatt = pm8921_bms_get_rbatt();

	if (rbatt < 0) {
		rc = -ENXIO;
		return rc;
	}

	rc =  pm8921_bms_get_simultaneous_battery_voltage_and_current
			(&ibatt_ua, &vbatt_uv);

	if (rc)
		return rc;

	ocv_uv = vbatt_uv + ibatt_ua*rbatt/1000;

	imax_ma = (ocv_uv - chip->min_voltage_mv*1000)/rbatt;

	if (imax_ma < 0)
		imax_ma = 0;

	return imax_ma*1000;

}

static int get_prop_batt_current(struct pm8921_chg_chip *chip)
{
	int result_ua, rc;

	rc = pm8921_bms_get_battery_current(&result_ua);
	if (rc == -ENXIO) {
		rc = pm8xxx_ccadc_get_battery_current(&result_ua);
	}

	if (rc) {
		pr_err("unable to get batt current rc = %d\n", rc);
		return rc;
	} else {
		return result_ua;
	}
}

static int get_prop_batt_fcc(struct pm8921_chg_chip *chip)
{
	int rc;

	rc = pm8921_bms_get_fcc();
	if (rc < 0)
		pr_err("unable to get batt fcc rc = %d\n", rc);
	return rc;
}

static int get_prop_batt_health(struct pm8921_chg_chip *chip)
{
/*                                  
                                                                                                                 */
#if defined(CONFIG_MACH_MSM8960_D1LU) || defined(CONFIG_MACH_MSM8960_D1LSK) || defined(CONFIG_MACH_MSM8960_D1LKT) || defined(CONFIG_MACH_MSM8960_L_DCM) || defined(CONFIG_MACH_MSM8960_L0)
		return batt_health;
#else
/*                                  */

	int temp;

	temp = pm_chg_get_rt_status(chip, BATTTEMP_HOT_IRQ);
	if (temp)
		return POWER_SUPPLY_HEALTH_OVERHEAT;

	temp = pm_chg_get_rt_status(chip, BATTTEMP_COLD_IRQ);
	if (temp)
		return POWER_SUPPLY_HEALTH_COLD;

	return POWER_SUPPLY_HEALTH_GOOD;
#endif

}

static int get_prop_charge_type(struct pm8921_chg_chip *chip)
{
	int temp;

	if (!get_prop_batt_present(chip))
		return POWER_SUPPLY_CHARGE_TYPE_NONE;

	if (is_ext_trickle_charging(chip))
		return POWER_SUPPLY_CHARGE_TYPE_TRICKLE;

	if (is_ext_charging(chip))
		return POWER_SUPPLY_CHARGE_TYPE_FAST;

	temp = pm_chg_get_rt_status(chip, TRKLCHG_IRQ);
	if (temp)
		return POWER_SUPPLY_CHARGE_TYPE_TRICKLE;

	temp = pm_chg_get_rt_status(chip, FASTCHG_IRQ);
	if (temp)
		return POWER_SUPPLY_CHARGE_TYPE_FAST;

	return POWER_SUPPLY_CHARGE_TYPE_NONE;
}

static int get_prop_batt_status(struct pm8921_chg_chip *chip)
{
	int batt_state = POWER_SUPPLY_STATUS_DISCHARGING;
	int fsm_state = pm_chg_get_fsm_state(chip);
	int i;

#ifdef CONFIG_LGE_PM
/*                                                                                            */
	int batt_capacity;
/*                                       */
#endif

	if (chip->ext_psy) {
		if (chip->ext_charge_done)
			return POWER_SUPPLY_STATUS_FULL;
		if (chip->ext_charging)
			return POWER_SUPPLY_STATUS_CHARGING;
	}

#ifdef CONFIG_LGE_PM

#ifdef CONFIG_BATTERY_MAX17043
	batt_capacity = max17043_get_capacity();
#else
	batt_capacity = get_prop_batt_capacity(chip);
#endif
#endif

	for (i = 0; i < ARRAY_SIZE(map); i++)
		if (map[i].fsm_state == fsm_state)
			batt_state = map[i].batt_state;

/*                                                                                            */
#ifdef CONFIG_LGE_PM
	/*                                              
                                   
  */
	if((batt_capacity >= 100) && (is_usb_chg_plugged_in(chip)))
		return POWER_SUPPLY_STATUS_FULL;

/*           
                            
                                
*/
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	if((batt_state == POWER_SUPPLY_STATUS_DISCHARGING ) || (batt_state == POWER_SUPPLY_STATUS_NOT_CHARGING ) || (batt_state == POWER_SUPPLY_STATUS_FULL ))
	{

		if(chip->thermal_mitigation_method == IUSB_USE_FOR_ISYSTEM_METHOD)
		{
			pr_info("xo_mitigation_stop_chg (%d),g_batt_temp (%d)", xo_mitigation_stop_chg, g_batt_temp );
			if(wake_lock_active(&chip->monitor_batt_temp_wake_lock)
			&& (is_usb_chg_plugged_in(chip))
			&&(xo_mitigation_stop_chg == true)
			&&(g_batt_temp < 55)
			&&(g_batt_temp > -10))
			{
				batt_state = POWER_SUPPLY_STATUS_CHARGING;
				return batt_state;
			}

		}
	}
#endif
/*                               */

	if(batt_state == POWER_SUPPLY_STATUS_FULL){
		if( pm_chg_get_rt_status(chip, BATT_INSERTED_IRQ)
			&& is_battery_valid(chip)
#ifndef CONFIG_LGE_PM
			&& pm_chg_get_rt_status(chip, BAT_TEMP_OK_IRQ)
#endif
			&& batt_capacity >= 100 ){
			return batt_state;
		}

#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
		if((chg_batt_temp_state == CHG_BATT_STOP_CHARGING_STATE) && !pseudo_ui_charging)
			return POWER_SUPPLY_STATUS_NOT_CHARGING;
		if((chg_batt_temp_state == CHG_BATT_STOP_CHARGING_STATE) && pseudo_ui_charging)
			return POWER_SUPPLY_STATUS_CHARGING;
#endif


/*                                                                 */
		if(!ac_online && !usb_online){
			batt_state = POWER_SUPPLY_STATUS_DISCHARGING;
			return batt_state;
		}
/*                                        */

		if( pm_chg_get_rt_status(chip, BATT_INSERTED_IRQ)
			&& is_battery_valid(chip)
#ifndef CONFIG_LGE_PM
			&& pm_chg_get_rt_status(chip, BAT_TEMP_OK_IRQ)
#endif
			&& batt_capacity >= 0 && batt_capacity < 100){

			batt_state = POWER_SUPPLY_STATUS_CHARGING;
			return batt_state;
		}
	}
#endif
/*                                      */

	if (fsm_state == FSM_STATE_ON_CHG_HIGHI_1) {
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
		if (!pm_chg_get_rt_status(chip, BATT_INSERTED_IRQ)
#ifndef CONFIG_LGE_PM
			|| !pm_chg_get_rt_status(chip, BAT_TEMP_OK_IRQ)
#endif
			|| pm_chg_get_rt_status(chip, CHGHOT_IRQ)
			|| ((pseudo_ui_charging == 0)
			    && (chg_batt_temp_state == CHG_BATT_STOP_CHARGING_STATE)))

			batt_state = POWER_SUPPLY_STATUS_NOT_CHARGING;
#else
		if (!pm_chg_get_rt_status(chip, BATT_INSERTED_IRQ)
#ifndef CONFIG_LGE_PM
			|| !pm_chg_get_rt_status(chip, BAT_TEMP_OK_IRQ)
#endif
			|| pm_chg_get_rt_status(chip, CHGHOT_IRQ)
			|| pm_chg_get_rt_status(chip, VBATDET_LOW_IRQ))

			batt_state = POWER_SUPPLY_STATUS_NOT_CHARGING;
#endif /* QCT_ORG */
	}
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	if((chg_batt_temp_state == CHG_BATT_STOP_CHARGING_STATE)
		&& (is_usb_chg_plugged_in(chip))
		&& !pseudo_ui_charging
		&& batt_capacity < 100){

		batt_state = POWER_SUPPLY_STATUS_NOT_CHARGING;
		return batt_state;
	}
#endif
	return batt_state;
}

#define MAX_TOLERABLE_BATT_TEMP_DDC	680
static int get_prop_batt_temp(struct pm8921_chg_chip *chip)
{
	int rc;
	struct pm8xxx_adc_chan_result result;

	rc = pm8xxx_adc_read(chip->batt_temp_channel, &result);
	if (rc) {
		pr_err("error reading adc channel = %d, rc = %d\n",
					chip->vbat_channel, rc);
		return rc;
	}
	pr_debug("batt_temp phy = %lld meas = 0x%llx\n", result.physical,
						result.measurement);
	if (result.physical > MAX_TOLERABLE_BATT_TEMP_DDC)
		pr_err("BATT_TEMP= %d > 68degC, device will be shutdown\n",
							(int) result.physical);

	return (int)result.physical;
}

static int pm_batt_power_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct pm8921_chg_chip *chip = container_of(psy, struct pm8921_chg_chip,
								batt_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
/* [START] sungsookim */
#ifdef CONFIG_LGE_PM
		if(block_charging_state == 0)
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		else
#endif
/* [END] */
		val->intval = get_prop_batt_status(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = get_prop_charge_type(chip);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = get_prop_batt_health(chip);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = get_prop_batt_present(chip);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = chip->max_voltage_mv * 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		val->intval = chip->min_voltage_mv * 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
#ifdef CONFIG_LGE_PM
		if(pseudo_batt_info.mode == 1) {
#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS)
			pseudo_batt_info.volt = 3900 * 1000;
			printk("[charger] Fake mode\n");
#endif
			val->intval = pseudo_batt_info.volt;
	/* removed by mansu kk
		else if (is_factory_cable())
			val->intval = 3990 * 1000;
	*/
		}
#ifdef CONFIG_BATTERY_MAX17043
		else
			val->intval = max17043_get_voltage() * 1000;
#else
		else
			val->intval = get_prop_battery_uvolts(chip);
#endif
#else
		val->intval = get_prop_battery_uvolts(chip);
#endif
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
#ifdef CONFIG_LGE_PM
		if(pseudo_batt_info.mode == 1)
			val->intval = pseudo_batt_info.capacity;
#ifdef CONFIG_BATTERY_MAX17043
		else
			val->intval = max17043_get_capacity();
#else
		else
			val->intval = get_prop_batt_capacity(chip);
#endif
#else
		val->intval = get_prop_batt_capacity(chip);
#endif
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = get_prop_batt_current(chip);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = get_prop_batt_current_max(chip);
		break;
/* [START] sungsookim, This code is for testing pseudo_batt_mode, it will be removed soon */
#ifdef CONFIG_LGE_PM
	case POWER_SUPPLY_PROP_TEMP:
#if defined (CONFIG_MACH_LGE_FX3_VZW) || defined (CONFIG_MACH_LGE_F6_TMUS) || defined(CONFIG_MACH_LGE_FX3Q_TMUS) || defined(CONFIG_MACH_LGE_L9II_COMMON) || defined(CONFIG_MACH_LGE_F6_VDF)|| defined(CONFIG_MACH_LGE_FX3_GLOBAL) 
		if(pseudo_batt_info.mode == 1)
			val->intval = pseudo_batt_info.temp * 10;
#else
		/*
		 * block below code by H/W request.
		if(pseudo_batt_info.mode == 1)
			val->intval = pseudo_batt_info.temp * 10;
		*/
		if(0) ;
#endif
		else if (is_factory_cable())
			val->intval = 25 * 10;
		else
			val->intval = get_prop_batt_temp(chip);
		break;
#else
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = get_prop_batt_temp(chip);
		break;
#endif
/* [END] */
	case POWER_SUPPLY_PROP_ENERGY_FULL:
		val->intval = get_prop_batt_fcc(chip) * 1000;
		break;
/* [START] sungsookim */
#ifdef CONFIG_LGE_PM
	case POWER_SUPPLY_PROP_PSEUDO_BATT:
		val->intval = pseudo_batt_info.mode;
		break;
	case POWER_SUPPLY_PROP_BLOCK_CHARGING:
		val->intval = block_charging_state;
		break;
	case POWER_SUPPLY_PROP_EXT_PWR_CHECK:
		val->intval = lge_pm_get_cable_type();
		break;
#ifdef CONFIG_MACH_LGE_FX3_VZW
	case POWER_SUPPLY_PROP_STOP_CHG_UPON_EXPIRY:
		val->intval = stop_chg_upon_expiry_state;
		break;
#endif
#endif
/* [END] */
#ifdef CONFIG_LGE_PM_BATTERY_ID_CHECKER
	case POWER_SUPPLY_PROP_BATTERY_ID_CHECK:
		val->intval = is_battery_valid(chip);
		break;
#endif
#ifdef CONFIG_LGE_PM_VZW_FAST_CHG
	case POWER_SUPPLY_PROP_VZW_CHG_STATE:
		val->intval = chg_state;
		break;
#endif
#if defined(CONFIG_MACH_LGE_L9II_COMMON)
	case POWER_SUPPLY_PROP_OFF_BATT:
		val->intval = off_batt_info.mode;
		break;
#endif
	default:
		return -EINVAL;
	}

	return 0;
}

/* [START] sungsookim */
#ifdef CONFIG_LGE_PM
/*                                                                                     */
#define CHG_BAT_TEMP_DIS_BIT	BIT(2)
/*                          */

int pseudo_batt_set(struct pseudo_batt_info_type* info)
{
	struct pm8921_chg_chip *chip = the_chip;
	int rc;

	pseudo_batt_info.mode = info->mode;
	pseudo_batt_info.id = info->id;
	pseudo_batt_info.therm = info->therm;
	pseudo_batt_info.temp = info->temp;
	pseudo_batt_info.volt = info->volt;
	pseudo_batt_info.capacity = info->capacity;
	pseudo_batt_info.charging = info->charging;

/*                                       
                                                                                       
                                                                                             
                                               
                                                                                                  
 */
	if(pseudo_batt_info.mode)
	{
		rc = pm_chg_masked_write(chip, CHG_CNTRL_2,
		CHG_BAT_TEMP_DIS_BIT, CHG_BAT_TEMP_DIS_BIT);
		if (rc) {
			pr_err("Failed to disable temp control chg rc=%d\n", rc);
		}
	}else	{
		rc = pm_chg_masked_write(chip, CHG_CNTRL_2,
		CHG_BAT_TEMP_DIS_BIT, 0);
		if (rc) {
			pr_err("Failed to enable temp control chg rc=%d\n", rc);
		}
	}
/*                                      */

	power_supply_changed(&chip->batt_psy);
	return 0;
}
EXPORT_SYMBOL(pseudo_batt_set);

#if defined(CONFIG_MACH_LGE_L9II_COMMON)

/*                                                                     */

int off_batt_set(struct off_batt_info_type* info)
{

	off_batt_info.mode = 0;
	printk("off mode : %d\n",off_batt_info.mode);

	return 0;
}
EXPORT_SYMBOL(off_batt_set);


/*                                                                   */
#endif

void block_charging_set(int block)
{
	if(block)
	{
		pm8921_charger_enable(true);
	}
	else
	{
		pm8921_charger_enable(false);
	}
}
void batt_block_charging_set(int block)
{
	struct pm8921_chg_chip *chip = the_chip;

	block_charging_state = block;
	block_charging_set(block);

	power_supply_changed(&chip->batt_psy);
}
EXPORT_SYMBOL(batt_block_charging_set);

#ifdef CONFIG_MACH_LGE_FX3_VZW
void stop_chg_upon_expiry_set(int on)
{
	struct pm8921_chg_chip *chip = the_chip;

	stop_chg_upon_expiry_state = on;

	if(on)
	{
		chip->stop_chg_upon_expiry = 1;
	}
	else
	{
		chip->stop_chg_upon_expiry = 0;
	}

	power_supply_changed(&chip->batt_psy);
}
EXPORT_SYMBOL(stop_chg_upon_expiry_set);
#endif
#endif
/* [END] */

static void (*notify_vbus_state_func_ptr)(int);
static int usb_chg_current;
static DEFINE_SPINLOCK(vbus_lock);

int pm8921_charger_register_vbus_sn(void (*callback)(int))
{
	pr_debug("%p\n", callback);
	notify_vbus_state_func_ptr = callback;
	return 0;
}
EXPORT_SYMBOL_GPL(pm8921_charger_register_vbus_sn);

/* this is passed to the hsusb via platform_data msm_otg_pdata */
void pm8921_charger_unregister_vbus_sn(void (*callback)(int))
{
	pr_debug("%p\n", callback);
	notify_vbus_state_func_ptr = NULL;
}
EXPORT_SYMBOL_GPL(pm8921_charger_unregister_vbus_sn);

static void notify_usb_of_the_plugin_event(int plugin)
{
	plugin = !!plugin;
	if (notify_vbus_state_func_ptr) {
		pr_debug("notifying plugin\n");
		(*notify_vbus_state_func_ptr) (plugin);
	} else {
		pr_debug("unable to notify plugin\n");
	}
}

/* assumes vbus_lock is held */
static void __pm8921_charger_vbus_draw(unsigned int mA)
{
	int i, rc;
	if (!the_chip) {
		pr_err("called before init\n");
		return;
	}
	if (usb_max_current && mA > usb_max_current) {
		pr_debug("restricting usb current to %d instead of %d\n",
					usb_max_current, mA);
		mA = usb_max_current;
	}
	if (mA <= 2) {
		usb_chg_current = 0;
		rc = pm_chg_iusbmax_set(the_chip, 0);
		if (rc) {
			pr_err("unable to set iusb to %d rc = %d\n", 0, rc);
		}
		rc = pm_chg_usb_suspend_enable(the_chip, 1);
		if (rc)
			pr_err("fail to set suspend bit rc=%d\n", rc);
	} else {
		rc = pm_chg_usb_suspend_enable(the_chip, 0);
		if (rc)
			pr_err("fail to reset suspend bit rc=%d\n", rc);
		for (i = ARRAY_SIZE(usb_ma_table) - 1; i >= 0; i--) {
			if (usb_ma_table[i].usb_ma <= mA)
				break;
		}
		/* Check if IUSB_FINE_RES is available */
		while ((usb_ma_table[i].value & PM8917_IUSB_FINE_RES)
				&& !the_chip->iusb_fine_res)
			i--;
		if (i < 0)
			i = 0;
		rc = pm_chg_iusbmax_set(the_chip, i);
		if (rc)
			pr_err("unable to set iusb to %d rc = %d\n", i, rc);
	}
}

/* USB calls these to tell us how much max usb current the system can draw */
void pm8921_charger_vbus_draw(unsigned int mA)
{
	unsigned long flags;
#ifdef CONFIG_LGE_PM_TRKLCHG_IN_KERNEL
	int fsm_state;
#endif

	pr_debug("Enter charge=%d\n", mA);

	if (!the_chip) {
		pr_err("chip not yet initalized\n");
		return;
	}

#ifdef CONFIG_LGE_PM_TRKLCHG_IN_KERNEL
	fsm_state = pm_chg_get_fsm_state(the_chip);

	// set trickle current, TA = 150mA, USB = 200mA
	if (mA > USB_WALL_THRESHOLD_MA)
	{
		pm_chg_itrkl_set(the_chip, 150); // TA = 150mA
		pr_info("[trkl_chg] TA current = 150mA, voltage = 3.2V \n");
	}
	else
	{
		pm_chg_itrkl_set(the_chip, 200); // USB = 200mA
		pm_chg_vweak_set(the_chip, 3000);
		pr_info("[trkl_chg] USB current = 200mA, voltage = 3.0V \n");
	}

	// ignore 100mA to prevent shutdown in USB case
	if (fsm_state == FSM_STATE_TRKL_CHG_8 && mA == 100)
		return;
#endif
	/*
	 * Reject VBUS requests if USB connection is the only available
	 * power source. This makes sure that if booting without
	 * battery the iusb_max value is not decreased avoiding potential
	 * brown_outs.
	 *
	 * This would also apply when the battery has been
	 * removed from the running system.
	 */
/*                                                                      */
#ifndef CONFIG_LGE_PM
	if (!get_prop_batt_present(the_chip)
		&& !is_dc_chg_plugged_in(the_chip)) {
		if (!the_chip->has_dc_supply) {
			pr_err("rejected: no other power source connected\n");
			return;
		}
	}
#endif
/*                                        */

	if (usb_max_current && mA > usb_max_current) {
		pr_warn("restricting usb current to %d instead of %d\n",
					usb_max_current, mA);
		mA = usb_max_current;
	}
#ifdef CONFIG_MACH_MSM8930_FX3
#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS) || defined(CONFIG_MACH_LGE_F6_TMUS) || defined(CONFIG_MACH_LGE_F6_VDF)
	/* Not use AICL for USB */
	if (usb_target_ma == 0 && the_chip->aicl
			&& mA > USB_WALL_THRESHOLD_MA)
#else /* AICL for USB */
	if (usb_target_ma == 0 && the_chip->aicl
			&& mA >= USB_WALL_THRESHOLD_MA)
#endif
#else /* QCT_ORG*/
	if (usb_target_ma == 0 && mA > USB_WALL_THRESHOLD_MA)
#endif
		usb_target_ma = mA;
	spin_lock_irqsave(&vbus_lock, flags);
	if (the_chip) {
#ifdef CONFIG_MACH_MSM8930_FX3
		if (mA > USB_WALL_THRESHOLD_MA && !is_factory_cable())
#else //QCT_ORG
		if (mA > USB_WALL_THRESHOLD_MA)
#endif
			__pm8921_charger_vbus_draw(USB_WALL_THRESHOLD_MA);
		else
			__pm8921_charger_vbus_draw(mA);
	} else {
		/*
		 * called before pmic initialized,
		 * save this value and use it at probe
		 */
#ifdef CONFIG_MACH_MSM8930_FX3
		if (mA > USB_WALL_THRESHOLD_MA && !is_factory_cable())
#else //QCT_ORG
		if (mA > USB_WALL_THRESHOLD_MA)
#endif
			usb_chg_current = USB_WALL_THRESHOLD_MA;
		else
			usb_chg_current = mA;
	}
	spin_unlock_irqrestore(&vbus_lock, flags);
}
EXPORT_SYMBOL_GPL(pm8921_charger_vbus_draw);

int pm8921_charger_enable(bool enable)
{
	int rc;

	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}
	enable = !!enable;
	rc = pm_chg_auto_enable(the_chip, enable);
	if (rc)
		pr_err("Failed rc=%d\n", rc);
	return rc;
}
EXPORT_SYMBOL(pm8921_charger_enable);

int pm8921_is_usb_chg_plugged_in(void)
{
	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}
	return is_usb_chg_plugged_in(the_chip);
}
EXPORT_SYMBOL(pm8921_is_usb_chg_plugged_in);

int pm8921_is_dc_chg_plugged_in(void)
{
	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}
	return is_dc_chg_plugged_in(the_chip);
}
EXPORT_SYMBOL(pm8921_is_dc_chg_plugged_in);

int pm8921_is_battery_present(void)
{
	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}
	return get_prop_batt_present(the_chip);
}
EXPORT_SYMBOL(pm8921_is_battery_present);

int pm8921_is_batfet_closed(void)
{
	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}
	return is_batfet_closed(the_chip);
}
EXPORT_SYMBOL(pm8921_is_batfet_closed);
/*
 * Disabling the charge current limit causes current
 * current limits to have no monitoring. An adequate charger
 * capable of supplying high current while sustaining VIN_MIN
 * is required if the limiting is disabled.
 */
int pm8921_disable_input_current_limit(bool disable)
{
	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}
	if (disable) {
		pr_warn("Disabling input current limit!\n");

		return pm8xxx_writeb(the_chip->dev->parent,
			 CHG_BUCK_CTRL_TEST3, 0xF2);
	}
	return 0;
}
EXPORT_SYMBOL(pm8921_disable_input_current_limit);

int pm8917_set_under_voltage_detection_threshold(int mv)
{
	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}
	return pm_chg_uvd_threshold_set(the_chip, mv);
}
EXPORT_SYMBOL(pm8917_set_under_voltage_detection_threshold);

int pm8921_set_max_battery_charge_current(int ma)
{
	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}
	return pm_chg_ibatmax_set(the_chip, ma);
}
EXPORT_SYMBOL(pm8921_set_max_battery_charge_current);

int pm8921_disable_source_current(bool disable)
{
	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}
	if (disable)
		pr_warn("current drawn from chg=0, battery provides current\n");

	pm_chg_usb_suspend_enable(the_chip, disable);

	return pm_chg_charge_dis(the_chip, disable);
}
EXPORT_SYMBOL(pm8921_disable_source_current);

int pm8921_regulate_input_voltage(int voltage)
{
	int rc;

	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}
	rc = pm_chg_vinmin_set(the_chip, voltage);

	if (rc == 0)
		the_chip->vin_min = voltage;

	return rc;
}

#define USB_OV_THRESHOLD_MASK  0x60
#define USB_OV_THRESHOLD_SHIFT  5
int pm8921_usb_ovp_set_threshold(enum pm8921_usb_ov_threshold ov)
{
	u8 temp;

	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}

	if (ov > PM_USB_OV_7V) {
		pr_err("limiting to over voltage threshold to 7volts\n");
		ov = PM_USB_OV_7V;
	}

	temp = USB_OV_THRESHOLD_MASK & (ov << USB_OV_THRESHOLD_SHIFT);

	return pm_chg_masked_write(the_chip, USB_OVP_CONTROL,
				USB_OV_THRESHOLD_MASK, temp);
}
EXPORT_SYMBOL(pm8921_usb_ovp_set_threshold);

#define USB_DEBOUNCE_TIME_MASK	0x06
#define USB_DEBOUNCE_TIME_SHIFT 1
int pm8921_usb_ovp_set_hystersis(enum pm8921_usb_debounce_time ms)
{
	u8 temp;

	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}

	if (ms > PM_USB_DEBOUNCE_80P5MS) {
		pr_err("limiting debounce to 80.5ms\n");
		ms = PM_USB_DEBOUNCE_80P5MS;
	}

	temp = USB_DEBOUNCE_TIME_MASK & (ms << USB_DEBOUNCE_TIME_SHIFT);

	return pm_chg_masked_write(the_chip, USB_OVP_CONTROL,
				USB_DEBOUNCE_TIME_MASK, temp);
}
EXPORT_SYMBOL(pm8921_usb_ovp_set_hystersis);

#define USB_OVP_DISABLE_MASK	0x80
int pm8921_usb_ovp_disable(int disable)
{
	u8 temp = 0;

	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}

	if (disable)
		temp = USB_OVP_DISABLE_MASK;

	return pm_chg_masked_write(the_chip, USB_OVP_CONTROL,
				USB_OVP_DISABLE_MASK, temp);
}

bool pm8921_is_battery_charging(int *source)
{
	int fsm_state, is_charging, dc_present, usb_present;

	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}
	fsm_state = pm_chg_get_fsm_state(the_chip);
	is_charging = is_battery_charging(fsm_state);
	if (is_charging == 0) {
		*source = PM8921_CHG_SRC_NONE;
		return is_charging;
	}

	if (source == NULL)
		return is_charging;

	/* the battery is charging, the source is requested, find it */
	dc_present = is_dc_chg_plugged_in(the_chip);
	usb_present = is_usb_chg_plugged_in(the_chip);

	if (dc_present && !usb_present)
		*source = PM8921_CHG_SRC_DC;

	if (usb_present && !dc_present)
		*source = PM8921_CHG_SRC_USB;

	if (usb_present && dc_present)
		/*
		 * The system always chooses dc for charging since it has
		 * higher priority.
		 */
		*source = PM8921_CHG_SRC_DC;

	return is_charging;
}
EXPORT_SYMBOL(pm8921_is_battery_charging);

int pm8921_set_usb_power_supply_type(enum power_supply_type type)
{
	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}

	if (type < POWER_SUPPLY_TYPE_USB)
		return -EINVAL;

	the_chip->usb_psy.type = type;
	power_supply_changed(&the_chip->usb_psy);
	power_supply_changed(&the_chip->dc_psy);
	return 0;
}
EXPORT_SYMBOL_GPL(pm8921_set_usb_power_supply_type);

int pm8921_batt_temperature(void)
{
	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}
	return get_prop_batt_temp(the_chip);
}

static int pm8921_charger_enable_batt_alarm(struct pm8921_chg_chip *chip)
{
	int rc = 0;

	rc = pm8xxx_batt_alarm_disable(PM8XXX_BATT_ALARM_UPPER_COMPARATOR);
	if (!rc)
		rc = pm8xxx_batt_alarm_enable(
			PM8XXX_BATT_ALARM_LOWER_COMPARATOR);
	if (rc) {
		pr_err("unable to set batt alarm state rc=%d\n", rc);
		return rc;
	}

	return rc;
}
static int pm8921_charger_configure_batt_alarm(struct pm8921_chg_chip *chip)
{
	int rc = 0;

	rc = pm8xxx_batt_alarm_disable(PM8XXX_BATT_ALARM_UPPER_COMPARATOR);
	if (!rc)
		rc = pm8xxx_batt_alarm_disable(
			PM8XXX_BATT_ALARM_LOWER_COMPARATOR);
	if (rc) {
		pr_err("unable to set batt alarm state rc=%d\n", rc);
		return rc;
	}

	/*
	 * The batt-alarm driver requires sane values for both min / max,
	 * regardless of whether they're both activated.
	 */
	rc = pm8xxx_batt_alarm_threshold_set(
			PM8XXX_BATT_ALARM_LOWER_COMPARATOR,
					chip->alarm_low_mv);
	if (!rc)
		rc = pm8xxx_batt_alarm_threshold_set(
			PM8XXX_BATT_ALARM_UPPER_COMPARATOR,
					chip->alarm_high_mv);
	if (rc) {
		pr_err("unable to set batt alarm threshold rc=%d\n", rc);
		return rc;
	}

	rc = pm8xxx_batt_alarm_hold_time_set(
				PM8XXX_BATT_ALARM_HOLD_TIME_16_MS);
	if (rc) {
		pr_err("unable to set batt alarm hold time rc=%d\n", rc);
		return rc;
	}

	/* PWM enabled at 2Hz */
	rc = pm8xxx_batt_alarm_pwm_rate_set(1, 7, 4);
	if (rc) {
		pr_err("unable to set batt alarm pwm rate rc=%d\n", rc);
		return rc;
	}

	rc = pm8xxx_batt_alarm_register_notifier(&alarm_notifier);
	if (rc) {
		pr_err("unable to register alarm notifier rc=%d\n", rc);
		return rc;
	}

	return rc;
}

static void handle_usb_insertion_removal(struct pm8921_chg_chip *chip)
{
	int usb_present;

#ifdef CONFIG_LGE_PM
	wake_lock_timeout(&uevent_wake_lock, HZ*2);
	pr_info("[POWER] handle_usb_insertion_removal!! \n");
#endif
	pm_chg_failed_clear(chip, 1);
	usb_present = is_usb_chg_plugged_in(chip);
	if (chip->usb_present ^ usb_present) {
		notify_usb_of_the_plugin_event(usb_present);
		chip->usb_present = usb_present;
		power_supply_changed(&chip->usb_psy);
		power_supply_changed(&chip->batt_psy);
#ifdef CONFIG_LGE_PM
		power_supply_changed(&chip->dc_psy);
#endif
		pm8921_bms_calibrate_hkadc();
	}

#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
/*           
                                   
                               
*/
	 if(chip->thermal_mitigation_method == IUSB_USE_FOR_ISYSTEM_METHOD)
	{
		pm_chg_auto_enable(chip,1);
		if(delayed_work_pending(&chip->monitor_batt_temp_work))
		{
			__cancel_delayed_work(&chip->monitor_batt_temp_work);
			schedule_delayed_work(&chip->monitor_batt_temp_work,round_jiffies_relative(msecs_to_jiffies(5000)));
		}
	}
/*                               */

	if (usb_present) {
           /*
                                                                                              
                                                                                                                    
                                                                                      
                                                                                               
                                           
            */
        if(chip->thermal_mitigation_method == IUSB_REDUCE_METHOD)
		{
			pm_chg_auto_enable(chip,1);

			schedule_delayed_work(&chip->monitor_batt_temp_work,round_jiffies_relative(msecs_to_jiffies(5000)));
		}

		last_stop_charging = 0;
		pseudo_ui_charging = 0;
		/*                                                                     */
		pre_xo_mitigation = 0;
		/*                          */
	}
#endif
	if (usb_present) {
#ifdef CONFIG_LGE_PM_VZW_FAST_CHG
		chg_state = VZW_NORMAL_CHARGING;
#endif
		schedule_delayed_work(&chip->unplug_check_work,
			msecs_to_jiffies(UNPLUG_CHECK_RAMP_MS));
#ifdef CONFIG_MACH_MSM8930_FX3
		schedule_delayed_work(&chip->increase_usb_ma_value_work,
			msecs_to_jiffies(UNPLUG_CHECK_RAMP_MS));
#endif
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
		schedule_delayed_work(&chip->monitor_batt_temp_work,
			msecs_to_jiffies(MONITOR_BATT_TEMP_START_MS));
#endif
		pm8921_chg_enable_irq(chip, CHG_GONE_IRQ);
	} else {
#ifdef CONFIG_LGE_PM_VZW_FAST_CHG
		is_finished_chg_state_check = 0;
		vzw_fast_chg_ma = 0;
		chg_state_check_count = 0;
		is_finished_first_rampup = false;
		stepdown_check_before_first_rampup = 0;
		chg_state = VZW_NO_CHARGER;
#endif
#if defined(CONFIG_MACH_LGE_L9II_COMMON)
/*                                                                     */
		if(off_batt_info.mode == 300)
			the_chip->eoc_check = 0;
/*                                                                   */
#endif
		/* USB unplugged reset target current */
		usb_target_ma = 0;
#ifndef CONFIG_MACH_MSM8930_FX3
		pm8921_chg_disable_irq(chip, CHG_GONE_IRQ);
//		power_supply_set_online(&chip->usb_psy, 0);
//		power_supply_set_online(&chip->dc_psy, 0);
#endif
#ifdef CONFIG_MACH_MSM8930_FX3
		cancel_delayed_work_sync(&chip->increase_usb_ma_value_work);
#endif
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
		cancel_delayed_work_sync(&chip->monitor_batt_temp_work);
		wake_unlock(&chip->monitor_batt_temp_wake_lock);
		last_stop_charging = 0;
#endif
	}
#ifdef CONFIG_MACH_MSM8930_FX3
	is_factory_cable();
#endif
	enable_input_voltage_regulation(chip);
	bms_notify_check(chip);
}

static void handle_stop_ext_chg(struct pm8921_chg_chip *chip)
{
	if (!chip->ext_psy) {
		pr_debug("external charger not registered.\n");
		return;
	}

	if (!chip->ext_charging) {
		pr_debug("already not charging.\n");
		return;
	}

	power_supply_set_charge_type(chip->ext_psy,
					POWER_SUPPLY_CHARGE_TYPE_NONE);
	pm8921_disable_source_current(false); /* release BATFET */
	power_supply_changed(&chip->dc_psy);
	chip->ext_charging = false;
	chip->ext_charge_done = false;
	bms_notify_check(chip);
	/* Update battery charging LEDs and user space battery info */
	power_supply_changed(&chip->batt_psy);
}

static void handle_start_ext_chg(struct pm8921_chg_chip *chip)
{
	int dc_present;
	int batt_present;
	int batt_temp_ok;
	int vbat_ov;
	unsigned long delay =
		round_jiffies_relative(msecs_to_jiffies(EOC_CHECK_PERIOD_MS));

	if (!chip->ext_psy) {
		pr_debug("external charger not registered.\n");
		return;
	}

	if (chip->ext_charging) {
		pr_debug("already charging.\n");
		return;
	}

	dc_present = is_dc_chg_plugged_in(the_chip);
	batt_present = pm_chg_get_rt_status(chip, BATT_INSERTED_IRQ);
#ifdef CONFIG_LGE_PM
	batt_temp_ok = 1;
#else
	batt_temp_ok = pm_chg_get_rt_status(chip, BAT_TEMP_OK_IRQ);
#endif

	if (!dc_present) {
		pr_warn("%s. dc not present.\n", __func__);
		return;
	}
	if (!batt_present) {
		pr_warn("%s. battery not present.\n", __func__);
		return;
	}
	if (!batt_temp_ok) {
		pr_warn("%s. battery temperature not ok.\n", __func__);
		return;
	}
	pm8921_disable_source_current(true); /* Force BATFET=ON */
	vbat_ov = pm_chg_get_rt_status(chip, VBAT_OV_IRQ);
	if (vbat_ov) {
		pr_warn("%s. battery over voltage.\n", __func__);
		return;
	}
	schedule_delayed_work(&chip->unplug_check_work,
		msecs_to_jiffies(UNPLUG_CHECK_RAMP_MS));
#ifdef CONFIG_MACH_MSM8930_FX3
	schedule_delayed_work(&chip->increase_usb_ma_value_work,
		msecs_to_jiffies(UNPLUG_CHECK_RAMP_MS));
#endif
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	schedule_delayed_work(&chip->monitor_batt_temp_work,
		msecs_to_jiffies(MONITOR_BATT_TEMP_START_MS));
#endif
	pm8921_chg_enable_irq(chip, CHG_GONE_IRQ);
	power_supply_set_online(chip->ext_psy, dc_present);
	power_supply_set_charge_type(chip->ext_psy,
					POWER_SUPPLY_CHARGE_TYPE_FAST);
	power_supply_changed(&chip->dc_psy);
	chip->ext_charging = true;
	chip->ext_charge_done = false;
	bms_notify_check(chip);
	/* Start BMS */
	schedule_delayed_work(&chip->eoc_work, delay);
	wake_lock(&chip->eoc_wake_lock);
	/* Update battery charging LEDs and user space battery info */
	power_supply_changed(&chip->batt_psy);
}

static void turn_off_ovp_fet(struct pm8921_chg_chip *chip, u16 ovptestreg)
{
	u8 temp;
	int rc;

	rc = pm8xxx_writeb(chip->dev->parent, ovptestreg, 0x30);
	if (rc) {
		pr_err("Failed to write 0x30 to OVP_TEST rc = %d\n", rc);
		return;
	}
	rc = pm8xxx_readb(chip->dev->parent, ovptestreg, &temp);
	if (rc) {
		pr_err("Failed to read from OVP_TEST rc = %d\n", rc);
		return;
	}
	/* set ovp fet disable bit and the write bit */
	temp |= 0x81;
	rc = pm8xxx_writeb(chip->dev->parent, ovptestreg, temp);
	if (rc) {
		pr_err("Failed to write 0x%x OVP_TEST rc=%d\n", temp, rc);
		return;
	}
}

static int pm8921_battery_gauge_alarm_notify(struct notifier_block *nb,
		unsigned long status, void *unused)
{
	int rc;

#ifndef CONFIG_LGE_PM
	pr_info("status: %lu\n", status);
#endif

	/* Check if called before init */

	switch (status) {
	case 0:
		pr_err("spurious interrupt\n");
		break;
	/* expected case - trip of low threshold */
	case 1:
		if (!the_chip) {
			pr_err("not initialized\n");
			return -EINVAL;
		}

		the_chip->disable_hw_clock_switching = 1;

		rc = pm8xxx_batt_alarm_disable(
				PM8XXX_BATT_ALARM_LOWER_COMPARATOR);
		if (!rc)
			rc = pm8xxx_batt_alarm_enable(
				PM8XXX_BATT_ALARM_UPPER_COMPARATOR);
		if (rc)
			pr_err("unable to set alarm state rc=%d\n", rc);
		break;
	case 2:
		if (!the_chip) {
			pr_err("not initialized\n");
			return -EINVAL;
		}

		the_chip->disable_hw_clock_switching = 0;

		rc = pm8xxx_batt_alarm_disable(
				PM8XXX_BATT_ALARM_UPPER_COMPARATOR);
		if (!rc)
			rc = pm8xxx_batt_alarm_enable(
				PM8XXX_BATT_ALARM_LOWER_COMPARATOR);
		if (rc)
			pr_err("unable to set alarm state rc=%d\n", rc);

		pr_err("trip of high threshold\n");
		break;
	default:
		pr_err("error received\n");
	};

	return 0;
}

static void turn_on_ovp_fet(struct pm8921_chg_chip *chip, u16 ovptestreg)
{
	u8 temp;
	int rc;

	rc = pm8xxx_writeb(chip->dev->parent, ovptestreg, 0x30);
	if (rc) {
		pr_err("Failed to write 0x30 to OVP_TEST rc = %d\n", rc);
		return;
	}
	rc = pm8xxx_readb(chip->dev->parent, ovptestreg, &temp);
	if (rc) {
		pr_err("Failed to read from OVP_TEST rc = %d\n", rc);
		return;
	}
	/* unset ovp fet disable bit and set the write bit */
	temp &= 0xFE;
	temp |= 0x80;
	rc = pm8xxx_writeb(chip->dev->parent, ovptestreg, temp);
	if (rc) {
		pr_err("Failed to write 0x%x to OVP_TEST rc = %d\n",
								temp, rc);
		return;
	}
}

static int param_open_ovp_counter = 10;
module_param(param_open_ovp_counter, int, 0644);


#define USB_ACTIVE_BIT BIT(5)
#define DC_ACTIVE_BIT BIT(6)
static int is_active_chg_plugged_in(struct pm8921_chg_chip *chip,
						u8 active_chg_mask)
{
	if (active_chg_mask & USB_ACTIVE_BIT)
		return pm_chg_get_rt_status(chip, USBIN_VALID_IRQ);
	else if (active_chg_mask & DC_ACTIVE_BIT)
		return pm_chg_get_rt_status(chip, DCIN_VALID_IRQ);
	else
		return 0;
}

#define WRITE_BANK_4		0xC0
#define OVP_DEBOUNCE_TIME 0x06
static void unplug_ovp_fet_open(struct pm8921_chg_chip *chip)
{
	int chg_gone = 0, active_chg_plugged_in = 0;
	int count = 0;
	u8 active_mask = 0;
	u16 ovpreg, ovptestreg;

	if (is_usb_chg_plugged_in(chip) &&
		(chip->active_path & USB_ACTIVE_BIT)) {
		ovpreg = USB_OVP_CONTROL;
		ovptestreg = USB_OVP_TEST;
		active_mask = USB_ACTIVE_BIT;
	} else if (is_dc_chg_plugged_in(chip) &&
		(chip->active_path & DC_ACTIVE_BIT)) {
		ovpreg = DC_OVP_CONTROL;
		ovptestreg = DC_OVP_TEST;
		active_mask = DC_ACTIVE_BIT;
	} else {
		return;
	}

	while (count++ < param_open_ovp_counter) {
		pm_chg_masked_write(chip, ovpreg, OVP_DEBOUNCE_TIME, 0x0);
		usleep(10);
		active_chg_plugged_in
			= is_active_chg_plugged_in(chip, active_mask);
		chg_gone = pm_chg_get_rt_status(chip, CHG_GONE_IRQ);
		pr_debug("OVP FET count = %d chg_gone=%d, active_valid = %d\n",
					count, chg_gone, active_chg_plugged_in);

		/* note usb_chg_plugged_in=0 => chg_gone=1 */
		if (chg_gone == 1 && active_chg_plugged_in == 1) {
			pr_debug("since chg_gone = 1 dis ovp_fet for 20msec\n");
			turn_off_ovp_fet(chip, ovptestreg);

			msleep(20);

			turn_on_ovp_fet(chip, ovptestreg);
		} else {
			break;
		}
	}
	pm_chg_masked_write(chip, ovpreg, OVP_DEBOUNCE_TIME, 0x2);
	pr_debug("Exit count=%d chg_gone=%d, active_valid=%d\n",
		count, chg_gone, active_chg_plugged_in);
	return;
}

static int find_usb_ma_value(int value)
{
	int i;

	for (i = ARRAY_SIZE(usb_ma_table) - 1; i >= 0; i--) {
		if (value >= usb_ma_table[i].usb_ma)
			break;
	}

	return i;
}

static void decrease_usb_ma_value(int *value)
{
	int i;

	if (value) {
		i = find_usb_ma_value(*value);
		if (i > 0)
			i--;
		while (!the_chip->iusb_fine_res && i > 0
			&& (usb_ma_table[i].value & PM8917_IUSB_FINE_RES))
			i--;
		*value = usb_ma_table[i].usb_ma;
	}
}

static void increase_usb_ma_value(int *value)
{
	int i;

	if (value) {
		i = find_usb_ma_value(*value);

		if (i < (ARRAY_SIZE(usb_ma_table) - 1))
			i++;
		/* Get next correct entry if IUSB_FINE_RES is not available */
		while (!the_chip->iusb_fine_res
			&& (usb_ma_table[i].value & PM8917_IUSB_FINE_RES)
			&& i < (ARRAY_SIZE(usb_ma_table) - 1))
			i++;

		*value = usb_ma_table[i].usb_ma;
	}
}

static void vin_collapse_check_worker(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct pm8921_chg_chip *chip = container_of(dwork,
			struct pm8921_chg_chip, vin_collapse_check_work);

	is_factory_cable();
	/* AICL only for wall-chargers */
	if (is_usb_chg_plugged_in(chip) &&
		usb_target_ma > USB_WALL_THRESHOLD_MA) {
		/* decrease usb_target_ma */
		decrease_usb_ma_value(&usb_target_ma);
		/* reset here, increase in unplug_check_worker */
		__pm8921_charger_vbus_draw(USB_WALL_THRESHOLD_MA);
		pr_debug("usb_now=%d, usb_target = %d\n",
				USB_WALL_THRESHOLD_MA, usb_target_ma);
		if (!delayed_work_pending(&chip->unplug_check_work))
			schedule_delayed_work(&chip->unplug_check_work,
				      msecs_to_jiffies
						(UNPLUG_CHECK_WAIT_PERIOD_MS));
#ifdef CONFIG_MACH_MSM8930_FX3
		if (!delayed_work_pending(&chip->increase_usb_ma_value_work))
			schedule_delayed_work(&chip->increase_usb_ma_value_work,
				      msecs_to_jiffies
						(UNPLUG_CHECK_WAIT_PERIOD_MS));
#endif
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
		if (!delayed_work_pending(&chip->monitor_batt_temp_work))
			schedule_delayed_work(&chip->monitor_batt_temp_work,
				      msecs_to_jiffies
						(UNPLUG_CHECK_WAIT_PERIOD_MS));
#endif
	} else {
		handle_usb_insertion_removal(chip);
	}
}

#define VIN_MIN_COLLAPSE_CHECK_MS	50
static irqreturn_t usbin_valid_irq_handler(int irq, void *data)
{
	if (usb_target_ma)
		schedule_delayed_work(&the_chip->vin_collapse_check_work,
				      round_jiffies_relative(msecs_to_jiffies
						(VIN_MIN_COLLAPSE_CHECK_MS)));
	else
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
		schedule_delayed_work(&the_chip->vin_collapse_check_work, 0);
#else //QCT_ORG
	    handle_usb_insertion_removal(data);
#endif
	return IRQ_HANDLED;
}

static irqreturn_t batt_inserted_irq_handler(int irq, void *data)
{
	struct pm8921_chg_chip *chip = data;
	int status;

	status = pm_chg_get_rt_status(chip, BATT_INSERTED_IRQ);
	schedule_work(&chip->battery_id_valid_work);
	handle_start_ext_chg(chip);
	pr_debug("battery present=%d", status);
	power_supply_changed(&chip->batt_psy);
	return IRQ_HANDLED;
}

/*
 * this interrupt used to restart charging a battery.
 *
 * Note: When DC-inserted the VBAT can't go low.
 * VPH_PWR is provided by the ext-charger.
 * After End-Of-Charging from DC, charging can be resumed only
 * if DC is removed and then inserted after the battery was in use.
 * Therefore the handle_start_ext_chg() is not called.
 */
static irqreturn_t vbatdet_low_irq_handler(int irq, void *data)
{
	struct pm8921_chg_chip *chip = data;
	int high_transition;

	high_transition = pm_chg_get_rt_status(chip, VBATDET_LOW_IRQ);
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	if((chg_batt_temp_state != CHG_BATT_STOP_CHARGING_STATE) && (lge_power_test_flag != 1)){
#endif

	if (high_transition) {
		/* enable auto charging */
		pm_chg_auto_enable(chip, !charging_disabled);
		pr_info("batt fell below resume voltage %s\n",
			charging_disabled ? "" : "charger enabled");
	}
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	} else {
		pr_debug("batt fell below resume voltage or POWER TEST: charger disabled\n");
	}
#endif
	pr_debug("fsm_state=%d\n", pm_chg_get_fsm_state(data));

	power_supply_changed(&chip->batt_psy);
	power_supply_changed(&chip->usb_psy);
	power_supply_changed(&chip->dc_psy);

	return IRQ_HANDLED;
}

static irqreturn_t vbat_ov_irq_handler(int irq, void *data)
{
	pr_debug("fsm_state=%d\n", pm_chg_get_fsm_state(data));
	return IRQ_HANDLED;
}

static irqreturn_t chgwdog_irq_handler(int irq, void *data)
{
	pr_debug("fsm_state=%d\n", pm_chg_get_fsm_state(data));
	return IRQ_HANDLED;
}

static irqreturn_t vcp_irq_handler(int irq, void *data)
{
	pr_debug("fsm_state=%d\n", pm_chg_get_fsm_state(data));
	return IRQ_HANDLED;
}

static irqreturn_t atcdone_irq_handler(int irq, void *data)
{
	pr_debug("fsm_state=%d\n", pm_chg_get_fsm_state(data));
	return IRQ_HANDLED;
}

static irqreturn_t atcfail_irq_handler(int irq, void *data)
{
	pr_debug("fsm_state=%d\n", pm_chg_get_fsm_state(data));
	return IRQ_HANDLED;
}

static irqreturn_t chgdone_irq_handler(int irq, void *data)
{
	struct pm8921_chg_chip *chip = data;

	pr_debug("state_changed_to=%d\n", pm_chg_get_fsm_state(data));

	handle_stop_ext_chg(chip);

	power_supply_changed(&chip->batt_psy);
	power_supply_changed(&chip->usb_psy);
	power_supply_changed(&chip->dc_psy);

	bms_notify_check(chip);

	return IRQ_HANDLED;
}

static irqreturn_t chgfail_irq_handler(int irq, void *data)
{
	struct pm8921_chg_chip *chip = data;
	int ret;

	if (!chip->stop_chg_upon_expiry) {
		ret = pm_chg_failed_clear(chip, 1);
		if (ret)
			pr_err("Failed to write CHG_FAILED_CLEAR bit\n");
	}
#ifdef CONFIG_LGE_PM
	else
	{
		pr_info("[PM] Safety timer was expired. charging will be stopped!! \n");
	}
#endif

	pr_err("batt_present = %d, batt_temp_ok = %d, state_changed_to=%d\n",
			get_prop_batt_present(chip),
#ifdef CONFIG_LGE_PM
			1,
#else
			pm_chg_get_rt_status(chip, BAT_TEMP_OK_IRQ),
#endif
			pm_chg_get_fsm_state(data));

	power_supply_changed(&chip->batt_psy);
	power_supply_changed(&chip->usb_psy);
	power_supply_changed(&chip->dc_psy);
	return IRQ_HANDLED;
}

static irqreturn_t chgstate_irq_handler(int irq, void *data)
{
	struct pm8921_chg_chip *chip = data;

	pr_debug("state_changed_to=%d\n", pm_chg_get_fsm_state(data));
	power_supply_changed(&chip->batt_psy);
	power_supply_changed(&chip->usb_psy);
	power_supply_changed(&chip->dc_psy);

	bms_notify_check(chip);

	return IRQ_HANDLED;
}

enum {
	PON_TIME_25NS	= 0x04,
	PON_TIME_50NS	= 0x08,
	PON_TIME_100NS	= 0x0C,
};

static void set_min_pon_time(struct pm8921_chg_chip *chip, int pon_time_ns)
{
	u8 temp;
	int rc;

	rc = pm8xxx_writeb(chip->dev->parent, CHG_BUCK_CTRL_TEST3, 0x40);
	if (rc) {
		pr_err("Failed to write 0x70 to CTRL_TEST3 rc = %d\n", rc);
		return;
	}
	rc = pm8xxx_readb(chip->dev->parent, CHG_BUCK_CTRL_TEST3, &temp);
	if (rc) {
		pr_err("Failed to read CTRL_TEST3 rc = %d\n", rc);
		return;
	}
	/* clear the min pon time select bit */
	temp &= 0xF3;
	/* set the pon time */
	temp |= (u8)pon_time_ns;
	/* write enable bank 4 */
	temp |= 0x80;
	rc = pm8xxx_writeb(chip->dev->parent, CHG_BUCK_CTRL_TEST3, temp);
	if (rc) {
		pr_err("Failed to write 0x%x to CTRL_TEST3 rc=%d\n", temp, rc);
		return;
	}
}

static void attempt_reverse_boost_fix(struct pm8921_chg_chip *chip)
{
	pr_debug("Start\n");
	set_min_pon_time(chip, PON_TIME_100NS);
	pm_chg_vinmin_set(chip, chip->vin_min + 200);
	msleep(250);
	pm_chg_vinmin_set(chip, chip->vin_min);
	set_min_pon_time(chip, PON_TIME_25NS);
	pr_debug("End\n");
}


#ifdef CONFIG_LGE_PM_VZW_FAST_CHG
#define VZW_CHG_RATED_CURRENT 850	// 1100 => 900(130604) =>850(130605)
#define VZW_CHG_MIN_CURRENT 500
#define VZW_CHG_USB_DRIVER_UNINSTALLED -500
void set_vzw_charging_state(void)
{
	//int usb_ma;

	//pm_chg_iusbmax_get(chip, &usb_ma);

	// usb configuration is failed, impossible to charge
	if (vzw_fast_chg_ma == VZW_CHG_USB_DRIVER_UNINSTALLED) {
		chg_state = VZW_USB_DRIVER_UNINSTALLED;
	}
	// incompatible charger, impossible to charge
	else if (vzw_fast_chg_ma >= 0 && vzw_fast_chg_ma < VZW_CHG_MIN_CURRENT) {
		chg_state = VZW_NOT_CHARGING;
		// disable charging ??
	}
	// incompatible charger, but possible to charge in under current
	else if (vzw_fast_chg_ma >= VZW_CHG_MIN_CURRENT && vzw_fast_chg_ma < VZW_CHG_RATED_CURRENT) {
		chg_state = VZW_UNDER_CURRENT_CHARGING;
	}
	// compatible charger, possible to charge in rated current
	else if (vzw_fast_chg_ma >= VZW_CHG_RATED_CURRENT) {
		chg_state = VZW_NORMAL_CHARGING;
	}
	else {
		chg_state = VZW_NO_CHARGER;
		pr_err("[AICL] Can's set VZW_CHG_STATE, vzw_fast_chg_ma = %d \n", vzw_fast_chg_ma);
		return;
	}

	pr_info("[AICL] Set VZW_CHG_STATE = %d :: 0=NO_CHAGER, 1=NORMAL, 2=NOT, 3=UNDER_CURRENT :: \n", chg_state);

	power_supply_changed(&the_chip->batt_psy);
	return;
}
EXPORT_SYMBOL(set_vzw_charging_state);
#endif

#define VIN_ACTIVE_BIT BIT(0)
#define UNPLUG_WRKARND_RESTORE_WAIT_PERIOD_US	200
#define VIN_MIN_INCREASE_MV	100
static void unplug_check_worker(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct pm8921_chg_chip *chip = container_of(dwork,
				struct pm8921_chg_chip, unplug_check_work);
	u8 reg_loop, active_path;
	int rc, ibat, active_chg_plugged_in, usb_ma;
	int chg_gone = 0;
	bool ramp = false;

#ifdef CONFIG_LGE_PM_VZW_FAST_CHG
	int fsm_state = 0;
	struct pm8xxx_adc_chan_result Vin_now;
#endif

	reg_loop = 0;

	rc = pm8xxx_readb(chip->dev->parent, PBL_ACCESS1, &active_path);
	if (rc) {
		pr_err("Failed to read PBL_ACCESS1 rc=%d\n", rc);
		return;
	}
	chip->active_path = active_path;

	active_chg_plugged_in = is_active_chg_plugged_in(chip, active_path);
	pr_debug("active_path = 0x%x, active_chg_plugged_in = %d\n",
			active_path, active_chg_plugged_in);
	if (active_path & USB_ACTIVE_BIT) {
		pr_debug("USB charger active\n");
#ifdef CONFIG_MACH_MSM8930_FX3
		if (!chip->aicl) {
			if (usb_online) {
				usb_ma = lge_pm_get_usb_current();
				if (!is_factory_cable()) {
					usb_target_ma = 0;
				} else {
					usb_target_ma = usb_ma;
				}
			} else {
				usb_ma = lge_pm_get_ta_current();
				usb_target_ma = usb_ma;
			}
		}
#endif

		pm_chg_iusbmax_get(chip, &usb_ma);
#ifndef CONFIG_LGE_PM_VZW_FAST_CHG
/*		if (usb_ma == 500 && !usb_target_ma) {
			pr_debug("Stopping Unplug Check Worker USB == 500mA\n");
			disable_input_voltage_regulation(chip);
			return;
		}
*/
#endif
		if (usb_ma <= 100) {
#ifdef CONFIG_LGE_PM_VZW_FAST_CHG
			if(usb_target_ma && !is_finished_chg_state_check) {
				pr_info("[AICL] Step Down to 100mA !! \n");
				vzw_fast_chg_ma = 100;
				set_vzw_charging_state();
				pr_info("[AICL] Determine vzw charging state, max(target) = %d\n", vzw_fast_chg_ma);
				is_finished_chg_state_check = 1;
			}
#endif
			pr_debug(
				"Unenumerated or suspended usb_ma = %d skip\n",
				usb_ma);
			goto check_again_later;
		}
	} else if (active_path & DC_ACTIVE_BIT) {
		pr_debug("DC charger active\n");
		/* Some board designs are not prone to reverse boost on DC
		 * charging path */

#ifndef CONFIG_LGE_PM
		if (!chip->dc_unplug_check)
			return;
#endif
	} else {
		/* No charger active */
		if (!(is_usb_chg_plugged_in(chip)
				&& !(is_dc_chg_plugged_in(chip)))) {
			pr_debug(
			"Stop: chg removed reg_loop = %d, fsm = %d ibat = %d\n",
				pm_chg_get_regulation_loop(chip),
				pm_chg_get_fsm_state(chip),
				get_prop_batt_current(chip)
				);
			return;
		} else {
			goto check_again_later;
		}
	}
	/* AICL only for usb path and that too only for usb wall chargers */
	if ((active_path & USB_ACTIVE_BIT) && usb_target_ma > 0) {
		reg_loop = pm_chg_get_regulation_loop(chip);
		pr_debug("reg_loop=0x%x usb_ma = %d\n", reg_loop, usb_ma);
		if ((reg_loop & VIN_ACTIVE_BIT)
#ifndef CONFIG_LGE_PM_VZW_FAST_CHG
			&& (usb_ma > USB_WALL_THRESHOLD_MA)
#endif
#ifdef CONFIG_MACH_MSM8930_FX3
			&& chip->aicl
#endif
			&& !charging_disabled) {
#ifdef CONFIG_LGE_PM_VZW_FAST_CHG
			pm8xxx_adc_read(CHANNEL_USBIN, &Vin_now);
			pr_info("[AICL][Step Down] Vin_min = %d Vin = %lld \n", chip->vin_min*1000, Vin_now.physical);
			pr_info("[AICL][Step Down] before = %d\n", usb_ma);
			if(!is_finished_first_rampup) {
				stepdown_check_before_first_rampup++;
				pr_info("[AICL][Step Down] is_finished_first_rampup = %d, stepdown_check_before_first_rampup = %d\n", is_finished_first_rampup, stepdown_check_before_first_rampup);
				if(stepdown_check_before_first_rampup > 5) {
#endif
			decrease_usb_ma_value(&usb_ma);
			usb_target_ma = usb_ma;
			/* end AICL here */
			__pm8921_charger_vbus_draw(usb_ma);
#ifdef CONFIG_LGE_PM_VZW_FAST_CHG
				}
			} else {
				decrease_usb_ma_value(&usb_ma);
				usb_target_ma = usb_ma;
				/* end AICL here */
				__pm8921_charger_vbus_draw(usb_ma);
				stepdown_check_before_first_rampup = 0;
			}
			pr_info("[AICL][Step Down] after = %d\n", usb_ma);
			pr_info("[AICL][Step Down] max(target) = %d\n", usb_target_ma);
#endif
			pr_debug("usb_now=%d, usb_target = %d\n",
				usb_ma, usb_target_ma);
		}
	}

	reg_loop = pm_chg_get_regulation_loop(chip);
	pr_debug("reg_loop=0x%x usb_ma = %d\n", reg_loop, usb_ma);

	ibat = get_prop_batt_current(chip);
	if (reg_loop & VIN_ACTIVE_BIT) {
		pr_debug("ibat = %d fsm = %d reg_loop = 0x%x\n",
				ibat, pm_chg_get_fsm_state(chip), reg_loop);
		if (ibat > 0) {
			pr_debug("revboost ibat = %d fsm = %d loop = 0x%x\n",
				ibat, pm_chg_get_fsm_state(chip), reg_loop);
			attempt_reverse_boost_fix(chip);
			/* after reverse boost fix check if the active
			 * charger was detected as removed */
			active_chg_plugged_in
				= is_active_chg_plugged_in(chip,
					active_path);
			pr_debug("revboost post: active_chg_plugged_in = %d\n",
					active_chg_plugged_in);
		}
	}

	active_chg_plugged_in = is_active_chg_plugged_in(chip, active_path);
	pr_debug("active_path = 0x%x, active_chg = %d\n",
			active_path, active_chg_plugged_in);
	chg_gone = pm_chg_get_rt_status(chip, CHG_GONE_IRQ);

	if (chg_gone == 1  && active_chg_plugged_in == 1) {
		pr_debug("chg_gone=%d, active_chg_plugged_in = %d\n",
					chg_gone, active_chg_plugged_in);
		unplug_ovp_fet_open(chip);
	}

	/* AICL only for usb path and that too only for usb wall chargers */
	if (!(reg_loop & VIN_ACTIVE_BIT) && (active_path & USB_ACTIVE_BIT)
		&& usb_target_ma > 0
		&& !charging_disabled) {
		/* only increase iusb_max if vin loop not active */
#ifdef CONFIG_MACH_MSM8930_FX3 //                                                 
		if (usb_ma < usb_target_ma
			&& usb_ma < lge_pm_get_ta_current()) {
#else //QCT_ORG
		if (usb_ma < usb_target_ma) {
#endif
#ifdef CONFIG_LGE_PM_VZW_FAST_CHG
			pm8xxx_adc_read(CHANNEL_USBIN, &Vin_now);
			pr_info("[AICL][Step Up] Vin_min = %d Vin = %lld \n", chip->vin_min*1000, Vin_now.physical);
			pr_info("[AICL][Step Up] before = %d\n", usb_ma);
#endif
			increase_usb_ma_value(&usb_ma);
			__pm8921_charger_vbus_draw(usb_ma);
#ifdef CONFIG_LGE_PM_VZW_FAST_CHG
			is_finished_first_rampup = true;
			pr_info("[AICL][Step Up] after = %d\n", usb_ma);
			pr_info("[AICL][Step Up] max(target) = %d\n", usb_target_ma);
#endif
			pr_debug("usb_now=%d, usb_target = %d\n",
					usb_ma, usb_target_ma);
			ramp = true;
		} else {
			usb_target_ma = usb_ma;
		}
	}

#ifdef CONFIG_LGE_PM_VZW_FAST_CHG
	if(!is_finished_chg_state_check && !charging_disabled && chip->aicl
		&& usb_target_ma > 0 && vzw_fast_chg_ma && is_finished_first_rampup) {
		fsm_state = pm_chg_get_fsm_state(chip);
		pr_info("[AICL] fsm_state=%d \n", fsm_state);

		if(fsm_state != FSM_STATE_TRKL_CHG_8)
		{
			//chg_state_check_count++;
			//pr_info("[AICL] chg_state_check_count = %d\n", chg_state_check_count);
			//if(chg_state_check_count > 20) {
				//pm_chg_iusbmax_get(chip, &usb_ma);
				set_vzw_charging_state();
				pr_info("[AICL] Determine vzw charging state, max(target) = %d\n", vzw_fast_chg_ma);
				//chg_state_check_count = 0;
				is_finished_chg_state_check = 1;
		}
	}
#endif
check_again_later:
	pr_debug("ramp: %d\n", ramp);
	/* schedule to check again later */
	if (ramp)
		schedule_delayed_work(&chip->unplug_check_work,
			msecs_to_jiffies(UNPLUG_CHECK_RAMP_MS));
	else
		schedule_delayed_work(&chip->unplug_check_work,
			msecs_to_jiffies(UNPLUG_CHECK_WAIT_PERIOD_MS));
}

static irqreturn_t loop_change_irq_handler(int irq, void *data)
{
	struct pm8921_chg_chip *chip = data;

	pr_debug("fsm_state=%d reg_loop=0x%x\n",
		pm_chg_get_fsm_state(data),
		pm_chg_get_regulation_loop(data));
	schedule_work(&chip->unplug_check_work.work);
#ifdef CONFIG_MACH_MSM8930_FX3
	schedule_work(&chip->increase_usb_ma_value_work.work);
#endif
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	schedule_work(&chip->monitor_batt_temp_work.work);
#endif
	return IRQ_HANDLED;
}

static irqreturn_t fastchg_irq_handler(int irq, void *data)
{
	struct pm8921_chg_chip *chip = data;
	int high_transition;

	high_transition = pm_chg_get_rt_status(chip, FASTCHG_IRQ);
#ifdef CONFIG_LGE_PM
	pr_info("fastchg_irq : high_transition = %d", high_transition);
#endif
	if (high_transition && !delayed_work_pending(&chip->eoc_work)) {

#if defined(CONFIG_MACH_LGE_L9II_COMMON)
		if(off_batt_info.mode !=300)
		{	
			wake_lock(&chip->eoc_wake_lock);
		}else
			chip->eoc_check = 0;
#else

			wake_lock(&chip->eoc_wake_lock);
#endif

#ifdef CONFIG_LGE_PM
		pr_info("fastchg_irq : eoc_work start ");
#endif
		schedule_delayed_work(&chip->eoc_work,
				      round_jiffies_relative(msecs_to_jiffies
						     (EOC_CHECK_PERIOD_MS)));
	}
	power_supply_changed(&chip->batt_psy);
	bms_notify_check(chip);
	return IRQ_HANDLED;
}

static irqreturn_t trklchg_irq_handler(int irq, void *data)
{
	struct pm8921_chg_chip *chip = data;

	power_supply_changed(&chip->batt_psy);
	return IRQ_HANDLED;
}

static irqreturn_t batt_removed_irq_handler(int irq, void *data)
{
	struct pm8921_chg_chip *chip = data;
	int status;

#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	if(is_usb_chg_plugged_in(chip)
		&& !is_factory_cable() && !pseudo_batt_info.mode){
		pr_info("[PM] Battery removed but cable exists. Now reset as scenario !!\n");
/*                                                                      
                               
*/
		machine_restart("battery"); 
	}
#endif

	status = pm_chg_get_rt_status(chip, BATT_REMOVED_IRQ);
	pr_debug("battery present=%d state=%d", !status,
					 pm_chg_get_fsm_state(data));
	handle_stop_ext_chg(chip);
	power_supply_changed(&chip->batt_psy);
	return IRQ_HANDLED;
}

static irqreturn_t batttemp_hot_irq_handler(int irq, void *data)
{
	struct pm8921_chg_chip *chip = data;

	handle_stop_ext_chg(chip);
	power_supply_changed(&chip->batt_psy);
	return IRQ_HANDLED;
}

static irqreturn_t chghot_irq_handler(int irq, void *data)
{
	struct pm8921_chg_chip *chip = data;

	pr_debug("Chg hot fsm_state=%d\n", pm_chg_get_fsm_state(data));
	power_supply_changed(&chip->batt_psy);
	power_supply_changed(&chip->usb_psy);
	handle_stop_ext_chg(chip);
	return IRQ_HANDLED;
}

static irqreturn_t batttemp_cold_irq_handler(int irq, void *data)
{
	struct pm8921_chg_chip *chip = data;

	pr_debug("Batt cold fsm_state=%d\n", pm_chg_get_fsm_state(data));
	handle_stop_ext_chg(chip);

	power_supply_changed(&chip->batt_psy);
	power_supply_changed(&chip->usb_psy);
	return IRQ_HANDLED;
}

static irqreturn_t chg_gone_irq_handler(int irq, void *data)
{
	struct pm8921_chg_chip *chip = data;
	int chg_gone, usb_chg_plugged_in;

	usb_chg_plugged_in = is_usb_chg_plugged_in(chip);
	chg_gone = pm_chg_get_rt_status(chip, CHG_GONE_IRQ);

	pr_debug("chg_gone=%d, usb_valid = %d\n", chg_gone, usb_chg_plugged_in);
	pr_debug("Chg gone fsm_state=%d\n", pm_chg_get_fsm_state(data));

	power_supply_changed(&chip->batt_psy);
	power_supply_changed(&chip->usb_psy);
	return IRQ_HANDLED;
}
#ifndef CONFIG_LGE_PM
/*
 *
 * bat_temp_ok_irq_handler - is edge triggered, hence it will
 * fire for two cases:
 *
 * If the interrupt line switches to high temperature is okay
 * and thus charging begins.
 * If bat_temp_ok is low this means the temperature is now
 * too hot or cold, so charging is stopped.
 *
 */
static irqreturn_t bat_temp_ok_irq_handler(int irq, void *data)
{
	int bat_temp_ok;
	struct pm8921_chg_chip *chip = data;

	bat_temp_ok = pm_chg_get_rt_status(chip, BAT_TEMP_OK_IRQ);

	pr_info("batt_temp_ok = %d fsm_state%d\n",
			 bat_temp_ok, pm_chg_get_fsm_state(data));

	if (bat_temp_ok)
		handle_start_ext_chg(chip);
	else
		handle_stop_ext_chg(chip);

	power_supply_changed(&chip->batt_psy);
	power_supply_changed(&chip->usb_psy);
	bms_notify_check(chip);
	return IRQ_HANDLED;
}
#endif

static irqreturn_t coarse_det_low_irq_handler(int irq, void *data)
{
	pr_debug("fsm_state=%d\n", pm_chg_get_fsm_state(data));
	return IRQ_HANDLED;
}

static irqreturn_t vdd_loop_irq_handler(int irq, void *data)
{
	pr_debug("fsm_state=%d\n", pm_chg_get_fsm_state(data));
	return IRQ_HANDLED;
}

static irqreturn_t vreg_ov_irq_handler(int irq, void *data)
{
	pr_debug("fsm_state=%d\n", pm_chg_get_fsm_state(data));
	return IRQ_HANDLED;
}

static irqreturn_t vbatdet_irq_handler(int irq, void *data)
{
	pr_debug("fsm_state=%d\n", pm_chg_get_fsm_state(data));
	return IRQ_HANDLED;
}

static irqreturn_t batfet_irq_handler(int irq, void *data)
{
	struct pm8921_chg_chip *chip = data;

	pr_debug("vreg ov\n");
	power_supply_changed(&chip->batt_psy);
	return IRQ_HANDLED;
}

static irqreturn_t dcin_valid_irq_handler(int irq, void *data)
{
	struct pm8921_chg_chip *chip = data;
	int dc_present;

	dc_present = pm_chg_get_rt_status(chip, DCIN_VALID_IRQ);
	if (chip->ext_psy)
		power_supply_set_online(chip->ext_psy, dc_present);
	chip->dc_present = dc_present;
	if (dc_present)
		handle_start_ext_chg(chip);
	else
		handle_stop_ext_chg(chip);
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	if (dc_present) {
		last_stop_charging = 0;
		pseudo_ui_charging = 0;
		/*                                                                     */
		pre_xo_mitigation  = 0;
		/*                         */
	}
#endif
	return IRQ_HANDLED;
}

static irqreturn_t dcin_ov_irq_handler(int irq, void *data)
{
	struct pm8921_chg_chip *chip = data;

	handle_stop_ext_chg(chip);
	return IRQ_HANDLED;
}

static irqreturn_t dcin_uv_irq_handler(int irq, void *data)
{
	struct pm8921_chg_chip *chip = data;

	handle_stop_ext_chg(chip);

	return IRQ_HANDLED;
}

#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
/*                                                      
                                                                     
                                                                            
                                                                             
                      
 */
struct charging_status {
	enum chg_status_enum	chg_status;
	int						pseudo_battery;
};
#ifdef CONFIG_MACH_LGE_FX3_VZW
static struct charging_status normal_charging[] = {
	//over 4V
	{CHG_BATT_STOP_CHARGING_STATE,	0}, //55 over
	{CHG_BATT_STOP_CHARGING_STATE,	1}, //53 over
	{CHG_BATT_STOP_CHARGING_STATE,	1}, //45 over
	{CHG_BATT_NORMAL_STATE,			0}, //42 over
	{CHG_BATT_NORMAL_STATE,			0}, //3 over
	{CHG_BATT_NORMAL_STATE,			0}, //1 over
	{CHG_BATT_STOP_CHARGING_STATE,	0}, //1 under
	//under 4V
	{CHG_BATT_STOP_CHARGING_STATE,	0},	//55 over
	{CHG_BATT_DC_CURRENT_STATE,		0}, //53 over
	{CHG_BATT_DC_CURRENT_STATE,		0}, //45 over
	{CHG_BATT_NORMAL_STATE,			0}, //42 over
	{CHG_BATT_NORMAL_STATE,			0}, //3 over
	{CHG_BATT_NORMAL_STATE,			0}, //1 over
	{CHG_BATT_STOP_CHARGING_STATE,	0}, //1 under
};
static struct charging_status decrease_charging[] = {
	//over 4V
	{CHG_BATT_STOP_CHARGING_STATE,	0}, //55 over
	{CHG_BATT_STOP_CHARGING_STATE,	1}, //53 over
	{CHG_BATT_STOP_CHARGING_STATE,	1}, //45 over
	{CHG_BATT_STOP_CHARGING_STATE,	1}, //42 over
	{CHG_BATT_NORMAL_STATE,			0}, //3 over
	{CHG_BATT_NORMAL_STATE,			0}, //1 over
	{CHG_BATT_STOP_CHARGING_STATE,	0}, //1 under
	//under 4V
	{CHG_BATT_STOP_CHARGING_STATE,	0},	//55 over
	{CHG_BATT_DC_CURRENT_STATE,		0}, //53 over
	{CHG_BATT_DC_CURRENT_STATE,		0}, //45 over
	{CHG_BATT_DC_CURRENT_STATE,		0}, //42 over
	{CHG_BATT_NORMAL_STATE,			0}, //3 over
	{CHG_BATT_NORMAL_STATE,			0}, //1 over
	{CHG_BATT_STOP_CHARGING_STATE,	0}, //1 under
};
static struct charging_status stop_charging[] = {
	//over 4V
	{CHG_BATT_STOP_CHARGING_STATE,	0}, //55 over
	{CHG_BATT_STOP_CHARGING_STATE,	1}, //53 over
	{CHG_BATT_STOP_CHARGING_STATE,	1}, //45 over
	{CHG_BATT_STOP_CHARGING_STATE,	1}, //42 over
	{CHG_BATT_NORMAL_STATE,			0}, //3 over
	{CHG_BATT_STOP_CHARGING_STATE,	0}, //1 over
	{CHG_BATT_STOP_CHARGING_STATE,	0}, //1 under
	//under 4V
	{CHG_BATT_STOP_CHARGING_STATE,	0},	//55 over
	{CHG_BATT_STOP_CHARGING_STATE,	1}, //53 over
	{CHG_BATT_STOP_CHARGING_STATE,	1}, //45 over
	{CHG_BATT_STOP_CHARGING_STATE,	1}, //42 over
	{CHG_BATT_NORMAL_STATE,			0}, //3 over
	{CHG_BATT_STOP_CHARGING_STATE,	0}, //1 over
	{CHG_BATT_STOP_CHARGING_STATE,	0}, //1 under
};
#else
static struct charging_status normal_charging[] = {
	//over 4V
	{CHG_BATT_STOP_CHARGING_STATE,	0}, //55 over
	{CHG_BATT_STOP_CHARGING_STATE,	1}, //53 over
	{CHG_BATT_STOP_CHARGING_STATE,	1}, //45 over
	{CHG_BATT_NORMAL_STATE,			0}, //42 over
	{CHG_BATT_NORMAL_STATE,			0}, //3 over
	{CHG_BATT_NORMAL_STATE,			0}, //1 over
	{CHG_BATT_STOP_CHARGING_STATE,	1}, //1 under
	//under 4V
	{CHG_BATT_STOP_CHARGING_STATE,	0},	//55 over
	{CHG_BATT_DC_CURRENT_STATE,		0}, //53 over
	{CHG_BATT_DC_CURRENT_STATE,		0}, //45 over
	{CHG_BATT_NORMAL_STATE,			0}, //42 over
	{CHG_BATT_NORMAL_STATE,			0}, //3 over
	{CHG_BATT_NORMAL_STATE,			0}, //1 over
	{CHG_BATT_STOP_CHARGING_STATE,	1}, //1 under
};
static struct charging_status decrease_charging[] = {
	//over 4V
	{CHG_BATT_STOP_CHARGING_STATE,	0}, //55 over
	{CHG_BATT_STOP_CHARGING_STATE,	1}, //53 over
	{CHG_BATT_STOP_CHARGING_STATE,	1}, //45 over
	{CHG_BATT_STOP_CHARGING_STATE,	1}, //42 over
	{CHG_BATT_NORMAL_STATE,			0}, //3 over
	{CHG_BATT_NORMAL_STATE,			0}, //1 over
	{CHG_BATT_STOP_CHARGING_STATE,	1}, //1 under
	//under 4V
	{CHG_BATT_STOP_CHARGING_STATE,	0},	//55 over
	{CHG_BATT_DC_CURRENT_STATE,		0}, //53 over
	{CHG_BATT_DC_CURRENT_STATE,		0}, //45 over
	{CHG_BATT_DC_CURRENT_STATE,		0}, //42 over
	{CHG_BATT_NORMAL_STATE,			0}, //3 over
	{CHG_BATT_NORMAL_STATE,			0}, //1 over
	{CHG_BATT_STOP_CHARGING_STATE,	1}, //1 under
};
static struct charging_status stop_charging[] = {
	//over 4V
	{CHG_BATT_STOP_CHARGING_STATE,	0}, //55 over
	{CHG_BATT_STOP_CHARGING_STATE,	1}, //53 over
	{CHG_BATT_STOP_CHARGING_STATE,	1}, //45 over
	{CHG_BATT_STOP_CHARGING_STATE,	1}, //42 over
	{CHG_BATT_NORMAL_STATE,			0}, //3 over
	{CHG_BATT_STOP_CHARGING_STATE,	1}, //1 over
	{CHG_BATT_STOP_CHARGING_STATE,	1}, //1 under
	//under 4V
	{CHG_BATT_STOP_CHARGING_STATE,	0},	//55 over
	{CHG_BATT_STOP_CHARGING_STATE,	1}, //53 over
	{CHG_BATT_STOP_CHARGING_STATE,	1}, //45 over
	{CHG_BATT_STOP_CHARGING_STATE,	1}, //42 over
	{CHG_BATT_NORMAL_STATE,			0}, //3 over
	{CHG_BATT_STOP_CHARGING_STATE,	1}, //1 over
	{CHG_BATT_STOP_CHARGING_STATE,	1}, //1 under
};
#endif

int batt_temp_threshold[] = { 550, 530, 450, 420, -50, -80, -1000};
	// Value is multiplied by 10. Do not remove last -1000. It is for index.
int batt_volt_threshold[] = { 4000000, 0};
	// unit: uV. Do not remove last 0. It is for index.
#if !defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS)
int batt_temp_threshold_pseudo[] = { 600, 580, 450, 420, -50, -80, -1000};
	// Value is multiplied by 10. Do not remove last -1000. It is for index.
int batt_volt_threshold_pseudo[] = { 4000000, 0};
	// unit: uV. Do not remove last 0. It is for index.
#endif
static int battery_status(int batt_temp, int batt_volt)
{
	int i, j;
	int *ptr_temp, *ptr_volt;
	int size_temp, size_volt;
	if (pseudo_batt_info.mode) {
#if !defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS)
		ptr_temp = batt_temp_threshold_pseudo;
		ptr_volt = batt_volt_threshold_pseudo;
		size_temp = ARRAY_SIZE(batt_temp_threshold_pseudo);
		size_volt = ARRAY_SIZE(batt_volt_threshold_pseudo);
#else
		batt_volt = 3900 *1000;
#endif
	} else {
		ptr_temp = batt_temp_threshold;
		ptr_volt = batt_volt_threshold;
		size_temp = ARRAY_SIZE(batt_temp_threshold);
		size_volt = ARRAY_SIZE(batt_volt_threshold);
	}
	for (i=0; i<size_temp; i++)
	{
		if (batt_temp > *(ptr_temp + i) )
		{
			for (j=0; j<size_volt; j++)
			{
				if (batt_volt > *(ptr_volt + j))
				{
					return (i + j * size_temp);
				}
			}
			pr_warn("Warning!! Under voltage is supplied!! \
				If you see this, ask god to fix the law of nature~:)\n");
			return -1;
		}
	}
	pr_warn("warning!! Under temperature!! \
		If you see this, send this log to yongk.kim@lge.com\n");
	return -2;
}

static int chg_update_charging_state(void *data, int batt_temp, int batt_level)
{
	int batt_temp_level;
	int rtnValue = 0;
	struct pm8921_chg_chip *chip = data;
	int i, arg_max, pseudo_batt_local = 0;
	int size_temp = -EINVAL, size_volt = -EINVAL;
	int iusb_mA;
	int reg_loop;
	int ibat_max_ma2;
	static int count = 0;
	int chg_current=0;

	unsigned int current_voltage_uv = get_prop_battery_uvolts(chip);

        unsigned int current_voltage_mv = current_voltage_uv / 1000;

	the_chip->curr_voltage_mv = current_voltage_mv;	

#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS)
	if (!pseudo_batt_info.mode) {
#else
	if (pseudo_batt_info.mode) {
		size_temp = ARRAY_SIZE(batt_temp_threshold_pseudo);
		size_volt = ARRAY_SIZE(batt_volt_threshold_pseudo);
#ifdef CONFIG_MACH_LGE_L9II_COMMON
		pr_info("[Temperature] Fake mode :  state of temperature set as normal , 1 \n");
		chg_batt_temp_state = CHG_BATT_NORMAL_STATE;
#endif
	} else {
#endif
		size_temp = ARRAY_SIZE(batt_temp_threshold);
		size_volt = ARRAY_SIZE(batt_volt_threshold);
	}
	arg_max = size_temp * size_volt;

	batt_temp_level = battery_status(batt_temp, batt_level);

	chg_current = get_prop_batt_current(chip)/1000;	
#if defined(CONFIG_MACH_LGE_L9II_COMMON)


/*                                                                     */
		
	if(off_batt_info.mode==300)
	{
		
		printk("off mode current voltage :%d, eoc check:%d\n",the_chip->curr_voltage_mv, the_chip->eoc_check);
		if(the_chip->curr_voltage_mv < 4260 && the_chip->eoc_check == 1)
		{	
			printk("off mode  recharging, voltaage :%d , chg_current:%d \n",the_chip->curr_voltage_mv, chg_current);
			if(chg_current < -100)
				the_chip->eoc_check=0;

			pm_chg_auto_enable(chip, 1);

		}	
	

	}
/*                                                                   */

#endif
	switch(chg_batt_temp_state)
	{
		case CHG_BATT_NORMAL_STATE:
		for(i=0; i<arg_max; i++)
		{
			if (batt_temp_level == i)
			{
#ifdef CONFIG_MACH_LGE_L9II_COMMON
				if (pseudo_batt_info.mode) {
					pr_info("[Temperature] Fake mode :  state of temperature set as normal , 2 \n");
					chg_batt_temp_state = CHG_BATT_NORMAL_STATE;
				}else{
					chg_batt_temp_state = normal_charging[i].chg_status;
				}
#else
					chg_batt_temp_state = normal_charging[i].chg_status;
#endif
				pseudo_batt_local = normal_charging[i].pseudo_battery;
#if !defined(CONFIG_MACH_LGE_L9II_COMMON)
				pr_info("[PM][Temperature] (%d) : normal tempterature status with normal(fast) charging.\n", batt_temp);
#endif
			}
		}
		break;
		case CHG_BATT_DC_CURRENT_STATE:
		for(i=0; i<arg_max; i++)
		{
			if (batt_temp_level == i)
			{
				chg_batt_temp_state = decrease_charging[i].chg_status;
				pseudo_batt_local = decrease_charging[i].pseudo_battery;
				pr_info("[PM][Temperature] (%d) : high/lower temperature status with decreasing current charging.\n", batt_temp);
			}
		}
		break;
		case CHG_BATT_STOP_CHARGING_STATE:
		for(i=0; i<arg_max; i++)
		{
			if (batt_temp_level == i)
			{
				chg_batt_temp_state = stop_charging[i].chg_status;
				pseudo_batt_local = stop_charging[i].pseudo_battery;
				pr_info("[PM][Temperature] (%d) : highest/lowest temperature status.\n", batt_temp);
			}
		}
		break;
		default:
		chg_batt_temp_state = CHG_BATT_NORMAL_STATE;
		pseudo_ui_charging = 0;
	}
	switch(chg_batt_temp_state_prev)
	{
		case CHG_BATT_NORMAL_STATE:
#if !defined(CONFIG_MACH_LGE_L9II_COMMON)
				pr_info("The previous state was  normal tempterature status.\n");
#endif
		break;
		case CHG_BATT_DC_CURRENT_STATE:
				pr_info("The previous state is high/lower temperature status.\n");
		break;
		case CHG_BATT_STOP_CHARGING_STATE:
				pr_info("The previous state is highest/lewest temperature status.\n");
		break;
	}

	pm_chg_iusbmax_get(chip, &iusb_mA);
	pm_chg_ibatmax_get(chip, &ibat_max_ma2);
	reg_loop = pm_chg_get_regulation_loop(chip);

	if (is_usb_chg_plugged_in(chip))
	{
		pseudo_ui_charging = pseudo_batt_local;
		if (chg_batt_temp_state == CHG_BATT_STOP_CHARGING_STATE)
		{
			if (chg_batt_temp_state_prev != CHG_BATT_STOP_CHARGING_STATE)
			{
				pr_info("Stop charging!!\n");
			}
			else
			{
			}
			rtnValue = 1;
		}
		else if (chg_batt_temp_state == CHG_BATT_DC_CURRENT_STATE)
		{
			if (chg_batt_temp_state_prev != CHG_BATT_DC_CURRENT_STATE)
			{
				pm_chg_ibatmax_set(chip, PM8921_CHG_I_DECREASE_CHARGING_CURRENT);
				pr_info("Reduce charging current!!\n");
			}
			else
			{
			}
			rtnValue = 0;
		}
		else if (chg_batt_temp_state == CHG_BATT_NORMAL_STATE)
		{
			if (chg_batt_temp_state_prev != CHG_BATT_NORMAL_STATE)
			{
				pm_chg_ibatmax_set(chip, chip->max_bat_chg_current);
				pr_info("Get back to normal charging!!\n");
			}
			else
			{
			}
			rtnValue = 0;
		}
		else
		{
			pr_info("Looks strange.... Add chg_batt_temp_state!\n");
		}
	}
	else
	{
		if (pm_chg_get_rt_status(chip, CHG_GONE_IRQ)) {
			pseudo_ui_charging = 0;
			rtnValue = 0;
		} else {
			pseudo_ui_charging = pseudo_batt_local;
			rtnValue = 0;
		}

	}
	if (count++ >= 20) {
		pr_info("SOC: %d, voltage:%d, chg_current:%d ,USBIN:%d, chg_gone:%d, usb:%d, ac:%d, iusb:%d, ibatMax1:%d, ibatMax2:%d, reg_loop=0x%x \n",
			chip->recent_reported_soc,
			chip->curr_voltage_mv,
			chg_current,
			is_usb_chg_plugged_in(chip),
			pm_chg_get_rt_status(chip, CHG_GONE_IRQ),
			usb_online,
			ac_online,
			iusb_mA,
			ibat_max_ma,
			ibat_max_ma2,
			reg_loop);
		count = 0;
	}
	chg_batt_temp_state_prev = chg_batt_temp_state;
	return rtnValue;
}
#endif

static int __pm_batt_external_power_changed_work(struct device *dev, void *data)
{
	struct power_supply *psy = &the_chip->batt_psy;
	struct power_supply *epsy = dev_get_drvdata(dev);
	int i, dcin_irq;

	/* Only search for external supply if none is registered */
	if (!the_chip->ext_psy) {
		dcin_irq = the_chip->pmic_chg_irq[DCIN_VALID_IRQ];
		for (i = 0; i < epsy->num_supplicants; i++) {
			if (!strncmp(epsy->supplied_to[i], psy->name, 7)) {
				if (!strncmp(epsy->name, "dc", 2)) {
					the_chip->ext_psy = epsy;
					dcin_valid_irq_handler(dcin_irq,
							the_chip);
				}
			}
		}
	}
	return 0;
}

static void pm_batt_external_power_changed(struct power_supply *psy)
{
	/* Only look for an external supply if it hasn't been registered */
	if (!the_chip->ext_psy)
		class_for_each_device(power_supply_class, NULL, psy,
					 __pm_batt_external_power_changed_work);
}

/**
 * update_heartbeat - internal function to update userspace
 *		per update_time minutes
 *
 */
#define LOW_SOC_HEARTBEAT_MS	20000
static void update_heartbeat(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct pm8921_chg_chip *chip = container_of(dwork,
				struct pm8921_chg_chip, update_heartbeat_work);

	power_supply_changed(&chip->batt_psy);
#ifndef CONFIG_BATTERY_MAX17043
	if (chip->recent_reported_soc <= 20)
		schedule_delayed_work(&chip->update_heartbeat_work,
			      round_jiffies_relative(msecs_to_jiffies
						     (LOW_SOC_HEARTBEAT_MS)));
	else
#endif
		schedule_delayed_work(&chip->update_heartbeat_work,
			      round_jiffies_relative(msecs_to_jiffies
						     (chip->update_time)));
}
#define VDD_LOOP_ACTIVE_BIT	BIT(3)
#define VDD_MAX_INCREASE_MV	400
static int vdd_max_increase_mv = VDD_MAX_INCREASE_MV;
module_param(vdd_max_increase_mv, int, 0644);

static int ichg_threshold_ua = -400000;
module_param(ichg_threshold_ua, int, 0644);

#define PM8921_CHG_VDDMAX_RES_MV	10
static void adjust_vdd_max_for_fastchg(struct pm8921_chg_chip *chip,
						int vbat_batt_terminal_uv)
{
	int adj_vdd_max_mv, programmed_vdd_max;
	int vbat_batt_terminal_mv;
	int reg_loop;
	int delta_mv = 0;

	if (chip->rconn_mohm == 0) {
#ifdef CONFIG_LGE_PM
		printk(KERN_INFO "Exiting as rconn_mohm is 0\n");
#else
		pr_debug("Exiting as rconn_mohm is 0\n");
#endif
		return;
	}
	/* adjust vdd_max only in normal temperature zone */
	if (chip->is_bat_cool || chip->is_bat_warm) {
		pr_debug("Exiting is_bat_cool = %d is_batt_warm = %d\n",
				chip->is_bat_cool, chip->is_bat_warm);
		return;
	}

	reg_loop = pm_chg_get_regulation_loop(chip);
	if (!(reg_loop & VDD_LOOP_ACTIVE_BIT)) {
		pr_debug("Exiting Vdd loop is not active reg loop=0x%x\n",
			reg_loop);
		return;
	}
	vbat_batt_terminal_mv = vbat_batt_terminal_uv/1000;
	pm_chg_vddmax_get(the_chip, &programmed_vdd_max);

	delta_mv =  chip->max_voltage_mv - vbat_batt_terminal_mv;

	adj_vdd_max_mv = programmed_vdd_max + delta_mv;
	pr_debug("vdd_max needs to be changed by %d mv from %d to %d\n",
			delta_mv,
			programmed_vdd_max,
			adj_vdd_max_mv);

	if (adj_vdd_max_mv < chip->max_voltage_mv) {
		pr_debug("adj vdd_max lower than default max voltage\n");
		return;
	}

	adj_vdd_max_mv = DIV_ROUND_UP(adj_vdd_max_mv, PM8921_CHG_VDDMAX_RES_MV)
					* PM8921_CHG_VDDMAX_RES_MV;

	if (adj_vdd_max_mv > (chip->max_voltage_mv + vdd_max_increase_mv))
		adj_vdd_max_mv = chip->max_voltage_mv + vdd_max_increase_mv;
	pr_debug("adjusting vdd_max_mv to %d to make "
		"vbat_batt_termial_uv = %d to %d\n",
		adj_vdd_max_mv, vbat_batt_terminal_uv, chip->max_voltage_mv);
	pm_chg_vddmax_set(chip, adj_vdd_max_mv);
}

enum {
	CHG_IN_PROGRESS,
	CHG_NOT_IN_PROGRESS,
	CHG_FINISHED,
};

#if defined(CONFIG_MACH_MSM8960_D1LU) || defined(CONFIG_MACH_MSM8960_D1LSK) || defined(CONFIG_MACH_MSM8960_D1LKT)
#define VBAT_TOLERANCE_MV	160
#elif defined(CONFIG_MACH_MSM8960_D1LV)
#define VBAT_TOLERANCE_MV       60
#elif defined(CONFIG_MACH_MSM8960_L0)
#define VBAT_TOLERANCE_MV	80			/* 4.28V = 4.36V(vddmax) - 0.08V(tolerance) */
#elif defined(CONFIG_MACH_LGE_L9II_COMMON)
#define VBAT_TOLERANCE_MV	50
#else
#define VBAT_TOLERANCE_MV	70
#endif
#define CHG_DISABLE_MSLEEP	100
static int is_charging_finished(struct pm8921_chg_chip *chip,
			int vbat_batt_terminal_uv, int ichg_meas_ma)
{
	int vbat_programmed, iterm_programmed, vbat_intended;
	int regulation_loop, fast_chg, vcp;
	int rc;
	static int last_vbat_programmed = -EINVAL;
#if defined(CONFIG_LGE_PM) && !defined(CONFIG_MACH_LGE_FX3_VZW) && !defined(CONFIG_MACH_LGE_F6_TMUS) && !defined(CONFIG_MACH_LGE_F6_VDF) && !defined(CONFIG_MACH_LGE_FX3Q_TMUS) && !defined(CONFIG_MACH_LGE_L9II_COMMON)
	int eoc_uvolts = -EINVAL;
#endif

#if defined(CONFIG_LGE_PM) && !defined(CONFIG_BATTERY_MAX17043)
	int soc;
#endif

	if (!is_ext_charging(chip)) {
#if defined(CONFIG_LGE_PM) && !defined(CONFIG_MACH_LGE_FX3_VZW) && !defined(CONFIG_MACH_LGE_F6_TMUS) && !defined(CONFIG_MACH_LGE_F6_VDF) && !defined(CONFIG_MACH_LGE_FX3Q_TMUS) && !defined(CONFIG_MACH_LGE_L9II_COMMON)
		if (pseudo_batt_info.mode) {
			eoc_uvolts = 4260000;
		} else {
			eoc_uvolts = 4345000;
		}

		if (get_prop_battery_uvolts(chip) > eoc_uvolts)
			return CHG_FINISHED;
#endif
		/* return if the battery is not being fastcharged */
		fast_chg = pm_chg_get_rt_status(chip, FASTCHG_IRQ);
		pr_debug("fast_chg = %d\n", fast_chg);
		if (fast_chg == 0)
			return CHG_NOT_IN_PROGRESS;

		vcp = pm_chg_get_rt_status(chip, VCP_IRQ);
		pr_debug("vcp = %d\n", vcp);
		if (vcp == 1)
			return CHG_IN_PROGRESS;

		/* reset count if battery is hot/cold */
#ifndef CONFIG_LGE_PM
		rc = pm_chg_get_rt_status(chip, BAT_TEMP_OK_IRQ);
		pr_debug("batt_temp_ok = %d\n", rc);
		if (rc == 0)
			return CHG_IN_PROGRESS;
#endif

		rc = pm_chg_vddmax_get(chip, &vbat_programmed);
		if (rc) {
			pr_err("couldnt read vddmax rc = %d\n", rc);
			return CHG_IN_PROGRESS;
		}
		pr_debug("vddmax = %d vbat_batt_terminal_uv=%d\n",
			 vbat_programmed, vbat_batt_terminal_uv);

		if (last_vbat_programmed == -EINVAL)
			last_vbat_programmed = vbat_programmed;
		if (last_vbat_programmed !=  vbat_programmed) {
			/* vddmax changed, reset and check again */
			pr_debug("vddmax = %d last_vdd_max=%d\n",
				 vbat_programmed, last_vbat_programmed);
			last_vbat_programmed = vbat_programmed;
			return CHG_IN_PROGRESS;
		}

		if (chip->is_bat_cool)
			vbat_intended = chip->cool_bat_voltage;
		else if (chip->is_bat_warm)
			vbat_intended = chip->warm_bat_voltage;
		else
#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_F6_TMUS) || defined(CONFIG_MACH_LGE_F6_VDF) || defined(CONFIG_MACH_LGE_FX3Q_TMUS) || defined(CONFIG_MACH_LGE_L9II_COMMON)
			vbat_intended = chip->max_voltage_mv - VBAT_TOLERANCE_MV;	// 4.28V = 4.35V - 0.07V
#else	// QCT ORG
			vbat_intended = chip->max_voltage_mv;
#endif
#if !defined(CONFIG_LGE_PM) || defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_F6_TMUS)|| defined(CONFIG_MACH_LGE_F6_VDF) || defined(CONFIG_MACH_LGE_FX3Q_TMUS) || defined(CONFIG_MACH_LGE_L9II_COMMON)
		if (vbat_batt_terminal_uv / 1000 < vbat_intended) {
			pr_debug("terminal_uv:%d < vbat_intended:%d.\n",
							vbat_batt_terminal_uv,
							vbat_intended);
			return CHG_IN_PROGRESS;
		}
#endif

		regulation_loop = pm_chg_get_regulation_loop(chip);
		if (regulation_loop < 0) {
			pr_err("couldnt read the regulation loop err=%d\n",
				regulation_loop);
			return CHG_IN_PROGRESS;
		}
		pr_debug("regulation_loop=%d\n", regulation_loop);

		if (regulation_loop != 0 && regulation_loop != VDD_LOOP)
			return CHG_IN_PROGRESS;
	} /* !is_ext_charging */

	/* reset count if battery chg current is more than iterm */
	rc = pm_chg_iterm_get(chip, &iterm_programmed);
	if (rc) {
		pr_err("couldnt read iterm rc = %d\n", rc);
		return CHG_IN_PROGRESS;
	}

	pr_debug("iterm_programmed = %d ichg_meas_ma=%d\n",
				iterm_programmed, ichg_meas_ma);
	/*
	 * ichg_meas_ma < 0 means battery is drawing current
	 * ichg_meas_ma > 0 means battery is providing current
	 */
	if (ichg_meas_ma > 0)
		return CHG_IN_PROGRESS;

	if (ichg_meas_ma * -1 > iterm_programmed)
		return CHG_IN_PROGRESS;

/*                                                   
                                                                                                                              
                                                                                 
                                  
 */
#if defined(CONFIG_LGE_PM) && !defined(CONFIG_BATTERY_MAX17043)
		soc = get_prop_batt_capacity(chip);
		if (soc < 96)
			return CHG_IN_PROGRESS;
#endif

	return CHG_FINISHED;
}

/**
 * eoc_worker - internal function to check if battery EOC
 *		has happened
 *
 * If all conditions favouring, if the charge current is
 * less than the term current for three consecutive times
 * an EOC has happened.
 * The wakelock is released if there is no need to reshedule
 * - this happens when the battery is removed or EOC has
 * happened
 */
#define CONSECUTIVE_COUNT	3
static void eoc_worker(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct pm8921_chg_chip *chip = container_of(dwork,
				struct pm8921_chg_chip, eoc_work);
	static int count;
	int end;
	int vbat_meas_uv, vbat_meas_mv;
	int ichg_meas_ua, ichg_meas_ma;
	int vbat_batt_terminal_uv;

	pm8921_bms_get_simultaneous_battery_voltage_and_current(
					&ichg_meas_ua,	&vbat_meas_uv);
	vbat_meas_mv = vbat_meas_uv / 1000;
	/* rconn_mohm is in milliOhms */
	ichg_meas_ma = ichg_meas_ua / 1000;
	vbat_batt_terminal_uv = vbat_meas_uv
					+ ichg_meas_ma
					* the_chip->rconn_mohm;

	end = is_charging_finished(chip, vbat_batt_terminal_uv, ichg_meas_ma);

	if (end == CHG_NOT_IN_PROGRESS) {
		count = 0;
		wake_unlock(&chip->eoc_wake_lock);
		return;
	}

	/* If the disable hw clock switching
	 * flag was set it can now be unset. Also, re-enable
	 * the battery alarm to set the flag again when needed
	 */
	if (chip->disable_hw_clock_switching) {
		/* Unset the hw clock switching flag */
		chip->disable_hw_clock_switching = 0;

		if (pm8921_charger_enable_batt_alarm(chip))
			pr_err("couldn't set up batt alarm!\n");
	}

	if (end == CHG_FINISHED) {
		count++;
	} else {
		count = 0;
	}

	if (count == CONSECUTIVE_COUNT) {
		count = 0;
		pr_info("End of Charging\n");


#if defined(CONFIG_MACH_LGE_L9II_COMMON)

/*                                                                     */

		if(off_batt_info.mode == 300)
			chip->eoc_check=1;

/*                                                                   */
#endif
		/* set the vbatdet back, in case it was changed
		 * to trigger charging */
		if (chip->is_bat_cool) {
			pm_chg_vbatdet_set(the_chip,
				the_chip->cool_bat_voltage
				- the_chip->resume_voltage_delta);
		} else if (chip->is_bat_warm) {
			pm_chg_vbatdet_set(the_chip,
				the_chip->warm_bat_voltage
				- the_chip->resume_voltage_delta);
		} else {
			pm_chg_vbatdet_set(the_chip,
				the_chip->max_voltage_mv
				- the_chip->resume_voltage_delta);
		}

		pm_chg_auto_enable(chip, 0);

		if (is_ext_charging(chip))
			chip->ext_charge_done = true;

		if (chip->is_bat_warm || chip->is_bat_cool)
			chip->bms_notify.is_battery_full = 0;
		else
			chip->bms_notify.is_battery_full = 1;
		/* declare end of charging by invoking chgdone interrupt */
		chgdone_irq_handler(chip->pmic_chg_irq[CHGDONE_IRQ], chip);
#ifdef CONFIG_LGE_PM
	//	pm8921_chg_enable_irq(chip, CHG_GONE_IRQ);
	//	enable_irq_wake(chip->pmic_chg_irq[CHG_GONE_IRQ]);
#endif

#if defined(CONFIG_MACH_LGE_L9II_COMMON)
/*                                                                     */
		if( off_batt_info.mode !=300){
			printk(" wake lock of eoc_wake_lock\n");
			wake_unlock(&chip->eoc_wake_lock);
		}
/*                                                                   */
#else

		wake_unlock(&chip->eoc_wake_lock);
#endif
	} else {
		adjust_vdd_max_for_fastchg(chip, vbat_batt_terminal_uv);
		pr_debug("EOC count = %d\n", count);
		schedule_delayed_work(&chip->eoc_work,
			      round_jiffies_relative(msecs_to_jiffies
						     (EOC_CHECK_PERIOD_MS)));
	}
}

static void btm_configure_work(struct work_struct *work)
{
	int rc;

	rc = pm8xxx_adc_btm_configure(&btm_config);
	if (rc)
		pr_err("failed to configure btm rc=%d", rc);
}

DECLARE_WORK(btm_config_work, btm_configure_work);

static void set_appropriate_battery_current(struct pm8921_chg_chip *chip)
{
	unsigned int chg_current = chip->max_bat_chg_current;

	if (chip->is_bat_cool)
		chg_current = min(chg_current, chip->cool_bat_chg_current);

	if (chip->is_bat_warm)
		chg_current = min(chg_current, chip->warm_bat_chg_current);
#ifdef CONFIG_LGE_PM 
	pr_info("thermal_mitigation=%d, ibatmax=%d\n"
		, thermal_mitigation
		, chip->thermal_mitigation[thermal_mitigation]);
#endif

	if (thermal_mitigation != 0 && chip->thermal_mitigation)
		chg_current = min(chg_current,
				chip->thermal_mitigation[thermal_mitigation]);

	pm_chg_ibatmax_set(the_chip, chg_current);
}

#define TEMP_HYSTERISIS_DEGC 2
static void battery_cool(bool enter)
{
	pr_debug("enter = %d\n", enter);
	if (enter == the_chip->is_bat_cool)
		return;
	the_chip->is_bat_cool = enter;
	if (enter) {
		btm_config.low_thr_temp =
			the_chip->cool_temp_dc + TEMP_HYSTERISIS_DEGC;
		set_appropriate_battery_current(the_chip);
		pm_chg_vddmax_set(the_chip, the_chip->cool_bat_voltage);
		pm_chg_vbatdet_set(the_chip,
			the_chip->cool_bat_voltage
			- the_chip->resume_voltage_delta);
	} else {
		btm_config.low_thr_temp = the_chip->cool_temp_dc;
		set_appropriate_battery_current(the_chip);
		pm_chg_vddmax_set(the_chip, the_chip->max_voltage_mv);
		pm_chg_vbatdet_set(the_chip,
			the_chip->max_voltage_mv
			- the_chip->resume_voltage_delta);
	}
	schedule_work(&btm_config_work);
}

static void battery_warm(bool enter)
{
	pr_debug("enter = %d\n", enter);
	if (enter == the_chip->is_bat_warm)
		return;
	the_chip->is_bat_warm = enter;
	if (enter) {
		btm_config.high_thr_temp =
			the_chip->warm_temp_dc - TEMP_HYSTERISIS_DEGC;
		set_appropriate_battery_current(the_chip);
		pm_chg_vddmax_set(the_chip, the_chip->warm_bat_voltage);
		pm_chg_vbatdet_set(the_chip,
			the_chip->warm_bat_voltage
			- the_chip->resume_voltage_delta);
	} else {
		btm_config.high_thr_temp = the_chip->warm_temp_dc;
		set_appropriate_battery_current(the_chip);
		pm_chg_vddmax_set(the_chip, the_chip->max_voltage_mv);
		pm_chg_vbatdet_set(the_chip,
			the_chip->max_voltage_mv
			- the_chip->resume_voltage_delta);
	}
	schedule_work(&btm_config_work);
}

static int configure_btm(struct pm8921_chg_chip *chip)
{
	int rc;

	if (chip->warm_temp_dc != INT_MIN)
		btm_config.btm_warm_fn = battery_warm;
	else
		btm_config.btm_warm_fn = NULL;

	if (chip->cool_temp_dc != INT_MIN)
		btm_config.btm_cool_fn = battery_cool;
	else
		btm_config.btm_cool_fn = NULL;

	btm_config.low_thr_temp = chip->cool_temp_dc;
	btm_config.high_thr_temp = chip->warm_temp_dc;
	btm_config.interval = chip->temp_check_period;
	rc = pm8xxx_adc_btm_configure(&btm_config);
	if (rc)
		pr_err("failed to configure btm rc = %d\n", rc);
	rc = pm8xxx_adc_btm_start();
	if (rc)
		pr_err("failed to start btm rc = %d\n", rc);

	return rc;
}

/**
 * set_disable_status_param -
 *
 * Internal function to disable battery charging and also disable drawing
 * any current from the source. The device is forced to run on a battery
 * after this.
 */
static int set_disable_status_param(const char *val, struct kernel_param *kp)
{
	int ret;
	struct pm8921_chg_chip *chip = the_chip;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_err("error setting value %d\n", ret);
		return ret;
	}
	pr_info("factory set disable param to %d\n", charging_disabled);
	if (chip) {
		pm_chg_auto_enable(chip, !charging_disabled);
		pm_chg_charge_dis(chip, charging_disabled);
	}
	return 0;
}
module_param_call(disabled, set_disable_status_param, param_get_uint,
					&charging_disabled, 0644);

static int rconn_mohm;
static int set_rconn_mohm(const char *val, struct kernel_param *kp)
{
	int ret;
	struct pm8921_chg_chip *chip = the_chip;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_err("error setting value %d\n", ret);
		return ret;
	}
	if (chip)
		chip->rconn_mohm = rconn_mohm;
	return 0;
}
module_param_call(rconn_mohm, set_rconn_mohm, param_get_uint,
					&rconn_mohm, 0644);
/**
 * set_thermal_mitigation_level -
 *
 * Internal function to control battery charging current to reduce
 * temperature
 */
static int set_therm_mitigation_level(const char *val, struct kernel_param *kp)
{
	int ret;
	struct pm8921_chg_chip *chip = the_chip;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_err("error setting value %d\n", ret);
		return ret;
	}

	if (!chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}

	if (!chip->thermal_mitigation) {
		pr_err("no thermal mitigation\n");
		return -EINVAL;
	}

	if (thermal_mitigation < 0
		|| thermal_mitigation >= chip->thermal_levels) {
		pr_err("out of bound level selected\n");
		return -EINVAL;
	}

	set_appropriate_battery_current(chip);
	return ret;
}
module_param_call(thermal_mitigation, set_therm_mitigation_level,
					param_get_uint,
					&thermal_mitigation, 0644);

static int set_usb_max_current(const char *val, struct kernel_param *kp)
{
	int ret, mA;
	struct pm8921_chg_chip *chip = the_chip;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_err("error setting value %d\n", ret);
		return ret;
	}
	if (chip) {
		pr_warn("setting current max to %d\n", usb_max_current);
		pm_chg_iusbmax_get(chip, &mA);
		if (mA > usb_max_current)
			pm8921_charger_vbus_draw(usb_max_current);
		return 0;
	}
	return -EINVAL;
}
module_param_call(usb_max_current, set_usb_max_current,
	param_get_uint, &usb_max_current, 0644);

static void free_irqs(struct pm8921_chg_chip *chip)
{
	int i;

	for (i = 0; i < PM_CHG_MAX_INTS; i++)
		if (chip->pmic_chg_irq[i]) {
			free_irq(chip->pmic_chg_irq[i], chip);
			chip->pmic_chg_irq[i] = 0;
		}
}

/* determines the initial present states */
static void __devinit determine_initial_state(struct pm8921_chg_chip *chip)
{
	unsigned long flags;
	int fsm_state;
	int is_fast_chg;

	chip->dc_present = !!is_dc_chg_plugged_in(chip);
	chip->usb_present = !!is_usb_chg_plugged_in(chip);

	notify_usb_of_the_plugin_event(chip->usb_present);
	if (chip->usb_present) {
#ifdef CONFIG_LGE_PM
		pr_info("[POWER] determine_initial_state, usb_present!! \n");
#endif
		schedule_delayed_work(&chip->unplug_check_work,
			msecs_to_jiffies(UNPLUG_CHECK_WAIT_PERIOD_MS));
#ifdef CONFIG_MACH_MSM8930_FX3
		schedule_delayed_work(&chip->increase_usb_ma_value_work,
			msecs_to_jiffies(UNPLUG_CHECK_WAIT_PERIOD_MS));
#endif
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
		schedule_delayed_work(&chip->monitor_batt_temp_work,
			msecs_to_jiffies(UNPLUG_CHECK_WAIT_PERIOD_MS));
#endif
		pm8921_chg_enable_irq(chip, CHG_GONE_IRQ);
	}

	pm8921_chg_enable_irq(chip, DCIN_VALID_IRQ);
	pm8921_chg_enable_irq(chip, USBIN_VALID_IRQ);
	pm8921_chg_enable_irq(chip, BATT_REMOVED_IRQ);
	pm8921_chg_enable_irq(chip, BATT_INSERTED_IRQ);
	pm8921_chg_enable_irq(chip, DCIN_OV_IRQ);
	pm8921_chg_enable_irq(chip, DCIN_UV_IRQ);
	pm8921_chg_enable_irq(chip, CHGFAIL_IRQ);
	pm8921_chg_enable_irq(chip, FASTCHG_IRQ);
	pm8921_chg_enable_irq(chip, VBATDET_LOW_IRQ);
#ifndef CONFIG_LGE_PM
	pm8921_chg_enable_irq(chip, BAT_TEMP_OK_IRQ);
#endif

	spin_lock_irqsave(&vbus_lock, flags);

/*                                                                        */
	if (usb_chg_current) {
#ifdef CONFIG_LGE_PM
	pr_info("[POWER] determine_initial_state, usb_chg_current=%d, \n",usb_chg_current);
#endif
		/* reissue a vbus draw call */
		__pm8921_charger_vbus_draw(usb_chg_current);
	}
/*                                                                      */

	spin_unlock_irqrestore(&vbus_lock, flags);
	/*
	 * The bootloader could have started charging, a fastchg interrupt
	 * might not happen. Check the real time status and if it is fast
	 * charging invoke the handler so that the eoc worker could be
	 * started
	 */
	is_fast_chg = pm_chg_get_rt_status(chip, FASTCHG_IRQ);
#ifdef CONFIG_LGE_PM
	pr_info("[POWER] determine_initial_state, is_fast_chg=%d, \n",is_fast_chg);
#endif
	if (is_fast_chg)
		fastchg_irq_handler(chip->pmic_chg_irq[FASTCHG_IRQ], chip);

	fsm_state = pm_chg_get_fsm_state(chip);
	if (is_battery_charging(fsm_state)) {
		chip->bms_notify.is_charging = 1;
		pm8921_bms_charging_began();
	}

	check_battery_valid(chip);

	pr_debug("usb = %d, dc = %d  batt = %d state=%d\n",
			chip->usb_present,
			chip->dc_present,
			get_prop_batt_present(chip),
			fsm_state);

	/* Determine which USB trim column to use */
	if (pm8xxx_get_version(chip->dev->parent) == PM8XXX_VERSION_8917)
		chip->usb_trim_table = usb_trim_8917_table;
	else if (pm8xxx_get_version(chip->dev->parent) == PM8XXX_VERSION_8038)
		chip->usb_trim_table = usb_trim_8038_table;
}

struct pm_chg_irq_init_data {
	unsigned int	irq_id;
	char		*name;
	unsigned long	flags;
	irqreturn_t	(*handler)(int, void *);
};

#define CHG_IRQ(_id, _flags, _handler) \
{ \
	.irq_id		= _id, \
	.name		= #_id, \
	.flags		= _flags, \
	.handler	= _handler, \
}
struct pm_chg_irq_init_data chg_irq_data[] = {
	CHG_IRQ(USBIN_VALID_IRQ, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
						usbin_valid_irq_handler),
	CHG_IRQ(BATT_INSERTED_IRQ, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
						batt_inserted_irq_handler),
	CHG_IRQ(VBATDET_LOW_IRQ, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
						vbatdet_low_irq_handler),
	CHG_IRQ(VBAT_OV_IRQ, IRQF_TRIGGER_RISING, vbat_ov_irq_handler),
	CHG_IRQ(CHGWDOG_IRQ, IRQF_TRIGGER_RISING, chgwdog_irq_handler),
	CHG_IRQ(VCP_IRQ, IRQF_TRIGGER_RISING, vcp_irq_handler),
	CHG_IRQ(ATCDONE_IRQ, IRQF_TRIGGER_RISING, atcdone_irq_handler),
	CHG_IRQ(ATCFAIL_IRQ, IRQF_TRIGGER_RISING, atcfail_irq_handler),
	CHG_IRQ(CHGDONE_IRQ, IRQF_TRIGGER_RISING, chgdone_irq_handler),
	CHG_IRQ(CHGFAIL_IRQ, IRQF_TRIGGER_RISING, chgfail_irq_handler),
	CHG_IRQ(CHGSTATE_IRQ, IRQF_TRIGGER_RISING, chgstate_irq_handler),
	CHG_IRQ(LOOP_CHANGE_IRQ, IRQF_TRIGGER_RISING, loop_change_irq_handler),
	CHG_IRQ(FASTCHG_IRQ, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
						fastchg_irq_handler),
	CHG_IRQ(TRKLCHG_IRQ, IRQF_TRIGGER_RISING, trklchg_irq_handler),
	CHG_IRQ(BATT_REMOVED_IRQ, IRQF_TRIGGER_RISING,
						batt_removed_irq_handler),
	CHG_IRQ(BATTTEMP_HOT_IRQ, IRQF_TRIGGER_RISING,
						batttemp_hot_irq_handler),
	CHG_IRQ(CHGHOT_IRQ, IRQF_TRIGGER_RISING, chghot_irq_handler),
	CHG_IRQ(BATTTEMP_COLD_IRQ, IRQF_TRIGGER_RISING,
						batttemp_cold_irq_handler),
	CHG_IRQ(CHG_GONE_IRQ, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
						chg_gone_irq_handler),
#ifndef CONFIG_LGE_PM
	CHG_IRQ(BAT_TEMP_OK_IRQ, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
						bat_temp_ok_irq_handler),
#endif
	CHG_IRQ(COARSE_DET_LOW_IRQ, IRQF_TRIGGER_RISING,
						coarse_det_low_irq_handler),
	CHG_IRQ(VDD_LOOP_IRQ, IRQF_TRIGGER_RISING, vdd_loop_irq_handler),
	CHG_IRQ(VREG_OV_IRQ, IRQF_TRIGGER_RISING, vreg_ov_irq_handler),
	CHG_IRQ(VBATDET_IRQ, IRQF_TRIGGER_RISING, vbatdet_irq_handler),
	CHG_IRQ(BATFET_IRQ, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
						batfet_irq_handler),
	CHG_IRQ(DCIN_VALID_IRQ, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
						dcin_valid_irq_handler),
	CHG_IRQ(DCIN_OV_IRQ, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
						dcin_ov_irq_handler),
	CHG_IRQ(DCIN_UV_IRQ, IRQF_TRIGGER_RISING, dcin_uv_irq_handler),
};

static int __devinit request_irqs(struct pm8921_chg_chip *chip,
					struct platform_device *pdev)
{
	struct resource *res;
	int ret, i;

	ret = 0;
	bitmap_fill(chip->enabled_irqs, PM_CHG_MAX_INTS);

	for (i = 0; i < ARRAY_SIZE(chg_irq_data); i++) {
		res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
				chg_irq_data[i].name);
		if (res == NULL) {
			pr_err("couldn't find %s\n", chg_irq_data[i].name);
			goto err_out;
		}
		chip->pmic_chg_irq[chg_irq_data[i].irq_id] = res->start;
		ret = request_irq(res->start, chg_irq_data[i].handler,
			chg_irq_data[i].flags,
			chg_irq_data[i].name, chip);
		if (ret < 0) {
			pr_err("couldn't request %d (%s) %d\n", res->start,
					chg_irq_data[i].name, ret);
			chip->pmic_chg_irq[chg_irq_data[i].irq_id] = 0;
			goto err_out;
		}
		pm8921_chg_disable_irq(chip, chg_irq_data[i].irq_id);
	}
	return 0;

err_out:
	free_irqs(chip);
	return -EINVAL;
}

static void pm8921_chg_force_19p2mhz_clk(struct pm8921_chg_chip *chip)
{
	int err;
	u8 temp;

	temp  = 0xD1;
	err = pm8xxx_writeb(chip->dev->parent, CHG_TEST, temp);
	if (err) {
		pr_err("Error %d writing %d to addr %d\n", err, temp, CHG_TEST);
		return;
	}

	temp  = 0xD3;
	err = pm8xxx_writeb(chip->dev->parent, CHG_TEST, temp);
	if (err) {
		pr_err("Error %d writing %d to addr %d\n", err, temp, CHG_TEST);
		return;
	}

	temp  = 0xD1;
	err = pm8xxx_writeb(chip->dev->parent, CHG_TEST, temp);
	if (err) {
		pr_err("Error %d writing %d to addr %d\n", err, temp, CHG_TEST);
		return;
	}

	temp  = 0xD5;
	err = pm8xxx_writeb(chip->dev->parent, CHG_TEST, temp);
	if (err) {
		pr_err("Error %d writing %d to addr %d\n", err, temp, CHG_TEST);
		return;
	}

	udelay(183);

	temp  = 0xD1;
	err = pm8xxx_writeb(chip->dev->parent, CHG_TEST, temp);
	if (err) {
		pr_err("Error %d writing %d to addr %d\n", err, temp, CHG_TEST);
		return;
	}

	temp  = 0xD0;
	err = pm8xxx_writeb(chip->dev->parent, CHG_TEST, temp);
	if (err) {
		pr_err("Error %d writing %d to addr %d\n", err, temp, CHG_TEST);
		return;
	}
	udelay(32);

	temp  = 0xD1;
	err = pm8xxx_writeb(chip->dev->parent, CHG_TEST, temp);
	if (err) {
		pr_err("Error %d writing %d to addr %d\n", err, temp, CHG_TEST);
		return;
	}

	temp  = 0xD3;
	err = pm8xxx_writeb(chip->dev->parent, CHG_TEST, temp);
	if (err) {
		pr_err("Error %d writing %d to addr %d\n", err, temp, CHG_TEST);
		return;
	}
}

static void pm8921_chg_set_hw_clk_switching(struct pm8921_chg_chip *chip)
{
	int err;
	u8 temp;

	temp  = 0xD1;
	err = pm8xxx_writeb(chip->dev->parent, CHG_TEST, temp);
	if (err) {
		pr_err("Error %d writing %d to addr %d\n", err, temp, CHG_TEST);
		return;
	}

	temp  = 0xD0;
	err = pm8xxx_writeb(chip->dev->parent, CHG_TEST, temp);
	if (err) {
		pr_err("Error %d writing %d to addr %d\n", err, temp, CHG_TEST);
		return;
	}
}

#define VREF_BATT_THERM_FORCE_ON	BIT(7)
static void detect_battery_removal(struct pm8921_chg_chip *chip)
{
	u8 temp;

	pm8xxx_readb(chip->dev->parent, CHG_CNTRL, &temp);
	pr_debug("upon restart CHG_CNTRL = 0x%x\n",  temp);

	if (!(temp & VREF_BATT_THERM_FORCE_ON))
		/*
		 * batt therm force on bit is battery backed and is default 0
		 * The charger sets this bit at init time. If this bit is found
		 * 0 that means the battery was removed. Tell the bms about it
		 */
		pm8921_bms_invalidate_shutdown_soc();
}

#define ENUM_TIMER_STOP_BIT	BIT(1)
#define BOOT_DONE_BIT		BIT(6)
#define CHG_BATFET_ON_BIT	BIT(3)
#define CHG_VCP_EN		BIT(0)
#define CHG_BAT_TEMP_DIS_BIT	BIT(2)
#define SAFE_CURRENT_MA		1500
static int __devinit pm8921_chg_hw_init(struct pm8921_chg_chip *chip)
{
	int rc;
#ifndef CONFIG_LGE_PM
	int vdd_safe;
#endif

/*                                         
                                                       */
#ifdef CONFIG_LGE_PM
	int m;
/*                                        */
/*                                                */
/*                                                   
                                                                                                         */
#ifdef CONFIG_MACH_LGE_L9II_COMMON                             
	unsigned int *cable_type=NULL;
#else
	unsigned int cable_type;
#endif
/*                                                   
                                                                                                         */                             
	unsigned int smem_size;
#endif
/*                                                */

#ifdef CONFIG_LGE_PM_TRKLCHG_IN_KERNEL
	unsigned int boot_reason;
	void *smem_info = NULL;
#endif

	/* forcing 19p2mhz before accessing any charger registers */
	pm8921_chg_force_19p2mhz_clk(chip);

	detect_battery_removal(chip);

	rc = pm_chg_masked_write(chip, SYS_CONFIG_2,
					BOOT_DONE_BIT, BOOT_DONE_BIT);
	if (rc) {
		pr_err("Failed to set BOOT_DONE_BIT rc=%d\n", rc);
		return rc;
	}

/*                                                                        */
#ifdef CONFIG_LGE_PM
	rc = pm_chg_vddsafe_set(chip, chip->max_voltage_mv);
#else
	vdd_safe = chip->max_voltage_mv + VDD_MAX_INCREASE_MV;

	if (vdd_safe > PM8921_CHG_VDDSAFE_MAX)
		vdd_safe = PM8921_CHG_VDDSAFE_MAX;

	rc = pm_chg_vddsafe_set(chip, vdd_safe);
#endif
/*                                        */
	if (rc) {
		pr_err("Failed to set safe voltage to %d rc=%d\n",
						chip->max_voltage_mv, rc);
		return rc;
	}
#ifdef CONFIG_LGE_PM
	rc = pm_chg_vbatdet_set(chip,PM8921_CHG_VBATDET_MAX);
#else
	rc = pm_chg_vbatdet_set(chip,
				chip->max_voltage_mv
				- chip->resume_voltage_delta);
#endif
	if (rc) {
		pr_err("Failed to set vbatdet comprator voltage to %d rc=%d\n",
			chip->max_voltage_mv - chip->resume_voltage_delta, rc);
		return rc;
	}

	rc = pm_chg_vddmax_set(chip, chip->max_voltage_mv);
	if (rc) {
		pr_err("Failed to set max voltage to %d rc=%d\n",
						chip->max_voltage_mv, rc);
		return rc;
	}
	rc = pm_chg_ibatsafe_set(chip, SAFE_CURRENT_MA);
	if (rc) {
		pr_err("Failed to set max voltage to %d rc=%d\n",
						SAFE_CURRENT_MA, rc);
		return rc;
	}

	rc = pm_chg_ibatmax_set(chip, chip->max_bat_chg_current);
	if (rc) {
		pr_err("Failed to set max current to 400 rc=%d\n", rc);
		return rc;
	}

	rc = pm_chg_iterm_set(chip, chip->term_current);
	if (rc) {
		pr_err("Failed to set term current to %d rc=%d\n",
						chip->term_current, rc);
		return rc;
	}

	/* Disable the ENUM TIMER */
	rc = pm_chg_masked_write(chip, PBL_ACCESS2, ENUM_TIMER_STOP_BIT,
			ENUM_TIMER_STOP_BIT);
	if (rc) {
		pr_err("Failed to set enum timer stop rc=%d\n", rc);
		return rc;
	}

/*                                         
                                                       */
#ifdef CONFIG_LGE_PM
	for (m = 0; m < ARRAY_SIZE(usb_ma_table); m++) {
		if (usb_ma_table[m].usb_ma == usb_chg_current)
			break;
	}
	if (m >= ARRAY_SIZE(usb_ma_table))
#ifdef CONFIG_LGE_PM_TRKLCHG_IN_KERNEL
{
		smem_info = smem_get_entry(SMEM_POWER_ON_STATUS_INFO, &smem_size);
		if(smem_info == NULL)
		{
			pr_err("smem_get_entry failed, smem_info.\n");
			return -EINVAL;
		}

		boot_reason = *(unsigned int *)smem_info;

		if (boot_reason == PWR_ON_EVENT_USB_CHG || boot_reason == PWR_ON_EVENT_WALL_CHG) {
			m = 2;	// set default current to 500mA when boot is done by charger for trickle charging in kernel.
			pr_info("[TRKL CHG] boot_reason = %d, Set defult current to %d \n", boot_reason, m);
		} else {
			m = 1;
		}
}
#else
		m = 1;
#endif
/*                                                */
/*                                                   
                                                                                                         */
#ifdef CONFIG_MACH_LGE_L9II_COMMON         
	cable_type = (smem_get_entry(SMEM_ID_VENDOR1, &smem_size));
       
	if (!cable_type) {
		pr_err("smem_get_entry failed --\n");
		return -EINVAL;
	}
	pr_info("kdw : *cable_type=%d\n", *cable_type);
	if (( *cable_type == LT_CABLE_56K) || ( *cable_type == LT_CABLE_130K)|| ( *cable_type == LT_CABLE_910K))
	{	
	     m = 15;
	     printk(KERN_INFO "cable_type is = (%d) \n", *cable_type );
	}
	else if( *cable_type == NO_BATT_QHSUSB_CHG_PORT_DCP )
	{
		m = 4;
	 	printk(KERN_INFO "cable_type is = (%d)\n", *cable_type );
	}
#else
	cable_type = *(unsigned int *) (smem_get_entry(SMEM_ID_VENDOR1, &smem_size));
	if (( cable_type == LT_CABLE_56K) || ( cable_type == LT_CABLE_130K)|| ( cable_type == LT_CABLE_910K))
	{	m = 15;
     printk(KERN_INFO "cable_type is = %d\n", cable_type );
	}
	else if( cable_type == NO_BATT_QHSUSB_CHG_PORT_DCP )
	{
	m = 4;
	 printk(KERN_INFO "cable_type is = %d\n", cable_type );
	}
#endif	
/*                                                   
                                                                                                         */	
/*                                                */

	rc = pm_chg_iusbmax_set(chip, m);

	if (rc) {
		pr_err("Failed to set usb max to %d rc=%d\n", 0, rc);
		return rc;
	}
/*                                                                        */
	init_chg_current = usb_ma_table[m].usb_ma;
/*                                                                      */
#endif
/*                                        */

	if (chip->safety_time != 0) {
		rc = pm_chg_tchg_max_set(chip, chip->safety_time);
		if (rc) {
			pr_err("Failed to set max time to %d minutes rc=%d\n",
							chip->safety_time, rc);
			return rc;
		}
	}

	if (chip->ttrkl_time != 0) {
		rc = pm_chg_ttrkl_max_set(chip, chip->ttrkl_time);
		if (rc) {
			pr_err("Failed to set trkl time to %d minutes rc=%d\n",
							chip->safety_time, rc);
			return rc;
		}
	}

	if (chip->vin_min != 0) {
		rc = pm_chg_vinmin_set(chip, chip->vin_min);
		if (rc) {
			pr_err("Failed to set vin min to %d mV rc=%d\n",
							chip->vin_min, rc);
			return rc;
		}
	} else {
		chip->vin_min = pm_chg_vinmin_get(chip);
	}

	rc = pm_chg_disable_wd(chip);
	if (rc) {
		pr_err("Failed to disable wd rc=%d\n", rc);
		return rc;
	}

#ifdef CONFIG_MACH_MSM8930_FX3
	rc = pm_chg_masked_write(chip, CHG_CNTRL_2,
				CHG_BAT_TEMP_DIS_BIT, 1);
#else
	rc = pm_chg_masked_write(chip, CHG_CNTRL_2,
				CHG_BAT_TEMP_DIS_BIT, 0);
#endif
	if (rc) {
		pr_err("Failed to enable temp control chg rc=%d\n", rc);
		return rc;
	}
	/* switch to a 3.2Mhz for the buck */
	rc = pm8xxx_writeb(chip->dev->parent, CHG_BUCK_CLOCK_CTRL, 0x15);
	if (rc) {
		pr_err("Failed to switch buck clk rc=%d\n", rc);
		return rc;
	}

	if (chip->trkl_voltage != 0) {
		rc = pm_chg_vtrkl_low_set(chip, chip->trkl_voltage);
		if (rc) {
			pr_err("Failed to set trkl voltage to %dmv  rc=%d\n",
							chip->trkl_voltage, rc);
			return rc;
		}
	}

	if (chip->weak_voltage != 0) {
		rc = pm_chg_vweak_set(chip, chip->weak_voltage);
		if (rc) {
			pr_err("Failed to set weak voltage to %dmv  rc=%d\n",
							chip->weak_voltage, rc);
			return rc;
		}
	}

	if (chip->trkl_current != 0) {
		rc = pm_chg_itrkl_set(chip, chip->trkl_current);
		if (rc) {
			pr_err("Failed to set trkl current to %dmA  rc=%d\n",
							chip->trkl_voltage, rc);
			return rc;
		}
	}

	if (chip->weak_current != 0) {
		rc = pm_chg_iweak_set(chip, chip->weak_current);
		if (rc) {
			pr_err("Failed to set weak current to %dmA  rc=%d\n",
							chip->weak_current, rc);
			return rc;
		}
	}

	rc = pm_chg_batt_cold_temp_config(chip, chip->cold_thr);
	if (rc) {
		pr_err("Failed to set cold config %d  rc=%d\n",
						chip->cold_thr, rc);
	}

	rc = pm_chg_batt_hot_temp_config(chip, chip->hot_thr);
	if (rc) {
		pr_err("Failed to set hot config %d  rc=%d\n",
						chip->hot_thr, rc);
	}

	rc = pm_chg_led_src_config(chip, chip->led_src_config);
	if (rc) {
		pr_err("Failed to set charger LED src config %d  rc=%d\n",
						chip->led_src_config, rc);
	}

	/* Workarounds for die 3.0 */
	if (pm8xxx_get_revision(chip->dev->parent) == PM8XXX_REVISION_8921_3p0
	&& pm8xxx_get_version(chip->dev->parent) == PM8XXX_VERSION_8921)
		pm8xxx_writeb(chip->dev->parent, CHG_BUCK_CTRL_TEST3, 0xAC);

	/* Enable isub_fine resolution AICL for PM8917 */
	if (pm8xxx_get_version(chip->dev->parent) == PM8XXX_VERSION_8917) {
		chip->iusb_fine_res = true;
		if (chip->uvd_voltage_mv)
			rc = pm_chg_uvd_threshold_set(chip,
					chip->uvd_voltage_mv);
			if (rc) {
				pr_err("Failed to set UVD threshold %drc=%d\n",
						chip->uvd_voltage_mv, rc);
			return rc;
		}
	}

	pm8xxx_writeb(chip->dev->parent, CHG_BUCK_CTRL_TEST3, 0xD9);

	/* Disable EOC FSM processing */
	pm8xxx_writeb(chip->dev->parent, CHG_BUCK_CTRL_TEST3, 0x91);

	rc = pm_chg_masked_write(chip, CHG_CNTRL, VREF_BATT_THERM_FORCE_ON,
						VREF_BATT_THERM_FORCE_ON);
	if (rc)
		pr_err("Failed to Force Vref therm rc=%d\n", rc);

	rc = pm_chg_charge_dis(chip, charging_disabled);
	if (rc) {
		pr_err("Failed to disable CHG_CHARGE_DIS bit rc=%d\n", rc);
		return rc;
	}

	rc = pm_chg_auto_enable(chip, !charging_disabled);
	if (rc) {
		pr_err("Failed to enable charging rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static int get_rt_status(void *data, u64 * val)
{
	int i = (int)data;
	int ret;

	/* global irq number is passed in via data */
	ret = pm_chg_get_rt_status(the_chip, i);
	*val = ret;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(rt_fops, get_rt_status, NULL, "%llu\n");

static int get_fsm_status(void *data, u64 * val)
{
	u8 temp;

	temp = pm_chg_get_fsm_state(the_chip);
	*val = temp;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(fsm_fops, get_fsm_status, NULL, "%llu\n");

static int get_reg_loop(void *data, u64 * val)
{
	u8 temp;

	if (!the_chip) {
		pr_err("%s called before init\n", __func__);
		return -EINVAL;
	}
	temp = pm_chg_get_regulation_loop(the_chip);
	*val = temp;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(reg_loop_fops, get_reg_loop, NULL, "0x%02llx\n");

static int get_reg(void *data, u64 * val)
{
	int addr = (int)data;
	int ret;
	u8 temp;

	ret = pm8xxx_readb(the_chip->dev->parent, addr, &temp);
	if (ret) {
		pr_err("pm8xxx_readb to %x value =%d errored = %d\n",
			addr, temp, ret);
		return -EAGAIN;
	}
	*val = temp;
	return 0;
}

static int set_reg(void *data, u64 val)
{
	int addr = (int)data;
	int ret;
	u8 temp;

	temp = (u8) val;
	ret = pm8xxx_writeb(the_chip->dev->parent, addr, temp);
	if (ret) {
		pr_err("pm8xxx_writeb to %x value =%d errored = %d\n",
			addr, temp, ret);
		return -EAGAIN;
	}
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(reg_fops, get_reg, set_reg, "0x%02llx\n");

enum {
	BAT_WARM_ZONE,
	BAT_COOL_ZONE,
};
static int get_warm_cool(void *data, u64 * val)
{
	if (!the_chip) {
		pr_err("%s called before init\n", __func__);
		return -EINVAL;
	}
	if ((int)data == BAT_WARM_ZONE)
		*val = the_chip->is_bat_warm;
	if ((int)data == BAT_COOL_ZONE)
		*val = the_chip->is_bat_cool;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(warm_cool_fops, get_warm_cool, NULL, "0x%lld\n");

static void create_debugfs_entries(struct pm8921_chg_chip *chip)
{
	int i;

	chip->dent = debugfs_create_dir("pm8921_chg", NULL);

	if (IS_ERR(chip->dent)) {
		pr_err("pmic charger couldnt create debugfs dir\n");
		return;
	}

	debugfs_create_file("CHG_CNTRL", 0644, chip->dent,
			    (void *)CHG_CNTRL, &reg_fops);
	debugfs_create_file("CHG_CNTRL_2", 0644, chip->dent,
			    (void *)CHG_CNTRL_2, &reg_fops);
	debugfs_create_file("CHG_CNTRL_3", 0644, chip->dent,
			    (void *)CHG_CNTRL_3, &reg_fops);
	debugfs_create_file("PBL_ACCESS1", 0644, chip->dent,
			    (void *)PBL_ACCESS1, &reg_fops);
	debugfs_create_file("PBL_ACCESS2", 0644, chip->dent,
			    (void *)PBL_ACCESS2, &reg_fops);
	debugfs_create_file("SYS_CONFIG_1", 0644, chip->dent,
			    (void *)SYS_CONFIG_1, &reg_fops);
	debugfs_create_file("SYS_CONFIG_2", 0644, chip->dent,
			    (void *)SYS_CONFIG_2, &reg_fops);
	debugfs_create_file("CHG_VDD_MAX", 0644, chip->dent,
			    (void *)CHG_VDD_MAX, &reg_fops);
	debugfs_create_file("CHG_VDD_SAFE", 0644, chip->dent,
			    (void *)CHG_VDD_SAFE, &reg_fops);
	debugfs_create_file("CHG_VBAT_DET", 0644, chip->dent,
			    (void *)CHG_VBAT_DET, &reg_fops);
	debugfs_create_file("CHG_IBAT_MAX", 0644, chip->dent,
			    (void *)CHG_IBAT_MAX, &reg_fops);
	debugfs_create_file("CHG_IBAT_SAFE", 0644, chip->dent,
			    (void *)CHG_IBAT_SAFE, &reg_fops);
	debugfs_create_file("CHG_VIN_MIN", 0644, chip->dent,
			    (void *)CHG_VIN_MIN, &reg_fops);
	debugfs_create_file("CHG_VTRICKLE", 0644, chip->dent,
			    (void *)CHG_VTRICKLE, &reg_fops);
	debugfs_create_file("CHG_ITRICKLE", 0644, chip->dent,
			    (void *)CHG_ITRICKLE, &reg_fops);
	debugfs_create_file("CHG_ITERM", 0644, chip->dent,
			    (void *)CHG_ITERM, &reg_fops);
	debugfs_create_file("CHG_TCHG_MAX", 0644, chip->dent,
			    (void *)CHG_TCHG_MAX, &reg_fops);
	debugfs_create_file("CHG_TWDOG", 0644, chip->dent,
			    (void *)CHG_TWDOG, &reg_fops);
	debugfs_create_file("CHG_TEMP_THRESH", 0644, chip->dent,
			    (void *)CHG_TEMP_THRESH, &reg_fops);
	debugfs_create_file("CHG_COMP_OVR", 0644, chip->dent,
			    (void *)CHG_COMP_OVR, &reg_fops);
	debugfs_create_file("CHG_BUCK_CTRL_TEST1", 0644, chip->dent,
			    (void *)CHG_BUCK_CTRL_TEST1, &reg_fops);
	debugfs_create_file("CHG_BUCK_CTRL_TEST2", 0644, chip->dent,
			    (void *)CHG_BUCK_CTRL_TEST2, &reg_fops);
	debugfs_create_file("CHG_BUCK_CTRL_TEST3", 0644, chip->dent,
			    (void *)CHG_BUCK_CTRL_TEST3, &reg_fops);
	debugfs_create_file("CHG_TEST", 0644, chip->dent,
			    (void *)CHG_TEST, &reg_fops);

	debugfs_create_file("FSM_STATE", 0644, chip->dent, NULL,
			    &fsm_fops);

	debugfs_create_file("REGULATION_LOOP_CONTROL", 0644, chip->dent, NULL,
			    &reg_loop_fops);

	debugfs_create_file("BAT_WARM_ZONE", 0644, chip->dent,
				(void *)BAT_WARM_ZONE, &warm_cool_fops);
	debugfs_create_file("BAT_COOL_ZONE", 0644, chip->dent,
				(void *)BAT_COOL_ZONE, &warm_cool_fops);

	for (i = 0; i < ARRAY_SIZE(chg_irq_data); i++) {
		if (chip->pmic_chg_irq[chg_irq_data[i].irq_id])
			debugfs_create_file(chg_irq_data[i].name, 0444,
				chip->dent,
				(void *)chg_irq_data[i].irq_id,
				&rt_fops);
	}
}

/*           
           
                                   
*/

#ifdef CONFIG_LGE_PM
extern void machine_restart(char *cmd);

int pm8921_stop_charging_for_ATCMD(void)
{

	struct pm8921_chg_chip *chip = the_chip;

	pm8921_chg_disable_irq(chip, ATCFAIL_IRQ);
	pm8921_chg_disable_irq(chip, CHGHOT_IRQ);
	pm8921_chg_disable_irq(chip, ATCDONE_IRQ);
	pm8921_chg_disable_irq(chip, FASTCHG_IRQ);
	pm8921_chg_disable_irq(chip, CHGDONE_IRQ);
	pm8921_chg_disable_irq(chip, VBATDET_IRQ);
	pm8921_chg_disable_irq(chip, VBATDET_LOW_IRQ);

	return 1;
}
int pm8921_start_charging_for_ATCMD(void)
{

	struct pm8921_chg_chip *chip = the_chip;

	pm8921_chg_enable_irq(chip, ATCFAIL_IRQ);
	pm8921_chg_enable_irq(chip, CHGHOT_IRQ);
	pm8921_chg_enable_irq(chip, ATCDONE_IRQ);
	pm8921_chg_enable_irq(chip, FASTCHG_IRQ);
	pm8921_chg_enable_irq(chip, CHGDONE_IRQ);
	pm8921_chg_enable_irq(chip, VBATDET_IRQ);
	pm8921_chg_enable_irq(chip, VBATDET_LOW_IRQ);

	return 1;
}


static ssize_t at_chg_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int fsm_state, is_charging, r;
	bool b_chg_ok = false;

	if (!the_chip) {
		pr_err("called before init\n");
		return -EINVAL;
	}
	fsm_state = pm_chg_get_fsm_state(the_chip);
	is_charging = is_battery_charging(fsm_state);

	if (is_charging) {
		b_chg_ok = true;
		r = sprintf(buf, "%d\n", b_chg_ok);
		printk(KERN_INFO "at_chg_status_show , true ! buf = %s, is_charging = %d\n",
							buf, is_charging);
	} else {
		b_chg_ok = false;
		r = sprintf(buf, "%d\n", b_chg_ok);
		printk(KERN_INFO "at_chg_status_show , false ! buf = %s, is_charging = %d\n",
							buf, is_charging);
	}

	return r;
}

static ssize_t at_chg_status_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret = 0, batt_status = 0;
	struct pm8921_chg_chip *chip = the_chip;
	/*                                                              */
	lge_power_test_flag = 1;
	/*                          */

	if (!count)
		return -EINVAL;

	batt_status = get_prop_batt_status(chip);

	if (strncmp(buf, "0", 1) == 0) {
		/* stop charging */
		printk(KERN_INFO "at_chg_status_store : stop charging start\n");
		if (batt_status == POWER_SUPPLY_STATUS_CHARGING) {
			ret = pm8921_stop_charging_for_ATCMD();
			pm_chg_auto_enable(chip, 0);
			pm_chg_charge_dis(chip,1);
			printk(KERN_INFO "at_chg_status_store : stop charging end\n");
		}
	} else if (strncmp(buf, "1", 1) == 0) {
		/* start charging */
		printk(KERN_INFO "at_chg_status_store : start charging start\n");
		if (batt_status != POWER_SUPPLY_STATUS_CHARGING) {
			ret = pm8921_start_charging_for_ATCMD();
			pm_chg_auto_enable(chip, 1);
			pm_chg_charge_dis(chip,0);
			printk(KERN_INFO "at_chg_status_store : start charging end\n");
		}
	}

	if(ret == 0)
		return -EINVAL;

	return ret;
}

static ssize_t at_chg_complete_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int guage_level = 0, r = 0;

#ifdef CONFIG_BATTERY_MAX17043
	guage_level = max17043_get_capacity();
#else
	guage_level = get_prop_batt_capacity(the_chip);
#endif

	if (guage_level == 100) {
		r = sprintf(buf, "%d\n", 0);
	} else {
		r = sprintf(buf, "%d\n", 1);
	}
	return r;

}

static ssize_t at_chg_complete_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret = 0, batt_status = 0;
	struct pm8921_chg_chip *chip = the_chip;

	if (!count)
		return -EINVAL;

	batt_status = get_prop_batt_status(chip);

	if (strncmp(buf, "0", 1) == 0) {
		/* charging not complete */
		printk(KERN_INFO "at_chg_complete_store : charging not complete start\n");
		if (batt_status != POWER_SUPPLY_STATUS_CHARGING) {
			ret = pm8921_start_charging_for_ATCMD();
			pm_chg_auto_enable(chip, 1);
			pm_chg_charge_dis(chip,0);
			printk(KERN_INFO "at_chg_complete_store : charging not complete end\n");
		}
	} else if (strncmp(buf, "1", 1) == 0) {
		/* charging complete */
		printk(KERN_INFO "at_chg_complete_store : charging complete start\n");
		if (batt_status != POWER_SUPPLY_STATUS_FULL) {
			ret = pm8921_stop_charging_for_ATCMD();
			pm_chg_auto_enable(chip, 0);
			pm_chg_charge_dis(chip,1);
			printk(KERN_INFO "at_chg_complete_store : charging complete end\n");
		}
	}

	if(ret == 0)
		return -EINVAL;

	return ret;
}

static ssize_t at_pmic_reset_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int r = 0;
	bool pm_reset = true;

	msleep(3000); /* for waiting return values of testmode */

	machine_restart(NULL);

	r = sprintf(buf, "%d\n", pm_reset);

	return r;
}

/* test code by sungsookim */
static ssize_t at_current_limit_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	int r = 0;
	bool current_limit = true;

	printk(KERN_INFO "Current limit is successfully called\n");

	__pm8921_charger_vbus_draw(1500);
	r = sprintf(buf, "%d\n", current_limit);

	return r;
}

static ssize_t at_vcoin_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int r;
	struct pm8xxx_adc_chan_result result = { 0 };

	r = pm8xxx_adc_read(CHANNEL_VCOIN, &result);

	r = sprintf(buf, "%lld\n", result.physical);

	return r;
}

static ssize_t at_battemp_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct pm8xxx_adc_chan_result result;

	ret = pm8xxx_adc_read(CHANNEL_BATT_THERM, &result);
	if (ret)
	{
		pr_err("error reading adc channel = %d, ret = %d\n",
		CHANNEL_BATT_THERM, ret);
		return ret;
	}

	return sprintf(buf, "%lld\n", result.physical);
}
#ifdef CONFIG_MACH_MSM8930_FX3
static ssize_t aicl_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	char on[] = "ON";
	char off[] = "OFF";
	int r = 0;
	struct pm8921_chg_chip *chip = the_chip;

	if (chip->aicl == false) {
		r = sprintf(buf, "%s\n", off);
	} else {
		r = sprintf(buf, "%s\n", on);
	}
	return r;

}

static ssize_t aicl_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct pm8921_chg_chip *chip = the_chip;
	if (!strncmp(buf, "0", 1)
		|| !strncmp(buf, "OFF", 3)
		|| !strncmp(buf, "off", 3)
		|| !strncmp(buf, "false", 5)
		) {
		chip->aicl = false;
	} else if (!strncmp(buf, "1", 1)
		|| !strncmp(buf, "ON", 2)
		|| !strncmp(buf, "on", 2)
		|| !strncmp(buf, "true", 4)
		) {
		chip->aicl = true;
	}
	pr_info("AICL is %s!!!!\n", chip->aicl ? "ON" : "OFF");
	return 1;
}

#define MAXLEN_CABLETYPE_BUF 10
static ssize_t cable_type_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int res;
	void *cable_info_smem;
	unsigned smem_size;
	unsigned int cable_type = -1;

	cable_info_smem = smem_get_entry(SMEM_ID_VENDOR1, &smem_size);

	if (!cable_info_smem || smem_size == 0) {
		pr_err("fail to get SMEM_ID_VENDOR1\n");
		res = snprintf(buf, MAXLEN_CABLETYPE_BUF, "%s\n", "Error");
	} else {
		cable_type = *(unsigned int *)cable_info_smem;
		res = snprintf(buf, MAXLEN_CABLETYPE_BUF, "%d\n", cable_type);
	}

	return res;
}

static ssize_t cable_type_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	pr_info("You can't store cable_type.\n");
	return 1;
}

DEVICE_ATTR(cable_type, 0644, cable_type_show, cable_type_store);
DEVICE_ATTR(aicl, 0644, aicl_show, aicl_store);
#endif
static int lge_pwr_off = 0;
static ssize_t at_pmoff_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n",lge_pwr_off);
}
static ssize_t at_pmoff_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int rc = 1;
        if (strncmp(buf, "0", 1) == 0)
                lge_pwr_off = 0;
        else if (strncmp(buf, "1", 1) == 0)
                lge_pwr_off = 1;
	return rc;
}
DEVICE_ATTR(at_pmoff, 0644, at_pmoff_show, at_pmoff_store);
DEVICE_ATTR(at_current, 0644, at_current_limit_show, NULL);
DEVICE_ATTR(at_charge, 0644, at_chg_status_show, at_chg_status_store);
DEVICE_ATTR(at_chcomp, 0644, at_chg_complete_show, at_chg_complete_store);
DEVICE_ATTR(at_pmrst, 0644, at_pmic_reset_show, NULL);
DEVICE_ATTR(at_vcoin, 0644, at_vcoin_show, NULL);
DEVICE_ATTR(at_battemp, 0644, at_battemp_show, NULL);
#endif

#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
static ssize_t pm8921_batt_therm_adc_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int rc;
	struct pm8xxx_adc_chan_result result;

	rc = pm8xxx_adc_read(CHANNEL_BATT_THERM, &result);
	if (rc) {
		pr_err("error reading adc channel = %d, rc = %d\n",
					CHANNEL_BATT_THERM, rc);
		return rc;
	}
	pr_debug("batt_temp phy = %lld meas = 0x%llx adc_code = %d\n",
				result.physical, result.measurement, result.adc_code);
	rc = sprintf(buf, "%d\n", result.adc_code);
	return rc;
}
DEVICE_ATTR(batt_temp_adc, 0644, pm8921_batt_therm_adc_show, NULL);
#endif

#ifdef CONFIG_MACH_MSM8930_FX3
static ssize_t pm8921_iusb_max_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int rc = 0;
	int iusbmax = 0;
	rc = pm_chg_iusbmax_get(the_chip, &iusbmax);
	if (rc) {
		pr_err("error reading iusbmax = %d, rc = %d\n",
					iusbmax, rc);
		return rc;
	}
	rc = sprintf(buf, "%d\n", iusbmax);
	return rc;
}
static ssize_t pm8921_iusb_max_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	return 1;
}
DEVICE_ATTR(iusbmax, 0644, pm8921_iusb_max_show, pm8921_iusb_max_store);
#endif
static int pm8921_charger_suspend_noirq(struct device *dev)
{
	int rc;
	struct pm8921_chg_chip *chip = dev_get_drvdata(dev);

	rc = pm_chg_masked_write(chip, CHG_CNTRL, VREF_BATT_THERM_FORCE_ON, 0);
	if (rc)
		pr_err("Failed to Force Vref therm off rc=%d\n", rc);
	if (!(chip->disable_hw_clock_switching))
		pm8921_chg_set_hw_clk_switching(chip);
	return 0;
}

static int pm8921_charger_resume_noirq(struct device *dev)
{
	int rc;
	struct pm8921_chg_chip *chip = dev_get_drvdata(dev);

	pm8921_chg_force_19p2mhz_clk(chip);

	rc = pm_chg_masked_write(chip, CHG_CNTRL, VREF_BATT_THERM_FORCE_ON,
						VREF_BATT_THERM_FORCE_ON);
	if (rc)
		pr_err("Failed to Force Vref therm on rc=%d\n", rc);
	return 0;
}
#ifdef CONFIG_LGE_PM
#define INCREASE_MA_MSEC	1000

#if defined(CONFIG_MACH_LGE_FX3_TMUS) || defined(CONFIG_MACH_LGE_L9II_COMMON)
#define INCREASE_MA_MSEC_LONG	60000
#elif defined(CONFIG_MACH_MSM8930_FX3)
#define INCREASE_MA_MSEC_LONG	300000
#else
#define INCREASE_MA_MSEC_LONG	180000
#endif
#define MAX_USB_TARGET_MA_COUNT	10
static void increase_usb_ma_value_worker(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct pm8921_chg_chip *chip = container_of(dwork,
		struct pm8921_chg_chip, increase_usb_ma_value_work);
	static u8 count = 0;
	static int prev_usb_target_ma = 0;
#ifdef CONFIG_LGE_PM_VZW_FAST_CHG
	int fsm_state;

	fsm_state = pm_chg_get_fsm_state(chip);
	if ((fsm_state != FSM_STATE_TRKL_CHG_8) && (usb_target_ma != 0))
		chg_state_check_count++;
	pr_info("[AICL] usb_target_ma = %d, prev_usb_target_ma = %d, count = %d, chg_state_check_count = %d \n", usb_target_ma, prev_usb_target_ma, count, chg_state_check_count);
#endif

	wake_lock(&chip->increase_usb_ma_value_wake_lock);
	if (usb_target_ma == prev_usb_target_ma) {
#ifdef CONFIG_LGE_PM_VZW_FAST_CHG
		if ((fsm_state != FSM_STATE_TRKL_CHG_8) && (usb_target_ma != 0))
			count++;

		if (count == VZW_FAST_CHG_CHK_CNT && !is_finished_chg_state_check) {
			vzw_fast_chg_ma = usb_target_ma;
			pr_info("[AICL] count = %d, vzw_fast_chg_ma = %d \n", count, vzw_fast_chg_ma);
		}

		if (usb_online && usb_connected_flag) {
			wake_unlock(&chip->increase_usb_ma_value_wake_lock);
			return;
		}
#else
		count++;
#endif
		if (count > MAX_USB_TARGET_MA_COUNT) {
			count = MAX_USB_TARGET_MA_COUNT;
		}
	} else {
#ifdef CONFIG_LGE_PM_VZW_FAST_CHG
		if (chg_state_check_count > 7) {
			count = MAX_USB_TARGET_MA_COUNT;
			chg_state_check_count =0 ;
			if (!is_finished_chg_state_check) {
				vzw_fast_chg_ma = prev_usb_target_ma;
				pr_info("[AICL] chg_state_check_count = %d, vzw_fast_chg_ma = %d \n", chg_state_check_count, vzw_fast_chg_ma);
			}
		} else {
#endif
		count = 0;
#ifdef CONFIG_LGE_PM_VZW_FAST_CHG
		}
#endif
	}
	prev_usb_target_ma = usb_target_ma;
	if (is_usb_chg_plugged_in(the_chip)
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
		&& chg_batt_temp_state == CHG_BATT_NORMAL_STATE
#endif
		&& usb_target_ma < lge_pm_get_ta_current()
#ifndef CONFIG_MACH_MSM8930_FX3
		&& count < MAX_USB_TARGET_MA_COUNT
#endif
	)
	{
		increase_usb_ma_value(&usb_target_ma);
	}
	wake_unlock(&chip->increase_usb_ma_value_wake_lock);
	if (count >= MAX_USB_TARGET_MA_COUNT) {
		schedule_delayed_work(&chip->increase_usb_ma_value_work,
			round_jiffies_relative(msecs_to_jiffies(INCREASE_MA_MSEC_LONG)));
	} else {
		schedule_delayed_work(&chip->increase_usb_ma_value_work,
			round_jiffies_relative(msecs_to_jiffies(INCREASE_MA_MSEC)));
	}
}
#endif

#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
static void monitor_batt_temp(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct pm8921_chg_chip *chip = container_of(dwork,
				struct pm8921_chg_chip, monitor_batt_temp_work);
	int batt_temp;
	int batt_volt;
	int stop_charging = 0;
	int ret = 0;

	/*                                                                     */
	int ma = 0;
	struct pm8xxx_adc_chan_result xo_therm;
	int iusb_pre = 0;
#ifdef CONFIG_MACH_MSM8960_L1A
	int mitigation_current = usb_ma_table[1].usb_ma;
#else
	int mitigation_current = usb_ma_table[2].usb_ma;
#endif
	int xo_mitigation = 0;
	/*                          */
#ifdef CONFIG_LGE_PM_BOOST_IC
	static int batt_volts_temp;
#endif
	if (is_factory_cable())
		batt_temp = 250;
	else
		batt_temp = get_prop_batt_temp(chip);

/*           
                            
                                
*/
	g_batt_temp = batt_temp;
#ifdef CONFIG_BATTERY_MAX17043
	batt_volt = max17043_get_voltage();
#else
	batt_volt = get_prop_battery_uvolts(chip);
#endif
#if defined(CONFIG_LGE_PM_BOOST_IC)
	batt_volts_temp = batt_volt / 1000;
#endif

#ifdef CONFIG_LGE_PM_BOOST_IC
	if (batt_volts_temp < BOOST_ENABLE_THRESHOLD) {
		boost_enable_flag = true;
	} else {
		boost_enable_flag = false;
	}

#endif
	stop_charging = chg_update_charging_state(chip, batt_temp, batt_volt);
	if(wake_lock_active(&chip->monitor_batt_temp_wake_lock) && !is_usb_chg_plugged_in(chip) && !is_dc_chg_plugged_in(chip)) {
		printk(KERN_INFO "[PM] monitor_batt_temp: Release wake lock , no charger\n");
		wake_unlock(&chip->monitor_batt_temp_wake_lock);
	}
	if (stop_charging == 1 && last_stop_charging == 0) {
		if(wake_lock_active(&chip->eoc_wake_lock)) {
			printk(KERN_INFO "[PM] monitor_batt_temp: Charging stop & wake lock by Temperature Scenario\n");
			wake_lock(&chip->monitor_batt_temp_wake_lock);
		}
		pm_chg_auto_enable(chip, 0);
	}
	else if (stop_charging == 0 && last_stop_charging == 1) {
		pseudo_ui_charging = 0;
		pm_chg_auto_enable(chip, 1);
		if(wake_lock_active(&chip->monitor_batt_temp_wake_lock)) {
			printk(KERN_INFO "[PM] monitor_batt_temp: Release wake lock by Temperature Scenario\n");
			wake_unlock(&chip->monitor_batt_temp_wake_lock);
		}
	}
	else if (stop_charging && last_stop_charging) {
		pm_chg_auto_enable(chip, 0);
	}
	else {
		pseudo_ui_charging = 0;
		pm_chg_auto_enable(chip, 1);
	}
	last_stop_charging = stop_charging;

	if(chip->thermal_mitigation_method == IUSB_REDUCE_METHOD)
	{
		/*
                                                                        
   
                                                                    
                                                                        
                                                                             
                                                  
                                                                       
  */
		pm8xxx_adc_read(CHANNEL_MUXOFF,&xo_therm);
		pr_debug("[Thermal] xo_therm = %lld , thermal_mitigation = %d\n",
			xo_therm.physical, thermal_mitigation);

		if(xo_therm.physical > (thermal_mitigation * 1000) )
			xo_mitigation = 1;
		else
			xo_mitigation = 0;

		pr_debug("[Thermal] last_stop_charging = %d. chg_state = %d. ac usb type = %d\n",
			last_stop_charging, chg_batt_temp_state, ac_online);

		if ((!last_stop_charging) && (chg_batt_temp_state != CHG_BATT_DC_CURRENT_STATE)) {
			if(ac_online){
				if ((xo_mitigation == 1) && (xo_mitigation != pre_xo_mitigation)){
					pm_chg_iusbmax_get(chip, &iusb_pre);
					pm8921_charger_vbus_draw(mitigation_current); /* set 700mA*/
					pr_err("[Thermal] resetting current iusb_pre = %d : mitigation current = %d\n",
						iusb_pre, mitigation_current);

					pre_xo_mitigation = xo_mitigation;
				}else if ((xo_mitigation == 0) && (xo_mitigation != pre_xo_mitigation)){
					ma = lge_pm_get_ta_current();
					pm_chg_iusbmax_get(chip, &iusb_pre);
					pm8921_charger_vbus_draw(ma); /* set max iusb current*/
					pr_err("[Thermal] resetting current iusb_pre = %d : retore current = %d\n",
						iusb_pre, ma);
					pre_xo_mitigation = xo_mitigation;
				}
				else{
					pr_debug("[Thermal] no mitigation : current mitigation = %d / pre= %d\n",
						xo_mitigation, pre_xo_mitigation);
				}
			}
			else {
			}
		}
		/*                          */
	}
	power_supply_changed(&chip->batt_psy);
	ret = schedule_delayed_work(&chip->monitor_batt_temp_work,
			      round_jiffies_relative(msecs_to_jiffies
						     (MONITOR_BATT_TEMP_PERIOD_MS)));
}
#endif

#ifdef CONFIG_LGE_PM_BOOST_IC
#define BOOST_IC_THR_DELTA_MV 50
#ifdef CONFIG_MACH_LGE_FX3Q_TMUS
#define CHECK_COUNT 3
#endif
/*                                             
                                 
                                                
                              
                                    */
static void monitor_boost_ic(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct pm8921_chg_chip *chip = container_of(dwork,
				struct pm8921_chg_chip, monitor_boost_work);

	int i, ret;
	int vbat_avg_mv = 0;
	int vbat_mv;
	static int vbat_avg_samples[VBAT_AVG_SAMPLES];
#ifndef CONFIG_MACH_LGE_FX3Q_TMUS
	static int vbat_avg_index;
#endif
	static int vbat_avg_num_samples;
	static int firsttime = 1;
	static int boost_byp_thr;

#ifdef CONFIG_MACH_LGE_FX3Q_TMUS
	int before_vbat_avg_mv = 0;

	if (boost_enable_flag) {
		for (i = 0; i <= CHECK_COUNT; i++) {
			vbat_mv = get_prop_battery_uvolts(chip) /1000;
			vbat_avg_samples[i] = vbat_mv;
			vbat_avg_mv += vbat_avg_samples[i];
			msleep(200);
		}
			vbat_avg_mv = DIV_ROUND_CLOSEST(vbat_avg_mv, vbat_avg_num_samples);
			if (vbat_avg_mv > BOOST_ENABLE_THRESHOLD) {
				pr_info ("[BOOST-IC] Not yet enable level : %d \n", vbat_avg_mv);
				return;
			}
	} else {
		return;
	}
#else
	vbat_mv = get_prop_battery_uvolts(chip) / 1000;
#endif
	/* firsttime gpio request */
	if (firsttime) {
		ret = gpio_request(chip->boost_byp_sw_gpio, "boost_byp_sw_en");

		if (ret) {
			pr_err("BOOST_BYP_SW_EN gpio_request failed :%d\n", ret);
			goto gpio_fail;
		}

		ret = gpio_direction_output(chip->boost_byp_sw_gpio, 0);

		if (ret) {
			pr_err("boost gpio_direction_output failed : %d\n", ret);
			goto gpio_fail;
		}
	}
#ifndef CONFIG_MACH_LGE_FX3Q_TMUS
	/* if we are first time fill all the samples
	 * with the first measured vbat */
	if (firsttime) {
		for (i = 0; i < VBAT_AVG_SAMPLES; i++)
			vbat_avg_samples[i] = vbat_mv;

		vbat_avg_index = 0;
		vbat_avg_num_samples = VBAT_AVG_SAMPLES;
	}
#endif
#ifdef CONFIG_MACH_LGE_FX3Q_TMUS
	vbat_mv = get_prop_battery_uvolts(chip) / 1000;
#else
	vbat_avg_samples[vbat_avg_index] = vbat_mv;
	vbat_avg_index = (vbat_avg_index + 1) % VBAT_AVG_SAMPLES;
	vbat_avg_num_samples++;

	if(vbat_avg_num_samples >= VBAT_AVG_SAMPLES)
		vbat_avg_num_samples = VBAT_AVG_SAMPLES;

	if (vbat_avg_num_samples != 0) {
		for (i = 0; i < vbat_avg_num_samples; i++)
			vbat_avg_mv += vbat_avg_samples[i];

		vbat_avg_mv = DIV_ROUND_CLOSEST(vbat_avg_mv, vbat_avg_num_samples);
	}
#endif
#ifdef CONFIG_MACH_LGE_FX3Q_TMUS
	for ( i = 0 ; i <= VBAT_AVG_SAMPLES ; i++) {
		vbat_avg_mv += vbat_avg_samples[i];
	}
	vbat_avg_mv = DIV_ROUND_CLOSEST(vbat_avg_mv, vbat_avg_num_samples);

	pr_debug("vbat_avg_mv: %d (vbatt: %d)\n", vbat_avg_mv, vbat_mv);

	/* change boost_byp_thr according to vbat_avg_ma
	 * chip->boost_byp_thr(3600mV) when vbat is increasing
	 * chip->boost_byp_thr - BOOST_IC_THR_DELTA_MV
	 *   (3550mV) when vbat is decreasig */
	if (vbat_avg_mv < before_vbat_avg_mv)
		boost_byp_thr = chip->boost_byp_thr + BOOST_IC_THR_DELTA_MV;
	 else if (vbat_avg_mv > before_vbat_avg_mv)
		boost_byp_thr = chip->boost_byp_thr - BOOST_IC_THR_DELTA_MV;
	 else
		boost_byp_thr = chip->boost_byp_thr;

	pr_debug("IC Threshold = %d\n", boost_byp_thr);
#else
	if (firsttime) {
		if (vbat_avg_mv > chip->boost_byp_thr - BOOST_IC_THR_DELTA_MV)
			boost_byp_thr = chip->boost_byp_thr - BOOST_IC_THR_DELTA_MV;
		else
			boost_byp_thr = chip->boost_byp_thr;
		pr_debug("boost_byp_thr = %d\n", boost_byp_thr);
	}
#endif
	if (vbat_avg_mv <= boost_byp_thr) {
		if (chip->boost_byp_sw_enable == 0) {
			gpio_set_value(chip->boost_byp_sw_gpio, 1);
			chip->boost_byp_sw_enable = 1;
			boost_byp_thr = chip->boost_byp_thr;
			pr_info("BOOST_BYP_SW_EN: 1 boost_byp_thr = %d\n", boost_byp_thr);
		}
	} else {
		if (chip->boost_byp_sw_enable == 1) {
			gpio_set_value(chip->boost_byp_sw_gpio, 0);
			chip->boost_byp_sw_enable = 0;
			boost_byp_thr = chip->boost_byp_thr - BOOST_IC_THR_DELTA_MV;
			pr_info("BOOST_BYP_SW_EN: 0 boost_byp_thr = %d\n", boost_byp_thr);
		}
	}
	firsttime = 0;

#ifdef CONFIG_MACH_LGE_FX3Q_TMUS
	before_vbat_avg_mv = vbat_avg_mv;
#endif

	ret = schedule_delayed_work(&chip->monitor_boost_work,
			      round_jiffies_relative(msecs_to_jiffies
						     (BOOST_MONITOR_MS)));
	return;

gpio_fail:
	gpio_free(chip->boost_byp_sw_gpio);
}
/*              */
#endif /*                        */

static int pm8921_charger_resume(struct device *dev)
{
	int rc;
	struct pm8921_chg_chip *chip = dev_get_drvdata(dev);

#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS) || defined(CONFIG_MACH_LGE_F6_TMUS)
	if (chip->usb_present) {
		if (!delayed_work_pending(&chip->monitor_batt_temp_work))
			rc = schedule_delayed_work(&chip->monitor_batt_temp_work,
					round_jiffies_relative(msecs_to_jiffies
						(MONITOR_BATT_TEMP_START_MS)));
	}
#endif
/*                                                                                        */
	if (!(chip->cool_temp_dc == INT_MIN && chip->warm_temp_dc == INT_MIN)
/*                                        */
		&& !(chip->keep_btm_on_suspend)) {
		rc = pm8xxx_adc_btm_configure(&btm_config);
		if (rc)
			pr_err("couldn't reconfigure btm rc=%d\n", rc);

		rc = pm8xxx_adc_btm_start();
		if (rc)
			pr_err("couldn't restart btm rc=%d\n", rc);
	}
	if (pm8921_chg_is_enabled(chip, LOOP_CHANGE_IRQ)) {
		disable_irq_wake(chip->pmic_chg_irq[LOOP_CHANGE_IRQ]);
		pm8921_chg_disable_irq(chip, LOOP_CHANGE_IRQ);
	}
	return 0;
}

static int pm8921_charger_suspend(struct device *dev)
{
	int rc;
	struct pm8921_chg_chip *chip = dev_get_drvdata(dev);

#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS) || defined(CONFIG_MACH_LGE_F6_TMUS)
	if (chip->usb_present)
		rc = cancel_delayed_work_sync(&chip->monitor_batt_temp_work);
#endif

/*                                                                                        */
#ifdef CONFIG_MACH_LGE
	if (!(chip->cool_temp_dc == 0 && chip->warm_temp_dc == 0)
#else /* QCT_ORG */
	if (!(chip->cool_temp_dc == INT_MIN && chip->warm_temp_dc == INT_MIN)
#endif
/*                                        */

		&& !(chip->keep_btm_on_suspend)) {
		rc = pm8xxx_adc_btm_end();
		if (rc)
			pr_err("Failed to disable BTM on suspend rc=%d\n", rc);
	}

/* Battery alarm for the model using BMS */
#if defined(CONFIG_LGE_PM) && !defined(CONFIG_BATTERY_MAX17043)
	if (batt_alarm_disabled == 1) {
		batt_alarm_disabled = 0;
		rc = pm8xxx_batt_alarm_enable(0);	// PM8XXX_BATT_ALARM_LOWER_COMPARATOR - 0
		if (rc)
			pr_err(	"[batt_alarm] pm8xxx_batt_alarm_enable : unable to set alarm state. rc = %d\n", rc);
		pr_info("[batt_alarm] enable the alarm.\n");
	}
#endif

	if (is_usb_chg_plugged_in(chip)) {
#ifdef CONFIG_LGE_PM
		pr_info("[POWER] pm8921_charger_suspend, and usb_chg_plugged_in TRUE \n");
#endif
		pm8921_chg_enable_irq(chip, LOOP_CHANGE_IRQ);
		enable_irq_wake(chip->pmic_chg_irq[LOOP_CHANGE_IRQ]);
	}

	return 0;
}
static int __devinit pm8921_charger_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct pm8921_chg_chip *chip;
	const struct pm8921_charger_platform_data *pdata
				= pdev->dev.platform_data;

	if (!pdata) {
		pr_err("missing platform data\n");
		return -EINVAL;
	}

	chip = kzalloc(sizeof(struct pm8921_chg_chip),
					GFP_KERNEL);
	if (!chip) {
		pr_err("Cannot allocate pm_chg_chip\n");
		return -ENOMEM;
	}

	chip->dev = &pdev->dev;
	chip->safety_time = pdata->safety_time;
	chip->ttrkl_time = pdata->ttrkl_time;
	chip->update_time = pdata->update_time;
/*                           
                                                     
                                  
*/
#if defined(CONFIG_MACH_MSM8960_L0)
	if(lge_get_board_revno() < HW_REV_D)
		chip->max_voltage_mv = 4200;
	else
#endif
	chip->max_voltage_mv = pdata->max_voltage;
	chip->alarm_low_mv = pdata->alarm_low_mv;
	chip->alarm_high_mv = pdata->alarm_high_mv;
	chip->min_voltage_mv = pdata->min_voltage;
	chip->uvd_voltage_mv = pdata->uvd_thresh_voltage;
	chip->resume_voltage_delta = pdata->resume_voltage_delta;
	chip->resume_charge_percent = pdata->resume_charge_percent;
	chip->term_current = pdata->term_current;
	chip->vbat_channel = pdata->charger_cdata.vbat_channel;
	chip->batt_temp_channel = pdata->charger_cdata.batt_temp_channel;
	chip->batt_id_channel = pdata->charger_cdata.batt_id_channel;
	chip->batt_id_min = pdata->batt_id_min;
	chip->batt_id_max = pdata->batt_id_max;
	if (pdata->cool_temp != INT_MIN)
		chip->cool_temp_dc = pdata->cool_temp * 10;
	else
		chip->cool_temp_dc = INT_MIN;

	if (pdata->warm_temp != INT_MIN)
		chip->warm_temp_dc = pdata->warm_temp * 10;
	else
		chip->warm_temp_dc = INT_MIN;

	chip->temp_check_period = pdata->temp_check_period;
#ifndef CONFIG_LGE_PM
	chip->dc_unplug_check = pdata->dc_unplug_check;
#endif
	chip->max_bat_chg_current = pdata->max_bat_chg_current;
	/* Assign to corresponding module parameter */
	usb_max_current = pdata->usb_max_current;
	chip->cool_bat_chg_current = pdata->cool_bat_chg_current;
	chip->warm_bat_chg_current = pdata->warm_bat_chg_current;
	chip->cool_bat_voltage = pdata->cool_bat_voltage;
	chip->warm_bat_voltage = pdata->warm_bat_voltage;
	chip->keep_btm_on_suspend = pdata->keep_btm_on_suspend;
	chip->trkl_voltage = pdata->trkl_voltage;
	chip->weak_voltage = pdata->weak_voltage;
	chip->trkl_current = pdata->trkl_current;
	chip->weak_current = pdata->weak_current;
	chip->vin_min = pdata->vin_min;
	chip->thermal_mitigation = pdata->thermal_mitigation;
	chip->thermal_levels = pdata->thermal_levels;

	chip->cold_thr = pdata->cold_thr;
	chip->hot_thr = pdata->hot_thr;
#ifdef CONFIG_LGE_PM
	chip->rconn_mohm = 0;
#else
	chip->rconn_mohm = pdata->rconn_mohm;
#endif
	chip->led_src_config = pdata->led_src_config;
#ifndef CONFIG_LGE_PM
	chip->has_dc_supply = pdata->has_dc_supply;
#endif
#ifdef CONFIG_MACH_MSM8930_FX3
	chip->aicl = pdata->aicl;;
#endif

#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	last_stop_charging = 0;
	chg_batt_temp_state = CHG_BATT_NORMAL_STATE;
	pseudo_ui_charging = 0;
/*                                                                     */
	pre_xo_mitigation = 0;
	thermal_mitigation = 100; /* default 100 DegC */
/*                          */
/*                                                                          */
/*                                       */
/*           
                                    
                                
*/
	chip->thermal_mitigation_method = pdata->thermal_mitigation_method;
/*                               */
#endif
/*
                      
                                          
                                                
      
*/
	rc = pm8921_chg_hw_init(chip);
	if (rc) {
		pr_err("couldn't init hardware rc=%d\n", rc);
		goto free_chip;
	}

	chip->stop_chg_upon_expiry = pdata->stop_chg_upon_expiry;
#ifdef CONFIG_MACH_LGE_FX3_VZW
	stop_chg_upon_expiry_state = chip->stop_chg_upon_expiry;
#endif

	chip->usb_psy.name = "usb",
	chip->usb_psy.type = POWER_SUPPLY_TYPE_USB,
	chip->usb_psy.supplied_to = pm_power_supplied_to,
	chip->usb_psy.num_supplicants = ARRAY_SIZE(pm_power_supplied_to),
	chip->usb_psy.properties = pm_power_props_usb,
	chip->usb_psy.num_properties = ARRAY_SIZE(pm_power_props_usb),
	chip->usb_psy.get_property = pm_power_get_property_usb,
	chip->usb_psy.set_property = pm_power_set_property_usb,

#ifdef CONFIG_LGE_PM
	chip->dc_psy.name = "ac",
#else
	chip->dc_psy.name = "pm8921-dc",
#endif
	chip->dc_psy.type = POWER_SUPPLY_TYPE_MAINS,
	chip->dc_psy.supplied_to = pm_power_supplied_to,
	chip->dc_psy.num_supplicants = ARRAY_SIZE(pm_power_supplied_to),
	chip->dc_psy.properties = pm_power_props_mains,
	chip->dc_psy.num_properties = ARRAY_SIZE(pm_power_props_mains),
	chip->dc_psy.get_property = pm_power_get_property_mains,
/*                                                                 */
#ifdef CONFIG_LGE_PM
	chip->dc_psy.set_property = pm_power_set_property_mains,
#endif
/*                                        */

	chip->batt_psy.name = "battery",
	chip->batt_psy.type = POWER_SUPPLY_TYPE_BATTERY,
	chip->batt_psy.properties = msm_batt_power_props,
	chip->batt_psy.num_properties = ARRAY_SIZE(msm_batt_power_props),
	chip->batt_psy.get_property = pm_batt_power_get_property,
	chip->batt_psy.external_power_changed = pm_batt_external_power_changed,
	rc = power_supply_register(chip->dev, &chip->usb_psy);
	if (rc < 0) {
		pr_err("power_supply_register usb failed rc = %d\n", rc);
		goto free_chip;
	}

	rc = power_supply_register(chip->dev, &chip->dc_psy);
	if (rc < 0) {
#ifdef CONFIG_LGE_PM
		pr_err("power_supply_register dc failed rc = %d\n", rc);
#else
		pr_err("power_supply_register usb failed rc = %d\n", rc);
#endif
		goto unregister_usb;
	}

	rc = power_supply_register(chip->dev, &chip->batt_psy);
	if (rc < 0) {
		pr_err("power_supply_register batt failed rc = %d\n", rc);
		goto unregister_dc;
	}

	platform_set_drvdata(pdev, chip);
	the_chip = chip;

	wake_lock_init(&chip->eoc_wake_lock, WAKE_LOCK_SUSPEND, "pm8921_eoc");

#if defined(CONFIG_LGE_PM) && !defined(CONFIG_BATTERY_MAX17043)
	wake_lock_init(&chip->batt_alarm_wake_up,
			WAKE_LOCK_SUSPEND, "pm8921_batt_alarm_wakeup");
#endif

	INIT_DELAYED_WORK(&chip->eoc_work, eoc_worker);
	INIT_DELAYED_WORK(&chip->vin_collapse_check_work,
						vin_collapse_check_worker);
	INIT_DELAYED_WORK(&chip->unplug_check_work, unplug_check_worker);
#ifdef CONFIG_MACH_MSM8930_FX3
	INIT_DELAYED_WORK(&chip->increase_usb_ma_value_work,
				increase_usb_ma_value_worker);
#endif
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	INIT_DELAYED_WORK(&chip->monitor_batt_temp_work,
				monitor_batt_temp);
#endif

	INIT_WORK(&chip->bms_notify.work, bms_notify);
	INIT_WORK(&chip->battery_id_valid_work, battery_id_valid);

	INIT_DELAYED_WORK(&chip->update_heartbeat_work, update_heartbeat);

	rc = request_irqs(chip, pdev);
	if (rc) {
		pr_err("couldn't register interrupts rc=%d\n", rc);
		goto unregister_batt;
	}

	enable_irq_wake(chip->pmic_chg_irq[USBIN_VALID_IRQ]);
	enable_irq_wake(chip->pmic_chg_irq[DCIN_VALID_IRQ]);
#ifndef CONFIG_LGE_PM
	enable_irq_wake(chip->pmic_chg_irq[BAT_TEMP_OK_IRQ]);
#endif
	enable_irq_wake(chip->pmic_chg_irq[VBATDET_LOW_IRQ]);
	enable_irq_wake(chip->pmic_chg_irq[FASTCHG_IRQ]);
	/*
	 * if both the cool_temp_dc and warm_temp_dc are invalid device doesnt
	 * care for jeita compliance
	 */
	if (!(chip->cool_temp_dc == INT_MIN && chip->warm_temp_dc == INT_MIN)) {
		rc = configure_btm(chip);
		if (rc) {
			pr_err("couldn't register with btm rc=%d\n", rc);
			goto free_irq;
		}
	}

	rc = pm8921_charger_configure_batt_alarm(chip);
	if (rc) {
		pr_err("Couldn't configure battery alarm! rc=%d\n", rc);
		goto free_irq;
	}

	rc = pm8921_charger_enable_batt_alarm(chip);
	if (rc) {
		pr_err("Couldn't enable battery alarm! rc=%d\n", rc);
		goto free_irq;
	}
	create_debugfs_entries(chip);

#ifdef CONFIG_MACH_LGE
#ifdef CONFIG_MACH_MSM8930_FX3
	rc = device_create_file(&pdev->dev, &dev_attr_aicl);
	rc = device_create_file(&pdev->dev, &dev_attr_cable_type);
	rc = device_create_file(&pdev->dev, &dev_attr_iusbmax);
#endif
	rc = device_create_file(&pdev->dev, &dev_attr_at_current);
	rc = device_create_file(&pdev->dev, &dev_attr_at_charge);
	rc = device_create_file(&pdev->dev, &dev_attr_at_chcomp);
	rc = device_create_file(&pdev->dev, &dev_attr_at_pmrst);
	rc = device_create_file(&pdev->dev, &dev_attr_at_vcoin);
	rc = device_create_file(&pdev->dev, &dev_attr_at_battemp);
	rc = device_create_file(&pdev->dev, &dev_attr_at_pmoff);
#endif
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	rc = device_create_file(&pdev->dev, &dev_attr_batt_temp_adc);
#endif

#ifdef CONFIG_LGE_PM
	wake_lock_init(&chip->increase_usb_ma_value_wake_lock, WAKE_LOCK_SUSPEND, "increase_usb_ma_value_wl");
#endif
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	wake_lock_init(&chip->monitor_batt_temp_wake_lock, WAKE_LOCK_SUSPEND,"pm8921_monitor_batt_temp");
#endif

#ifdef CONFIG_LGE_PM_BOOST_IC
		chip->boost_byp_sw_gpio = pdata->boost_byp_sw_gpio;
		chip->boost_byp_thr = pdata->boost_byp_thr;
		chip->boost_byp_sw_enable = 0;

		INIT_DELAYED_WORK(&chip->monitor_boost_work,
							monitor_boost_ic);
		rc = schedule_delayed_work(&chip->monitor_boost_work,
				round_jiffies_relative(msecs_to_jiffies(BOOST_MONITOR_MS)));
#endif /*                        */

/* Battery alarm for the model using BMS */
#if 0 //                                                           
	rc = pm8xxx_batt_alarm_disable(1);	// PM8XXX_BATT_ALARM_UPPER_COMPARATOR - 1

	if (rc)
		pr_err(	"[batt_alarm] pm8xxx_batt_alarm_disable : unable to set alarm state. rc = %d\n", rc);

	rc = pm8xxx_batt_alarm_disable(1);	// PM8XXX_BATT_ALARM_LOWER_COMPARATOR - 0

	if (rc)
		pr_err(	"[batt_alarm] pm8xxx_batt_alarm_enable : unable to set alarm state. rc = %d\n", rc);

	rc = pm8xxx_batt_alarm_threshold_set(1, BATTERY_UPPER_THRESHOLD);	// PM8XXX_BATT_ALARM_UPPER_COMPARATOR - 1

	if (rc)
		pr_err("[batt_alarm] threshold_set failed 1, rc=%d\n", rc);

	rc = pm8xxx_batt_alarm_threshold_set(0, BATTERY_LOW_THRESHOLD);	// PM8XXX_BATT_ALARM_LOWER_COMPARATOR - 0

	if (rc)
		pr_err("[batt_alarm] threshold_set failed 0, rc=%d\n", rc);

	rc = pm8xxx_batt_alarm_hold_time_set(7);	// PM8XXX_BATT_ALARM_HOLD_TIME_16_MS - 7

	if (rc)
		pr_err("[batt_alarm] pm8xxx_batt_alarm_hold_time_set : unable to set batt alarm hold time. rc = %d\n", rc);

	/* PWM enabled at 2Hz */
	rc = pm8xxx_batt_alarm_pwm_rate_set(1, 7, 4);

	if (rc)
		pr_err("[batt_alarm] pm8xxx_batt_alarm_pwm_rate_set : unable to set batt alarm pwm rate. rc = %d\n", rc);

	rc = pm8xxx_batt_alarm_register_notifier(&alarm_notifier);

	if (rc)
		pr_err("[batt_alarm] pm8xxx_batt_alarm_register_notifier : unable to register alarm notifier. rc = %d\n", rc);

#endif

#ifdef CONFIG_LGE_PM
	wake_lock_init(&uevent_wake_lock, WAKE_LOCK_SUSPEND, "pm8921_chg_uevent");
#endif

#ifdef CONFIG_MACH_MSM8930_FX3
	rc = pm8xxx_smpl_set_delay(PM8XXX_SMPL_DELAY_0p5);
	if (rc < 0)
		pr_err("%s: failed to config smpl delay: %d\n",
								__func__, rc);
	is_factory_cable();
#endif

	/* determine what state the charger is in */
	determine_initial_state(chip);

	if (chip->update_time)
		schedule_delayed_work(&chip->update_heartbeat_work,
				      round_jiffies_relative(msecs_to_jiffies
							(chip->update_time)));
	return 0;

free_irq:
	free_irqs(chip);
unregister_batt:
	power_supply_unregister(&chip->batt_psy);
unregister_dc:
	power_supply_unregister(&chip->dc_psy);
unregister_usb:
	power_supply_unregister(&chip->usb_psy);
free_chip:
	kfree(chip);
	return rc;
}

static int __devexit pm8921_charger_remove(struct platform_device *pdev)
{
	struct pm8921_chg_chip *chip = platform_get_drvdata(pdev);

#ifdef CONFIG_LGE_PM
#ifdef CONFIG_MACH_MSM8930_FX3
	device_remove_file(&pdev->dev, &dev_attr_aicl);
	device_remove_file(&pdev->dev, &dev_attr_cable_type);
	device_remove_file(&pdev->dev, &dev_attr_iusbmax);
#endif
	device_remove_file(&pdev->dev, &dev_attr_at_current); /* sungsookim */
	device_remove_file(&pdev->dev, &dev_attr_at_charge);
	device_remove_file(&pdev->dev, &dev_attr_at_chcomp);
	device_remove_file(&pdev->dev, &dev_attr_at_pmrst);
	device_remove_file(&pdev->dev, &dev_attr_at_vcoin);
	device_remove_file(&pdev->dev, &dev_attr_at_battemp);
	device_remove_file(&pdev->dev, &dev_attr_at_pmoff);
#endif
#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
    device_remove_file(&pdev->dev, &dev_attr_batt_temp_adc);
#endif
#ifdef CONFIG_LGE_PM
	wake_lock_destroy(&uevent_wake_lock);
#endif
#ifdef CONFIG_LGE_PM_BOOST_IC
	gpio_free(chip->boost_byp_sw_gpio);
#endif /*                        */
	free_irqs(chip);
	platform_set_drvdata(pdev, NULL);
	the_chip = NULL;
	kfree(chip);
	return 0;
}
static const struct dev_pm_ops pm8921_pm_ops = {
	.suspend	= pm8921_charger_suspend,
	.suspend_noirq  = pm8921_charger_suspend_noirq,
	.resume_noirq   = pm8921_charger_resume_noirq,
	.resume		= pm8921_charger_resume,
};
static struct platform_driver pm8921_charger_driver = {
	.probe		= pm8921_charger_probe,
	.remove		= __devexit_p(pm8921_charger_remove),
	.driver		= {
			.name	= PM8921_CHARGER_DEV_NAME,
			.owner	= THIS_MODULE,
			.pm	= &pm8921_pm_ops,
	},
};

static int __init pm8921_charger_init(void)
{
	return platform_driver_register(&pm8921_charger_driver);
}

static void __exit pm8921_charger_exit(void)
{
	platform_driver_unregister(&pm8921_charger_driver);
}

late_initcall(pm8921_charger_init);
module_exit(pm8921_charger_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PMIC8921 charger/battery driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:" PM8921_CHARGER_DEV_NAME);
