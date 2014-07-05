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
#include <mach/msm_bus_board.h>
#include <mach/msm_memtypes.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/socinfo.h>
#include <linux/ion.h>
#include <mach/ion.h>

#include "devices.h"

#include <mach/board_lge.h>
#include "board-fx3.h"

#include <linux/fb.h>
#include "../../../../drivers/video/msm/msm_fb.h"
#include "../../../../drivers/video/msm/msm_fb_def.h"
#include "../../../../drivers/video/msm/mipi_dsi.h"

#ifdef CONFIG_LGE_KCAL
#ifdef CONFIG_LGE_QC_LCDC_LUT
extern int set_qlut_kcal_values(int kcal_r, int kcal_g, int kcal_b);
extern int refresh_qlut_display(void);
#else
#error only kcal by Qucalcomm LUT is supported now!!!
#endif
#endif
#ifdef CONFIG_FB_MSM_TRIPLE_BUFFER
#define MSM_FB_PRIM_BUF_SIZE \
		(roundup((LCD_RESOLUTION_X  * LCD_RESOLUTION_Y * 4), 4096) * 3) /* 4 bpp x 3 pages */
#else
#define MSM_FB_PRIM_BUF_SIZE \
		(roundup((LCD_RESOLUTION_X  * LCD_RESOLUTION_Y * 4), 4096) * 2) /* 4 bpp x 2 pages */
#endif
/* Note: must be multiple of 4096 */
#define MSM_FB_SIZE roundup(MSM_FB_PRIM_BUF_SIZE, 4096)

#ifdef CONFIG_FB_MSM_OVERLAY0_WRITEBACK
#define MSM_FB_OVERLAY0_WRITEBACK_SIZE roundup((LCD_RESOLUTION_X * LCD_RESOLUTION_Y * 3 * 2), 4096)
#else
#define MSM_FB_OVERLAY0_WRITEBACK_SIZE (0)
#endif  /* CONFIG_FB_MSM_OVERLAY0_WRITEBACK */

#ifdef CONFIG_FB_MSM_OVERLAY1_WRITEBACK
#define MSM_FB_OVERLAY1_WRITEBACK_SIZE roundup((1920 * 1088 * 3 * 2), 4096)
#else
#define MSM_FB_OVERLAY1_WRITEBACK_SIZE (0)
#endif  /* CONFIG_FB_MSM_OVERLAY1_WRITEBACK */

#define MDP_VSYNC_GPIO 0

#define MIPI_CMD_NOVATEK_QHD_PANEL_NAME	"mipi_cmd_novatek_qhd"
#define MIPI_VIDEO_NOVATEK_QHD_PANEL_NAME	"mipi_video_novatek_qhd"
#define MIPI_VIDEO_TOSHIBA_WSVGA_PANEL_NAME	"mipi_video_toshiba_wsvga"
#define MIPI_VIDEO_CHIMEI_WXGA_PANEL_NAME	"mipi_video_chimei_wxga"
#define MIPI_VIDEO_SIMULATOR_VGA_PANEL_NAME	"mipi_video_simulator_vga"
#define MIPI_CMD_RENESAS_FWVGA_PANEL_NAME	"mipi_cmd_renesas_fwvga"
#define HDMI_PANEL_NAME	"hdmi_msm"
#define MHL_PANEL_NAME "hdmi_msm,mhl_8334"
#define TVOUT_PANEL_NAME	"tvout_msm"

static unsigned char mhl_display_enabled;

unsigned char msm8930_mhl_display_enabled(void)
{
	return mhl_display_enabled;
}

#ifdef CONFIG_LGE_LCD_TUNING
#define TUNING_BUFSIZE 4096
#define TUNING_REGSIZE 50
#define TUNING_REGNUM 16
#if defined(CONFIG_FB_MSM_MIPI_TX11D108VM_R69324A_VIDEO_QHD_PT)
static u32 porch_value[6] = {52, 48, 8, 7, 8, 1};
#elif defined(CONFIG_FB_MSM_MIPI_HIMAX_H8379A_VIDEO_WVGA_PT)
static u32 porch_value[6] = {64, 136, 4, 11, 13, 2};
#else
static u32 porch_value[6] = {64, 144, 4, 7, 8, 1};
#endif  //defined ( CONFIG_FB_MSM_MIPI_TX11D108VM_R69324A_VIDEO_QHD_PT )
#endif

#if (defined(CONFIG_FB_MSM_MIPI_R61529_VIDEO_HVGA_PT_PANEL) || \
	defined(CONFIG_FB_MSM_MIPI_R61529_CMD_HVGA_PT_PANEL))
#define R61529_CMD_DELAY 0 /* 50 */
#define R61529_IFMODE_GPIO 3
#endif

#if defined(CONFIG_FB_MSM_MIPI_HITACHI_R69324A_VIDEO_WVGA_PT)
#define HITACHI_R69324A_GAMMA
#endif

//                                                 
#if defined(CONFIG_FB_MSM_MIPI_HITACHI_R69328A_VIDEO_HD_PT) 
#define HITACHI_R69328_GAMMA
#endif
//                                                 


static struct resource msm_fb_resources[] = {
	{
		.flags = IORESOURCE_DMA,
	}
};

#ifndef CONFIG_MACH_LGE
#ifndef CONFIG_FB_MSM_MIPI_PANEL_DETECT
static void set_mdp_clocks_for_wuxga(void);
#endif
#endif
static int msm_fb_detect_panel(const char *name)
{
#ifndef CONFIG_MACH_LGE
	if (!strncmp(name, MIPI_CMD_NOVATEK_QHD_PANEL_NAME,
			strnlen(MIPI_CMD_NOVATEK_QHD_PANEL_NAME,
				PANEL_NAME_MAX_LEN)))
		return 0;

#if !defined(CONFIG_FB_MSM_LVDS_MIPI_PANEL_DETECT) && \
	!defined(CONFIG_FB_MSM_MIPI_PANEL_DETECT)
	if (!strncmp(name, MIPI_VIDEO_NOVATEK_QHD_PANEL_NAME,
			strnlen(MIPI_VIDEO_NOVATEK_QHD_PANEL_NAME,
				PANEL_NAME_MAX_LEN)))
		return 0;

	if (!strncmp(name, MIPI_VIDEO_TOSHIBA_WSVGA_PANEL_NAME,
			strnlen(MIPI_VIDEO_TOSHIBA_WSVGA_PANEL_NAME,
				PANEL_NAME_MAX_LEN)))
		return 0;

	if (!strncmp(name, MIPI_VIDEO_SIMULATOR_VGA_PANEL_NAME,
			strnlen(MIPI_VIDEO_SIMULATOR_VGA_PANEL_NAME,
				PANEL_NAME_MAX_LEN)))
		return 0;

	if (!strncmp(name, MIPI_CMD_RENESAS_FWVGA_PANEL_NAME,
			strnlen(MIPI_CMD_RENESAS_FWVGA_PANEL_NAME,
				PANEL_NAME_MAX_LEN)))
		return 0;
#endif

	if (!strncmp(name, HDMI_PANEL_NAME,
			strnlen(HDMI_PANEL_NAME,
				PANEL_NAME_MAX_LEN)))
		return 0;

	if (!strncmp(name, TVOUT_PANEL_NAME,
			strnlen(TVOUT_PANEL_NAME,
				PANEL_NAME_MAX_LEN)))
		return 0;

	pr_warning("%s: not supported '%s'", __func__, name);
	return -ENODEV;
#else
	return 0;
#endif
}

static struct msm_fb_platform_data msm_fb_pdata = {
	.detect_client = msm_fb_detect_panel,
#ifdef CONFIG_LGE_LCD_TUNING
	.porch = porch_value,
#endif
		
};

static struct platform_device msm_fb_device = {
	.name   = "msm_fb",
	.id     = 0,
	.num_resources     = ARRAY_SIZE(msm_fb_resources),
	.resource          = msm_fb_resources,
	.dev.platform_data = &msm_fb_pdata,
};

static bool dsi_power_on;

#if	defined(CONFIG_FB_MSM_MIPI_DSI_HIMAX)
static int lcd_maker_id = 0;
static int get_lcd_initial_table ( int lcd_id );
#endif

#if defined(CONFIG_FB_MSM_MIPI_TX11D108VM_R69324A_VIDEO_QHD_PT)
extern unsigned int system_rev;
#endif

//                                                 
#if defined(CONFIG_LCD_BIAS_TPS65132)
extern void tps65132_lcd_bias_enable(int onoff);
#endif
//                                                 

#if defined(CONFIG_FB_MSM_MIPI_HITACHI_R69324A_VIDEO_WVGA_PT) || \
	defined(CONFIG_FB_MSM_MIPI_HIMAX_H8379A_VIDEO_WVGA_PT) || \
	defined(CONFIG_FB_MSM_MIPI_TX11D108VM_R69324A_VIDEO_QHD_PT) || \
	defined(CONFIG_FB_MSM_MIPI_HITACHI_R69328A_VIDEO_HD_PT) ||\
	defined(CONFIG_FB_MSM_MIPI_TX13D107VM_NT35521_VIDEO_HD_PT) 

