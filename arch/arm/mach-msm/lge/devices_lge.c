/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/i2c/sx150x.h>
#include <linux/gpio.h>
#include <linux/usb/android_composite.h>
#include <linux/msm_ssbi.h>
#include <linux/regulator/msm-gpio-regulator.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>
#include <linux/slimbus/slimbus.h>
#include <linux/bootmem.h>
#include <linux/msm_kgsl.h>
#ifdef CONFIG_ANDROID_PMEM
#include <linux/android_pmem.h>
#endif
#include <linux/dma-mapping.h>
#include <linux/platform_data/qcom_crypto_device.h>
#include <linux/platform_data/qcom_wcnss_device.h>
#include <linux/leds.h>
#include <linux/leds-pm8xxx.h>
#include <linux/msm_tsens.h>
#include <linux/memory.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/setup.h>
#include <asm/hardware/gic.h>
#include <asm/mach/mmc.h>

#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <mach/msm_spi.h>
#ifdef CONFIG_USB_MSM_OTG_72K
#include <mach/msm_hsusb.h>
#else
#include <linux/usb/msm_hsusb.h>
#endif
#include <mach/usbdiag.h>
#include <mach/socinfo.h>
#include <mach/rpm.h>
#include <mach/gpio.h>
#include <mach/gpiomux.h>
#include <mach/msm_bus_board.h>
#include <mach/msm_memtypes.h>
#include <mach/dma.h>
#include <mach/msm_dsps.h>
#include <mach/msm_xo.h>
#include "pm.h"
#include <mach/cpuidle.h>

#include <mach/board_lge.h> 
#include "./fx3/board-fx3.h"

#if 0 // CONFIG_MACH_MSM8930_LGPS9
#ifdef CONFIG_WCD9310_CODEC
#include <linux/slimbus/slimbus.h>
#include <linux/mfd/wcd9310/core.h>
#include <linux/mfd/wcd9310/pdata.h>
#endif

#include <linux/ion.h>
#include <mach/ion.h>

#include <mach/board_lge.h>

#include "timer.h"
#include "devices.h"
#include "devices-msm8x60.h"
#include "spm.h"
#include "rpm_resources.h"
#include "rpm_log.h"
#include "msm_watchdog.h"
#endif

/*                     */
#define MAX_GPIO_I2C_DEV_NUM     20
#define LOWEST_GPIO_I2C_BUS_NUM	 0
#define MSM8960_I2C_DEV_NUM_MAX     20
#define MSM8960_I2C_DEV_NUM_START   0

static int msm_i2c_dev_num __initdata;
static struct i2c_registry* msm_i2c_devices[MSM8960_I2C_DEV_NUM_MAX] __initdata;

static int gpio_i2c_dev_num __initdata;
static gpio_i2c_init_func_t *i2c_init_func[MAX_GPIO_I2C_DEV_NUM] __initdata;

extern int get_detected_cable(void);

struct chg_cable_info_table {
	int threshhold;
	acc_cable_type type;
	unsigned ta_ma;
	unsigned usb_ma;
};


static struct chg_cable_info_table pm8921_acc_cable_type_data[]={
	{ADC_NO_INIT_CABLE, NO_INIT_CABLE,  C_NO_INIT_TA_MA,    C_NO_INIT_USB_MA},
	{ADC_CABLE_MHL_1K,  CABLE_MHL_1K,   C_MHL_1K_TA_MA,     C_MHL_1K_USB_MA},
	{ADC_CABLE_U_28P7K, CABLE_U_28P7K,  C_U_28P7K_TA_MA,    C_U_28P7K_USB_MA},
	{ADC_CABLE_28P7K,   CABLE_28P7K,    C_28P7K_TA_MA,      C_28P7K_USB_MA},
	{ADC_CABLE_56K,     CABLE_56K,      C_56K_TA_MA,        C_56K_USB_MA},
	{ADC_CABLE_100K,    CABLE_100K,     C_100K_TA_MA,       C_100K_USB_MA},
	{ADC_CABLE_130K,    CABLE_130K,     C_130K_TA_MA,       C_130K_USB_MA},
	{ADC_CABLE_180K,    CABLE_180K,     C_180K_TA_MA,       C_180K_USB_MA},
	{ADC_CABLE_200K,    CABLE_200K,     C_200K_TA_MA,       C_200K_USB_MA},
	{ADC_CABLE_220K,    CABLE_220K,     C_220K_TA_MA,       C_220K_USB_MA},
	{ADC_CABLE_270K,    CABLE_270K,     C_270K_TA_MA,       C_270K_USB_MA},
	{ADC_CABLE_330K,    CABLE_330K,     C_330K_TA_MA,       C_330K_USB_MA},
	{ADC_CABLE_620K,    CABLE_620K,     C_620K_TA_MA,       C_620K_USB_MA},
	{ADC_CABLE_910K,    CABLE_910K,     C_910K_TA_MA,       C_910K_USB_MA},
	{ADC_CABLE_NONE,    CABLE_NONE,     C_NONE_TA_MA,       C_NONE_USB_MA},
};


