/* Copyright (c) 2008-2010, Code Aurora Forum. All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mipi_tx13d107vm.h"
#include <linux/gpio.h>
#if defined(CONFIG_MACH_MSM8930_LGPS9) || defined(CONFIG_MACH_MSM8930_FX3)
#include <linux/leds.h>
#endif

static struct msm_panel_common_pdata *mipi_tx13d107vm_pdata;

#if (defined(CONFIG_MACH_MSM8930_LGPS9) || defined(CONFIG_MACH_MSM8930_FX3)) \
        && !defined(CONFIG_BACKLIGHT_LM3639)
static int wled_trigger_initialized;
#endif

static struct dsi_buf tx13d107vm_tx_buf;
static struct dsi_buf tx13d107vm_rx_buf;

#if (defined(CONFIG_MACH_MSM8930_LGPS9) || defined(CONFIG_MACH_MSM8930_FX3)) \
        && !defined(CONFIG_BACKLIGHT_LM3639)
static char led_pwm1[2] = {0x51, 0x0};	/* DTYPE_DCS_WRITE1 */
#endif


static char display_on[2] = {0x29, 0x00};
static char display_off[2] = {0x28, 0x00};

static struct dsi_cmd_desc tx13d107vm_display_on_set[] = {
        {DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(display_on), display_on},
}; 

static struct dsi_cmd_desc tx13d107vm_display_off_set[] = {
        {DTYPE_DCS_WRITE, 1, 0, 0, 20, sizeof(display_off), display_off},
}; 

static int mipi_tx13d107vm_lcd_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	mfd = platform_get_drvdata(pdev);
	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	printk(KERN_INFO "[JK]%s: mipi tx13d107vm lcd on started \n", __func__);

	mipi_set_tx_power_mode(1); //LP mode 
	mipi_dsi_cmds_tx(&tx13d107vm_tx_buf, mipi_tx13d107vm_pdata->power_on_set,
			mipi_tx13d107vm_pdata->power_on_set_size);

	mipi_set_tx_power_mode(0); //HS mode 
	mipi_dsi_cmds_tx(&tx13d107vm_tx_buf, tx13d107vm_display_on_set,
			ARRAY_SIZE(tx13d107vm_display_on_set));

	return 0;
}


static int mipi_tx13d107vm_lcd_off(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	mfd = platform_get_drvdata(pdev);
	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	printk(KERN_INFO "[JK]%s: mipi tx13d107vm lcd off started \n", __func__);

	mipi_set_tx_power_mode(0); //HS mode 
	mipi_dsi_cmds_tx(&tx13d107vm_tx_buf, tx13d107vm_display_off_set,
			ARRAY_SIZE(tx13d107vm_display_off_set));

	mipi_set_tx_power_mode(1); //LP mode 
	mipi_dsi_cmds_tx(&tx13d107vm_tx_buf,
			mipi_tx13d107vm_pdata->power_off_set,
			mipi_tx13d107vm_pdata->power_off_set_size);
	return 0;
}


#if (defined(CONFIG_MACH_MSM8930_LGPS9) || defined(CONFIG_MACH_MSM8930_FX3)) \
        && !defined(CONFIG_BACKLIGHT_LM3639)
DEFINE_LED_TRIGGER(bkl_led_trigger);

static void mipi_tx13d107vm_set_backlight_board(struct msm_fb_data_type *mfd)
{
	struct mipi_panel_info *mipi;

	if ((mipi_tx13d107vm_pdata->enable_wled_bl_ctrl)
	    && (wled_trigger_initialized)) {
		led_trigger_event(bkl_led_trigger, mfd->bl_level);
		return;
	}
	mipi  = &mfd->panel_info.mipi;

	led_pwm1[1] = (unsigned char)(mfd->bl_level);
	return;
}
#else

static void mipi_tx13d107vm_set_backlight_board(struct msm_fb_data_type *mfd)
{
	int level;

	level = (int)mfd->bl_level;
	mipi_tx13d107vm_pdata->backlight_level(level, 0, 0);
}
#endif

static int mipi_tx13d107vm_lcd_probe(struct platform_device *pdev)
{
	struct msm_fb_panel_data *pdata;

	if (pdev->id == 0) {
		mipi_tx13d107vm_pdata = pdev->dev.platform_data;
		return 0;
	}

	printk(KERN_INFO "%s: mipi tx13d107vm lcd probe start\n", __func__);
	pdata = pdev->dev.platform_data;
	if (!pdata)
		return 0;

#if (!defined(CONFIG_MACH_MSM8930_LGPS9) && !defined(CONFIG_MACH_MSM8930_FX3))  \
                ||defined(CONFIG_BACKLIGHT_LM3639) 
	pdata->panel_info.bl_max = mipi_tx13d107vm_pdata->max_backlight_level;
#endif
	msm_fb_add_device(pdev);

	return 0;
}

static struct platform_driver this_driver = {
	.probe  = mipi_tx13d107vm_lcd_probe,
	.driver = {
		.name   = "mipi_tx13d107vm",
	},
};

static struct msm_fb_panel_data tx13d107vm_panel_data = {
	.on		= mipi_tx13d107vm_lcd_on,
	.off		= mipi_tx13d107vm_lcd_off,
	.set_backlight = mipi_tx13d107vm_set_backlight_board,
};

static int ch_used[3];

int mipi_tx13d107vm_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel)
{
	struct platform_device *pdev = NULL;
	int ret;

	if ((channel >= 3) || ch_used[channel])
		return -ENODEV;

	ch_used[channel] = TRUE;

	pdev = platform_device_alloc("mipi_tx13d107vm", (panel << 8)|channel);
	if (!pdev)
		return -ENOMEM;

	tx13d107vm_panel_data.panel_info = *pinfo;

	ret = platform_device_add_data(pdev, &tx13d107vm_panel_data,
		sizeof(tx13d107vm_panel_data));
	if (ret) {
		printk(KERN_ERR
		  "%s: platform_device_add_data failed!\n", __func__);
		goto err_device_put;
	}

	ret = platform_device_add(pdev);
	if (ret) {
		printk(KERN_ERR
		  "%s: platform_device_register failed!\n", __func__);
		goto err_device_put;
	}

	if (ret) {
		printk(KERN_ERR
		  "%s: device_create_file failed!\n", __func__);
		goto err_device_put;
	}

	return 0;

err_device_put:
	platform_device_put(pdev);
	return ret;
}

static int __init mipi_tx13d107vm_lcd_init(void)
{
#if (defined(CONFIG_MACH_MSM8930_LGPS9) || defined(CONFIG_MACH_MSM8930_FX3))  \
                && !defined(CONFIG_BACKLIGHT_LM3639) 
	led_trigger_register_simple("bkl_trigger", &bkl_led_trigger);
	pr_info("%s: SUCCESS (WLED TRIGGER)\n", __func__);
	wled_trigger_initialized = 1;
#endif

	mipi_dsi_buf_alloc(&tx13d107vm_tx_buf, DSI_BUF_SIZE);
	mipi_dsi_buf_alloc(&tx13d107vm_rx_buf, DSI_BUF_SIZE);

	return platform_driver_register(&this_driver);
}

module_init(mipi_tx13d107vm_lcd_init);
