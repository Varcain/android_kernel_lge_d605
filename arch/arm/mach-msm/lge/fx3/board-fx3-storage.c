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

#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/bootmem.h>
#include <linux/gpio.h>
#include <asm/mach-types.h>
#include <asm/mach/mmc.h>
#include <mach/msm_bus_board.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/socinfo.h>
#include "devices.h"

#include <mach/board_lge.h>
#include "board-fx3.h"
#include "board-storage-common-a.h"
#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
#include <linux/skbuff.h>
#include <linux/wlan_plat.h>
#endif /* CONFIG_MMC_MSM_SDC4_SUPPORT */
#include <linux/random.h> //for test . Get Random MAC address when booting time.

/* MSM8960 has 5 SDCC controllers */
enum sdcc_controllers {
	SDCC1,
	SDCC2,
	SDCC3,
	SDCC4,
	SDCC5,
	MAX_SDCC_CONTROLLER
};

/* All SDCC controllers require VDD/VCC voltage */
static struct msm_mmc_reg_data mmc_vdd_reg_data[MAX_SDCC_CONTROLLER] = {
	/* SDCC1 : eMMC card connected */
	[SDCC1] = {
		.name = "sdc_vdd",
		.high_vol_level = 2950000,
		.low_vol_level = 2950000,
		.always_on = 1,
		.lpm_sup = 1,
		.lpm_uA = 18000, //9000-->18000 (18mA)
		.hpm_uA = 200000, /* 200mA */
	},
	/* SDCC3 : External card slot connected */
	[SDCC3] = {
		.name = "sdc_vdd",
		.high_vol_level = 2950000,
		.low_vol_level = 2950000,
		/*
		 * Normally this is not an always ON regulator. On this
		 * platform, unfortunately the sd detect line is connected
		 * to this via esd circuit and so turn this off/on while card
		 * is not present causes the sd detect line to toggle
		 * continuously. This is expected to be fixed in the newer
		 * hardware revisions - maybe once that is done, this can be
		 * reverted.
		 */
		.always_on = 1,
		.lpm_sup = 1,
		.hpm_uA = 800000, /* 800mA */
		.lpm_uA = 9000,
		.reset_at_init = true,
	},
#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
	/* SDCC4 : External card slot connected */
	[SDCC4] = {
		.name = "sdc_vdd",
		.always_on = 1, // for test
		.set_voltage_sup = 0,
		.high_vol_level = 1800000,
		.low_vol_level = 1800000,
		.hpm_uA = 600000, /* 600mA */
	}
#endif /* CONFIG_MMC_MSM_SDC4_SUPPORT */
};

/* All SDCC controllers may require voting for VDD PAD voltage */
static struct msm_mmc_reg_data mmc_vdd_io_reg_data[MAX_SDCC_CONTROLLER] = {
	/* SDCC1 : eMMC card connected */
	[SDCC1] = {
		.name = "sdc_vdd_io",
		.always_on = 1,
		.high_vol_level = 1800000,
		.low_vol_level = 1800000,
		.hpm_uA = 200000, /* 200mA */
	},
	/* SDCC3 : External card slot connected */
	[SDCC3] = {
		.name = "sdc_vdd_io",
		.high_vol_level = 2950000,
		.low_vol_level = 1850000,
		.always_on = 1,
		.lpm_sup = 1,
		/* Max. Active current required is 16 mA */
		.hpm_uA = 16000,
		/*
		 * Sleep current required is ~300 uA. But min. vote can be
		 * in terms of mA (min. 1 mA). So let's vote for 2 mA
		 * during sleep.
		 */
		.lpm_uA = 2000,
	},
	/* SDCC4 : SDIO slot connected */
	[SDCC4] = {
		.name = "sdc_vdd_io",
		.high_vol_level = 1800000,
		.low_vol_level = 1800000,
		.always_on = 1,
		.lpm_sup = 1,
		.hpm_uA = 200000, /* 200mA */
		.lpm_uA = 2000,
	},
};

static struct msm_mmc_slot_reg_data mmc_slot_vreg_data[MAX_SDCC_CONTROLLER] = {
	/* SDCC1 : eMMC card connected */
	[SDCC1] = {
		.vdd_data = &mmc_vdd_reg_data[SDCC1],
		.vdd_io_data = &mmc_vdd_io_reg_data[SDCC1],
	},
	/* SDCC3 : External card slot connected */
	[SDCC3] = {
		.vdd_data = &mmc_vdd_reg_data[SDCC3],
		.vdd_io_data = &mmc_vdd_io_reg_data[SDCC3],
#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
	},
	/* SDCC4 : External card slot connected */
	[SDCC4] = {
		.vdd_data = &mmc_vdd_reg_data[SDCC4],
#endif /* CONFIG_MMC_MSM_SDC4_SUPPORT */
	},
};

/* SDC1 pad data */
static struct msm_mmc_pad_drv sdc1_pad_drv_on_cfg[] = {
	{TLMM_HDRV_SDC1_CLK, GPIO_CFG_16MA},
	{TLMM_HDRV_SDC1_CMD, GPIO_CFG_10MA},
	{TLMM_HDRV_SDC1_DATA, GPIO_CFG_10MA}
};

static struct msm_mmc_pad_drv sdc1_pad_drv_off_cfg[] = {
	{TLMM_HDRV_SDC1_CLK, GPIO_CFG_2MA},
	{TLMM_HDRV_SDC1_CMD, GPIO_CFG_2MA},
	{TLMM_HDRV_SDC1_DATA, GPIO_CFG_2MA}
};

static struct msm_mmc_pad_pull sdc1_pad_pull_on_cfg[] = {
	{TLMM_PULL_SDC1_CLK, GPIO_CFG_NO_PULL},
	{TLMM_PULL_SDC1_CMD, GPIO_CFG_PULL_UP},
	{TLMM_PULL_SDC1_DATA, GPIO_CFG_PULL_UP}
};

static struct msm_mmc_pad_pull sdc1_pad_pull_off_cfg[] = {
	{TLMM_PULL_SDC1_CLK, GPIO_CFG_NO_PULL},
	{TLMM_PULL_SDC1_CMD, GPIO_CFG_PULL_UP},
	{TLMM_PULL_SDC1_DATA, GPIO_CFG_PULL_UP}
};

/* SDC3 pad data */
static struct msm_mmc_pad_drv sdc3_pad_drv_on_cfg[] = {
	{TLMM_HDRV_SDC3_CLK, GPIO_CFG_8MA},
	{TLMM_HDRV_SDC3_CMD, GPIO_CFG_8MA},
	{TLMM_HDRV_SDC3_DATA, GPIO_CFG_8MA}
};

static struct msm_mmc_pad_drv sdc3_pad_drv_off_cfg[] = {
	{TLMM_HDRV_SDC3_CLK, GPIO_CFG_2MA},
	{TLMM_HDRV_SDC3_CMD, GPIO_CFG_2MA},
	{TLMM_HDRV_SDC3_DATA, GPIO_CFG_2MA}
};

static struct msm_mmc_pad_pull sdc3_pad_pull_on_cfg[] = {
	{TLMM_PULL_SDC3_CLK, GPIO_CFG_NO_PULL},
	{TLMM_PULL_SDC3_CMD, GPIO_CFG_PULL_UP},
	{TLMM_PULL_SDC3_DATA, GPIO_CFG_PULL_UP}
};