#if 0
/*                                                                 */
#if defined(CONFIG_LGE_BROADCAST_1SEG) || defined(CONFIG_LGE_BROADCAST_TDMB)
#define MSM_GSBI10_PHYS		0x1A200000
#define MSM_GSBI10_QUP_PHYS	(MSM_GSBI10_PHYS + 0x80000)
#endif
/*                                                               */ 
#endif

/* for board revision */
static hw_rev_type lge_bd_rev = HW_REV_A;

extern unsigned int system_rev;
static int __init board_revno_setup(char *rev_info)
{
	/* CAUTION: These strings are come from LK. */
	char *rev_str[] = {"evb1", "evb2", "rev_a", "rev_b", "rev_c", "rev_d",
			"rev_e", "rev_f", "rev_g", "rev_10", "rev_11", "rev_12", "reserved"};
	int i;

	for(i=0; i< HW_REV_MAX; i++)
		if( !strncmp(rev_info, rev_str[i], 6)) {
			lge_bd_rev = (hw_rev_type) i;
			system_rev = lge_bd_rev;
			break;
		}

	printk(KERN_INFO "BOARD : LGE %s \n", rev_str[lge_bd_rev]);
	return 1;
}
__setup("lge.rev=", board_revno_setup);

hw_rev_type lge_get_board_revno(void)
{
    return lge_bd_rev;
}

bool lge_get_board_usembhc(void)
{
#ifdef CONFIG_SWITCH_FSA8008
#if defined(CONFIG_MACH_MSM8930_LGPS9) || defined(CONFIG_MACH_MSM8930_FX3)
	return false; //FSA8008 
#endif
	return false; //FSA8008(default)
#else
	return true; //MBHC
#endif
	return true;
}

#ifdef CONFIG_LGE_PM_TRKLCHG_IN_KERNEL
static int is_battery_low = 0;
static int __init chk_is_battery_low(char *str)
{
    s32 value = simple_strtol(str, NULL, 10);
    is_battery_low = value;
    printk("%s : %s(%d)\n", __func__, is_battery_low ? "TRUE" : "FALSE", is_battery_low);

    return 1;
}
__setup("is_battery_low=", chk_is_battery_low);

int is_battery_low_in_lk(void)
{
	return is_battery_low;
}
#endif

/*           
                            
                                     
 */
 #ifdef CONFIG_LGE_PM
