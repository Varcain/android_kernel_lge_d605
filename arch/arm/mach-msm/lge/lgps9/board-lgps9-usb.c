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

#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/usb/android.h>
#include <linux/msm_ssbi.h>
#include <linux/regulator/msm-gpio-regulator.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/mfd/pm8xxx/pm8xxx-adc.h>
#ifdef CONFIG_USB_MSM_OTG_72K
#include <mach/msm_hsusb.h>
#else
#include <linux/usb/msm_hsusb.h>
#endif
#include <mach/usbdiag.h>
#include <mach/socinfo.h>
#include <mach/rpm.h>
#include <mach/gpio.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>

#include "devices.h"
#include "devices-msm8x60.h"

#include <mach/board_lge.h>
#include "board-lgps9.h"
#include <linux/platform_data/lge_android_usb.h>
#ifdef CONFIG_USB_MSM_OTG_72K
static struct msm_otg_platform_data msm_otg_pdata;
#else
#if 0
	/*                       
                                  
                                   
  */
static int hsusb_phy_init_seq[] = {
	0x44, 0x80, /* set VBUS valid threshold
			and disconnect valid threshold */
	/*                          */
	0x5B, 0x81, /* update DC voltage level */
	0x3C, 0x82, /* set preemphasis and rise/fall time */
	0x33, 0x83, /* set source impedance adjusment */
	-1};
#else
static int hsusb_phy_init_seq[] = {
	0x44, 0x80, /* set VBUS valid threshold
			and disconnect valid threshold */
	0x68, 0x81, /* update DC voltage level */
	0x24, 0x82, /* set preemphasis and rise/fall time */
	0x13, 0x83, /* set source impedance adjusment */
	-1};
#endif

#ifdef CONFIG_MSM_BUS_SCALING
/* Bandwidth requests (zero) if no vote placed */
static struct msm_bus_vectors usb_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_SPS,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};

/* Bus bandwidth requests in Bytes/sec */
static struct msm_bus_vectors usb_max_vectors[] = {
	{
		.src = MSM_BUS_MASTER_SPS,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 60000000,		/* At least 480Mbps on bus. */
		.ib = 960000000,	/* MAX bursts rate */
	},
};

static struct msm_bus_paths usb_bus_scale_usecases[] = {
	{
		ARRAY_SIZE(usb_init_vectors),
		usb_init_vectors,
	},
	{
		ARRAY_SIZE(usb_max_vectors),
		usb_max_vectors,
	},
};

static struct msm_bus_scale_pdata usb_bus_scale_pdata = {
	usb_bus_scale_usecases,
	ARRAY_SIZE(usb_bus_scale_usecases),
	.name = "usb",
};
#endif

#define MSM_MPM_PIN_USB1_OTGSESSVLD	40
static struct msm_otg_platform_data msm_otg_pdata = {
#ifdef CONFIG_USB_OTG
	.mode			= USB_OTG,
#else
	.mode			= USB_PERIPHERAL,
#endif
	.otg_control		= OTG_PMIC_CONTROL,
	.phy_type		= SNPS_28NM_INTEGRATED_PHY,
	.pmic_id_irq		= PM8038_USB_ID_IN_IRQ(PM8038_IRQ_BASE),
	.power_budget		= 750,
#ifdef CONFIG_MSM_BUS_SCALING
	.bus_scale_table	= &usb_bus_scale_pdata,
#endif
	.mpm_otgsessvld_int	= MSM_MPM_PIN_USB1_OTGSESSVLD,
};

#define PID_MAGIC_ID		0x71432909
#define SERIAL_NUM_MAGIC_ID	0x61945374
#define SERIAL_NUMBER_LENGTH	127
#define DLOAD_USB_BASE_ADD	0x2A03F0C8

struct magic_num_struct {
	uint32_t pid;
	uint32_t serial_num;
};

struct dload_struct {
	uint32_t	reserved1;
	uint32_t	reserved2;
	uint32_t	reserved3;
	uint16_t	reserved4;
	uint16_t	pid;
	char		serial_number[SERIAL_NUMBER_LENGTH];
	uint16_t	reserved5;
	struct magic_num_struct magic_struct;
};