static struct msm_mmc_pad_pull sdc3_pad_pull_off_cfg[] = {
	{TLMM_PULL_SDC3_CLK, GPIO_CFG_PULL_DOWN},
	/*
	 * SDC3 CMD line should be PULLed UP otherwise fluid platform will
	 * see transitions (1 -> 0 and 0 -> 1) on card detection line,
	 * which would result in false card detection interrupts.
	 */
	{TLMM_PULL_SDC3_CMD, GPIO_CFG_PULL_DOWN},
	/*
	 * Keeping DATA lines status to PULL UP will make sure that
	 * there is no current leak during sleep if external pull up
	 * is connected to DATA lines.
	 */
	{TLMM_PULL_SDC3_DATA, GPIO_CFG_PULL_DOWN}
};

#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
/* SDC4 pad data */
static struct msm_mmc_pad_drv sdc4_pad_drv_on_cfg[] = {
	{TLMM_HDRV_SDC4_CLK, GPIO_CFG_8MA},
	{TLMM_HDRV_SDC4_CMD, GPIO_CFG_8MA},
	{TLMM_HDRV_SDC4_DATA, GPIO_CFG_8MA}
};

static struct msm_mmc_pad_drv sdc4_pad_drv_off_cfg[] = {
	{TLMM_HDRV_SDC4_CLK, GPIO_CFG_2MA},
	{TLMM_HDRV_SDC4_CMD, GPIO_CFG_2MA},
	{TLMM_HDRV_SDC4_DATA, GPIO_CFG_2MA}
};

static struct msm_mmc_pad_pull sdc4_pad_pull_on_cfg[] = {
	{TLMM_PULL_SDC4_CMD, GPIO_CFG_PULL_UP},
	{TLMM_PULL_SDC4_DATA, GPIO_CFG_PULL_UP}
};

static struct msm_mmc_pad_pull sdc4_pad_pull_off_cfg[] = {
	{TLMM_PULL_SDC4_CMD, GPIO_CFG_PULL_DOWN},
	{TLMM_PULL_SDC4_DATA, GPIO_CFG_PULL_DOWN}
};
#endif /* CONFIG_MMC_MSM_SDC4_SUPPORT */

static struct msm_mmc_pad_pull_data mmc_pad_pull_data[MAX_SDCC_CONTROLLER] = {
	[SDCC1] = {
		.on = sdc1_pad_pull_on_cfg,
		.off = sdc1_pad_pull_off_cfg,
		.size = ARRAY_SIZE(sdc1_pad_pull_on_cfg)
	},
	[SDCC3] = {
		.on = sdc3_pad_pull_on_cfg,
		.off = sdc3_pad_pull_off_cfg,
		.size = ARRAY_SIZE(sdc3_pad_pull_on_cfg)
	},
#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
	[SDCC4] = {
		.on = sdc4_pad_pull_on_cfg,
		.off = sdc4_pad_pull_off_cfg,
		.size = ARRAY_SIZE(sdc4_pad_pull_on_cfg)
	},
#endif /* CONFIG_MMC_MSM_SDC4_SUPPORT */
};

static struct msm_mmc_pad_drv_data mmc_pad_drv_data[MAX_SDCC_CONTROLLER] = {
	[SDCC1] = {
		.on = sdc1_pad_drv_on_cfg,
		.off = sdc1_pad_drv_off_cfg,
		.size = ARRAY_SIZE(sdc1_pad_drv_on_cfg)
	},
	[SDCC3] = {
		.on = sdc3_pad_drv_on_cfg,
		.off = sdc3_pad_drv_off_cfg,
		.size = ARRAY_SIZE(sdc3_pad_drv_on_cfg)
	},
#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
	[SDCC4] = {
		.on = sdc4_pad_drv_on_cfg,
		.off = sdc4_pad_drv_off_cfg,
		.size = ARRAY_SIZE(sdc4_pad_drv_on_cfg)
	},
#endif /* CONFIG_MMC_MSM_SDC4_SUPPORT */
};
struct msm_mmc_gpio sdc4_gpio[] = {
	{83, "sdc4_dat_3"},
	{84, "sdc4_dat_2"},
	{85, "sdc4_dat_1"},
	{86, "sdc4_dat_0"},
	{87, "sdc4_cmd"},
	{88, "sdc4_clk"}
};
struct msm_mmc_gpio_data mmc_gpio_data[MAX_SDCC_CONTROLLER] = {
	[SDCC4] = {
		.gpio = sdc4_gpio,
		.size = ARRAY_SIZE(sdc4_gpio),
	},
};

static struct msm_mmc_pad_data mmc_pad_data[MAX_SDCC_CONTROLLER] = {
	[SDCC1] = {
		.pull = &mmc_pad_pull_data[SDCC1],
		.drv = &mmc_pad_drv_data[SDCC1]
	},
	[SDCC3] = {
		.pull = &mmc_pad_pull_data[SDCC3],
		.drv = &mmc_pad_drv_data[SDCC3]
	},
#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
	[SDCC4] = {
		.pull = &mmc_pad_pull_data[SDCC4],
		.drv = &mmc_pad_drv_data[SDCC4]
	},
#endif /* CONFIG_MMC_MSM_SDC4_SUPPORT */
};

static struct msm_mmc_pin_data mmc_slot_pin_data[MAX_SDCC_CONTROLLER] = {
	[SDCC1] = {
		.pad_data = &mmc_pad_data[SDCC1],
	},
	[SDCC3] = {
		.pad_data = &mmc_pad_data[SDCC3],
	},
#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
	[SDCC4] = {
		.is_gpio = 1,
		.gpio_data = &mmc_gpio_data[SDCC4],
//		.pad_data = &mmc_pad_data[SDCC4],
	},
#endif /* CONFIG_MMC_MSM_SDC4_SUPPORT */
};

#define MSM_MPM_PIN_SDC1_DAT1	17
#define MSM_MPM_PIN_SDC3_DAT1	21

static unsigned int sdc1_sup_clk_rates[] = {
	400000, 24000000, 48000000, 96000000
};

#ifdef CONFIG_MMC_MSM_SDC3_SUPPORT
static unsigned int sdc3_sup_clk_rates[] = {
	400000, 24000000, 48000000, 96000000, 192000000,
};
#endif

#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
static struct mmc_platform_data msm8960_sdc1_data = {
	.ocr_mask       = MMC_VDD_27_28 | MMC_VDD_28_29,
#ifdef CONFIG_MMC_MSM_SDC1_8_BIT_SUPPORT
	.mmc_bus_width  = MMC_CAP_8_BIT_DATA,
#else
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
#endif
	.sup_clk_table	= sdc1_sup_clk_rates,
	.sup_clk_cnt	= ARRAY_SIZE(sdc1_sup_clk_rates),
	.nonremovable	= 1,
	.vreg_data	= &mmc_slot_vreg_data[SDCC1],
	.pin_data	= &mmc_slot_pin_data[SDCC1],
	.mpm_sdiowakeup_int = MSM_MPM_PIN_SDC1_DAT1,
	.msm_bus_voting_data = &sps_to_ddr_bus_voting_data,
	.uhs_caps2	= MMC_CAP2_HS200_1_8V_SDR,
};
#endif