static int mipi_dsi_panel_power(int on)
{
#if defined(CONFIG_FB_MSM_MIPI_TX13D107VM_NT35521_VIDEO_HD_PT)
	static struct regulator *reg_l2, *reg_l23;
#else
	static struct regulator *reg_l8, *reg_l2, *reg_l23;
#endif
	int rc;

	printk("%s: state : %d\n", __func__, on);

	if (!dsi_power_on) {
#if	defined(CONFIG_FB_MSM_MIPI_DSI_HIMAX)
		gpio_tlmm_config(GPIO_CFG(89, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE); 
		rc = gpio_request(89, "lcd_maker_id");
		if (rc) {
			pr_err("request gpio lcd_maker_id failed, rc=%d\n", rc);
			return -ENODEV;
		}
#endif

#if !defined(CONFIG_FB_MSM_MIPI_TX13D107VM_NT35521_VIDEO_HD_PT)
		reg_l8 = regulator_get(&msm_mipi_dsi1_device.dev,
				"dsi_vdc");
		if (IS_ERR(reg_l8)) {
			pr_err("could not get 8038_l8, rc = %ld\n",
				PTR_ERR(reg_l8));
			return -ENODEV;
		}
#endif

		reg_l23 = regulator_get(&msm_mipi_dsi1_device.dev,
				"8038_l23");
		if (IS_ERR(reg_l23)) {
			pr_err("could not get 8038_l23, rc = %ld\n",
				PTR_ERR(reg_l23));
			return -ENODEV;
		}

		reg_l2 = regulator_get(&msm_mipi_dsi1_device.dev,
				"dsi_vdda");
		if (IS_ERR(reg_l2)) {
			pr_err("could not get 8038_l2, rc = %ld\n",
				PTR_ERR(reg_l2));
			return -ENODEV;
		}
#if !defined(CONFIG_FB_MSM_MIPI_TX13D107VM_NT35521_VIDEO_HD_PT)
#if defined(CONFIG_FB_MSM_MIPI_TX11D108VM_R69324A_VIDEO_QHD_PT)
		rc = regulator_set_voltage(reg_l8, 3000000, 3000000);
#else
		rc = regulator_set_voltage(reg_l8, 2800000, 2800000);
#endif
		if (rc) {
			pr_err("set_voltage l8 failed, rc=%d\n", rc);
			return -EINVAL;
		}
#endif

		rc = regulator_set_voltage(reg_l2, 1200000, 1200000);
		if (rc) {
			pr_err("set_voltage l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}

		rc = regulator_set_voltage(reg_l23, 1800000, 1800000);
		if (rc) {
			pr_err("set_voltage l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}

		gpio_tlmm_config(GPIO_CFG(58, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE); 

		rc = gpio_request(58, "disp_rst_n");
		if (rc) {
			pr_err("request gpio 58 failed, rc=%d\n", rc);
			return -ENODEV;
		}
#if defined(CONFIG_FB_MSM_MIPI_TX11D108VM_R69324A_VIDEO_QHD_PT)
		gpio_tlmm_config(GPIO_CFG(53, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

		rc = gpio_request(53, "lcd_ldo_en");
		if (rc) {
			pr_err("request gpio 53 failed, rc=%d\n", rc);
			return -ENODEV;
		}
#endif

		dsi_power_on = true;
	}

	if (on) {
#if	defined(CONFIG_FB_MSM_MIPI_DSI_HIMAX)
		lcd_maker_id = gpio_get_value_cansleep(89);
		printk(KERN_INFO "lcd_maker_id = %d\n", lcd_maker_id);
		get_lcd_initial_table( lcd_maker_id );
#endif		

#if defined(CONFIG_FB_MSM_MIPI_TX11D108VM_R69324A_VIDEO_QHD_PT)
		gpio_direction_output(53, 1);
#endif

#if !defined(CONFIG_FB_MSM_MIPI_TX13D107VM_NT35521_VIDEO_HD_PT)
		rc = regulator_set_optimum_mode(reg_l8, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l8 failed, rc=%d\n", rc);
			return -EINVAL;
		}
#endif

		rc = regulator_set_optimum_mode(reg_l23, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l23 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = regulator_set_optimum_mode(reg_l2, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}

		rc = regulator_enable(reg_l23);
		if (rc) {
			pr_err("enable l23 failed, rc=%d\n", rc);
			return -ENODEV;
		}

#if !defined(CONFIG_FB_MSM_MIPI_TX11D108VM_R69324A_VIDEO_QHD_PT)
		mdelay(5);
#endif

#if !defined(CONFIG_FB_MSM_MIPI_TX13D107VM_NT35521_VIDEO_HD_PT)
		rc = regulator_enable(reg_l8);
		if (rc) {
			pr_err("enable l8 failed, rc=%d\n", rc);
			return -ENODEV;
		}
#endif

#if !defined(CONFIG_FB_MSM_MIPI_TX11D108VM_R69324A_VIDEO_QHD_PT)
		mdelay(5);
#endif

		rc = regulator_enable(reg_l2);
		if (rc) {
			pr_err("enable l2 failed, rc=%d\n", rc);
			return -ENODEV;
		}

#if !defined(CONFIG_FB_MSM_MIPI_TX11D108VM_R69324A_VIDEO_QHD_PT)
		mdelay(5);
#endif

#if defined(CONFIG_FB_MSM_MIPI_TX13D107VM_NT35521_VIDEO_HD_PT)
		tps65132_lcd_bias_enable(1);
		mdelay(1);
#endif

#if !defined(CONFIG_FB_MSM_MIPI_DSI_HIMAX) || defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS)
		mipi_dsi_phy_ctrl(0);
#endif

#ifndef CONFIG_FB_MSM_MIPI_DSI_HIMAX
#if defined(CONFIG_FB_MSM_MIPI_TX11D108VM_R69324A_VIDEO_QHD_PT)
		mdelay(2);
		gpio_direction_output(58, 1);
		mdelay(10);
#elif defined(CONFIG_FB_MSM_MIPI_TX13D107VM_NT35521_VIDEO_HD_PT)
		mdelay(50); //                                                 
		gpio_direction_output(58, 1);
		mdelay(1);
		gpio_direction_output(58, 0);
		mdelay(1);                
		gpio_direction_output(58, 1);
		mdelay(150); //                                                   
#else
		gpio_direction_output(58, 1);
		mdelay(2);
		gpio_direction_output(58, 0);
		mdelay(2);
		gpio_direction_output(58, 1);
		mdelay(7);
#endif
#endif
	} else {
		gpio_direction_output(58, 0);
#if defined (CONFIG_FB_MSM_MIPI_TX11D108VM_R69324A_VIDEO_QHD_PT)
		gpio_direction_output(53, 0);
#endif		

#if defined(CONFIG_FB_MSM_MIPI_TX13D107VM_NT35521_VIDEO_HD_PT)
		tps65132_lcd_bias_enable(0);
#endif

#if !defined(CONFIG_FB_MSM_MIPI_TX13D107VM_NT35521_VIDEO_HD_PT)
		rc = regulator_disable(reg_l8);
		if (rc) {
			pr_err("disable reg_l8 failed, rc=%d\n", rc);
			return -ENODEV;
		}
#endif
		rc = regulator_disable(reg_l23);
		if (rc) {
			pr_err("disable reg_l23 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = regulator_disable(reg_l2);
		if (rc) {
			pr_err("enable l2 failed, rc=%d\n", rc);
			return -ENODEV;
		}
#if !defined(CONFIG_FB_MSM_MIPI_TX13D107VM_NT35521_VIDEO_HD_PT)
		rc = regulator_set_optimum_mode(reg_l8, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l8 failed, rc=%d\n", rc);
			return -EINVAL;
		}
#endif
		rc = regulator_set_optimum_mode(reg_l23, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l23 failed, rc=%d\n", rc);
			return -EINVAL;
		}

		rc = regulator_set_optimum_mode(reg_l2, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}
	}
	printk("%s: state : %d - OK\n", __func__, on);
	return 0;
}
#else /* QCT Original */
/*
 * TODO: When physical 8930/PM8038 hardware becomes
 * available, replace mipi_dsi_cdp_panel_power with
 * appropriate function.
 */
#define DISP_RST_GPIO 58
#define DISP_3D_2D_MODE 1
static int mipi_dsi_cdp_panel_power(int on)
{
//                                                                                        
#if defined(CONFIG_FB_MSM_MIPI_TX13D107VM_NT35521_VIDEO_HD_PT)
	static struct regulator *reg_l23, *reg_l2;
#else
	static struct regulator *reg_l8, *reg_l23, *reg_l2;
#endif
//                                              
	/* Control backlight GPIO (24) directly when using PM8917 */
	int gpio24 = PM8917_GPIO_PM_TO_SYS(24);
	int rc;

	pr_debug("%s: state : %d\n", __func__, on);

	if (!dsi_power_on) {

//                                                                                                   
#if defined(CONFIG_FB_MSM_MIPI_TX13D107VM_NT35521_VIDEO_HD_PT)
#else
		reg_l8 = regulator_get(&msm_mipi_dsi1_device.dev,
				"dsi_vdc");
		if (IS_ERR(reg_l8)) {
			pr_err("could not get 8038_l8, rc = %ld\n",
				PTR_ERR(reg_l8));
			return -ENODEV;
		}
#endif
//                                              
		reg_l23 = regulator_get(&msm_mipi_dsi1_device.dev,
				"dsi_vddio");
		if (IS_ERR(reg_l23)) {
			pr_err("could not get 8038_l23, rc = %ld\n",
				PTR_ERR(reg_l23));
			return -ENODEV;
		}
		reg_l2 = regulator_get(&msm_mipi_dsi1_device.dev,
				"dsi_vdda");
		if (IS_ERR(reg_l2)) {
			pr_err("could not get 8038_l2, rc = %ld\n",
				PTR_ERR(reg_l2));
			return -ENODEV;
		}
//                                                                                                   
#if defined(CONFIG_FB_MSM_MIPI_TX13D107VM_NT35521_VIDEO_HD_PT)
#else
#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS) 
		rc = regulator_set_voltage(reg_l8, 3000000, 3000000);
#else
		rc = regulator_set_voltage(reg_l8, 2800000, 3000000);
#endif
		if (rc) {
			pr_err("set_voltage l8 failed, rc=%d\n", rc);
			return -EINVAL;
		}
#endif
//                                              
		rc = regulator_set_voltage(reg_l23, 1800000, 1800000);
		if (rc) {
			pr_err("set_voltage l23 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = regulator_set_voltage(reg_l2, 1200000, 1200000);
		if (rc) {
			pr_err("set_voltage l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = gpio_request(DISP_RST_GPIO, "disp_rst_n");
		if (rc) {
			pr_err("request gpio DISP_RST_GPIO failed, rc=%d\n",
				rc);
			gpio_free(DISP_RST_GPIO);
			return -ENODEV;
		}
		rc = gpio_request(DISP_3D_2D_MODE, "disp_3d_2d");
		if (rc) {
			pr_err("request gpio DISP_3D_2D_MODE failed, rc=%d\n",
				 rc);
			gpio_free(DISP_3D_2D_MODE);
			return -ENODEV;
		}
		rc = gpio_direction_output(DISP_3D_2D_MODE, 0);
		if (rc) {
			pr_err("gpio_direction_output failed for %d gpio rc=%d\n",
			DISP_3D_2D_MODE, rc);
			return -ENODEV;
		}
		if (socinfo_get_pmic_model() == PMIC_MODEL_PM8917) {
			rc = gpio_request(gpio24, "disp_bl");
			if (rc) {
				pr_err("request for gpio 24 failed, rc=%d\n",
					rc);
				return -ENODEV;
			}
		}
		dsi_power_on = true;
	}
	if (on) {
//                                                                                                   
#if defined(CONFIG_FB_MSM_MIPI_TX13D107VM_NT35521_VIDEO_HD_PT)
#else
		rc = regulator_set_optimum_mode(reg_l8, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l8 failed, rc=%d\n", rc);
			return -EINVAL;
		}
#endif
//                                              
		rc = regulator_set_optimum_mode(reg_l23, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l23 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = regulator_set_optimum_mode(reg_l2, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}
//                                                                                                   
#if defined(CONFIG_FB_MSM_MIPI_TX13D107VM_NT35521_VIDEO_HD_PT)
#else
		rc = regulator_enable(reg_l8);
		if (rc) {
			pr_err("enable l8 failed, rc=%d\n", rc);
			return -ENODEV;
		}
#endif
//                                              
		rc = regulator_enable(reg_l23);
		if (rc) {
			pr_err("enable l8 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = regulator_enable(reg_l2);
		if (rc) {
			pr_err("enable l2 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		usleep(10000);
		gpio_set_value(DISP_RST_GPIO, 1);
		usleep(10);
		gpio_set_value(DISP_RST_GPIO, 0);
		usleep(20);
		gpio_set_value(DISP_RST_GPIO, 1);
		gpio_set_value(DISP_3D_2D_MODE, 1);
		usleep(20);
		if (socinfo_get_pmic_model() == PMIC_MODEL_PM8917)
			gpio_set_value_cansleep(gpio24, 1);
	} else {

		gpio_set_value(DISP_RST_GPIO, 0);

		rc = regulator_disable(reg_l2);
		if (rc) {
			pr_err("disable reg_l2 failed, rc=%d\n", rc);
			return -ENODEV;
		}
//                                                                                                   
#if defined(CONFIG_FB_MSM_MIPI_TX13D107VM_NT35521_VIDEO_HD_PT)
#else
		rc = regulator_disable(reg_l8);
		if (rc) {
			pr_err("disable reg_l8 failed, rc=%d\n", rc);
			return -ENODEV;
		}
#endif
//                                              
		rc = regulator_disable(reg_l23);
		if (rc) {
			pr_err("disable reg_l23 failed, rc=%d\n", rc);
			return -ENODEV;
		}
//                                                                                                   
#if defined(CONFIG_FB_MSM_MIPI_TX13D107VM_NT35521_VIDEO_HD_PT)
#else
		rc = regulator_set_optimum_mode(reg_l8, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l8 failed, rc=%d\n", rc);
			return -EINVAL;
		}
#endif
//                                              
		rc = regulator_set_optimum_mode(reg_l23, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l23 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = regulator_set_optimum_mode(reg_l2, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		gpio_set_value(DISP_3D_2D_MODE, 0);
		usleep(20);
		if (socinfo_get_pmic_model() == PMIC_MODEL_PM8917)
			gpio_set_value_cansleep(gpio24, 0);
	}
	return 0;
}

static int mipi_dsi_panel_power(int on)
{
	pr_debug("%s: on=%d\n", __func__, on);

	return mipi_dsi_cdp_panel_power(on);
}
#endif

static struct mipi_dsi_platform_data mipi_dsi_pdata = {
	.vsync_gpio = MDP_VSYNC_GPIO,
	.dsi_power_save = mipi_dsi_panel_power,
};

#ifdef CONFIG_MSM_BUS_SCALING

static struct msm_bus_vectors mdp_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};

#ifdef CONFIG_FB_MSM_HDMI_AS_PRIMARY
static struct msm_bus_vectors hdmi_as_primary_vectors[] = {
	/* If HDMI is used as primary */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 2000000000,
		.ib = 2000000000,
	},
};
static struct msm_bus_paths mdp_bus_scale_usecases[] = {
	{
		ARRAY_SIZE(mdp_init_vectors),
		mdp_init_vectors,
	},
	{
		ARRAY_SIZE(hdmi_as_primary_vectors),
		hdmi_as_primary_vectors,
	},
	{
		ARRAY_SIZE(hdmi_as_primary_vectors),
		hdmi_as_primary_vectors,
	},
	{
		ARRAY_SIZE(hdmi_as_primary_vectors),
		hdmi_as_primary_vectors,
	},
	{
		ARRAY_SIZE(hdmi_as_primary_vectors),
		hdmi_as_primary_vectors,
	},
	{
		ARRAY_SIZE(hdmi_as_primary_vectors),
		hdmi_as_primary_vectors,
	},
};
#else
static struct msm_bus_vectors mdp_ui_vectors[] = {
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 216000000 * 2,
#if defined(CONFIG_MACH_LGE_L9II_COMMON)
		.ib = 334080000 * 2,
#else
		.ib = 270000000 * 2,
#endif
	},
};

static struct msm_bus_vectors mdp_vga_vectors[] = {
	/* VGA and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 216000000 * 2,
#if defined(CONFIG_MACH_LGE_L9II_COMMON)
		.ib = 334080000 * 2,
#else
		.ib = 270000000 * 2,
#endif
	},
};

static struct msm_bus_vectors mdp_720p_vectors[] = {
	/* 720p and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 230400000 * 2,
#if defined(CONFIG_MACH_LGE_L9II_COMMON)
		.ib = 535000000 * 2,
#else
		.ib = 288000000 * 2,
#endif
	},
};

static struct msm_bus_vectors mdp_1080p_vectors[] = {
	/* 1080p and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 334080000 * 2,
#if defined(CONFIG_MACH_LGE_L9II_COMMON)
		.ib = 535000000 * 2,
#else
		.ib = 417600000 * 2,
#endif
	},
};

static struct msm_bus_paths mdp_bus_scale_usecases[] = {
	{
		ARRAY_SIZE(mdp_init_vectors),
		mdp_init_vectors,
	},
	{
		ARRAY_SIZE(mdp_ui_vectors),
		mdp_ui_vectors,
	},
	{
		ARRAY_SIZE(mdp_ui_vectors),
		mdp_ui_vectors,
	},
	{
		ARRAY_SIZE(mdp_vga_vectors),
		mdp_vga_vectors,
	},
	{
		ARRAY_SIZE(mdp_720p_vectors),
		mdp_720p_vectors,
	},
	{
		ARRAY_SIZE(mdp_1080p_vectors),
		mdp_1080p_vectors,
	},
};
#endif

static struct msm_bus_scale_pdata mdp_bus_scale_pdata = {
	mdp_bus_scale_usecases,
	ARRAY_SIZE(mdp_bus_scale_usecases),
	.name = "mdp",
};

#endif


struct msm_fb_info_st {
	unsigned int width_mm;
	unsigned int height_mm;
};

static struct msm_fb_info_st msm_fb_info_data = {
	.width_mm = MSM_FB_WIDTH_MM,
	.height_mm = MSM_FB_HEIGHT_MM
};

static int msm_fb_event_notify(struct notifier_block *self,
		unsigned long action, void *data)
{
	struct fb_event *event = data;
	struct fb_info *info = event->info;
	struct msm_fb_info_st *fb_info_mm = &msm_fb_info_data;
	int ret = 0;

	switch (action) {
	case FB_EVENT_FB_REGISTERED:
		info->var.width = fb_info_mm->width_mm;
		info->var.height = fb_info_mm->height_mm;
		break;
	}
	return ret;
}

static struct notifier_block msm_fb_event_notifier = {
	.notifier_call = msm_fb_event_notify,
};

static struct msm_panel_common_pdata mdp_pdata = {
	.gpio = MDP_VSYNC_GPIO,
	.mdp_max_clk = 200000000,
//                                                         
#if defined(CONFIG_MACH_LGE_L9II_COMMON)
	.mdp_max_bw = 2000000000,
	.mdp_bw_ab_factor = 200,
	.mdp_bw_ib_factor = 225,
#endif
//                                    
#ifdef CONFIG_MSM_BUS_SCALING
	.mdp_bus_scale_table = &mdp_bus_scale_pdata,
#endif
	.mdp_rev = MDP_REV_43,
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	.mem_hid = BIT(ION_CP_MM_HEAP_ID),
#else
	.mem_hid = MEMTYPE_EBI1,
#endif

#if defined(CONFIG_FB_MSM_MIPI_DSI_HIMAX) && !defined(CONFIG_MACH_LGE_FX3_VZW) && !defined(CONFIG_MACH_LGE_FX3Q_TMUS)
	.cont_splash_enabled = 0x1,
#else
	.cont_splash_enabled = 0x0,
#endif
	.splash_screen_addr = 0x00,
	.splash_screen_size = 0x00,
	.mdp_iommu_split_domain = 0,
};

void __init msm8930_mdp_writeback(struct memtype_reserve* reserve_table)
{
	mdp_pdata.ov0_wb_size = MSM_FB_OVERLAY0_WRITEBACK_SIZE;
	mdp_pdata.ov1_wb_size = MSM_FB_OVERLAY1_WRITEBACK_SIZE;
#if defined(CONFIG_ANDROID_PMEM) && !defined(CONFIG_MSM_MULTIMEDIA_USE_ION)
	reserve_table[mdp_pdata.mem_hid].size +=
		mdp_pdata.ov0_wb_size;
	reserve_table[mdp_pdata.mem_hid].size +=
		mdp_pdata.ov1_wb_size;
#endif
}


#if !defined(CONFIG_MACH_MSM8930_FX3)
static int mipi_hitachi_backlight_level(int level, int max, int min)
{
	/* TODO */
	return 0;
}
#endif

#ifndef CONFIG_MACH_MSM8930_FX3
#define LPM_CHANNEL0 0
static int toshiba_gpio[] = {LPM_CHANNEL0};

static struct mipi_dsi_panel_platform_data toshiba_pdata = {
	.gpio = toshiba_gpio,
};

static struct platform_device mipi_dsi_toshiba_panel_device = {
	.name = "mipi_toshiba",
	.id = 0,
	.dev = {
		.platform_data = &toshiba_pdata,
	}
};
#endif

#if defined(CONFIG_FB_MSM_MIPI_TX11D108VM_R69324A_VIDEO_QHD_PT)
/* JAPAN DISPLAY 4.5" QHD Panel */
static char set_address_mode[2] = {0x36, 0xC8};
static char set_pixel_format[2] = {0x3A, 0x70};

static char exit_sleep[2] = {0x11, 0x00};
static char display_on[2] = {0x29, 0x00};
static char enter_sleep[2] = {0x10, 0x00};
static char display_off[2] = {0x28, 0x00};

static char macp_off[2] = {0xB0, 0x04};
static char charger_pump[2] = {0xd0, 0x22}; 
static char macp_on[2] = {0xB0, 0x03};

static char gamma_setting_a[23] = {
	0xC8, 0x00, 0x03, 0x03,
	0x09, 0x88, 0x07, 0x0A,
	0x02, 0x02, 0x04, 0x04,
	0x00, 0x03, 0x03, 0x09,
	0x88, 0x07, 0x0A, 0x02,
	0x02, 0x04, 0x04
};
static char gamma_setting_b[23] = {
	0xC9, 0x00, 0x03, 0x03,
	0x09, 0x88, 0x07, 0x0A,
	0x02, 0x02, 0x04, 0x04,
	0x00, 0x03, 0x03, 0x09,
	0x88, 0x07, 0x0A, 0x02,
	0x02, 0x04, 0x04
};
static char gamma_setting_c[23] = {
	0xCA, 0x00, 0x03, 0x03,
	0x09, 0x88, 0x07, 0x0A,
	0x02, 0x02, 0x04, 0x04,
	0x00, 0x03, 0x03, 0x09,
	0x88, 0x07, 0x0A, 0x02,
	0x02, 0x04, 0x04
};
#endif

//                                                       
#if defined (CONFIG_BACKLIGHT_LM3639)
extern void lm3639_lcd_backlight_set_level(int level);
#endif
//                                                       

//                                                 
#if defined(CONFIG_FB_MSM_MIPI_TX13D107VM_NT35521_VIDEO_HD_PT) 
//                                                       
#if defined (CONFIG_BACKLIGHT_LM3639)
static int mipi_tx13d107vm_backlight_level(int level, int max, int min)
{
	lm3639_lcd_backlight_set_level(level);
	return 0;
}
#endif
//                                                       


//State[1] h->z Power On   START
static char power_on_01_00[5] = {
		0xFF, 0xAA, 0x55, 0xA5,
		0x80,
};
static char power_on_01_01[3] = {
		0x6F, 0x11, 0x00,
};
static char power_on_01_02[3] = {
		0xF7, 0x20, 0x00,
};
//State[1] h->z Power On   END

//State[3] z->a Exit Sleep mode START
static char exit_sleep[2] = {0x11, 0x00};
//State[3] z->a Exit Sleep mode END
//State[4] a->z Enter Sleep mode START
static char enter_sleep[2] = {0x10, 0x00};
//State[4] a->z Enter Sleep mode END


static char maucctr_p2_en[6] = {
        0xF0, 0x55, 0xAA, 0x52,
        0x08, 0x02
};
static char gmprctr1[17] = {
        0xB0,0x00,0x00,0x00,
        0x0A,0x00,0x1D,0x00,
        0x2E,0x00,0x3E,0x00,
        0x58,0x00,0x70,0x00,
        0x99
};
static char gmprctr2[17] = {
        0xB1,0x00,0xBC,0x00,
        0xF8,0x01,0x28,0x01,
        0x82,0x01,0xCB,0x01,
        0xCD,0x02,0x10,0x02,
        0x58
};
static char gmprctr3[17] = {
        0xB2,0x02,0x87,0x02,
        0xC6,0x02,0xEC,0x03,
        0x2A,0x03,0x52,0x03,
        0x7A,0x03,0x8E,0x03,
        0xA2
};
static char gmprctr4[5] = {
        0xB3,0x03,0xB6,0x03,
        0xCC
};
static char gmpgctr1[17] = {
        0xB4,0x00,0x00,0x00,
        0x0A,0x00,0x1D,0x00,
        0x2E,0x00,0x3E,0x00,
        0x58,0x00,0x70,0x00,
        0x99
};
static char gmpgctr2[17] = {
        0xB5,0x00,0xBC,0x00,
        0xF8,0x01,0x28,0x01,
        0x82,0x01,0xCB,0x01,
        0xCD,0x02,0x10,0x02,
        0x58
};
static char gmpgctr3[17] = {
        0xB6,0x02,0x87,0x02,
        0xC6,0x02,0xEC,0x03,
        0x2A,0x03,0x52,0x03,
        0x7A,0x03,0x8E,0x03,
        0xA2
};
static char gmpgctr4[5] = {
        0xB7,0x03,0xB6,0x03,
        0xCC
};
static char gmpbctr1[17] = {
        0xB8,0x00,0x00,0x00,
        0x0A,0x00,0x1D,0x00,
        0x2E,0x00,0x3E,0x00,
        0x58,0x00,0x70,0x00,
        0x99
};
static char gmpbctr2[17] = {
        0xB9,0x00,0xBC,0x00,
        0xF8,0x01,0x28,0x01,
        0x82,0x01,0xCB,0x01,
        0xCD,0x02,0x10,0x02,
        0x58
};
static char gmpbctr3[17] = {
        0xBA,0x02,0x87,0x02,
        0xC6,0x02,0xEC,0x03,
        0x2A,0x03,0x52,0x03,
        0x7A,0x03,0x8E,0x03,
        0xA2
};
static char gmpbctr4[5] = {
        0xBB,0x03,0xB6,0x03,
        0xCC
};
static char gmnrctr1[17] = {
        0xBC,0x00,0x00,0x00,
        0x0A,0x00,0x1D,0x00,
        0x2E,0x00,0x3E,0x00,
        0x58,0x00,0x70,0x00,
        0x99
};
static char gmnrctr2[17] = {
        0xBD,0x00,0xBC,0x00,
        0xF8,0x01,0x28,0x01,
        0x82,0x01,0xCB,0x01,
        0xCD,0x02,0x10,0x02,
        0x58
};
static char gmnrctr3[17] = {
        0xBE,0x02,0x87,0x02,
        0xC6,0x02,0xEC,0x03,
        0x2A,0x03,0x52,0x03,
        0x7A,0x03,0x8E,0x03,
        0xA2
};
static char gmnrctr4[5] = {
        0xBF,0x03,0xB7,0x03,
        0xCC
};
static char gmngctr1[17] = {
        0xC0,0x00,0x00,0x00,
        0x0A,0x00,0x1D,0x00,
        0x2E,0x00,0x3E,0x00,
        0x58,0x00,0x70,0x00,
        0x99
};
static char gmngctr2[17] = {
        0xC1,0x00,0xBC,0x00,
        0xF8,0x01,0x28,0x01,
        0x82,0x01,0xCB,0x01,
        0xCD,0x02,0x10,0x02,
        0x58
};
static char gmngctr3[17] = {
        0xC2,0x02,0x87,0x02,
        0xC6,0x02,0xEC,0x03,
        0x2A,0x03,0x52,0x03,
        0x7A,0x03,0x8E,0x03,
        0xA2
};
static char gmngctr4[5] = {
        0xC3,0x03,0xB6,0x03,
        0xCC
};
static char gmnbctr1[17] = {
        0xC4,0x00,0x00,0x00,
        0x0A,0x00,0x1D,0x00,
        0x2E,0x00,0x3E,0x00,
        0x58,0x00,0x70,0x00,
        0x99
};
static char gmnbctr2[17] = {
        0xC5,0x00,0xBC,0x00,
        0xF8,0x01,0x28,0x01,
        0x82,0x01,0xCB,0x01,
        0xCD,0x02,0x10,0x02,
        0x58
};
static char gmnbctr3[17] = {
        0xC6,0x02,0x87,0x02,
        0xC6,0x02,0xEC,0x03,
        0x2A,0x03,0x52,0x03,
        0x7A,0x03,0x8E,0x03,
        0xA2
};
static char gmnbctr4[5] = {
        0xC7,0x03,0xB6,0x03,
        0xCC
};
static char gmargesel[2] = {
        0xEE,0x00
};
static char maucctr_p2_dis[6] = {
        0xF0,0x55,0xAA,0x52,
        0x00,0x00
};
//                                          

#endif
//                                                 


#if defined(CONFIG_FB_MSM_MIPI_HITACHI_R69324A_VIDEO_WVGA_PT)
/* HITACHI 4.0" WVGA panel */
static char set_address_mode[2] = {0x36, 0x00};
static char set_pixel_format[2] = {0x3A, 0x70};

static char exit_sleep[2] = {0x11, 0x00};
static char display_on[2] = {0x29, 0x00};
static char enter_sleep[2] = {0x10, 0x00};
static char display_off[2] = {0x28, 0x00};

static char macp_off[2] = {0xB0, 0x04};
static char macp_on[2] = {0xB0, 0x03};

#if defined (HITACHI_R69324A_GAMMA)
static char gamma_setting_a[23] = {
	0xC8, 0x00, 0x05, 0x03,
	0x08, 0xB9, 0x06, 0x0A,
	0x00, 0x00, 0x06, 0x04,
	0x00, 0x05, 0x03, 0x08,
	0xB9, 0x06, 0x0A, 0x00,
	0x00, 0x06, 0x04
};
static char gamma_setting_b[23] = {
	0xC9, 0x00, 0x05, 0x03,
	0x08, 0xB9, 0x06, 0x0A,
	0x00, 0x00, 0x06, 0x04,
	0x00, 0x05, 0x03, 0x08,
	0xB9, 0x06, 0x0A, 0x00,
	0x00, 0x06, 0x04
};
static char gamma_setting_c[23] = {
	0xCA, 0x00, 0x05, 0x03,
	0x08, 0xB9, 0x06, 0x0A,
	0x00, 0x00, 0x06, 0x04,
	0x00, 0x05, 0x03, 0x08,
	0xB9, 0x06, 0x0A, 0x00,
	0x00, 0x06, 0x04
};
#endif
#endif

//                                                 
#if defined(CONFIG_FB_MSM_MIPI_HITACHI_R69328A_VIDEO_HD_PT) 
/* HITACHI HD panel */
static char set_address_mode[2] = {0x36, 0x00};
static char set_pixel_format[2] = {0x3A, 0x70};

static char exit_sleep[2] = {0x11, 0x00};
static char display_on[2] = {0x29, 0x00};
static char enter_sleep[2] = {0x10, 0x00};
static char display_off[2] = {0x28, 0x00};

static char macp_off[2] = {0xB0, 0x04};
static char macp_on[2] = {0xB0, 0x03};

#if defined (HITACHI_R69328_GAMMA)
//                                                                                      
static char gamma_setting_a[29] = {
	0xC8, 0x00, 0x1A, 0x20,
	0x28, 0x25, 0x24, 0x26,
	0x15, 0x13, 0x11, 0x18,
	0x1E, 0x1C, 0x00, 0x00,
	0x1A, 0x20, 0x28, 0x25,
	0x24, 0x26, 0x15, 0x13,
	0x11, 0x18, 0x1E, 0x1C, 
	0x00
};
static char gamma_setting_b[29] = {
	0xC9, 0x00, 0x1A, 0x20,
	0x28, 0x25, 0x24, 0x26,
	0x15, 0x13, 0x11, 0x18,
	0x1E, 0x1C, 0x00, 0x00,
	0x1A, 0x20, 0x28, 0x25,
	0x24, 0x26, 0x15, 0x13,
	0x11, 0x18, 0x1E, 0x1C, 
	0x00
};
static char gamma_setting_c[29] = {
	0xCA, 0x00, 0x1A, 0x20,
	0x28, 0x25, 0x24, 0x26,
	0x15, 0x13, 0x11, 0x18,
	0x1E, 0x1C, 0x00, 0x00,
	0x1A, 0x20, 0x28, 0x25,
	0x24, 0x26, 0x15, 0x13,
	0x11, 0x18, 0x1E, 0x1C, 
	0x00
};
//                                                                                      
#endif
#endif //CONFIG_FB_MSM_MIPI_HITACHI_R69328A_VIDEO_HD_PT
//                                                 

#if defined(CONFIG_FB_MSM_MIPI_HIMAX_H8379A_VIDEO_WVGA_PT)
static char set_extc[4] = {0xB9, 0xFF, 0x83, 0x79};
#if defined(CONFIG_MACH_LGE_FX3_TMUS)
static char set_power[32] = {
	0xB1, 0x00, 0x50, 0x34, 0xE8, 0x4F, 0x08, 0x11,
	0x11, 0x11, 0x2F, 0x37, 0xBF, 0x3F, 0x32, 0x05,
	0x6A, 0xF1, 0x00, 0xE6, 0xE6, 0xE6, 0xE6, 0xE6,
	0x00, 0x04, 0x05, 0x0A, 0x0B, 0x04, 0x05, 0x6F
};
#else
static char set_power[32] = {
	0xB1, 0x00, 0x50, 0x34, 0xE8, 0x4F, 0x08, 0x11,
	0x11, 0x11, 0x2F, 0x37, 0xBF, 0x3F, 0x42, 0x05,
	0x6A, 0xF1, 0x00, 0xE6, 0xE6, 0xE6, 0xE6, 0xE6,
	0x00, 0x04, 0x05, 0x0A, 0x0B, 0x04, 0x05, 0x6F
};
#endif
static char set_disp[14] = {
	0xB2, 0x00, 0x00, 0x3C, 0x0B, 0x11, 0x19, 0xB2,
	0x00, 0xFF, 0x0B, 0x11, 0x19, 0x20
};
static char set_cyc[32] = {
	0xB4, 0x00, 0x08, 0x00, 0x32, 0x10, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x37, 0x0A, 0x40,
	0x04, 0x37, 0x0A, 0x30, 0x14, 0x3C, 0x51, 0x0A,
	0x0A, 0x40, 0x0A, 0x30, 0x14, 0x40, 0x55, 0x0A
};
static char set_tvcom[5] = {0xB6, 0x00, 0xA2, 0x00, 0xA2};
static char set_te[4] = {0xB7, 0x10, 0x50, 0x00};
static char set_panel[2] = { 0xCC, 0x0B};
static char set_gipsignal[48] = {
	0xD5, 0x00, 0x03, 0x0A, 0x00, 0x01, 0x00, 0x00,
	0x03, 0x00, 0x88, 0x88, 0x88, 0x88, 0x67, 0x45,
	0x23, 0x01, 0x01, 0x23, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x67, 0x45,
	0x23, 0x01, 0x01, 0x23, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x01, 0x01, 0x00, 0x00, 0x03, 0x00
};
static char set_ptba[4] = {0xDE, 0x05, 0x70, 0x04};
static char set_gamma[36]={
	0xE0, 0x79, 0x01, 0x0C, 0x0E, 0x29, 0x2A, 0x3B,
	0x1A, 0x42, 0x06, 0x0D, 0x0E, 0x11, 0x13, 0x11, 
	0x12, 0x11, 0x18, 0x00, 0x04, 0x06, 0x27, 0x28, 
	0x3B, 0x12, 0x3F, 0x06, 0x0D, 0x0E, 0x11, 0x13, 
	0x11, 0x12, 0x11, 0x18
};

// Tovis LCD - [Start]
static char tovis_set_extc[4] = {0xB9, 0xFF, 0x83, 0x79};
static char tovis_set_mipi[2] = {0xBA, 0x51};
static char tovis_set_power[20] = {
	0xB1, 0x00, 0x50, 0x44, 0xE3, 0x6F, 0x08, 0x11, 
	0x11, 0x11, 0x36, 0x38, 0xA9, 0x29, 0x42, 0x0B, 
	0x79, 0xF1, 0x00, 0xE6
};
static char tovis_set_disp[14] = {
	0xB2, 0x00, 0x00, 0x3C, 0x09, 0x04, 0x19, 0x22, 
	0x00, 0xFF, 0x09, 0x04, 0x19, 0x20
};
static char tovis_set_cyc[32] = {
#if defined(CONFIG_MACH_LGE_F6_TMUS) || defined(CONFIG_MACH_LGE_F6_VDF)
	0xB4, 0x00, 0x0C, 0x00, 0x30, 0x10, 0x06, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x48, 
	0x07, 0x23, 0x3C, 0x40, 0x08, 0x30, 0x30, 0x04,
	0x00, 0x40, 0x08, 0x28, 0x08, 0x30, 0x30, 0x04
#else
	0xB4, 0x00, 0x12, 0x00, 0x30, 0x10, 0x06, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x48,
	0x07, 0x23, 0x3C, 0x3E, 0x08, 0x30, 0x30, 0x04,
	0x00, 0x40, 0x08, 0x28, 0x08, 0x30, 0x30, 0x04
#endif
};
static char tovis_set_panel[2] = {0xCC, 0x0C};
/*                                                            */
#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS) 
static char tovis_set_address_mode[2] = {0x36, 0x02}; // Flip only Horizontal
#endif
static char tovis_set_gipsignal[48] = {
	0xD5, 0x00, 0x00, 0x0A, 0x00, 0x01, 0x00, 0x00,
	0x00, 0x10, 0x88, 0x99, 0x10, 0x32, 0x10, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x88, 0x88, 0x23, 0x01, 0x01, 0x99,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
	0x88, 0x88, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00
};
static char tovis_set_gamma[36]={
	0xE0, 0x79, 0x00, 0x08, 0x11, 0x35, 0x3C, 0x3F,
	0x1F, 0x4E, 0x02, 0x09, 0x0E, 0x13, 0x15, 0x14,
	0x15, 0x15, 0x17, 0x00, 0x08, 0x11, 0x35, 0x3C,
	0x3F, 0x1F, 0x4E, 0x02, 0x09, 0x0E, 0x13, 0x16,
	0x14, 0x15, 0x15, 0x17
};
static char tovis_set_tvcom[5] = {
	0xB6, 0x00, 0x94, 0x00, 0x94
};
// Tovis LCD - [End]

static char sleep_out[2] = {0x11, 0x00};
static char display_on[2] = {0x29, 0x00};
static char enter_sleep[2] = {0x10, 0x00};
static char display_off[2] = {0x28, 0x00};
#endif //CONFIG_FB_MSM_MIPI_HIMAX_H8379A_VIDEO_WVGA_PT


#if defined(CONFIG_FB_MSM_MIPI_DSI_HITACHI)
static struct dsi_cmd_desc hitachi_power_on_set[] = {
	/* Display initial set */
	{DTYPE_DCS_WRITE1, 1, 0, 0, 20, sizeof(set_address_mode),
		set_address_mode},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(set_pixel_format),
		set_pixel_format},

	/* Sleep mode exit */
	{DTYPE_DCS_WRITE, 1, 0, 0, 80, sizeof(exit_sleep), exit_sleep},

	/* Manufacturer command protect off */
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(macp_off), macp_off},
#if defined (HITACHI_R69324A_GAMMA)
	/* Gamma setting */
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(gamma_setting_a),
		gamma_setting_a},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(gamma_setting_b),
		gamma_setting_b},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(gamma_setting_c),
		gamma_setting_c},
#endif

//                                                 
#if defined (HITACHI_R69328_GAMMA) //koh.euije
	/* Gamma setting */
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(gamma_setting_a),
		gamma_setting_a},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(gamma_setting_b),
		gamma_setting_b},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(gamma_setting_c),
		gamma_setting_c},
#endif
//                                                 

	/* Manufacturer command protect on */
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(macp_on), macp_on},

	/* Display on */
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(display_on), display_on},
};

static struct dsi_cmd_desc hitachi_power_off_set[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(enter_sleep), enter_sleep}
}; 
#endif

#if defined(CONFIG_FB_MSM_MIPI_DSI_HIMAX)
static struct dsi_cmd_desc himax_power_on_set[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(set_extc),
		set_extc},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(set_power),
		set_power},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(set_disp),
		set_disp},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(set_cyc),
		set_cyc},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(set_tvcom),
		set_tvcom},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(set_te),
		set_te},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(set_panel),
		set_panel},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(set_gipsignal),
		set_gipsignal},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(set_ptba),
		set_ptba},		
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(set_gamma),
		set_gamma},
	{DTYPE_DCS_WRITE, 1, 0, 0, 150, sizeof(sleep_out), sleep_out},
	{DTYPE_DCS_WRITE, 1, 0, 0, 20, sizeof(display_on), display_on},
};