#define CABLE_ADC_COUNT 5
int lge_pm_get_cable_info(struct chg_cable_info *cable_info)
{
	char *type_str[] = {"NOT INIT", "MHL 1K", "U_28P7K", "28P7K", "56K",
		"100K", "130K", "180K", "200K", "220K", "270K", "330K", "620K", "910K",
		"OPEN"};

	struct pm8xxx_adc_chan_result result;
	struct chg_cable_info *info = cable_info;
	struct chg_cable_info_table *table;
	int table_size = ARRAY_SIZE(pm8921_acc_cable_type_data);
	int acc_read_value = 0;
	int acc_read_value_arg[CABLE_ADC_COUNT];
	int high = 0;
	int low = 0;
	int i, rc;
	int count = CABLE_ADC_COUNT;
    int detected_cable=0;
    	for (i=0; i<CABLE_ADC_COUNT; i++)
		acc_read_value_arg[i] = 0;

	if (!info) {
		pr_err("lge_pm_get_cable_info: invalid info parameters\n");
		return -1;
	}

	for (i = 0; i < count; i++) {
		rc = pm8xxx_adc_mpp_config_read(0x6,
				ADC_MPP_1_AMUX6, &result);

	if (rc < 0) {
			if (rc == -ETIMEDOUT) {
				/* reason: adc read timeout, assume it is open cable */
				info->cable_type = CABLE_NONE;
				info->ta_ma = C_NONE_TA_MA;
				info->usb_ma = C_NONE_USB_MA;
			}
			pr_err("lge_pm_get_cable_info: adc read error - %d\n", rc);
			return rc;
		}

		acc_read_value_arg[i] = (int)result.physical;
		acc_read_value += (int)result.physical;

		pr_info("%s: acc_read_value - %d\n", __func__, (int)result.physical);
		mdelay(10);
	}
	low = acc_read_value_arg[0];
	high = acc_read_value_arg[0];
	for (i=0;i<CABLE_ADC_COUNT;i++) {
		if (acc_read_value_arg[i] > high)
			high = acc_read_value_arg[i];
		if (acc_read_value_arg[i] < low)
			low = acc_read_value_arg[i];
	}
	acc_read_value = (acc_read_value - high - low) / (count - 2);

	info->cable_type = NO_INIT_CABLE;
	info->ta_ma = C_NO_INIT_TA_MA;
	info->usb_ma = C_NO_INIT_USB_MA;

	/* assume: adc value must be existed in ascending order */
	for (i = 0; i < table_size; i++) {
		table = &pm8921_acc_cable_type_data[i];

		if (acc_read_value <= table->threshhold) {
			info->cable_type = table->type;
			info->ta_ma = table->ta_ma;
			info->usb_ma = table->usb_ma;
			break;
		}
	}

	/* FIXME: Invalid information on cable_check*/
	#if 0
	detected_cable=get_detected_cable();
	
	table = &pm8921_acc_cable_type_data[detected_cable];

	info->cable_type = table->type;
	info->ta_ma = table->ta_ma;
	info->usb_ma = table->usb_ma;
	#endif

	pr_info("\n\n[PM]Cable detected: %d(%s)(%d, %d, %d)\n\n",
			acc_read_value, type_str[info->cable_type],
			info->ta_ma, info->usb_ma, detected_cable);

	return 0;
}

/* Belows are for using in interrupt context */
static struct chg_cable_info lge_cable_info;

acc_cable_type lge_pm_get_cable_type(void)
{
	return lge_cable_info.cable_type;
}

unsigned lge_pm_get_ta_current(void)
{
	return lge_cable_info.ta_ma;
}

unsigned lge_pm_get_usb_current(void)
{
	return lge_cable_info.usb_ma;
}

/* This must be invoked in process context */
void lge_pm_read_cable_info(void)
{
	lge_cable_info.cable_type = NO_INIT_CABLE;
	lge_cable_info.ta_ma = C_NO_INIT_TA_MA;
	lge_cable_info.usb_ma = C_NO_INIT_USB_MA;

	lge_pm_get_cable_info(&lge_cable_info);
}
#endif
/*                                        */

#ifdef CONFIG_LGE_KCAL
int g_kcal_r = 255;
int g_kcal_g = 255;
int g_kcal_b = 255;
static int __init display_kcal_setup(char *kcal)
{
	char vaild_k = 0;
	sscanf(kcal, "%d|%d|%d|%c", &g_kcal_r, &g_kcal_g, &g_kcal_b, &vaild_k );
	printk(KERN_INFO "kcal is %d|%d|%d|%c\n",
					g_kcal_r, g_kcal_g, g_kcal_b, vaild_k);

	if(vaild_k != 'K') {
		printk(KERN_INFO "kcal not calibrated yet : %d\n", vaild_k);
		g_kcal_r = g_kcal_g = g_kcal_b = 255;
		printk(KERN_INFO "set to default : %d\n", g_kcal_r);
	}
	return 1;
}
__setup("lge.kcal=", display_kcal_setup);
#endif


#if defined(CONFIG_MACH_LGE_FX3_SPCS) && defined(CONFIG_LGE_USB_DIAG_DISABLE)
static int lge_emergency_key = -1;
static int __init emergency_key_check(char *s)
{
	if (!strcmp(s, "1"))
		lge_emergency_key = 1;
	else
		lge_emergency_key = 0;

	return 1;
}
__setup("lge.usb=", emergency_key_check);

int lge_get_emergency_status(void)
{
	return lge_emergency_key;
}
#endif


#ifdef CONFIG_LGE_PM_BATTERY_ID_CHECKER
int lge_battery_info = BATT_UNKNOWN;