#ifdef CONFIG_MMC_MSM_SDC3_SUPPORT
static struct mmc_platform_data msm8960_sdc3_data = {
	.ocr_mask       = MMC_VDD_27_28 | MMC_VDD_28_29,
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
	.sup_clk_table	= sdc3_sup_clk_rates,
	.sup_clk_cnt	= ARRAY_SIZE(sdc3_sup_clk_rates),
#ifdef CONFIG_MMC_MSM_SDC3_WP_SUPPORT
/*TODO: Insert right replacement for PM8038 */
#ifndef MSM8930_PHASE_2
	.wpswitch_gpio	= PM8921_GPIO_PM_TO_SYS(16),
#else
	.wpswitch_gpio	= 66,
	.is_wpswitch_active_low = true,
#endif
#endif
	.vreg_data	= &mmc_slot_vreg_data[SDCC3],
	.pin_data	= &mmc_slot_pin_data[SDCC3],
/*TODO: Insert right replacement for PM8038 */
#ifndef MSM8930_PHASE_2
	.status_gpio	= PM8921_GPIO_PM_TO_SYS(26),
	.status_irq	= PM8921_GPIO_IRQ(PM8921_IRQ_BASE, 26),
#else
#if defined(CONFIG_MACH_LGE_FX3_VZW) 
        .status_gpio    = 100,
        .status_irq     = MSM_GPIO_TO_INT(100),
#else
	.status_gpio	= 94,
	.status_irq	= MSM_GPIO_TO_INT(94),
#endif
#endif
	.irq_flags	= IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS)
        .is_status_gpio_active_low = false,
#else
	.is_status_gpio_active_low = true,
#endif
	.xpc_cap	= 1,
	.uhs_caps	= (MMC_CAP_UHS_SDR12 | MMC_CAP_UHS_SDR25 |
			MMC_CAP_UHS_SDR50 | MMC_CAP_UHS_DDR50 |
			MMC_CAP_UHS_SDR104 | MMC_CAP_MAX_CURRENT_800),
	.mpm_sdiowakeup_int = MSM_MPM_PIN_SDC3_DAT1,
	.msm_bus_voting_data = &sps_to_ddr_bus_voting_data,
};
#endif

#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
static unsigned int sdc4_sup_clk_rates[] = {
	400000, 24000000, 48000000
};
static unsigned int g_wifi_detect;
static void *sdc4_dev;
void (*sdc4_status_cb)(int card_present, void *dev);
extern void
msmsdcc_set_mmc_enable(int card_present, void *dev_id);

int sdc4_status_register(void (*cb)(int card_present, void *dev), void *dev)
{
	if(sdc4_status_cb) {
		return -EINVAL;
	}
	sdc4_status_cb = cb;
	sdc4_dev = dev;
	return 0;
}

unsigned int sdc4_status(struct device *dev)
{
	return g_wifi_detect;
}
static struct mmc_platform_data msm8930_sdc4_data = {
	.ocr_mask       = MMC_VDD_165_195 | MMC_VDD_27_28 | MMC_VDD_28_29,
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
	.sup_clk_table	= sdc4_sup_clk_rates,
	.sup_clk_cnt	= ARRAY_SIZE(sdc4_sup_clk_rates),
	//                                                                                 
#ifdef CONFIG_BCMDHD_MODULE
	.nonremovable	= 0,
#else
	.nonremovable	= 1,
#endif //CONFIG_BCMDHD_MODULE
	.vreg_data	= &mmc_slot_vreg_data[SDCC4],
	.pin_data	= &mmc_slot_pin_data[SDCC4],

	.status		= sdc4_status,
	.register_status_notify	= sdc4_status_register,
};
#endif

#if defined(CONFIG_MMC_MSM_SDC4_SUPPORT)

static int sdc4_clock_force_disable=0;
static int get_sdc4_clock_force_disable(void)
{

	return sdc4_clock_force_disable;
}
static void set_sdc4_clock_force_disable(int enable)
{
	sdc4_clock_force_disable=enable;
	return;
}
EXPORT_SYMBOL(get_sdc4_clock_force_disable);
EXPORT_SYMBOL(set_sdc4_clock_force_disable);

#define WLAN_HOSTWAKE 81
#define WLAN_POWER 82
static unsigned wlan_wakes_msm[] = {
	GPIO_CFG(WLAN_HOSTWAKE, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA) };

/* for wifi power supply */
static unsigned wifi_config_power_on[] = {
	GPIO_CFG(WLAN_POWER, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA) };


// For broadcom
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM

#define WLAN_STATIC_SCAN_BUF0		5
#define WLAN_STATIC_SCAN_BUF1		6
#define PREALLOC_WLAN_SEC_NUM		4
#define PREALLOC_WLAN_BUF_NUM		160
#define PREALLOC_WLAN_SECTION_HEADER		24

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_BUF_NUM * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_BUF_NUM * 1024)

#define DHD_SKB_HDRSIZE			336
#define DHD_SKB_1PAGE_BUFSIZE	((PAGE_SIZE*1)-DHD_SKB_HDRSIZE)
#define DHD_SKB_2PAGE_BUFSIZE	((PAGE_SIZE*2)-DHD_SKB_HDRSIZE)
#define DHD_SKB_4PAGE_BUFSIZE	((PAGE_SIZE*4)-DHD_SKB_HDRSIZE)

#define WLAN_SKB_BUF_NUM	17

static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

struct wlan_mem_prealloc {
	void *mem_ptr;
	unsigned long size;
};

static struct wlan_mem_prealloc wlan_mem_array[PREALLOC_WLAN_SEC_NUM] = {
	{ NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER) }
};

void *wlan_static_scan_buf0;
void *wlan_static_scan_buf1;
static void *brcm_wlan_mem_prealloc(int section, unsigned long size)
{
	if (section == PREALLOC_WLAN_SEC_NUM)
		return wlan_static_skb;
	if (section == WLAN_STATIC_SCAN_BUF0)
		return wlan_static_scan_buf0;
	if (section == WLAN_STATIC_SCAN_BUF1)
		return wlan_static_scan_buf1;
	if ((section < 0) || (section > PREALLOC_WLAN_SEC_NUM))
		return NULL;

	if (wlan_mem_array[section].size < size)
		return NULL;

	return wlan_mem_array[section].mem_ptr;
}