static struct dsi_cmd_desc himax_power_off_set[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 1, 0, 0, 10, sizeof(enter_sleep), enter_sleep}
};

// Tovis LCD - [Start]
static struct dsi_cmd_desc tovis_power_on_set[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(tovis_set_extc),
		tovis_set_extc},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(tovis_set_mipi),
		tovis_set_mipi},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(tovis_set_power),
		tovis_set_power},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(tovis_set_disp),
		tovis_set_disp},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(tovis_set_cyc),
		tovis_set_cyc},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(tovis_set_panel),
		tovis_set_panel},
/*                                                            */
#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS) 
	{DTYPE_DCS_LWRITE, 1, 0, 0, 10, sizeof(tovis_set_address_mode),
		tovis_set_address_mode},
#endif
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(tovis_set_gipsignal),
		tovis_set_gipsignal},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(tovis_set_gamma),
		tovis_set_gamma},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(tovis_set_tvcom),
		tovis_set_tvcom},
	{DTYPE_DCS_WRITE, 1, 0, 0, 150, sizeof(sleep_out), sleep_out},
	{DTYPE_DCS_WRITE, 1, 0, 0, 10, sizeof(display_on), display_on},
};

static struct dsi_cmd_desc tovis_power_off_set[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 50, sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(enter_sleep), enter_sleep}
};
// Tovis LCD - [End]
#endif //CONFIG_FB_MSM_MIPI_DSI_HIMAX