static int usb_diag_update_pid_and_serial_num(uint32_t pid, const char *snum)
{
	struct dload_struct __iomem *dload = 0;

	dload = ioremap(DLOAD_USB_BASE_ADD, sizeof(*dload));
	if (!dload) {
		pr_err("%s: cannot remap I/O memory region: %08x\n",
					__func__, DLOAD_USB_BASE_ADD);
		return -ENXIO;
	}

	pr_debug("%s: dload:%p pid:%x serial_num:%s\n",
				__func__, dload, pid, snum);
	/* update pid */
	dload->magic_struct.pid = PID_MAGIC_ID;
	dload->pid = pid;

	/* update serial number */
	dload->magic_struct.serial_num = 0;
	if (!snum) {
		memset(dload->serial_number, 0, SERIAL_NUMBER_LENGTH);
		goto out;
	}

	dload->magic_struct.serial_num = SERIAL_NUM_MAGIC_ID;
	strlcpy(dload->serial_number, snum, SERIAL_NUMBER_LENGTH);
out:
	iounmap(dload);
	return 0;
}

static struct android_usb_platform_data android_usb_pdata = {
    .update_pid_and_serial_num = usb_diag_update_pid_and_serial_num,
};

static struct platform_device android_usb_device = {
	.name	= "android_usb",
	.id	= -1,
	.dev	= {
		.platform_data = &android_usb_pdata,
	},
};
#endif
#ifdef CONFIG_LGE_USB_DIAG_DISABLE
static struct platform_device lg_diag_cmd_device = {
	.name = "lg_diag_cmd",
	.id = -1,
	.dev    = {
		.platform_data = 0, //&lg_diag_cmd_pdata
	},
};
#endif

#ifdef CONFIG_USB_G_LGE_ANDROID
static int get_factory_cable(void)
{
	struct chg_cable_info info;
	/* FIXME : boot mode not set. */
#if 0
	enum lge_boot_mode_type boot_mode;
#endif
	int res;

	/* get cable infomation */
	res = lge_pm_get_cable_info(&info);
	if (res < 0) {
		pr_err("Error get cable information from PMIC %d\n", res);
		return 0;
	}

	switch(info.cable_type) {
	/* It is factory cable */
	case CABLE_56K:
		res = LGEUSB_FACTORY_56K;
		break;
	case CABLE_130K:
		res = LGEUSB_FACTORY_130K;
		break;
	case CABLE_910K:
		res = LGEUSB_FACTORY_910K;
		break;
	/* It is normal cable */
	default:
		res = 0;
		break;
	}
	/*                           
                                  
                            
                                
  */
#if 0
	boot_mode = lge_get_boot_mode();
	switch(boot_mode) {
	case LGE_BOOT_MODE_FACTORY:
		res = LGEUSB_FACTORY_130K;
		break;
	case LGE_BOOT_MODE_FACTORY2:
	case LGE_BOOT_MODE_PIFBOOT:
		res = LGEUSB_FACTORY_56K;
		break;
	default:
		break;
	}
#endif

	return res;
}

static int get_cable_adc(struct chg_cable_info *c_info)
{
	/* get cable infomation */
    return lge_pm_get_cable_info(c_info);
}

struct lge_android_usb_platform_data lge_android_usb_pdata = {
	.vendor_id = 0x1004,
	.factory_pid = 0x6000,
	.iSerialNumber = 0,
	.product_name = "LGE Android Phone",
	.manufacturer_name = "LG Electronics Inc.",
	.factory_composition = "acm,diag",
	.get_factory_cable = get_factory_cable,
	.get_cable_adc = get_cable_adc,
};

struct platform_device lge_android_usb_device = {
	.name = "lge_android_usb",
	.id = -1,
	.dev = {
		.platform_data = &lge_android_usb_pdata,
	},
};
#endif

static struct platform_device *usb_devices[] __initdata = {
#ifdef CONFIG_USB_MSM_OTG
	&msm8960_device_otg,
	&msm8960_device_gadget_peripheral,
	&msm_device_hsusb_host,
#endif
    &android_usb_device,
#ifdef CONFIG_USB_G_LGE_ANDROID
    &lge_android_usb_device,
#endif
#ifdef CONFIG_LGE_USB_DIAG_DISABLE
	&lg_diag_cmd_device,
#endif
};

void __init lge_add_usb_devices(void)
{
	android_usb_pdata.swfi_latency = 100; // msm_rpmrs_levels[0].latency_us; original data is 1.
	msm_otg_pdata.phy_init_seq = hsusb_phy_init_seq;
	msm8960_device_otg.dev.platform_data = &msm_otg_pdata;
	platform_add_devices(usb_devices, ARRAY_SIZE(usb_devices));
}