static int __init battery_information_setup(char *batt_info)
{
	if(!strcmp(batt_info, "ds2704_n"))
		lge_battery_info = BATT_DS2704_N;
	else if(!strcmp(batt_info, "ds2704_l"))
		lge_battery_info = BATT_DS2704_L;
	else if(!strcmp(batt_info, "isl6296_n"))
		lge_battery_info = BATT_ISL6296_N;
	else if(!strcmp(batt_info, "isl6296_l"))
		lge_battery_info = BATT_ISL6296_L;
	else if(!strcmp(batt_info, "ds2704_c"))
		lge_battery_info = BATT_DS2704_C;
	else if(!strcmp(batt_info, "isl6296_c"))
		lge_battery_info = BATT_ISL6296_C;
/*                                         
                                            
                                  
*/
#ifdef CONFIG_MACH_MSM8960_L0
	else if(lge_get_board_revno() < HW_REV_D) {
		batt_info = "ds2704_l";
		lge_battery_info = BATT_DS2704_L;
	}
#endif
	else
		lge_battery_info = BATT_UNKNOWN;

	printk(KERN_INFO "Battery : %s %d\n", batt_info, lge_battery_info);

	return 1;
}
__setup("lge.batt_info=", battery_information_setup);
#endif

/*                            */
/* get boot mode information from cmdline.
 * If any boot mode is not specified,
 * boot mode is normal type.
 */
static enum lge_boot_mode_type lge_boot_mode = LGE_BOOT_MODE_NORMAL;
int __init lge_boot_mode_init(char *s)
{
	if (!strcmp(s, "charger"))
		lge_boot_mode = LGE_BOOT_MODE_CHARGER;
	else if (!strcmp(s, "chargerlogo"))
		lge_boot_mode = LGE_BOOT_MODE_CHARGERLOGO;
	else if (!strcmp(s, "factory"))
		lge_boot_mode = LGE_BOOT_MODE_FACTORY;
	else if (!strcmp(s, "factory2"))
		lge_boot_mode = LGE_BOOT_MODE_FACTORY2;
	else if (!strcmp(s, "pifboot"))
		lge_boot_mode = LGE_BOOT_MODE_PIFBOOT;
	/*                            */
	else if (!strcmp(s, "miniOS"))
		lge_boot_mode = LGE_BOOT_MODE_MINIOS;
	printk("ANDROID BOOT MODE : %d %s\n", lge_boot_mode, s);
	/*                            */

	return 1;
}
__setup("androidboot.mode=", lge_boot_mode_init);

enum lge_boot_mode_type lge_get_boot_mode(void)
{
	return lge_boot_mode;
}
/*                            */

#if 0 // CONFIG_MACH_MSM8930_LGPS9

/* for supporting two LCDs with one image */
static int maker_id = PRIMARY;

static int __init lcd_maker_id_setup(char *lcd_maker_id)
{
	/* CAUTION : These strings are come from LK. */
	char *maker_str[] = {"primary", "secondary", "reserved",};
	int i;

	for(i=0;i<LCD_MAX;i++)
		if (!strncmp(lcd_maker_id, maker_str[i], 9)) {
			maker_id = (lcd_maker_type) i;
			break;
		}

    printk(KERN_INFO "LCD Maker ID : %s \n", maker_str[maker_id]);
	return 1;
}
__setup("lcd_maker_id=", lcd_maker_id_setup);


/* setting whether uart console is enalbed or disabled */
static int uart_console_mode = 0;

int __init lge_get_uart_mode(void)
{
	return uart_console_mode;
}

static int __init lge_uart_mode(char *uart_mode)
{
	if (!strncmp("enable", uart_mode, 5)) {
		printk(KERN_INFO"UART CONSOLE : enable\n");
		uart_console_mode = 1;
	}
	else
		printk(KERN_INFO"UART CONSOLE : disable\n");

	return 1;
}
__setup("uart_console=", lge_uart_mode);

int lge_get_lcd_maker_id(void)
{
	return maker_id;
}