#if defined(CONFIG_FB_MSM_MIPI_DSI_TX11D108VM)
static struct dsi_cmd_desc tx11d108vm_power_on_set[] = {
	/* Display initial set */
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(set_address_mode),
		set_address_mode},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(set_pixel_format),
		set_pixel_format},

	/* Sleep mode exit */
	{DTYPE_DCS_WRITE, 1, 0, 0, 120, sizeof(exit_sleep), exit_sleep},

	/* Manufacturer command protect off */
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(macp_off), macp_off},

	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(charger_pump), charger_pump},
	
	/* Gamma setting */
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(gamma_setting_a),
		gamma_setting_a},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(gamma_setting_b),
		gamma_setting_b},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(gamma_setting_c),
		gamma_setting_c},

	/* Manufacturer command protect on */
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(macp_on), macp_on},

	/* Display on */
	{DTYPE_DCS_WRITE, 1, 0, 0, 20, sizeof(display_on), display_on},
};

static struct dsi_cmd_desc tx11d108vm_power_off_set[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 1, 0, 0, 20, sizeof(enter_sleep), enter_sleep}
};
#endif

#if defined(CONFIG_FB_MSM_MIPI_TX13D107VM_NT35521_VIDEO_HD_PT)
//                                                 
static struct dsi_cmd_desc tx13d107vm_power_on_set[] = {
//State[1] h->z Power On   START
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(power_on_01_00), power_on_01_00},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(power_on_01_01), power_on_01_01},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(power_on_01_02), power_on_01_02},
//State[1] h->z Power On   END


