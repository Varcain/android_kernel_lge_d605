/* Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
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

static struct msm_panel_info pinfo;

static struct mipi_dsi_phy_ctrl dsi_video_mode_phy_db = {
	/* DSIPHY_REGULATOR_CTRL */
	{0x03, 0x0a, 0x04, 0x00, 0x20},	/* Fixed values */

	/* DSIPHY_TIMING_CTRL */
	{0x7B, 0x1B, 0x12, 0x00, 0x40, 0x49, 0x17, 0x1E,
	0x1E, 0x03, 0x04, 0xA0},

	/* DSIPHY_CTRL */
	{0x5f, 0x00, 0x00, 0x10},	/* Fixed values */
	/* DSIPHY_STRENGTH_CTRL */
	{0xff, 0x00, 0x06, 0x00},	/* Fixed values */

	/* DSIPHY_PLL_CTRL */
	{0x00, 0x9E, 0x31, 0xD9, 0x00, 0x50, 0x48, 0x63,
	0x41, 0x0F, 0x03, 0x00, 0x14, 0x03, 0x00, 0x02,
	0x00, 0x20, 0x00, 0x01 },
};

static int __init mipi_video_tx13d107vm_hd_pt_init(void)
{
	int ret;

	pinfo.xres = 720;
	pinfo.yres = 1280;
	pinfo.type = MIPI_VIDEO_PANEL;
	pinfo.pdest = DISPLAY_1;
	pinfo.wait_cycle = 0;
	pinfo.bpp = 24;

	/* QCT Limitation :
	 * All proch values must be a multiple of 4. 2011.01.20 */
	pinfo.lcdc.h_back_porch = 132;
	pinfo.lcdc.h_front_porch = 44;
	pinfo.lcdc.h_pulse_width = 8;
	pinfo.lcdc.v_back_porch = 7;
	pinfo.lcdc.v_front_porch = 13;
	pinfo.lcdc.v_pulse_width = 4;

	pinfo.lcdc.border_clr = 0;	/* blk */
	pinfo.lcdc.underflow_clr = 0xff;	/* blue */
	pinfo.lcdc.hsync_skew = 0;
	pinfo.bl_max = 0xff;
	pinfo.bl_min = 1;
	pinfo.fb_num = 2;

	pinfo.mipi.mode = DSI_VIDEO_MODE;
	pinfo.mipi.pulse_mode_hsa_he = FALSE;
	pinfo.mipi.hfp_power_stop = TRUE;
	pinfo.mipi.hbp_power_stop = TRUE;
	pinfo.mipi.hsa_power_stop = TRUE;
	pinfo.mipi.eof_bllp_power_stop = TRUE;
	pinfo.mipi.bllp_power_stop = TRUE;
	pinfo.mipi.traffic_mode = DSI_NON_BURST_SYNCH_EVENT;
	pinfo.mipi.dst_format = DSI_VIDEO_DST_FORMAT_RGB888;
	pinfo.mipi.vc = 0;
	pinfo.mipi.rgb_swap = DSI_RGB_SWAP_RGB;
	pinfo.mipi.data_lane0 = TRUE;
	pinfo.mipi.data_lane1 = TRUE;
	pinfo.mipi.data_lane2 = TRUE;
	pinfo.mipi.data_lane3 = TRUE;
	pinfo.mipi.dlane_swap = 0x00;

	pinfo.mipi.t_clk_post = 0x22;
	pinfo.mipi.t_clk_pre = 0x35;
	pinfo.clk_rate = 430960000;
	pinfo.mipi.frame_rate = 61;

	pinfo.mipi.stream = 0; /* dma_p */
	pinfo.mipi.mdp_trigger = DSI_CMD_TRIGGER_NONE;
	pinfo.mipi.dma_trigger = DSI_CMD_TRIGGER_SW;
	pinfo.mipi.dsi_phy_db = &dsi_video_mode_phy_db;
	pinfo.mipi.esc_byte_ratio = 2;
	ret = mipi_tx13d107vm_device_register(&pinfo, MIPI_DSI_PRIM,
						MIPI_DSI_PANEL_720P_PT);
	if (ret)
		printk(KERN_ERR "%s: failed to register device!\n", __func__);

	return ret;
}

module_init(mipi_video_tx13d107vm_hd_pt_init);