/*                                                                 */
#if defined(CONFIG_LGE_BROADCAST_1SEG) || defined(CONFIG_LGE_BROADCAST_TDMB)
static struct resource resources_qup_spi_gsbi10[] = {
	{
		.name   = "spi_base",
		.start  = MSM_GSBI10_QUP_PHYS,
		.end    = MSM_GSBI10_QUP_PHYS + SZ_4K - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "gsbi_base",
		.start  = MSM_GSBI10_PHYS,
		.end    = MSM_GSBI10_PHYS + 4 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "spi_irq_in",
		.start  = GSBI10_QUP_IRQ,
		.end    = GSBI10_QUP_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
	{
		.name   = "spi_clk",
		.start  = 74,
		.end    = 74,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "spi_cs",
		.start  = 73,
		.end    = 73,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "spi_miso",
		.start  = 72,
		.end    = 72,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "spi_mosi",
		.start  = 71,
		.end    = 71,
		.flags  = IORESOURCE_IO,
	},
};

struct platform_device msm8960_device_qup_spi_gsbi10 = {
	.name	= "spi_qsd",
	.id	= 1,
	.num_resources	= ARRAY_SIZE(resources_qup_spi_gsbi10),
	.resource	= resources_qup_spi_gsbi10,
};
#endif /*                      */
/*                                                               */
#endif // temporary

#ifdef CONFIG_ANDROID_RAM_CONSOLE
static struct resource ram_console_resource[] = {
	{
		.name = "ram_console",
		.flags = IORESOURCE_MEM,
	}
};
static struct platform_device ram_console_device = {
	.name = "ram_console",
	.id = -1,
	.num_resources = ARRAY_SIZE(ram_console_resource),
	.resource = ram_console_resource,
};

void __init lge_add_ramconsole_devices(void)
{
	struct resource* res = ram_console_resource;
	struct membank* bank = &meminfo.bank[0];

	res->start = PHYS_OFFSET + bank->size;
	res->end = res->start + LGE_RAM_CONSOLE_SIZE -1;
	printk("RAM CONSOLE START ADDR : 0x%x\n", res->start);
	printk("RAM CONSOLE END ADDR   : 0x%x\n", res->end);

	platform_device_register(&ram_console_device);
}
#endif

#ifdef CONFIG_LGE_HANDLE_PANIC
static struct resource crash_log_resource[] = {
	{
		.name = "crash_log",
		.flags = IORESOURCE_MEM,
	}
};

static struct platform_device panic_handler_device = {
	.name = "panic-handler",
	.num_resources = ARRAY_SIZE(crash_log_resource),
	.resource = crash_log_resource,
	.dev = {
		.platform_data = NULL,
	}
};

volatile resource_size_t lge_add_info;

void __init lge_add_panic_handler_devices(void)
{
	struct resource* res = crash_log_resource;
	struct membank* bank = &meminfo.bank[0];

	res->start = bank->start + bank->size + LGE_RAM_CONSOLE_SIZE;
	res->end = res->start + LGE_CRASH_LOG_SIZE - 1;

	lge_add_info = res->start + LGE_CRASH_LOG_SIZE + (1024 * 4);

	printk(KERN_INFO "CRASH LOG START ADDR : %x\n", res->start);
	printk(KERN_INFO "CRASH LOG END ADDR   : %x\n", res->end);
	printk(KERN_INFO "ADDITIONAL INFO ADDR : %x\n", lge_add_info);

	platform_device_register(&panic_handler_device);
}
#endif

#ifdef CONFIG_LGE_QFPROM_INTERFACE
static struct platform_device qfprom_device = {
	.name = "lge-msm8930-qfprom",
	.id = -1,
};

void __init lge_add_qfprom_devices(void)
{
	platform_device_register(&qfprom_device);
}
#endif
void __init lge_add_gpio_i2c_device(gpio_i2c_init_func_t *init_func)
{
	i2c_init_func[gpio_i2c_dev_num] = init_func;
	gpio_i2c_dev_num++;
}

void __init lge_add_gpio_i2c_devices(void)
{
	int index;
	gpio_i2c_init_func_t *init_func_ptr;

	for (index = 0; index < gpio_i2c_dev_num; index++) {
		init_func_ptr = i2c_init_func[index];
		(*init_func_ptr)(LOWEST_GPIO_I2C_BUS_NUM + index);
	}
}

static void __init register_i2c_devices(void)
{
#ifdef CONFIG_I2C
	u8 mach_mask = 0;
	int i;

	/* Build the matching 'supported_machs' bitmask */
	mach_mask = I2C_SURF;

	/* Run the array and install devices as appropriate */
	for (i = 0; i < msm_i2c_dev_num; i++) {
		if (msm_i2c_devices[i]->machs & mach_mask)
			i2c_register_board_info(msm_i2c_devices[i]->bus,
						msm_i2c_devices[i]->info,
						msm_i2c_devices[i]->len);
	}
#endif
}

void __init lge_add_msm_i2c_device(struct i2c_registry *device)
{
	msm_i2c_devices[msm_i2c_dev_num++] = device;
}

void __init lge_add_i2c_devices(void)
{
	register_i2c_devices();
	lge_add_gpio_i2c_devices();
}