// gamma setting start
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(maucctr_p2_en), maucctr_p2_en},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(gmprctr1), gmprctr1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(gmprctr2), gmprctr2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(gmprctr3), gmprctr3},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(gmprctr4), gmprctr4},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(gmpgctr1), gmpgctr1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(gmpgctr2), gmpgctr2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(gmpgctr3), gmpgctr3},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(gmpgctr4), gmpgctr4},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(gmpbctr1), gmpbctr1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(gmpbctr2), gmpbctr2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(gmpbctr3), gmpbctr3},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(gmpbctr4), gmpbctr4},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(gmnrctr1), gmnrctr1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(gmnrctr2), gmnrctr2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(gmnrctr3), gmnrctr3},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(gmnrctr4), gmnrctr4},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(gmngctr1), gmngctr1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(gmngctr2), gmngctr2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(gmngctr3), gmngctr3},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(gmngctr4), gmngctr4},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(gmnbctr1), gmnbctr1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(gmnbctr2), gmnbctr2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(gmnbctr3), gmnbctr3},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(gmnbctr4), gmnbctr4},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(gmargesel), gmargesel},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(maucctr_p2_dis), maucctr_p2_dis},
// gamma setting end


//State[3] z->a Exit Sleep mode START
        {DTYPE_DCS_WRITE, 1, 0, 0, 150, sizeof(exit_sleep), exit_sleep},