static int brcm_init_wlan_mem(void)
{
	int i;
	int j;

	for (i = 0; i < 8; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_1PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	for (; i < 16; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_2PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_4PAGE_BUFSIZE);
	if (!wlan_static_skb[i])
		goto err_skb_alloc;

	for (i = 0 ; i < PREALLOC_WLAN_SEC_NUM ; i++) {
		wlan_mem_array[i].mem_ptr =
				kmalloc(wlan_mem_array[i].size, GFP_KERNEL);

		if (!wlan_mem_array[i].mem_ptr)
			goto err_mem_alloc;
	}
	wlan_static_scan_buf0 = kmalloc (65536, GFP_KERNEL);
	if(!wlan_static_scan_buf0)
		goto err_mem_alloc;
	wlan_static_scan_buf1 = kmalloc (65536, GFP_KERNEL);
	if(!wlan_static_scan_buf1)
		goto err_mem_alloc;

	printk("%s: WIFI MEM Allocated\n", __FUNCTION__);
	return 0;

 err_mem_alloc:
	pr_err("Failed to mem_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		kfree(wlan_mem_array[j].mem_ptr);

	i = WLAN_SKB_BUF_NUM;

 err_skb_alloc:
	pr_err("Failed to skb_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		dev_kfree_skb(wlan_static_skb[j]);

	return -ENOMEM;
}
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */

int bcm_wifi_set_power(int enable)
{
	int ret = 0;

	// garry temp msmsdcc_set_mmc_enable(enable,sdc4_dev);
	set_sdc4_clock_force_disable(enable);

	if (enable)
	{
		ret = gpio_direction_output(WLAN_POWER, 1); 
		if (ret) 
		{
			printk(KERN_ERR "%s: WL_REG_ON  failed to pull up (%d)\n",
					__func__, ret);
			return -EIO;
		}

		// WLAN chip to reset
		//mdelay(300);
		mdelay(150); //to save booting time
		printk(KERN_ERR "%s: wifi power successed to pull up\n",__func__);

	}
	else{
		ret = gpio_direction_output(WLAN_POWER, 0); 
		if (ret) 
		{
			printk(KERN_ERR "%s:  WL_REG_ON  failed to pull down (%d)\n",
					__func__, ret);
			return -EIO;
		}

		// WLAN chip down 
		//                                                              
		//mdelay(10);  //to save booting time
		printk(KERN_ERR "%s: wifi power successed to pull down\n",__func__);
	}

	return ret;
}

EXPORT_SYMBOL(bcm_wifi_set_power);

int __init bcm_wifi_init_gpio_mem(void)
{
	int rc=0;

	if (gpio_tlmm_config(wifi_config_power_on[0], GPIO_CFG_ENABLE))
		printk(KERN_ERR "%s: Failed to configure WLAN_POWER\n", __func__);

	if (gpio_request(WLAN_POWER, "WL_REG_ON"))		
		printk("Failed to request gpio %d for WL_REG_ON\n", WLAN_POWER);	

	if (gpio_direction_output(WLAN_POWER, 0)) 
		printk(KERN_ERR "%s: WL_REG_ON  failed to pull down \n", __func__);
	//gpio_free(WLAN_POWER);

	if (gpio_request(WLAN_HOSTWAKE, "wlan_wakes_msm"))		
		printk("Failed to request gpio %d for wlan_wakes_msm\n", WLAN_HOSTWAKE);			

	rc = gpio_tlmm_config(wlan_wakes_msm[0], GPIO_CFG_ENABLE);	
	if (rc)		
		printk(KERN_ERR "%s: Failed to configure wlan_wakes_msm = %d\n",__func__, rc);

	//gpio_free(WLAN_HOSTWAKE); 

#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	brcm_init_wlan_mem();
#endif

	printk("bcm_wifi_init_gpio_mem successfully \n");
	
	return 0;
}

static int bcm_wifi_reset(int on)
{
	return 0;
}

static int bcm_wifi_carddetect(int val)
{
	g_wifi_detect = val;
	if(sdc4_status_cb)
		sdc4_status_cb(val, sdc4_dev);
	else
		printk("%s:There is no callback for notify\n", __FUNCTION__);
	return 0;
}

static int bcm_wifi_get_mac_addr(unsigned char* buf)
{
    uint rand_mac;
    static unsigned char mymac[6] = {0,};
    const unsigned char nullmac[6] = {0,};
    pr_debug("%s: %p\n", __func__, buf);

    printk("[%s] Entering...\n", __func__  );

    if( buf == NULL ) return -EAGAIN;

    if( memcmp( mymac, nullmac, 6 ) != 0 )
    {
        /* Mac displayed from UI are never updated..
           So, mac obtained on initial time is used */
        memcpy( buf, mymac, 6 );
        return 0;
    }

    srandom32((uint)jiffies);
    rand_mac = random32();
    buf[0] = 0x00;
    buf[1] = 0x90;
    buf[2] = 0x4c;
    buf[3] = (unsigned char)rand_mac;
    buf[4] = (unsigned char)(rand_mac >> 8);
    buf[5] = (unsigned char)(rand_mac >> 16);

    memcpy( mymac, buf, 6 );
	
    printk("[%s] Exiting. MyMac :  %x : %x : %x : %x : %x : %x \n",__func__ , buf[0], buf[1], buf[2], buf[3], buf[4], buf[5] );

    return 0;
}

#define COUNTRY_BUF_SZ	4
struct cntry_locales_custom {
	char iso_abbrev[COUNTRY_BUF_SZ];
	char custom_locale[COUNTRY_BUF_SZ];
	int custom_locale_rev;
};

/* Customized Locale table */
#if defined(CONFIG_BCM4330)
const struct cntry_locales_custom bcm_wifi_translate_custom_table[] = {
/* Table should be filled out based on custom platform regulatory requirement */
        {"",   "XY", 4}, /* Universal if Country code is unknown or empty */
        {"AD", "GB", 0}, //Andorra
        {"AE", "KR", 24}, //UAE
        {"AF", "AF", 0}, //Afghanistan
#if defined(CARRIER_TMO)
        {"AG", "XW", 0}, //Antigua & Barbuda
        {"AI", "XW", 0}, //Anguilla
#else
        {"AG", "US", 46}, //Antigua & Barbuda
        {"AI", "US", 46}, //Anguilla
#endif
        {"AL", "GB", 0}, //Albania
        {"AM", "IL", 10}, //Armenia
        {"AN", "BR", 0}, //Netherlands Antilles
        {"AO", "IL", 10}, //Angola
        {"AR", "BR", 0}, //Argentina
#if defined(CARRIER_TMO)
        {"AS", "XW", 0}, //American Samoa (USA)
#else
        {"AS", "US", 46}, //American Samoa (USA)
#endif
        {"AT", "GB", 0}, //Austria
        {"AU", "AU", 2}, //Australia
        {"AW", "KR", 24}, //Aruba
        {"AZ", "BR", 0}, //Azerbaijan
        {"BA", "GB", 0}, //Bosnia and Herzegovina
        {"BB", "RU", 1}, //Barbados
        {"BD", "CN", 0}, //Bangladesh
        {"BE", "GB", 0}, //Belgium
        {"BF", "CN", 0}, //Burkina Faso
        {"BG", "GB", 0}, //Bulgaria
        {"BH", "RU", 1}, //Bahrain
        {"BI", "IL", 10}, //Burundi
        {"BJ", "IL", 10}, //Benin
#if defined(CARRIER_TMO)
        {"BM", "XW", 0}, //Bermuda
#else
        {"BM", "US", 46}, //Bermuda
#endif
        {"BN", "RU", 1}, //Brunei
        {"BO", "IL", 10}, //Bolivia
        {"BR", "BR", 0}, //Brazil
        {"BS", "RU", 1}, //Bahamas
        {"BT", "IL", 10}, //Bhntan
        {"BW", "GB", 0}, //Botswana
        {"BY", "GB", 0}, //Belarus
        {"BZ", "IL", 10}, //Belize
#if defined(CARRIER_TMO)
        {"CA", "XW", 0}, //Canada
#else
        {"CA", "US", 46}, //Canada
#endif
        {"CD", "IL", 10}, //Congo. Democratic Republic of the
        {"CF", "IL", 10}, //Central African Republic
        {"CG", "IL", 10}, //Congo. Republic of the
        {"CH", "GB", 0}, //Switzerland
        {"CI", "IL", 10}, //Cote d'lvoire
        {"CK", "BR", 0}, //Cook Island
        {"CL", "RU", 1}, //Chile
        {"CM", "IL", 10}, //Cameroon
        {"CN", "CN", 0}, //China
        {"CO", "BR", 0}, //Columbia
        {"CR", "BR", 0}, //Costa Rica
        {"CU", "BR", 0}, //Cuba
        {"CV", "GB", 0}, //Cape Verde
        {"CX", "AU", 2}, //Christmas Island (Australia)
        {"CY", "GB", 0}, //Cyprus
        {"CZ", "GB", 0}, //Czech
        {"DE", "GB", 0}, //Germany
        {"DJ", "IL", 10}, //Djibouti
        {"DK", "GB", 0}, //Denmark
        {"DM", "BR", 0}, //Dominica
        {"DO", "BR", 0}, //Dominican Republic
        {"DZ", "KW", 1}, //       
        {"EC", "BR", 0}, //Ecuador
        {"EE", "GB", 0}, //Estonia
        {"EG", "RU", 1}, //Egypt
        {"ER", "IL", 10}, //Eritrea
        {"ES", "GB", 0}, //Spain
        {"ET", "GB", 0}, //Ethiopia
        {"FI", "GB", 0}, //Finland
        {"FJ", "IL", 10}, //Fiji
#if defined(CARRIER_TMO)
        {"FM", "XW", 0}, //Federated States of Micronesia
#else
        {"FM", "US", 46}, //Federated States of Micronesia
#endif
        {"FO", "GB", 0}, //Faroe Island
        {"FR", "GB", 0}, //France
        {"GA", "IL", 10}, //Gabon
        {"GB", "GB", 0}, //United Kingdom
        {"GD", "BR", 0}, //Grenada
        {"GE", "GB", 0}, //Georgia
        {"GF", "GB", 0}, //French Guiana
        {"GH", "BR", 0}, //Ghana
        {"GI", "GB", 0}, //Gibraltar
        {"GM", "IL", 10}, //Gambia
        {"GN", "IL", 10}, //Guinea
        {"GP", "GB", 0}, //Guadeloupe
        {"GQ", "IL", 10}, //Equatorial Guinea
        {"GR", "GB", 0}, //Greece
        {"GT", "RU", 1}, //Guatemala
#if defined(CARRIER_TMO)
        {"GU", "XW", 0}, //Guam
#else
        {"GU", "US", 46}, //Guam
#endif
        {"GW", "IL", 10}, //Guinea-Bissau
        {"GY", "QA", 0}, //Guyana
        {"HK", "BR", 0}, //Hong Kong
        {"HN", "CN", 0}, //Honduras
        {"HR", "GB", 0}, //Croatia
        {"HT", "RU", 1}, //Haiti
        {"HU", "GB", 0}, //Hungary
        {"ID", "QA", 0}, //Indonesia
        {"IE", "GB", 0}, //Ireland
        {"IL", "IL", 10}, //Israel
        {"IM", "GB", 0}, //Isle of Man
        {"IN", "RU", 1}, //India
        {"IQ", "IL", 10}, //Iraq
        {"IR", "IL", 10}, //Iran
        {"IS", "GB", 0}, //Iceland
        {"IT", "GB", 0}, //Italy
        {"JE", "GB", 0}, //Jersey
        {"JM", "GB", 0}, //Jameica
        {"JO", "XY", 3}, //Jordan
        {"JP", "JP", 5}, //Japan
        {"KE", "GB", 0}, //Kenya
        {"KG", "IL", 10}, //Kyrgyzstan
        {"KH", "BR", 0}, //Cambodia
        {"KI", "AU", 2}, //Kiribati
        {"KM", "IL", 10}, //Comoros
        {"KP", "IL", 10}, //North Korea
        {"KR", "KR", 24}, //South Korea
        {"KW", "KW", 1}, //Kuwait
#if defined(CARRIER_TMO)
        {"KY", "XW", 0}, //Cayman Islands
#else
        {"KY", "US", 46}, //Cayman Islands
#endif
        {"KZ", "BR", 0}, //Kazakhstan
        {"LA", "KR", 24}, //Laos
        {"LB", "BR", 0}, //Lebanon
        {"LC", "BR", 0}, //Saint Lucia
        {"LI", "GB", 0}, //Liechtenstein
        {"LK", "BR", 0}, //Sri Lanka
        {"LR", "BR", 0}, //Liberia
        {"LS", "GB", 0}, //Lesotho
        {"LT", "GB", 0}, //Lithuania
        {"LU", "GB", 0}, //Luxemburg
        {"LV", "GB", 0}, //Latvia
        {"LY", "IL", 10}, //Libya
        {"MA", "KW", 1}, //Morocco
        {"MC", "GB", 0}, //Monaco
        {"MD", "GB", 0}, //Moldova
        {"ME", "GB", 0}, //Montenegro
        {"MF", "GB", 0}, //Saint Martin / Sint Marteen (Added on window's list)
        {"MG", "IL", 10}, //Madagascar
        {"MH", "BR", 0}, //Marshall Islands
        {"MK", "GB", 0}, //Macedonia
        {"ML", "IL", 10}, //Mali
        {"MM", "IL", 10}, //Burma (Myanmar)
        {"MN", "IL", 10}, //Mongolia
        {"MO", "CN", 0}, //Macau
#if defined(CARRIER_TMO)
        {"MP", "XW", 0}, //Northern Mariana Islands (Rota Island. Saipan and Tinian Island)
#else
        {"MP", "US", 46}, //Northern Mariana Islands (Rota Island. Saipan and Tinian Island)
#endif
        {"MQ", "GB", 0}, //Martinique (France)
        {"MR", "GB", 0}, //Mauritania
        {"MS", "GB", 0}, //Montserrat (UK)
        {"MT", "GB", 0}, //Malta
        {"MU", "GB", 0}, //Mauritius
        {"MD", "GB", 0}, //Moldova
        {"ME", "GB", 0}, //Montenegro
        {"MF", "GB", 0}, //Saint Martin / Sint Marteen (Added on window's list)
        {"MG", "IL", 10}, //Madagascar
        {"MH", "BR", 0}, //Marshall Islands
        {"MK", "GB", 0}, //Macedonia
        {"ML", "IL", 10}, //Mali
        {"MM", "IL", 10}, //Burma (Myanmar)
        {"MN", "IL", 10}, //Mongolia
        {"MO", "CN", 0}, //Macau
#if defined(CARRIER_TMO)
        {"MP", "XW", 0}, //Northern Mariana Islands (Rota Island. Saipan and Tinian Island)
#else
        {"MP", "US", 46}, //Northern Mariana Islands (Rota Island. Saipan and Tinian Island)
#endif
        {"MQ", "GB", 0}, //Martinique (France)
        {"MR", "GB", 0}, //Mauritania
        {"MS", "GB", 0}, //Montserrat (UK)
        {"MT", "GB", 0}, //Malta
        {"MU", "GB", 0}, //Mauritius
        {"MV", "RU", 1}, //Maldives
        {"MW", "CN", 0}, //Malawi
        {"MX", "RU", 1}, //Mexico
        {"MY", "RU", 1}, //Malaysia
        {"MZ", "BR", 0}, //Mozambique
        {"NA", "BR", 0}, //Namibia
        {"NC", "IL", 10}, //New Caledonia
        {"NE", "BR", 0}, //Niger
        {"NF", "BR", 0}, //Norfolk Island
        {"NG", "NG", 0}, //Nigeria
        {"NI", "BR", 0}, //Nicaragua
        {"NL", "GB", 0}, //Netherlands
        {"NO", "GB", 0}, //Norway
        {"NP", "SA", 0}, //Nepal
        {"NR", "IL", 10}, //Nauru
        {"NU", "BR", 0}, //Niue
        {"NZ", "BR", 0}, //New Zealand
        {"OM", "GB", 0}, //Oman
        {"PA", "RU", 1}, //Panama
        {"PE", "BR", 0}, //Peru
        {"PF", "GB", 0}, //French Polynesia (France)
        {"PG", "XY", 3}, //Papua New Guinea
        {"PH", "BR", 0}, //Philippines
        {"PK", "CN", 0}, //Pakistan
        {"PL", "GB", 0}, //Poland
        {"PM", "GB", 0}, //Saint Pierre and Miquelon
        {"PN", "GB", 0}, //Pitcairn Islands
#if defined(CARRIER_TMO)
        {"PR", "XW", 0}, //Puerto Rico (USA)
#else
        {"PR", "US", 46}, //Puerto Rico (USA)
#endif
        {"PS", "BR", 0}, //Palestinian Authority
        {"PT", "GB", 0}, //Portugal
        {"PW", "BR", 0}, //Palau
        {"PY", "BR", 0}, //Paraguay
        {"QA", "CN", 0}, //Qatar
        {"RE", "GB", 0}, //Reunion (France)
        {"RKS", "IL", 10}, //Kosvo (Added on window's list)
        {"RO", "GB", 0}, //Romania
        {"RS", "GB", 0}, //Serbia
        {"RU", "RU", 10}, //Russia
        {"RW", "CN", 0}, //Rwanda
        {"SA", "SA", 0}, //Saudi Arabia
        {"SB", "IL", 10}, //Solomon Islands
        {"SC", "IL", 10}, //Seychelles
        {"SD", "GB", 0}, //Sudan
        {"SE", "GB", 0}, //Sweden
        {"SG", "BR", 0}, //Singapole
        {"SI", "GB", 0}, //Slovenia
        {"SK", "GB", 0}, //Slovakia
        {"SKN", "CN", 0}, //Saint Kitts and Nevis
        {"SL", "IL", 10}, //Sierra Leone
        {"SM", "GB", 0}, //San Marino
        {"SN", "GB", 0}, //Senegal
        {"SO", "IL", 10}, //Somalia
        {"SR", "IL", 10}, //Suriname
        {"SS", "GB", 0}, //South_Sudan
        {"ST", "IL", 10}, //Sao Tome and Principe
        {"SV", "RU", 1}, //El Salvador
        {"SY", "BR", 0}, //Syria
        {"SZ", "IL", 10}, //Swaziland
        {"TC", "GB", 0}, //Turks and Caicos Islands (UK)
        {"TD", "IL", 10}, //Chad
        {"TF", "GB", 0}, //French Southern and Antarctic Lands)
        {"TG", "IL", 10}, //Togo
        {"TH", "BR", 0}, //Thailand
        {"TJ", "IL", 10}, //Tajikistan
        {"TL", "BR", 0}, //East Timor
        {"TM", "IL", 10}, //Turkmenistan
        {"TN", "KW", 1}, //Tunisia
        {"TO", "IL", 10}, //Tonga
        {"TR", "GB", 0}, //Turkey
        {"TT", "BR", 0}, //Trinidad and Tobago
        {"TV", "IL", 10}, //Tuvalu
        {"TW", "TW", 2}, //Taiwan
        {"TZ", "CN", 0}, //Tanzania
        {"UA", "RU", 1}, //Ukraine
        {"UG", "BR", 0}, //Ugnada
#if defined(CARRIER_TMO)
        {"US", "XW", 0}, //US TMUS
#else
        {"US", "US", 46}, //US 4330
#endif
        {"UY", "BR", 0}, //Uruguay
        {"UZ", "IL", 10}, //Uzbekistan
        {"VA", "GB", 0}, //Vatican (Holy See)
        {"VC", "BR", 0}, //Saint Vincent and the Grenadines
        {"VE", "RU", 1}, //Venezuela
        {"VG", "GB", 0}, //British Virgin Islands
#if defined(CARRIER_TMO)
        {"VI", "XW", 0}, //US Virgin Islands
#else
        {"VI", "US", 46}, //US Virgin Islands
#endif
        {"VN", "BR", 0}, //Vietnam
        {"VU", "IL", 10}, //Vanuatu
        {"WS", "SA", 0}, //Samoa
        {"YE", "IL", 10}, //Yemen
        {"YT", "GB", 0}, //Mayotte (France)
        {"ZA", "GB", 0}, //South Africa
        {"ZM", "RU", 1}, //Zambia
        {"ZW", "BR", 0}, //Zimbabwe
};
#else //CONFIG_BCM4334

const struct cntry_locales_custom bcm_wifi_translate_custom_table[] = {
/* Table should be filled out based on custom platform regulatory requirement */
        {"",   "XY", 4}, /* Universal if Country code is unknown or empty */
        {"AD", "GB", 0}, //Andorra
        {"AE", "KR", 24}, //UAE
        {"AF", "AF", 0}, //Afghanistan
#if defined(CARRIER_TMO)
        {"AG", "XW", 0}, //Antigua & Barbuda
        {"AI", "XW", 0}, //Anguilla
#else
        {"AG", "US", 100}, //Antigua & Barbuda
        {"AI", "US", 100}, //Anguilla
#endif
        {"AL", "GB", 0}, //Albania
        {"AM", "IL", 10}, //Armenia
        {"AN", "BR", 0}, //Netherlands Antilles
        {"AO", "IL", 10}, //Angola
        {"AR", "BR", 0}, //Argentina
#if defined(CARRIER_TMO)
        {"AS", "XW", 0}, //American Samoa (USA)
#else
        {"AS", "US", 100}, //American Samoa (USA)
#endif
        {"AT", "GB", 0}, //Austria
        {"AU", "AU", 2}, //Australia
        {"AW", "KR", 24}, //Aruba
        {"AZ", "BR", 0}, //Azerbaijan
        {"BA", "GB", 0}, //Bosnia and Herzegovina
        {"BB", "BS", 0}, //Barbados
        {"BD", "CN", 0}, //Bangladesh
        {"BE", "GB", 0}, //Belgium
        {"BF", "CN", 0}, //Burkina Faso
        {"BG", "GB", 0}, //Bulgaria
        {"BH", "BS", 0}, //Bahrain
        {"BI", "IL", 10}, //Burundi
        {"BJ", "IL", 10}, //Benin
#if defined(CARRIER_TMO)
        {"BM", "XW", 0}, //Bermuda
#else
        {"BM", "US", 100}, //Bermuda
#endif
        {"BN", "BS", 0}, //Brunei
        {"BO", "IL", 10}, //Bolivia
        {"BR", "BR", 0}, //Brazil
        {"BS", "BS", 0}, //Bahamas
        {"BT", "IL", 10}, //Bhntan
        {"BW", "GB", 0}, //Botswana
        {"BY", "GB", 0}, //Belarus
        {"BZ", "IL", 10}, //Belize
#if defined(CARRIER_TMO)
        {"CA", "XW", 0}, //Canada
#else
        {"CA", "US", 100}, //Canada
#endif
        {"CD", "IL", 10}, //Congo. Democratic Republic of the
        {"CF", "IL", 10}, //Central African Republic
        {"CG", "IL", 10}, //Congo. Republic of the
        {"CH", "GB", 0}, //Switzerland
        {"CI", "IL", 10}, //Cote d'lvoire
        {"CK", "BR", 0}, //Cook Island
        {"CL", "BS", 0}, //Chile
        {"CM", "IL", 10}, //Cameroon
        {"CN", "CN", 0}, //China
        {"CO", "BR", 0}, //Columbia
        {"CR", "BR", 0}, //Costa Rica
        {"CU", "BR", 0}, //Cuba
        {"CV", "GB", 0}, //Cape Verde
        {"CX", "AU", 2}, //Christmas Island (Australia)
        {"CY", "GB", 0}, //Cyprus
        {"CZ", "GB", 0}, //Czech
        {"DE", "GB", 0}, //Germany
        {"DJ", "IL", 10}, //Djibouti
        {"DK", "GB", 0}, //Denmark
        {"DM", "BR", 0}, //Dominica
        {"DO", "BR", 0}, //Dominican Republic
        {"DZ", "KW", 1}, //       
        {"EC", "BR", 0}, //Ecuador
        {"EE", "GB", 0}, //Estonia
        {"EG", "BS", 0}, //Egypt
        {"ER", "IL", 10}, //Eritrea
        {"ES", "GB", 0}, //Spain
        {"ET", "GB", 0}, //Ethiopia
        {"FI", "GB", 0}, //Finland
        {"FJ", "IL", 10}, //Fiji
#if defined(CARRIER_TMO)
        {"FM", "XW", 0}, //Federated States of Micronesia
#else
        {"FM", "US", 100}, //Federated States of Micronesia
#endif
        {"FO", "GB", 0}, //Faroe Island
        {"FR", "GB", 0}, //France
        {"GA", "IL", 10}, //Gabon
        {"GB", "GB", 0}, //United Kingdom
        {"GD", "BR", 0}, //Grenada
        {"GE", "GB", 0}, //Georgia
        {"GF", "GB", 0}, //French Guiana
        {"GH", "BR", 0}, //Ghana
        {"GI", "GB", 0}, //Gibraltar
        {"GM", "IL", 10}, //Gambia
        {"GN", "IL", 10}, //Guinea
        {"GP", "GB", 0}, //Guadeloupe
        {"GQ", "IL", 10}, //Equatorial Guinea
        {"GR", "GB", 0}, //Greece
        {"GT", "BS", 0}, //Guatemala
#if defined(CARRIER_TMO)
        {"GU", "XW", 0}, //Guam
#else
        {"GU", "US", 100}, //Guam
#endif
        {"GW", "IL", 10}, //Guinea-Bissau
        {"GY", "ID", 1}, //Guyana
        {"HK", "BR", 0}, //Hong Kong
        {"HN", "CN", 0}, //Honduras
        {"HR", "GB", 0}, //Croatia
        {"HT", "BS", 0}, //Haiti
        {"HU", "GB", 0}, //Hungary
        {"ID", "ID", 1}, //Indonesia
        {"IE", "GB", 0}, //Ireland
        {"IL", "IL", 10}, //Israel
        {"IM", "GB", 0}, //Isle of Man
        {"IN", "BS", 0}, //India
        {"IQ", "IL", 10}, //Iraq
        {"IR", "IL", 10}, //Iran
        {"IS", "GB", 0}, //Iceland
        {"IT", "GB", 0}, //Italy
        {"JE", "GB", 0}, //Jersey
        {"JM", "GB", 0}, //Jameica
        {"JO", "XY", 3}, //Jordan
        {"JP", "JP", 5}, //Japan
        {"KE", "GB", 0}, //Kenya
        {"KG", "IL", 10}, //Kyrgyzstan
        {"KH", "BR", 0}, //Cambodia
        {"KI", "AU", 2}, //Kiribati
        {"KM", "IL", 10}, //Comoros
        {"KP", "IL", 10}, //North Korea
        {"KR", "KR", 24}, //South Korea
        {"KW", "KW", 1}, //Kuwait
#if defined(CARRIER_TMO)
        {"KY", "XW", 0}, //Cayman Islands
#else
        {"KY", "US", 100}, //Cayman Islands
#endif
        {"KZ", "BR", 0}, //Kazakhstan
        {"LA", "KR", 24}, //Laos
        {"LB", "BR", 0}, //Lebanon
        {"LC", "BR", 0}, //Saint Lucia
        {"LI", "GB", 0}, //Liechtenstein
        {"LK", "BR", 0}, //Sri Lanka
        {"LR", "BR", 0}, //Liberia
        {"LS", "GB", 0}, //Lesotho
        {"LT", "GB", 0}, //Lithuania
        {"LU", "GB", 0}, //Luxemburg
        {"LV", "GB", 0}, //Latvia
        {"LY", "IL", 10}, //Libya
        {"MA", "KW", 1}, //Morocco
        {"MC", "GB", 0}, //Monaco
        {"MD", "GB", 0}, //Moldova
        {"ME", "GB", 0}, //Montenegro
        {"MF", "GB", 0}, //Saint Martin / Sint Marteen (Added on window's list)
        {"MG", "IL", 10}, //Madagascar
        {"MH", "BR", 0}, //Marshall Islands
        {"MK", "GB", 0}, //Macedonia
        {"ML", "IL", 10}, //Mali
        {"MM", "IL", 10}, //Burma (Myanmar)
        {"MN", "IL", 10}, //Mongolia
        {"MO", "CN", 0}, //Macau
#if defined(CARRIER_TMO)
        {"MP", "XW", 0}, //Northern Mariana Islands (Rota Island. Saipan and Tinian Island)
#else
        {"MP", "US", 100}, //Northern Mariana Islands (Rota Island. Saipan and Tinian Island)
#endif
        {"MQ", "GB", 0}, //Martinique (France)
        {"MR", "GB", 0}, //Mauritania
        {"MS", "GB", 0}, //Montserrat (UK)
        {"MT", "GB", 0}, //Malta
        {"MU", "GB", 0}, //Mauritius
        {"MD", "GB", 0}, //Moldova
        {"ME", "GB", 0}, //Montenegro
        {"MF", "GB", 0}, //Saint Martin / Sint Marteen (Added on window's list)
        {"MG", "IL", 10}, //Madagascar
        {"MH", "BR", 0}, //Marshall Islands
        {"MK", "GB", 0}, //Macedonia
        {"ML", "IL", 10}, //Mali
        {"MM", "IL", 10}, //Burma (Myanmar)
        {"MN", "IL", 10}, //Mongolia
        {"MO", "CN", 0}, //Macau
#if defined(CARRIER_TMO)
        {"MP", "XW", 0}, //Northern Mariana Islands (Rota Island. Saipan and Tinian Island)
#else
        {"MP", "US", 100}, //Northern Mariana Islands (Rota Island. Saipan and Tinian Island)
#endif
        {"MQ", "GB", 0}, //Martinique (France)
        {"MR", "GB", 0}, //Mauritania
        {"MS", "GB", 0}, //Montserrat (UK)
        {"MT", "GB", 0}, //Malta
        {"MU", "GB", 0}, //Mauritius
        {"MV", "BS", 0}, //Maldives
        {"MW", "CN", 0}, //Malawi
        {"MX", "BS", 0}, //Mexico
        {"MY", "BS", 0}, //Malaysia
        {"MZ", "BR", 0}, //Mozambique
        {"NA", "BR", 0}, //Namibia
        {"NC", "IL", 10}, //New Caledonia
        {"NE", "BR", 0}, //Niger
        {"NF", "BR", 0}, //Norfolk Island
        {"NG", "NG", 0}, //Nigeria
        {"NI", "BR", 0}, //Nicaragua
        {"NL", "GB", 0}, //Netherlands
        {"NO", "GB", 0}, //Norway
        {"NP", "NP", 0}, //Nepal
        {"NR", "IL", 10}, //Nauru
        {"NU", "BR", 0}, //Niue
        {"NZ", "BR", 0}, //New Zealand
        {"OM", "GB", 0}, //Oman
        {"PA", "BS", 0}, //Panama
        {"PE", "BR", 0}, //Peru
        {"PF", "GB", 0}, //French Polynesia (France)
        {"PG", "XY", 3}, //Papua New Guinea
        {"PH", "BR", 0}, //Philippines
        {"PK", "CN", 0}, //Pakistan
        {"PL", "GB", 0}, //Poland
        {"PM", "GB", 0}, //Saint Pierre and Miquelon
        {"PN", "GB", 0}, //Pitcairn Islands
#if defined(CARRIER_TMO)
        {"PR", "XW", 0}, //Puerto Rico (USA)
#else
        {"PR", "US", 100}, //Puerto Rico (USA)
#endif
        {"PS", "BR", 0}, //Palestinian Authority
        {"PT", "GB", 0}, //Portugal
        {"PW", "BR", 0}, //Palau
        {"PY", "BR", 0}, //Paraguay
        {"QA", "CN", 0}, //Qatar
        {"RE", "GB", 0}, //Reunion (France)
        {"RKS", "IL", 10}, //Kosvo (Added on window's list)
        {"RO", "GB", 0}, //Romania
        {"RS", "GB", 0}, //Serbia
        {"RU", "BS", 0}, //Russia
        {"RW", "CN", 0}, //Rwanda
        {"SA", "NP", 0}, //Saudi Arabia
        {"SB", "IL", 10}, //Solomon Islands
        {"SC", "IL", 10}, //Seychelles
        {"SD", "GB", 0}, //Sudan
        {"SE", "GB", 0}, //Sweden
        {"SG", "BR", 0}, //Singapole
        {"SI", "GB", 0}, //Slovenia
        {"SK", "GB", 0}, //Slovakia
        {"SKN", "CN", 0}, //Saint Kitts and Nevis
        {"SL", "IL", 10}, //Sierra Leone
        {"SM", "GB", 0}, //San Marino
        {"SN", "GB", 0}, //Senegal
        {"SO", "IL", 10}, //Somalia
        {"SR", "IL", 10}, //Suriname
        {"SS", "GB", 0}, //South_Sudan
        {"ST", "IL", 10}, //Sao Tome and Principe
        {"SV", "BS", 0}, //El Salvador
        {"SY", "BR", 0}, //Syria
        {"SZ", "IL", 10}, //Swaziland
        {"TC", "GB", 0}, //Turks and Caicos Islands (UK)
        {"TD", "IL", 10}, //Chad
        {"TF", "GB", 0}, //French Southern and Antarctic Lands)
        {"TG", "IL", 10}, //Togo
        {"TH", "BR", 0}, //Thailand
        {"TJ", "IL", 10}, //Tajikistan
        {"TL", "BR", 0}, //East Timor
        {"TM", "IL", 10}, //Turkmenistan
        {"TN", "KW", 1}, //Tunisia
        {"TO", "IL", 10}, //Tonga
        {"TR", "GB", 0}, //Turkey
        {"TT", "BR", 0}, //Trinidad and Tobago
        {"TV", "IL", 10}, //Tuvalu
        {"TW", "TW", 2}, //Taiwan
        {"TZ", "CN", 0}, //Tanzania
        {"UA", "BS", 0}, //Ukraine
        {"UG", "BR", 0}, //Ugnada
#if defined(CARRIER_TMO)
//        {"US", "XW", 0}, //US TMUS
        {"US", "US", 100}, //US 4334
#else
        {"US", "US", 100}, //US 4334
#endif
        {"UY", "BR", 0}, //Uruguay
        {"UZ", "IL", 10}, //Uzbekistan
        {"VA", "GB", 0}, //Vatican (Holy See)
        {"VC", "BR", 0}, //Saint Vincent and the Grenadines
        {"VE", "BS", 0}, //Venezuela
        {"VG", "GB", 0}, //British Virgin Islands
#if defined(CARRIER_TMO)
        {"VI", "XW", 0}, //US Virgin Islands
#else
        {"VI", "US", 100}, //US Virgin Islands
#endif
        {"VN", "BR", 0}, //Vietnam
        {"VU", "IL", 10}, //Vanuatu
        {"WS", "NP", 0}, //Samoa
        {"YE", "IL", 10}, //Yemen
        {"YT", "GB", 0}, //Mayotte (France)
        {"ZA", "GB", 0}, //South Africa
        {"ZM", "BS", 0}, //Zambia
        {"ZW", "BR", 0}, //Zimbabwe
};
#endif

static void *bcm_wifi_get_country_code(char *ccode)
{
	int size, i;

	size = ARRAY_SIZE(bcm_wifi_translate_custom_table);

	if (size == 0)
		return NULL;

	for (i = 0; i < size; i++) {
		if (strcmp(ccode, bcm_wifi_translate_custom_table[i].iso_abbrev) == 0) {
			return (void *)&bcm_wifi_translate_custom_table[i];
		}
	}   

	/* if no country code matched return first universal code from bcm_wifi_translate_custom_table */
	return (void *)&bcm_wifi_translate_custom_table[0];
}

static struct wifi_platform_data bcm_wifi_control = {
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	.mem_prealloc	= brcm_wlan_mem_prealloc,
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */
	.set_power	= bcm_wifi_set_power,
	.set_reset      = bcm_wifi_reset,
	.set_carddetect = bcm_wifi_carddetect,
	.get_mac_addr   = bcm_wifi_get_mac_addr, // Get custom MAC address
	.get_country_code = bcm_wifi_get_country_code,
	.get_sdc4_clock_force_disable = get_sdc4_clock_force_disable,
};

static struct resource wifi_resource[] = {
	[0] = {
		.name = "bcmdhd_wlan_irq",
		.start = MSM_GPIO_TO_INT(WLAN_HOSTWAKE),
		.end   = MSM_GPIO_TO_INT(WLAN_HOSTWAKE),
#ifdef CONFIG_BCMDHD_HW_OOB
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL | IORESOURCE_IRQ_SHAREABLE, //for HW_OOB
#else 
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE | IORESOURCE_IRQ_SHAREABLE,  //for SW_OOB
#endif
	},
};

static struct platform_device bcm_wifi_device = {
	.name           = "bcmdhd_wlan",
	.id             = 1,
	.num_resources  = ARRAY_SIZE(wifi_resource),
	.resource       = wifi_resource,
	.dev            = {
		.platform_data = &bcm_wifi_control,
	},
};
#endif /* CONFIG_MMC_MSM_SDC4_SUPPORT */
void __init msm8930_init_mmc(void)
{

#ifdef CONFIG_MACH_MSM8930_FX3
	/* It need to gpio init */
#if defined(CONFIG_MACH_LGE_FX3_VZW)
	gpio_tlmm_config(GPIO_CFG(100, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
#else
	gpio_tlmm_config(GPIO_CFG(94, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
#endif
#endif

#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
	/*
	 * When eMMC runs in DDR mode on CDP platform, we have
	 * seen instability due to DATA CRC errors. These errors are
	 * attributed to long physical path between MSM and eMMC on CDP.
	 * So let's not enable the DDR mode on CDP platform but let other
	 * platforms take advantage of eMMC DDR mode.
	 */
	if (!machine_is_msm8930_cdp())
		msm8960_sdc1_data.uhs_caps |= (MMC_CAP_1_8V_DDR |
					       MMC_CAP_UHS_DDR50);
	/* SDC1 : eMMC card connected */
	msm_add_sdcc(1, &msm8960_sdc1_data);
#endif
#ifdef CONFIG_MMC_MSM_SDC3_SUPPORT
	/*
	 * All 8930 platform boards using the 1.2 SoC have been reworked so that
	 * the sd card detect line's esd circuit is no longer powered by the sd
	 * card's voltage regulator. So this means we can turn the regulator off
	 * to save power without affecting the sd card detect functionality.
	 * This change to the boards will be true for newer versions of the SoC
	 * as well.
	 */
	if ((SOCINFO_VERSION_MAJOR(socinfo_get_version()) >= 1 &&
			SOCINFO_VERSION_MINOR(socinfo_get_version()) >= 2) ||
			machine_is_msm8930_cdp()) {
		msm8960_sdc3_data.vreg_data->vdd_data->always_on = false;
		msm8960_sdc3_data.vreg_data->vdd_data->reset_at_init = false;
	}

	/* SDC3: External card slot */
	if (!machine_is_msm8930_cdp()) {
		msm8960_sdc3_data.wpswitch_gpio = 0;
		msm8960_sdc3_data.is_wpswitch_active_low = false;
	}

	msm_add_sdcc(3, &msm8960_sdc3_data);
#endif
#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
	/* SDC4: SDIO slot for WLAN */
	msm_add_sdcc(4, &msm8930_sdc4_data); //for compiling
#endif
#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
	bcm_wifi_init_gpio_mem();
	platform_device_register(&bcm_wifi_device);
#endif /* CONFIG_MMC_MSM_SDC4_SUPPORT */
}