//State[3] z->a Exit Sleep mode END

};

static struct dsi_cmd_desc tx13d107vm_power_off_set[] = {
//        {DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(display_off), display_off},
#if defined(CONFIG_MACH_LGE_F6_TMUS) || defined(CONFIG_MACH_LGE_F6_VDF)
		{DTYPE_DCS_WRITE, 1, 0, 0, 120, sizeof(enter_sleep), enter_sleep}
#else
        {DTYPE_DCS_WRITE, 1, 0, 0, 150, sizeof(enter_sleep), enter_sleep}
#endif
}; 
#endif
//                                                 

#define FPGA_3D_GPIO_CONFIG_ADDR	0xB5
#ifdef CONFIG_LGE_LCD_TUNING
struct tuning_buff{
	char buf[TUNING_REGNUM][TUNING_REGSIZE];
	int size;
	int idx;
};

char set_buff[TUNING_REGNUM][TUNING_REGSIZE];

static int tuning_read_regset(unsigned long tmp)
{
	struct tuning_buff *rbuf = (struct tuning_buff *)tmp;
	int i;
	int size = 0; //HMOON TEMP

#if defined(CONFIG_FB_MSM_MIPI_TX11D108VM_R69324A_VIDEO_QHD_PT)
	size = ARRAY_SIZE(tx11d108vm_power_on_set);
#elif defined(CONFIG_FB_MSM_MIPI_HIMAX_H8379A_VIDEO_WVGA_PT)
	if(lcd_maker_id) {
		size = ARRAY_SIZE(tovis_power_on_set);
	}else{
		size = ARRAY_SIZE(himax_power_on_set);
	}
#endif

	printk(KERN_INFO "read_file\n");

	for (i = 0; i < size; i++) {
#if defined(CONFIG_FB_MSM_MIPI_TX11D108VM_R69324A_VIDEO_QHD_PT)
		if (copy_to_user(rbuf->buf[i], tx11d108vm_power_on_set[i].payload,
					tx11d108vm_power_on_set[i].dlen)) {
			printk(KERN_ERR "read_file : error of copy_to_user_buff\n");
			return -EFAULT;
		}
#elif defined(CONFIG_FB_MSM_MIPI_HIMAX_H8379A_VIDEO_WVGA_PT)
		if(lcd_maker_id){
			if (copy_to_user(rbuf->buf[i], tovis_power_on_set[i].payload,
						tovis_power_on_set[i].dlen)) {
				printk(KERN_ERR "read_file : error of copy_to_user_buff\n");
				return -EFAULT;
			}
		}else{
			if (copy_to_user(rbuf->buf[i], himax_power_on_set[i].payload,
						himax_power_on_set[i].dlen)) {
				printk(KERN_ERR "read_file : error of copy_to_user_buff\n");
				return -EFAULT;
			}
		}
#endif 
	}

	if (put_user(size, &(rbuf->size))) {
		printk(KERN_ERR "read_file : error of copy_to_user_buffsize\n");
		return -EFAULT;
	}

	return 0;
}

static int tuning_write_regset(unsigned long tmp)
{
	char *buff;
	struct tuning_buff *wbuf = (struct tuning_buff *)tmp;
	int i = wbuf->idx;

	printk(KERN_INFO "write file\n");

	buff = kmalloc(TUNING_BUFSIZE, GFP_KERNEL);
	if (copy_from_user(buff, wbuf->buf, wbuf->size)) {
		printk(KERN_ERR "write_file : error of copy_from_user\n");
		return -EFAULT;
	}

	memset(set_buff[i], 0x00, TUNING_REGSIZE);
	memcpy(set_buff[i], buff, wbuf->size);
#if defined(CONFIG_FB_MSM_MIPI_TX11D108VM_R69324A_VIDEO_QHD_PT)
	tx11d108vm_power_on_set[i].payload = set_buff[i];
	tx11d108vm_power_on_set[i].dlen = wbuf->size;
#elif defined(CONFIG_FB_MSM_MIPI_HIMAX_H8379A_VIDEO_WVGA_PT)
	if(lcd_maker_id){
		tovis_power_on_set[i].payload = set_buff[i];
		tovis_power_on_set[i].dlen = wbuf->size;
	} else {
		himax_power_on_set[i].payload = set_buff[i];
		himax_power_on_set[i].dlen = wbuf->size;
	}
#endif
	kfree(buff);
	return 0;
}
static int tuning_read_porch(unsigned long tmp)
{
        int size = ARRAY_SIZE(porch_value)*4;

        printk(KERN_INFO "read_porch_value\n");

        if (copy_to_user((uint32_t *)tmp, porch_value,
                                size)) {
                printk(KERN_ERR "read_file : error of copy_to_user_buff\n");
                return -EFAULT;
        }

        return 0;
}

static int tuning_write_porch(unsigned long tmp)
{
        u32 *buf;
        int size = ARRAY_SIZE(porch_value)*4;

        printk(KERN_INFO "write porch file\n");

        buf = kmalloc(size, GFP_KERNEL);
        if (copy_from_user(buf, (u32 *)tmp, size)) {
                printk(KERN_ERR "write_file : error of copy_from_user\n");
                return -EFAULT;
        }

        memcpy(porch_value, buf, size);
        kfree(buf);
        return 0;
}

static struct msm_panel_common_pdata lge_lcd_tunning_pdata = {
	.read_regset = tuning_read_regset,
	.write_regset = tuning_write_regset,
	.read_porch = tuning_read_porch,
	.write_porch = tuning_write_porch,
};

static struct platform_device lcd_misc_device = {
	.name   = "lcd_misc_msm",
	.id = 0,
	.dev = {
		.platform_data = &lge_lcd_tunning_pdata,
	}
};
#endif

#ifndef CONFIG_MACH_MSM8930_FX3
static struct mipi_dsi_phy_ctrl dsi_novatek_cmd_mode_phy_db = {

/* DSI_BIT_CLK at 500MHz, 2 lane, RGB888 */
	{0x09, 0x08, 0x05, 0x00, 0x20},	/* regulator */
	/* timing   */
	{0xab, 0x8a, 0x18, 0x00, 0x92, 0x97, 0x1b, 0x8c,
	0x0c, 0x03, 0x04, 0xa0},
	{0x5f, 0x00, 0x00, 0x10},	/* phy ctrl */
	{0xff, 0x00, 0x06, 0x00},	/* strength */
	/* pll control */
	{0x0, 0xe, 0x30, 0xda, 0x00, 0x10, 0x0f, 0x61,
	0x40, 0x07, 0x03,
	0x00, 0x1a, 0x00, 0x00, 0x02, 0x00, 0x20, 0x00, 0x02},
};
#endif

#ifdef CONFIG_FB_MSM_MIPI_DSI_TX11D108VM
static struct msm_panel_common_pdata mipi_tx11d108vm_pdata = {

	.enable_wled_bl_ctrl = 0x1,

#if defined(CONFIG_FB_MSM_MIPI_TX11D108VM_R69324A_VIDEO_QHD_PT)
	.power_on_set = tx11d108vm_power_on_set,
	.power_off_set = tx11d108vm_power_off_set,
	.power_on_set_size = ARRAY_SIZE(tx11d108vm_power_on_set),
	.power_off_set_size = ARRAY_SIZE(tx11d108vm_power_off_set),
#endif

#if !defined(CONFIG_MACH_MSM8930_FX3)
	.max_backlight_level = 0x71, /* TODO */
#endif
};

static struct platform_device mipi_dsi_tx11d108vm_panel_device = {
	.name = "mipi_tx11d108vm",
	.id = 0,
	.dev = {
		.platform_data = &mipi_tx11d108vm_pdata,
	}
};
#endif

//                                                 
#ifdef CONFIG_FB_MSM_MIPI_DSI_TX13D107VM
static struct msm_panel_common_pdata mipi_tx13d107vm_pdata = {

//                                                       
#if !defined(CONFIG_MACH_MSM8930_FX3) || defined(CONFIG_BACKLIGHT_LM3639)
	.backlight_level = mipi_tx13d107vm_backlight_level,
#else
	.enable_wled_bl_ctrl = 0x1,
#endif
//                                                       

#if defined (CONFIG_FB_MSM_MIPI_TX13D107VM_NT35521_VIDEO_HD_PT)
	.power_on_set = tx13d107vm_power_on_set,
	.power_off_set = tx13d107vm_power_off_set,
	.power_on_set_size = ARRAY_SIZE(tx13d107vm_power_on_set),
	.power_off_set_size = ARRAY_SIZE(tx13d107vm_power_off_set),
#endif

//                                                       
#if !defined(CONFIG_MACH_MSM8930_FX3) || defined(CONFIG_BACKLIGHT_LM3639)
//                                                                  
	.max_backlight_level = 0xFF, //0x7F -> 0xFF
//                                                                   
#endif
//                                                       
};

static struct platform_device mipi_dsi_tx13d107vm_panel_device = {
	.name = "mipi_tx13d107vm",
	.id = 0,
	.dev = {
		.platform_data = &mipi_tx13d107vm_pdata,
	}
};
#endif
//                                                 

#ifdef CONFIG_FB_MSM_MIPI_DSI_HITACHI
static struct msm_panel_common_pdata mipi_hitachi_pdata = {
#if !defined(CONFIG_MACH_MSM8930_FX3)
	.backlight_level = mipi_hitachi_backlight_level,
#else
	.enable_wled_bl_ctrl = 0x1,
#endif
//                                                 
/*
#if (defined (CONFIG_FB_MSM_MIPI_HITACHI_VIDEO_HD_PT) \
	|| defined (CONFIG_FB_MSM_MIPI_HITACHI_VIDEO_WVGA_PT) \
	|| defined (CONFIG_FB_MSM_MIPI_HITACHI_R69324A_VIDEO_WVGA_PT))
*/
#if (defined (CONFIG_FB_MSM_MIPI_HITACHI_VIDEO_HD_PT) \
	|| defined (CONFIG_FB_MSM_MIPI_HITACHI_VIDEO_WVGA_PT) \
	|| defined (CONFIG_FB_MSM_MIPI_HITACHI_R69324A_VIDEO_WVGA_PT) \
	|| defined (CONFIG_FB_MSM_MIPI_HITACHI_R69328A_VIDEO_HD_PT))
//                                                  
	.power_on_set = hitachi_power_on_set,
	.power_off_set = hitachi_power_off_set,
	.power_on_set_size = ARRAY_SIZE(hitachi_power_on_set),
	.power_off_set_size = ARRAY_SIZE(hitachi_power_off_set),
#endif
#if !defined(CONFIG_MACH_MSM8930_FX3)
	.max_backlight_level = 0x71, /* TODO */
#endif
};

static struct platform_device mipi_dsi_hitachi_panel_device = {
	.name = "mipi_hitachi",
	.id = 0,
	.dev = {
		.platform_data = &mipi_hitachi_pdata,
	}
};
#endif

#ifdef CONFIG_FB_MSM_MIPI_DSI_HIMAX
static struct msm_panel_common_pdata mipi_himax_pdata = {
#if !defined(CONFIG_MACH_MSM8930_FX3)
	.backlight_level = mipi_himax_backlight_level,
#else
	.enable_wled_bl_ctrl = 0x1,
#endif
	.power_on_set = himax_power_on_set,
	.power_off_set = himax_power_off_set,
	.power_on_set_size = ARRAY_SIZE(himax_power_on_set),
	.power_off_set_size = ARRAY_SIZE(himax_power_off_set),
#if !defined(CONFIG_MACH_MSM8930_FX3)
	.max_backlight_level = 0x71, /* TODO */
#endif
};

static struct platform_device mipi_dsi_himax_panel_device = {
	.name = "mipi_himax",
	.id = 0,
	.dev = {
		.platform_data = &mipi_himax_pdata,
	}
};

static int get_lcd_initial_table ( int lcd_id )
{
	if ( lcd_id ) // Tovis LCD
	{
	   	mipi_himax_pdata.power_on_set = tovis_power_on_set;
	   	mipi_himax_pdata.power_off_set = tovis_power_off_set;
	   	mipi_himax_pdata.power_on_set_size = ARRAY_SIZE(tovis_power_on_set);
	   	mipi_himax_pdata.power_off_set_size = ARRAY_SIZE(tovis_power_off_set);
	}else // Himax LCD
	{
	   	mipi_himax_pdata.power_on_set = himax_power_on_set;
	   	mipi_himax_pdata.power_off_set = himax_power_off_set;
	   	mipi_himax_pdata.power_on_set_size = ARRAY_SIZE(himax_power_on_set);
	   	mipi_himax_pdata.power_off_set_size = ARRAY_SIZE(himax_power_off_set);
	}

	return 0;
}

#endif //CONFIG_FB_MSM_MIPI_DSI_HIMAX


#ifndef CONFIG_MACH_MSM8930_FX3
static struct mipi_dsi_panel_platform_data novatek_pdata = {
	.fpga_3d_config_addr  = FPGA_3D_GPIO_CONFIG_ADDR,
	.fpga_ctrl_mode = FPGA_SPI_INTF,
	.phy_ctrl_settings = &dsi_novatek_cmd_mode_phy_db,
	.dlane_swap = 0x1,
	.enable_wled_bl_ctrl = 0x1,
};

static struct platform_device mipi_dsi_novatek_panel_device = {
	.name = "mipi_novatek",
	.id = 0,
	.dev = {
		.platform_data = &novatek_pdata,
	}
};
#endif

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
static struct resource hdmi_msm_resources[] = {
	{
		.name  = "hdmi_msm_qfprom_addr",
		.start = 0x00700000,
		.end   = 0x007060FF,
		.flags = IORESOURCE_MEM,
	},
	{
		.name  = "hdmi_msm_hdmi_addr",
		.start = 0x04A00000,
		.end   = 0x04A00FFF,
		.flags = IORESOURCE_MEM,
	},
	{
		.name  = "hdmi_msm_irq",
		.start = HDMI_IRQ,
		.end   = HDMI_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static int hdmi_enable_5v(int on);
static int hdmi_core_power(int on, int show);
static int hdmi_cec_power(int on);
static int hdmi_gpio_config(int on);
static int hdmi_panel_power(int on);

static struct msm_hdmi_platform_data hdmi_msm_data = {
	.irq = HDMI_IRQ,
	.enable_5v = hdmi_enable_5v,
	.core_power = hdmi_core_power,
	.cec_power = hdmi_cec_power,
	.panel_power = hdmi_panel_power,
	.gpio_config = hdmi_gpio_config,
};

static struct platform_device hdmi_msm_device = {
	.name = "hdmi_msm",
	.id = 0,
	.num_resources = ARRAY_SIZE(hdmi_msm_resources),
	.resource = hdmi_msm_resources,
	.dev.platform_data = &hdmi_msm_data,
};
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL */

#ifdef CONFIG_FB_MSM_WRITEBACK_MSM_PANEL
static struct platform_device wfd_panel_device = {
	.name = "wfd_panel",
	.id = 0,
	.dev.platform_data = NULL,
};

static struct platform_device wfd_device = {
	.name          = "msm_wfd",
	.id            = -1,
};
#endif

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
#ifdef CONFIG_MSM_BUS_SCALING
static struct msm_bus_vectors dtv_bus_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};

#ifdef CONFIG_FB_MSM_HDMI_AS_PRIMARY
static struct msm_bus_vectors dtv_bus_def_vectors[] = {
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 2000000000,
		.ib = 2000000000,
	},
};
#else
static struct msm_bus_vectors dtv_bus_def_vectors[] = {
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 566092800 * 2,
		.ib = 707616000 * 2,
	},
};
#endif
#endif

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
static struct msm_bus_paths dtv_bus_scale_usecases[] = {
	{
		ARRAY_SIZE(dtv_bus_init_vectors),
		dtv_bus_init_vectors,
	},
	{
		ARRAY_SIZE(dtv_bus_def_vectors),
		dtv_bus_def_vectors,
	},
};
static struct msm_bus_scale_pdata dtv_bus_scale_pdata = {
	dtv_bus_scale_usecases,
	ARRAY_SIZE(dtv_bus_scale_usecases),
	.name = "dtv",
};

static struct lcdc_platform_data dtv_pdata = {
	.bus_scale_table = &dtv_bus_scale_pdata,
	.lcdc_power_save = hdmi_panel_power,
};

static int hdmi_panel_power(int on)
{
	int rc;

	pr_debug("%s: HDMI Core: %s\n", __func__, (on ? "ON" : "OFF"));
	rc = hdmi_core_power(on, 1);
	if (rc)
		rc = hdmi_cec_power(on);

	pr_debug("%s: HDMI Core: %s Success\n", __func__, (on ? "ON" : "OFF"));
	return rc;
}
#endif
#endif

static struct gpiomux_setting mdp_vsync_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting mdp_vsync_active_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config msm8960_mdp_vsync_configs[] __initdata = {
	{
		.gpio = MDP_VSYNC_GPIO,
		.settings = {
			[GPIOMUX_ACTIVE]    = &mdp_vsync_active_cfg,
			[GPIOMUX_SUSPENDED] = &mdp_vsync_suspend_cfg,
		},
	}
}; 

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
static int hdmi_enable_5v(int on)
{
	static struct regulator *reg_ext_5v;	/* HDMI_5V */
	static int prev_on;
	int rc;

	if (on == prev_on)
		return 0;

	if (!reg_ext_5v) {
		reg_ext_5v = regulator_get(&hdmi_msm_device.dev, "hdmi_mvs");
		if (IS_ERR(reg_ext_5v)) {
			pr_err("'%s' regulator not found, rc=%ld\n",
				"hdmi_mvs", IS_ERR(reg_ext_5v));
			reg_ext_5v = NULL;
			return -ENODEV;
		}
	}

	if (on) {
		rc = regulator_enable(reg_ext_5v);
		if (rc) {
			pr_err("'%s' regulator enable failed, rc=%d\n",
				"reg_ext_5v", rc);
			return rc;
		}
		pr_debug("%s(on): success\n", __func__);
	} else {
		rc = regulator_disable(reg_ext_5v);
		if (rc)
			pr_warning("'%s' regulator disable failed, rc=%d\n",
				"reg_ext_5v", rc);
		pr_debug("%s(off): success\n", __func__);
	}

	prev_on = on;

	return 0;
}

static int hdmi_core_power(int on, int show)
{
	/* Both HDMI "avdd" and "vcc" are powered by 8038_l23 regulator */
	static struct regulator *reg_8038_l23;
	static int prev_on;
	int rc;

	if (on == prev_on)
		return 0;

	if (!reg_8038_l23) {
		reg_8038_l23 = regulator_get(&hdmi_msm_device.dev, "hdmi_avdd");
		if (IS_ERR(reg_8038_l23)) {
			pr_err("could not get reg_8038_l23, rc = %ld\n",
				PTR_ERR(reg_8038_l23));
			return -ENODEV;
		}
		rc = regulator_set_voltage(reg_8038_l23, 1800000, 1800000);
		if (rc) {
			pr_err("set_voltage failed for 8921_l23, rc=%d\n", rc);
			return -EINVAL;
		}
	}

	if (on) {
		rc = regulator_set_optimum_mode(reg_8038_l23, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l23 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		rc = regulator_enable(reg_8038_l23);
		if (rc) {
			pr_err("'%s' regulator enable failed, rc=%d\n",
				"hdmi_avdd", rc);
			return rc;
		}
		pr_debug("%s(on): success\n", __func__);
	} else {
		rc = regulator_disable(reg_8038_l23);
		if (rc) {
			pr_err("disable reg_8038_l23 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		rc = regulator_set_optimum_mode(reg_8038_l23, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l23 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		pr_debug("%s(off): success\n", __func__);
	}

	prev_on = on;

	return 0;
}

static int hdmi_gpio_config(int on)
{
	int rc = 0;
	static int prev_on;

	if (on == prev_on)
		return 0;

	if (on) {
		rc = gpio_request(100, "HDMI_DDC_CLK");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_DDC_CLK", 100, rc);
			return rc;
		}
		rc = gpio_request(101, "HDMI_DDC_DATA");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_DDC_DATA", 101, rc);
			goto error1;
		}
		rc = gpio_request(102, "HDMI_HPD");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_HPD", 102, rc);
			goto error2;
		}
		pr_debug("%s(on): success\n", __func__);
	} else {
		gpio_free(100);
		gpio_free(101);
		gpio_free(102);
		pr_debug("%s(off): success\n", __func__);
	}

	prev_on = on;
	return 0;

error2:
	gpio_free(101);
error1:
	gpio_free(100);
	return rc;
}

static int hdmi_cec_power(int on)
{
	static int prev_on;
	int rc;

	if (on == prev_on)
		return 0;

	if (on) {
		rc = gpio_request(99, "HDMI_CEC_VAR");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_CEC_VAR", 99, rc);
			goto error;
		}
		pr_debug("%s(on): success\n", __func__);
	} else {
		gpio_free(99);
		pr_debug("%s(off): success\n", __func__);
	}

	prev_on = on;

	return 0;
error:
	return rc;
}
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL */

#ifdef CONFIG_LGE_HIDDEN_RESET
int lge_get_fb_phys_info(unsigned long *start, unsigned long *size)
{
       if (!start || !size)
               return -1;

       *start = (unsigned long)msm_fb_resources[0].start;
       *size = (unsigned long)(LCD_RESOLUTION_X * LCD_RESOLUTION_Y * 4);

       return 0;
}

void *lge_get_hreset_fb_phys_addr(void)
{
       return (void *)0x8F200000;
}
#endif

static void __init msm_fb_add_devices(void)
{
#ifdef CONFIG_FB_MSM_WRITEBACK_MSM_PANEL
	platform_device_register(&wfd_panel_device);
	platform_device_register(&wfd_device);
#endif

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
	platform_device_register(&hdmi_msm_device);
#endif

	msm_fb_register_device("mdp", &mdp_pdata);
	msm_fb_register_device("mipi_dsi", &mipi_dsi_pdata);
#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
#ifdef CONFIG_MSM_BUS_SCALING
	msm_fb_register_device("dtv", &dtv_pdata);
#endif
#endif
} 

#ifndef CONFIG_MACH_MSM8930_FX3
void __init msm8930_init_fb(void)
{
	platform_device_register(&msm_fb_device);

#ifdef CONFIG_FB_MSM_WRITEBACK_MSM_PANEL
	platform_device_register(&wfd_panel_device);
	platform_device_register(&wfd_device);
#endif

	platform_device_register(&mipi_dsi_novatek_panel_device);

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
	platform_device_register(&hdmi_msm_device);
#endif

	platform_device_register(&mipi_dsi_toshiba_panel_device);

	msm_fb_register_device("mdp", &mdp_pdata);
	msm_fb_register_device("mipi_dsi", &mipi_dsi_pdata);
#ifdef CONFIG_MSM_BUS_SCALING
	msm_fb_register_device("dtv", &dtv_pdata);
#endif
}
#endif

void __init msm8930_allocate_fb_region(void)
{
	void *addr;
	unsigned long size;

	size = MSM_FB_SIZE;
	addr = alloc_bootmem_align(size, 0x1000);
	msm_fb_resources[0].start = __pa(addr);
	msm_fb_resources[0].end = msm_fb_resources[0].start + size - 1;
	pr_info("allocating %lu bytes at %p (%lx physical) for fb\n",
			size, addr, __pa(addr));

}

#ifdef CONFIG_I2C
//                                                 
#if defined (CONFIG_LCD_BIAS_TPS65132)
#define TPS65132_ADDRESS 0x3E

static struct i2c_board_info msm_i2c_lcd_bias_info[] = {
       {
                I2C_BOARD_INFO("tps65132", TPS65132_ADDRESS),
        }
};

static struct i2c_registry i2c_lcd_bias_device __initdata = {
		I2C_SURF | I2C_FFA | I2C_FLUID | I2C_RUMI,
		MSM_8930_GSBI2_QUP_I2C_BUS_ID,
		msm_i2c_lcd_bias_info,
		ARRAY_SIZE(msm_i2c_lcd_bias_info),
};
#endif
//                                                 

#if defined (CONFIG_BACKLIGHT_LM3639)
#define LCD_BL_PM_EN    2
#define LM3639_BACKLIGHT_ADDRESS 0x39

struct backlight_platform_data {
        void (*platform_init)(void);
        int gpio;
        unsigned int mode;
        int max_current;
        int init_on_boot;
        int min_brightness;
        int max_brightness;
        int default_brightness;
        int factory_brightness;
};

static struct backlight_platform_data lm3639_data = {
        .gpio = LCD_BL_PM_EN,
        .max_current = 0x07,
        .min_brightness = 0x01,
//                                                                       
        .max_brightness = 0xFF,
//                                                                       
        .default_brightness = 0x73,
        .factory_brightness = 0x2B,
};

static struct i2c_board_info msm_i2c_backlight_info[] = {
       {
                I2C_BOARD_INFO("lm3639_bled", LM3639_BACKLIGHT_ADDRESS),
                .platform_data = &lm3639_data,
        }
};

static struct i2c_registry i2c_backlight_device __initdata = {
		I2C_SURF | I2C_FFA | I2C_FLUID | I2C_RUMI,
		MSM_8930_GSBI2_QUP_I2C_BUS_ID,
		msm_i2c_backlight_info,
		ARRAY_SIZE(msm_i2c_backlight_info),
};
#endif
//                                                       

static int __init panel_gpiomux_init(void)
{
	int rc;

	rc = msm_gpiomux_init(NR_GPIO_IRQS);
	if (rc == -EPERM) {
		pr_info("%s : msm_gpiomux_init is already initialized\n",
				__func__);
	} else if (rc) {
		pr_err(KERN_ERR "msm_gpiomux_init failed %d\n", rc);
		return rc;
	}

	msm_gpiomux_install(msm8960_mdp_vsync_configs,
			ARRAY_SIZE(msm8960_mdp_vsync_configs));

	return 0;
}
#endif
#ifdef CONFIG_LGE_KCAL
extern int set_kcal_values(int kcal_r, int kcal_g, int kcal_b);
extern int refresh_kcal_display(void);
extern int get_kcal_values(int *kcal_r, int *kcal_g, int *kcal_b);

static struct kcal_platform_data kcal_pdata = {
	.set_values = set_kcal_values,
	.get_values = get_kcal_values,
	.refresh_display = refresh_kcal_display
};

static struct platform_device kcal_platrom_device = {
	.name   = "kcal_ctrl",
	.dev = {
		.platform_data = &kcal_pdata,
	}
};
#endif

static struct platform_device *d1l_panel_devices[] __initdata = {
#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
	&hdmi_msm_device,
#endif
#if defined(CONFIG_FB_MSM_MIPI_DSI_LGIT)
	&mipi_dsi_lgit_panel_device,
#elif defined(CONFIG_FB_MSM_MIPI_DSI_HITACHI)
	&mipi_dsi_hitachi_panel_device,
#elif defined(CONFIG_FB_MSM_MIPI_DSI_HIMAX)
	&mipi_dsi_himax_panel_device,
#elif defined(CONFIG_FB_MSM_MIPI_DSI_R61529)
	&mipi_dsi_r61529_panel_device,
#elif defined(CONFIG_FB_MSM_MIPI_DSI_LGD)
	&mipi_dsi_lgd_panel_device,
#elif defined(CONFIG_FB_MSM_MIPI_DSI_TX11D108VM)
	&mipi_dsi_tx11d108vm_panel_device,
//                                                 
#elif defined(CONFIG_FB_MSM_MIPI_DSI_TX13D107VM)
	&mipi_dsi_tx13d107vm_panel_device,
#endif
//                                                 
#ifdef CONFIG_LGE_KCAL
	&kcal_platrom_device,
#endif
#ifdef CONFIG_LGE_LCD_TUNING
	&lcd_misc_device,
#endif

};

void __init lge_add_lcd_devices(void)
{
	panel_gpiomux_init();

	fb_register_client(&msm_fb_event_notifier);

	platform_add_devices(d1l_panel_devices, ARRAY_SIZE(d1l_panel_devices));

//                                                 
#if defined (CONFIG_LCD_BIAS_TPS65132)
	lge_add_msm_i2c_device(&i2c_lcd_bias_device);
#endif
//                                                 
//                                                       
#if defined (CONFIG_BACKLIGHT_LM3639)
	lge_add_msm_i2c_device(&i2c_backlight_device);
#endif
//                                                       

	msm_fb_add_devices();
	platform_device_register(&msm_fb_device);
}
