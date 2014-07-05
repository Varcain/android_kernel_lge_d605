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

#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <media/msm_camera.h>
#include <media/v4l2-subdev.h>
#include <mach/gpio.h>
#include <mach/camera.h>
#include "imx111.h"
#include "msm.h"
/*=============================================================
	SENSOR REGISTER DEFINES
==============================================================*/
#define REG_GROUPED_PARAMETER_HOLD		0x0104
#define GROUPED_PARAMETER_HOLD_OFF		0x00
#define GROUPED_PARAMETER_HOLD			0x01
/* Integration Time */
#define REG_COARSE_INTEGRATION_TIME		0x0202
/* Gain */
#define REG_GLOBAL_GAIN	0x0204
/* PLL registers */
#define REG_FRAME_LENGTH_LINES		0x0340
/* Test Pattern */
#define REG_TEST_PATTERN_MODE			0x0601
#define REG_VCM_CONTROL			0x30F0
#define REG_VCM_NEW_CODE			0x30F2
#define REG_VCM_STEP_TIME			0x30F4

/*============================================================================
							 TYPE DECLARATIONS
============================================================================*/

/* 16bit address - 8 bit context register structure */
#define Q8	0x00000100
#define Q10	0x00000400
#define IMX111_MASTER_CLK_RATE 24000000

/* AF Total steps parameters */
#define IMX111_TOTAL_STEPS_NEAR_TO_FAR    37

static uint16_t imx111_linear_total_step = IMX111_TOTAL_STEPS_NEAR_TO_FAR;
static uint16_t imx111_step_position_table[IMX111_TOTAL_STEPS_NEAR_TO_FAR+1];
uint16_t imx111_nl_region_boundary1 = 2;
uint16_t imx111_nl_region_code_per_step1 = 25;
uint16_t imx111_l_region_code_per_step = 3;
uint16_t imx111_vcm_step_time;
uint16_t imx111_sw_damping_time_wait;


struct imx111_work_t {
	struct work_struct work;
};

static struct imx111_work_t *imx111_sensorw;
static struct i2c_client *imx111_client;

struct imx111_ctrl_t {
	const struct  msm_camera_sensor_info *sensordata;

	uint32_t sensormode;
	uint32_t fps_divider;/* init to 1 * 0x00000400 */
	uint32_t pict_fps_divider;/* init to 1 * 0x00000400 */
	uint16_t fps;

	uint16_t curr_lens_pos;
	uint16_t curr_step_pos;
	uint16_t my_reg_gain;
	uint32_t my_reg_line_count;
	uint16_t total_lines_per_frame;

	enum imx111_resolution_t prev_res;
	enum imx111_resolution_t pict_res;
	enum imx111_resolution_t curr_res;

	struct v4l2_subdev *sensor_dev;
	struct imx111_format *fmt;
};


static bool CSI_CONFIG;
static struct imx111_ctrl_t *imx111_ctrl;
static DECLARE_WAIT_QUEUE_HEAD(imx111_wait_queue);
static DEFINE_MUTEX(imx111_mut);

static int cam_debug_init(void);
static struct dentry *debugfs_base;

struct imx111_format {
	enum v4l2_mbus_pixelcode code;
	enum v4l2_colorspace colorspace;
	u16 fmt;
	u16 order;
};
/*=============================================================*/
#if 0
static int imx111_i2c_rxdata(unsigned short saddr,
	unsigned char *rxdata, int length)
{
	struct i2c_msg msgs[] = {
		{
			.addr  = saddr,
			.flags = 0,
			.len   = 2,
			.buf   = rxdata,
		},
		{
			.addr  = saddr,
			.flags = I2C_M_RD,
			.len   = 2,
			.buf   = rxdata,
		},
	};
	if (i2c_transfer(imx111_client->adapter, msgs, 2) < 0) {
		pr_err("imx111_i2c_rxdata faild 0x%x\n", saddr);
		return -EIO;
	}
	return 0;
}
#endif
static int32_t imx111_i2c_txdata(unsigned short saddr,
				unsigned char *txdata, int length)
{
	struct i2c_msg msg[] = {
		{
			.addr = saddr,
			.flags = 0,
			.len = length,
			.buf = txdata,
		 },
	};
	if (i2c_transfer(imx111_client->adapter, msg, 1) < 0) {
		pr_err("imx111_i2c_txdata faild 0x%x\n", saddr);
		return -EIO;
	}

	return 0;
}
#if 0
static int32_t imx111_i2c_read(unsigned short raddr,
	unsigned short *rdata, int rlen)
{
	int32_t rc = 0;
	unsigned char buf[2];
	if (!rdata)
		return -EIO;
	memset(buf, 0, sizeof(buf));
	buf[0] = (raddr & 0xFF00) >> 8;
	buf[1] = (raddr & 0x00FF);
	rc = imx111_i2c_rxdata(imx111_client->addr<<1, buf, rlen);
	if (rc < 0) {
		pr_err("imx111_i2c_read 0x%x failed!\n", raddr);
		return rc;
	}
	*rdata = (rlen == 2 ? buf[0] << 8 | buf[1] : buf[0]);
//	pr_err("imx111_i2c_read 0x%x val = 0x%x!\n", raddr, *rdata);
	return rc;
}
static int32_t imx111_i2c_write_w_sensor(unsigned short waddr, uint16_t wdata)
{
	int32_t rc = -EFAULT;
	unsigned char buf[4];
	memset(buf, 0, sizeof(buf));
	buf[0] = (waddr & 0xFF00) >> 8;
	buf[1] = (waddr & 0x00FF);
	buf[2] = (wdata & 0xFF00) >> 8;
	buf[3] = (wdata & 0x00FF);
//	pr_err("i2c_write_b addr = 0x%x, val = 0x%x\n", waddr, wdata);
	rc = imx111_i2c_txdata(imx111_client->addr<<1, buf, 4);
	if (rc < 0) {
		pr_err("i2c_write_b failed, addr = 0x%x, val = 0x%x!\n",
			waddr, wdata);
	}
	return rc;
}
#endif

static int32_t imx111_i2c_write_b_sensor(unsigned short waddr, uint8_t bdata)
{
	int32_t rc = -EFAULT;
	unsigned char buf[3];
	memset(buf, 0, sizeof(buf));
	buf[0] = (waddr & 0xFF00) >> 8;
	buf[1] = (waddr & 0x00FF);
	buf[2] = bdata;
//	pr_err("i2c_write_b addr = 0x%x, val = 0x%x\n", waddr, bdata);
	rc = imx111_i2c_txdata(imx111_client->addr<<1, buf, 3);
	if (rc < 0) {
		pr_err("i2c_write_b failed, addr = 0x%x, val = 0x%x!\n",
			waddr, bdata);
	}
	return rc;
}

static int32_t imx111_i2c_write_w_table(struct imx111_i2c_reg_conf const
					 *reg_conf_tbl, int num)
{
	int i;
	int32_t rc = -EIO;
	for (i = 0; i < num; i++) {
		rc = imx111_i2c_write_b_sensor(reg_conf_tbl->waddr,
			reg_conf_tbl->wdata);
		if (rc < 0)
			break;
		reg_conf_tbl++;
	}
	return rc;
}

static void imx111_group_hold_on(void)
{
	imx111_i2c_write_b_sensor(REG_GROUPED_PARAMETER_HOLD,
						GROUPED_PARAMETER_HOLD);
}

static void imx111_group_hold_off(void)
{
	imx111_i2c_write_b_sensor(REG_GROUPED_PARAMETER_HOLD,
						GROUPED_PARAMETER_HOLD_OFF);
}

static void imx111_start_stream(void)
{
	imx111_i2c_write_b_sensor(0x0100, 0x01);
}

static void imx111_stop_stream(void)
{
	imx111_i2c_write_b_sensor(0x0100, 0x00);
}

static void imx111_get_pict_fps(uint16_t fps, uint16_t *pfps)
{
#if 0
	/* input fps is preview fps in Q8 format */
	uint16_t preview_frame_length_lines, snapshot_frame_length_lines;
	uint32_t divider;
	/* Total frame_length_lines for preview */
	preview_frame_length_lines = IMX111_QTR_SIZE_HEIGHT +
		IMX111_VER_QTR_BLK_LINES;
	/* Total frame_length_lines for snapshot */
	snapshot_frame_length_lines = IMX111_FULL_SIZE_HEIGHT +
		IMX111_VER_FULL_BLK_LINES;

	divider = preview_frame_length_lines * 0x00010000/
		snapshot_frame_length_lines;

	/*Verify PCLK settings and frame sizes.*/
	*pfps = (uint16_t) ((uint32_t)fps * divider / 0x10000);
	/* 2 is the ratio of no.of snapshot channels
	to number of preview channels */
#endif
}

static uint16_t imx111_get_prev_lines_pf(void)
{
	if (imx111_ctrl->prev_res == QTR_SIZE)
		return 0x04E6;
	else if (imx111_ctrl->prev_res == FULL_SIZE)
		return 0x09D4;
	else
		return 0x04E6;
}

static uint16_t imx111_get_prev_pixels_pl(void)
{
	if (imx111_ctrl->prev_res == QTR_SIZE)
		return 0x0DD0;
	else if (imx111_ctrl->prev_res == FULL_SIZE)
		return 0xDD0;
	else
		return 0xDD0;
}

static uint16_t imx111_get_pict_lines_pf(void)
{
	if (imx111_ctrl->pict_res == QTR_SIZE)
		return 0x04E6;
	else if (imx111_ctrl->pict_res == FULL_SIZE)
		return 0x09D4;
	else
		return 0x04E6;
}

static uint16_t imx111_get_pict_pixels_pl(void)
{
	if (imx111_ctrl->pict_res == QTR_SIZE)
		return 0x0DD0;
	else if (imx111_ctrl->pict_res == FULL_SIZE)
		return 0xDD0;
	else
		return 0xDD0;
}

static uint32_t imx111_get_pict_max_exp_lc(void)
{
	if (imx111_ctrl->pict_res == QTR_SIZE)
		return 0x04E6	* 24;
	else if (imx111_ctrl->pict_res == FULL_SIZE)
		return 0x09D4	* 24;
	else
		return 0x04E6	* 24;
}

static int32_t imx111_set_fps(struct fps_cfg   *fps)
{
	uint16_t total_lines_per_frame;
	int32_t rc = 0;
	if (imx111_ctrl->curr_res == QTR_SIZE)
		total_lines_per_frame =	0x04;
	else if (imx111_ctrl->curr_res == FULL_SIZE)
		total_lines_per_frame = 0x07;

#if 0

	else if (imx111_ctrl->curr_res == HFR_60FPS)
		total_lines_per_frame =
		imx111_regs.reg_60fps[E013_FRAME_LENGTH_LINES].wdata;
	else if (imx111_ctrl->curr_res == HFR_90FPS)
		total_lines_per_frame =
		imx111_regs.reg_120fps[E013_FRAME_LENGTH_LINES].wdata;
	else
		total_lines_per_frame =
		imx111_regs.reg_120fps[E013_FRAME_LENGTH_LINES].wdata;

	imx111_ctrl->fps_divider = fps->fps_div;
	imx111_ctrl->pict_fps_divider = fps->pict_fps_div;

	if (imx111_ctrl->curr_res == FULL_SIZE) {
		total_lines_per_frame = (uint16_t)
		(total_lines_per_frame * imx111_ctrl->pict_fps_divider/0x400);
	} else {
		total_lines_per_frame = (uint16_t)
		(total_lines_per_frame * imx111_ctrl->fps_divider/0x400);
	}
#endif
	imx111_group_hold_on();
	rc = imx111_i2c_write_b_sensor(REG_FRAME_LENGTH_LINES,
							total_lines_per_frame);
	imx111_group_hold_off();
	return rc;
}

static int32_t imx111_write_exp_gain(uint16_t gain, uint32_t line)
{
	uint16_t max_legal_gain = 0xE7F;
	int32_t rc = 0;
	if (gain > max_legal_gain) {
		pr_err("Max legal gain Line:%d\n", __LINE__);
		gain = max_legal_gain;
	}

	if (imx111_ctrl->curr_res != FULL_SIZE) {
		imx111_ctrl->my_reg_gain = gain;
		imx111_ctrl->my_reg_line_count = (uint16_t) line;
		line = (uint32_t) (line * imx111_ctrl->fps_divider /
						   0x00000400);
	} else {
		line = (uint32_t) (line * imx111_ctrl->pict_fps_divider /
						   0x00000400);
	}

	gain |= 0x1000;

	imx111_group_hold_on();
	rc = imx111_i2c_write_b_sensor(REG_GLOBAL_GAIN, gain);
	rc = imx111_i2c_write_b_sensor(REG_COARSE_INTEGRATION_TIME, line);
	imx111_group_hold_off();
	return rc;
}

static int32_t imx111_set_pict_exp_gain(uint16_t gain, uint32_t line)
{
	int32_t rc = 0;
	rc = imx111_write_exp_gain(gain, line);
	//imx111_i2c_write_w_sensor(0x301A, 0x065C|0x2);
	return rc;
}

static int32_t imx111_move_focus(int direction,
	int32_t num_steps)
{
#if 0

	int16_t step_direction, dest_lens_position, dest_step_position;
	if (direction == MOVE_NEAR)
		step_direction = 1;
	else
		step_direction = -1;

	dest_step_position = imx111_ctrl->curr_step_pos
						+ (step_direction * num_steps);

	if (dest_step_position < 0)
		dest_step_position = 0;
	else if (dest_step_position > imx111_linear_total_step)
		dest_step_position = imx111_linear_total_step;

	if (dest_step_position == imx111_ctrl->curr_step_pos)
		return 0;

	CDBG("__debug:MoveFocus, dest_step_position:%d \n", dest_step_position);
	dest_lens_position = imx111_step_position_table[dest_step_position];
	if ((dest_step_position <= 4) && (step_direction == 1)) {
		imx111_i2c_write_w_sensor(REG_VCM_STEP_TIME, 0x0000);
		if (num_steps == 4) {
			CDBG("__debug:MoveFocus, jumpvalue:%d \n",
			imx111_nl_region_boundary1 * imx111_nl_region_code_per_step1);
			imx111_i2c_write_w_sensor(REG_VCM_NEW_CODE,
			imx111_nl_region_boundary1 * imx111_nl_region_code_per_step1);
		} else {
			if (dest_step_position <= imx111_nl_region_boundary1) {
				CDBG("__debug:MoveFocus, fine search:%d \n",
					dest_lens_position);
				imx111_i2c_write_w_sensor(REG_VCM_NEW_CODE,
					dest_lens_position);
				imx111_ctrl->curr_lens_pos = dest_lens_position;
				imx111_ctrl->curr_step_pos = dest_step_position;
				return 0;
			}
		}
	}

	if(step_direction < 0) {
		if(num_steps > 20) {
			/*macro to infinity*/
			imx111_vcm_step_time = 0x0050;
			imx111_sw_damping_time_wait = 5;
		} else if (num_steps <= 4) {
			/*reverse search fine step  dir - macro to infinity*/
			imx111_vcm_step_time = 0x0400;
			imx111_sw_damping_time_wait = 4;
		} else {
			/*reverse search Coarse Jump ( > 4) dir - macro to infinity*/
			imx111_vcm_step_time = 0x96;
			imx111_sw_damping_time_wait = 3;
			}
	} else {
		if(num_steps >= 4) {
			/*coarse jump  dir - infinity to macro*/
			imx111_vcm_step_time = 0x0200;
			imx111_sw_damping_time_wait = 2;
		} else {
			/*fine step  dir - infinity to macro*/
			imx111_vcm_step_time = 0x0400;
			imx111_sw_damping_time_wait = 4;
		}
	}
	imx111_i2c_write_w_sensor(REG_VCM_STEP_TIME,
				imx111_vcm_step_time);
	CDBG("__debug:MoveFocus, imx111_vcm_step_time:%d \n", imx111_vcm_step_time);
	CDBG("__debug:MoveFocus, DestLensPosition:%d \n", dest_lens_position);

	if (imx111_ctrl->curr_lens_pos != dest_lens_position) {
		imx111_i2c_write_w_sensor(REG_VCM_NEW_CODE,
		dest_lens_position);
		usleep(imx111_sw_damping_time_wait * 1000);
	}
	imx111_ctrl->curr_lens_pos = dest_lens_position;
	imx111_ctrl->curr_step_pos = dest_step_position;
#endif
	return 0;
}

static int32_t imx111_set_default_focus(uint8_t af_step)
{
	int32_t rc = 0;
#if 0
	if (imx111_ctrl->curr_step_pos != 0) {
		rc = imx111_move_focus(MOVE_FAR,
		imx111_ctrl->curr_step_pos);
	} else {
		imx111_i2c_write_w_sensor(REG_VCM_NEW_CODE, 0x00);
	}

	imx111_ctrl->curr_lens_pos = 0;
	imx111_ctrl->curr_step_pos = 0;
#endif
	return rc;
}

static void imx111_init_focus(void)
{
	uint8_t i;
	imx111_step_position_table[0] = 0;
	for (i = 1; i <= imx111_linear_total_step; i++) {
		if (i <= imx111_nl_region_boundary1) {
			imx111_step_position_table[i] =
				imx111_step_position_table[i-1]
				+ imx111_nl_region_code_per_step1;
		} else {
			imx111_step_position_table[i] =
				imx111_step_position_table[i-1]
				+ imx111_l_region_code_per_step;
		}

		if (imx111_step_position_table[i] > 255)
			imx111_step_position_table[i] = 255;
	}
	imx111_ctrl->curr_lens_pos = 0;
}


static int32_t imx111_sensor_setting(int update_type, int rt)
{

	int32_t rc = 0;
	struct msm_camera_csid_params imx111_csid_params;
	struct msm_camera_csiphy_params imx111_csiphy_params;

	pr_err("%s: update_type = %d, rt = %d\n", __func__, update_type, rt);

	imx111_stop_stream();
	msleep(15);
	if (update_type == REG_INIT) {
#if 0
		imx111_i2c_write_w_table(imx111_regs.reg_mipi,
			imx111_regs.reg_mipi_size);
#endif
		imx111_i2c_write_w_table(imx111_regs.rec_settings,
			imx111_regs.rec_size);
		cam_debug_init();
		CSI_CONFIG = 0;
	} else if (update_type == UPDATE_PERIODIC) {
		//msleep(100);
		if (!CSI_CONFIG) {
			struct msm_camera_csid_vc_cfg imx111_vccfg[] = {
				{0, CSI_RAW10, CSI_DECODE_10BIT},
				{1, CSI_EMBED_DATA, CSI_DECODE_8BIT},
				{2, CSI_RESERVED_DATA, CSI_DECODE_8BIT},
			};
			imx111_csid_params.lane_cnt = 2;
			imx111_csid_params.lane_assign = 0xe4;
			imx111_csid_params.lut_params.num_cid =
				ARRAY_SIZE(imx111_vccfg);
			imx111_csid_params.lut_params.vc_cfg =
				&imx111_vccfg[0];
			imx111_csiphy_params.lane_cnt = 2;
			imx111_csiphy_params.settle_cnt = 0x14;//0x1B;
			rc = msm_camio_csid_config(&imx111_csid_params);
			v4l2_subdev_notify(imx111_ctrl->sensor_dev,
					NOTIFY_CID_CHANGE, NULL);
			dsb();
			rc = msm_camio_csiphy_config
				(&imx111_csiphy_params);
			dsb();
			msleep(10);
			CSI_CONFIG = 1;
		}
		if (rt == QTR_SIZE) {
			imx111_i2c_write_w_table(imx111_regs.reg_prev,
				imx111_regs.reg_prev_size);
		} else if (rt == FULL_SIZE) {
			imx111_i2c_write_w_table(imx111_regs.reg_snap,
				imx111_regs.reg_snap_size);
		}
#if 0
		else if (rt == HFR_60FPS) {
			imx111_i2c_write_w_table(imx111_regs.reg_pll_120fps,
				imx111_regs.reg_pll_120fps_size);
			imx111_i2c_write_w_sensor(0x0306, 0x0029);
			imx111_i2c_write_w_table(imx111_regs.reg_120fps,
				imx111_regs.reg_120fps_size);
		} else if (rt == HFR_90FPS) {
			imx111_i2c_write_w_table(imx111_regs.reg_pll_120fps,
				imx111_regs.reg_pll_120fps_size);
			imx111_i2c_write_w_sensor(0x0306, 0x003D);
			imx111_i2c_write_w_table(imx111_regs.reg_120fps,
				imx111_regs.reg_120fps_size);
		} else if (rt == HFR_120FPS) {
			msm_camio_vfe_clk_rate_set(266667000);
			imx111_i2c_write_w_table(imx111_regs.reg_pll_120fps,
				imx111_regs.reg_pll_120fps_size);
			imx111_i2c_write_w_table(imx111_regs.reg_120fps,
				imx111_regs.reg_120fps_size);
		}
#endif
		imx111_start_stream();
	}
	return rc;
}

static int32_t imx111_video_config(int mode)
{

	int32_t rc = 0;

	pr_err("%s: Start: mode = %d\n", __func__, mode);

	/* change sensor resolution if needed */
	if (imx111_sensor_setting(UPDATE_PERIODIC,
			imx111_ctrl->prev_res) < 0)
		return rc;

	imx111_ctrl->curr_res = imx111_ctrl->prev_res;
	imx111_ctrl->sensormode = mode;
	return rc;
}

static int32_t imx111_snapshot_config(int mode)
{
	int32_t rc = 0;

	pr_err("%s: Start: mode = %d\n", __func__, mode);

	/*change sensor resolution if needed */
	if (imx111_ctrl->curr_res != imx111_ctrl->pict_res) {
		if (imx111_sensor_setting(UPDATE_PERIODIC,
				imx111_ctrl->pict_res) < 0)
			return rc;
	}

	imx111_ctrl->curr_res = imx111_ctrl->pict_res;
	imx111_ctrl->sensormode = mode;
	return rc;
}

static int32_t imx111_raw_snapshot_config(int mode)
{
	int32_t rc = 0;

	pr_err("%s: Start: mode = %d\n", __func__, mode);

	/* change sensor resolution if needed */
	if (imx111_ctrl->curr_res != imx111_ctrl->pict_res) {
		if (imx111_sensor_setting(UPDATE_PERIODIC,
				imx111_ctrl->pict_res) < 0)
			return rc;
	}

	imx111_ctrl->curr_res = imx111_ctrl->pict_res;
	imx111_ctrl->sensormode = mode;
	return rc;
}

static int32_t imx111_set_sensor_mode(int mode,
	int res)
{
	int32_t rc = 0;

	pr_err("%s: mode = %d, res = %d", __func__, mode, res);

	switch (mode) {
	case SENSOR_PREVIEW_MODE:
	case SENSOR_HFR_60FPS_MODE:
	case SENSOR_HFR_90FPS_MODE:
	case SENSOR_HFR_120FPS_MODE:
		imx111_ctrl->prev_res = res;
		rc = imx111_video_config(mode);
		break;
	case SENSOR_SNAPSHOT_MODE:
		imx111_ctrl->pict_res = res;
		rc = imx111_snapshot_config(mode);
		break;
	case SENSOR_RAW_SNAPSHOT_MODE:
		imx111_ctrl->pict_res = res;
		rc = imx111_raw_snapshot_config(mode);
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int imx111_probe_init_done(const struct msm_camera_sensor_info *data)
{
	gpio_set_value_cansleep(data->sensor_reset, 0);
	gpio_direction_input(data->sensor_reset);
	gpio_free(data->sensor_reset);
	return 0;
}

static int imx111_probe_init_sensor(const struct msm_camera_sensor_info *data)
{
	int32_t rc = 0;

	pr_info("%s: Entered!\n", __func__);

	/* Reset */
	rc = gpio_request(data->sensor_reset, "imx111");
	if (!rc) {
		rc = gpio_direction_output(data->sensor_reset, 0);
		pr_info("%s: sensor_reset = %d, rc = %d\n", __func__, data->sensor_reset, rc);
		msleep(10);
		gpio_set_value_cansleep(data->sensor_reset, 1);
		msleep(10);
	} else {
		goto probe_init_sensor_fail;
	}

	pr_err(" imx111_probe_init_sensor finishes\n");
	return rc;

probe_init_sensor_fail:
	pr_err(" imx111_probe_init_sensor fails\n");
	imx111_probe_init_done(data);
	return rc;
}

static int imx111_sensor_open_init(const struct msm_camera_sensor_info *data)
{
	int32_t rc = 0;

	pr_info("%s: Entered!\n", __func__);

	imx111_ctrl->fps_divider = 1 * 0x00000400;
	imx111_ctrl->pict_fps_divider = 1 * 0x00000400;
	imx111_ctrl->fps = 30*Q8;
	imx111_ctrl->prev_res = QTR_SIZE;
	imx111_ctrl->pict_res = FULL_SIZE;
	imx111_ctrl->curr_res = INVALID_SIZE;

	if (data)
		imx111_ctrl->sensordata = data;

	/* Enable MCLK first */
	msm_camio_clk_rate_set(IMX111_MASTER_CLK_RATE);

	rc = imx111_probe_init_sensor(data);
	if (rc < 0)
		goto sensor_open_init_fail;

	pr_err("%s: init settings\n", __func__);
	rc = imx111_sensor_setting(REG_INIT, imx111_ctrl->prev_res);
	if (rc < 0)
		goto sensor_open_init_fail;

	//imx111_init_focus();

	pr_err("imx111_sensor_open_init done\n");
	return rc;

sensor_open_init_fail:
	pr_err("imx111_sensor_open_init fail\n");
	imx111_probe_init_done(data);
	return rc;
}

static int imx111_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&imx111_wait_queue);
	return 0;
}

static const struct i2c_device_id imx111_i2c_id[] = {
	{"imx111", 0},
	{ }
};

static int imx111_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	pr_err("imx111_i2c_probe called!\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		rc = -ENOTSUPP;
		pr_err("i2c_check_functionality failed\n");
		goto i2c_probe_failure;
	}

	imx111_sensorw = kzalloc(sizeof(struct imx111_work_t), GFP_KERNEL);
	if (!imx111_sensorw) {
		rc = -ENOMEM;
		pr_err("kzalloc failed.\n");
		goto i2c_probe_failure;
	}

	i2c_set_clientdata(client, imx111_sensorw);
	imx111_init_client(client);
	imx111_client = client;

	pr_err("imx111_i2c_probe successed! rc = %d\n", rc);
	return 0;

i2c_probe_failure:
	if( imx111_sensorw ) kfree(imx111_sensorw);
	imx111_sensorw = NULL;
	pr_err("imx111_i2c_probe failed! rc = %d\n", rc);
	return rc;
}

static int imx111_send_wb_info(struct wb_info_cfg *wb)
{
	return 0;
}

static int __exit imx111_remove(struct i2c_client *client)
{
	struct imx111_work_t_t *sensorw = i2c_get_clientdata(client);
	free_irq(client->irq, sensorw);
	imx111_client = NULL;
	kfree(sensorw);
	return 0;
}

static struct i2c_driver imx111_i2c_driver = {
	.id_table = imx111_i2c_id,
	.probe  = imx111_i2c_probe,
	.remove = __exit_p(imx111_i2c_remove),
	.driver = {
		.name = "imx111",
	},
};

static int imx111_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cdata;
	long   rc = 0;

	if (copy_from_user(&cdata,
		(void *)argp,
		sizeof(struct sensor_cfg_data)))
		return -EFAULT;

	mutex_lock(&imx111_mut);
	pr_err("%s: cfgtype = %d\n", __func__, cdata.cfgtype);

		switch (cdata.cfgtype) {
		case CFG_GET_PICT_FPS:
			imx111_get_pict_fps(
				cdata.cfg.gfps.prevfps,
				&(cdata.cfg.gfps.pictfps));

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_PREV_L_PF:
			cdata.cfg.prevl_pf =
			imx111_get_prev_lines_pf();

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_PREV_P_PL:
			cdata.cfg.prevp_pl =
				imx111_get_prev_pixels_pl();

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_PICT_L_PF:
			cdata.cfg.pictl_pf =
				imx111_get_pict_lines_pf();

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_PICT_P_PL:
			cdata.cfg.pictp_pl =
				imx111_get_pict_pixels_pl();

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_PICT_MAX_EXP_LC:
			cdata.cfg.pict_max_exp_lc =
				imx111_get_pict_max_exp_lc();

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_SET_FPS:
		case CFG_SET_PICT_FPS:
			rc = imx111_set_fps(&(cdata.cfg.fps));
			break;

		case CFG_SET_EXP_GAIN:
			rc =
				imx111_write_exp_gain(
					cdata.cfg.exp_gain.gain,
					cdata.cfg.exp_gain.line);
			break;

		case CFG_SET_PICT_EXP_GAIN:
			rc =
				imx111_set_pict_exp_gain(
				cdata.cfg.exp_gain.gain,
				cdata.cfg.exp_gain.line);
			break;

		case CFG_SET_MODE:
			rc = imx111_set_sensor_mode(cdata.mode,
					cdata.rs);
			break;

		case CFG_MOVE_FOCUS:
			rc =
				imx111_move_focus(
				cdata.cfg.focus.dir,
				cdata.cfg.focus.steps);
			break;

		case CFG_SET_DEFAULT_FOCUS:
			rc =
				imx111_set_default_focus(
				cdata.cfg.focus.steps);
			break;

		case CFG_GET_AF_MAX_STEPS:
			cdata.max_steps = imx111_linear_total_step;
			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_SET_EFFECT:
			rc = imx111_set_default_focus(
				cdata.cfg.effect);
			break;


		case CFG_SEND_WB_INFO:
			rc = imx111_send_wb_info(
				&(cdata.cfg.wb_info));
			break;

		default:
			rc = -EFAULT;
			break;
		}

	mutex_unlock(&imx111_mut);

	return rc;
}

static int imx111_sensor_release(void)
{
	int rc = -EBADF;

	mutex_lock(&imx111_mut);
	gpio_set_value_cansleep(imx111_ctrl->sensordata->sensor_reset, 0);
	msleep(5);
	gpio_free(imx111_ctrl->sensordata->sensor_reset);
	mutex_unlock(&imx111_mut);
	pr_err("%s: imx111_release completed\n", __func__);

	return rc;
}

static int imx111_sensor_probe(const struct msm_camera_sensor_info *info,
		struct msm_sensor_ctrl *s)
{
	int rc = 0;
	rc = i2c_add_driver(&imx111_i2c_driver);
	if (rc < 0 || imx111_client == NULL) {
		rc = -ENOTSUPP;
		pr_err("I2C add driver failed");
		goto probe_fail;
	}

	/* Set clock */
	msm_camio_clk_rate_set(IMX111_MASTER_CLK_RATE);

	/* Reset sensor and probe sensor registers thru I2C */
	rc = imx111_probe_init_sensor(info);
	if (rc < 0)
		goto probe_fail;

	s->s_init = imx111_sensor_open_init;
	s->s_release = imx111_sensor_release;
	s->s_config  = imx111_sensor_config;
	s->s_camera_type = BACK_CAMERA_2D;
	s->s_mount_angle = info->sensor_platform_info->mount_angle;

	imx111_probe_init_done(info);

	pr_info("imx111_sensor_probe : SUCCESS!\n");
	return rc;

probe_fail:
	pr_err("imx111_sensor_probe: SENSOR PROBE FAILS!\n");
	return rc;
}

static struct imx111_format imx111_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order  = 0,
	},
	/* more can be supported, to be added later */
};

static int imx111_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			   enum v4l2_mbus_pixelcode *code)
{
	printk(KERN_DEBUG "Index is %d\n", index);
	if ((unsigned int)index >= ARRAY_SIZE(imx111_subdev_info))
		return -EINVAL;

	*code = imx111_subdev_info[index].code;
	return 0;
}

static struct v4l2_subdev_core_ops imx111_subdev_core_ops;
static struct v4l2_subdev_video_ops imx111_subdev_video_ops = {
	.enum_mbus_fmt = imx111_enum_fmt,
};

static struct v4l2_subdev_ops imx111_subdev_ops = {
	.core = &imx111_subdev_core_ops,
	.video  = &imx111_subdev_video_ops,
};

static int imx111_sensor_probe_cb(const struct msm_camera_sensor_info *info,
	struct v4l2_subdev *sdev, struct msm_sensor_ctrl *s)
{
	int rc = 0;

	imx111_ctrl = kzalloc(sizeof(struct imx111_ctrl_t), GFP_KERNEL);
	if (!imx111_ctrl) {
		pr_err("%s: kzalloc failed!\n", __func__);
		return -ENOMEM;
	}

	rc = imx111_sensor_probe(info, s);
	if (rc < 0) {
		pr_err("%s: mt9m114_sensor_probe fail\n", __func__);
		return rc;
	}

	/* probe is successful, init a v4l2 subdevice */
	printk(KERN_DEBUG "going into v4l2_i2c_subdev_init\n");
	if (sdev) {
		v4l2_i2c_subdev_init(sdev, imx111_client,
						&imx111_subdev_ops);
		imx111_ctrl->sensor_dev = sdev;
	}
	return rc;
}

static int __imx111_probe(struct platform_device *pdev)
{
	pr_err("####### __imx111_probe()");
	return msm_sensor_register(pdev, imx111_sensor_probe_cb);
}

static struct platform_driver msm_camera_driver = {
	.probe = __imx111_probe,
	.driver = {
		.name = "msm_camera_imx111",
		.owner = THIS_MODULE,
	},
};

static int __init imx111_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}

module_init(imx111_init);
MODULE_DESCRIPTION("Aptina 8 MP Bayer sensor driver");
MODULE_LICENSE("GPL v2");

static bool streaming = 1;

static int imx111_set_af_codestep(void *data, u64 val)
{
	imx111_l_region_code_per_step = val;
	imx111_init_focus();
	return 0;
}

static int imx111_get_af_codestep(void *data, u64 *val)
{
	*val = imx111_l_region_code_per_step;
	return 0;
}

static int imx111_set_linear_total_step(void *data, u64 val)
{
	imx111_linear_total_step = val;
	return 0;
}

static int imx111_af_linearity_test(void *data, u64 *val)
{
	int i = 0;
	imx111_set_default_focus(0);
	msleep(3000);
	for (i = 0; i < imx111_linear_total_step; i++) {
		imx111_move_focus(MOVE_NEAR, 1);
		pr_err("__debug:MOVE_NEAR moved to index =[%d]\n", i);
	msleep(1000);
	}
	for (i = 0; i < imx111_linear_total_step; i++) {
		imx111_move_focus(MOVE_FAR, 1);
		CDBG("__debug:MOVE_FAR moved to index =[%d]\n", i);
		msleep(1000);
	}
	return 0;
}
static uint16_t imx111_step_jump = 4;
static uint8_t imx111_step_dir = MOVE_NEAR;
static int imx111_af_step_config(void *data, u64 val)
{
	imx111_step_jump = val & 0xFFFF;
	imx111_step_dir = (val >> 16) & 0x1;
	return 0;
}

static int imx111_af_step(void *data, u64 *val)
{
	int i = 0;
	int dir = MOVE_NEAR;
	imx111_set_default_focus(0);
	if (imx111_step_dir == 1)
			dir = MOVE_FAR;

	for (i = 1; i < IMX111_TOTAL_STEPS_NEAR_TO_FAR; i+=imx111_step_jump) {
		imx111_move_focus(dir, imx111_step_jump);
		msleep(1000);
	}
	imx111_set_default_focus(0);
	return 0;
}
static int imx111_af_set_slew(void *data, u64 val)
{
	imx111_vcm_step_time = val & 0xFFFF;
	return 0;
}

static int imx111_af_get_slew(void *data, u64 *val)
{
	*val = imx111_vcm_step_time;
	return 0;
}

static int imx111_set_sw_damping(void *data, u64 val)
{
	imx111_sw_damping_time_wait = val;
	return 0;
}

static int imx111_get_sw_damping(void *data, u64 *val)
{
	*val = imx111_sw_damping_time_wait;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(af_damping, imx111_get_sw_damping,
			imx111_set_sw_damping, "%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(af_codeperstep, imx111_get_af_codestep,
	imx111_set_af_codestep, "%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(af_linear, imx111_af_linearity_test,
	imx111_set_linear_total_step, "%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(af_step, imx111_af_step,
	imx111_af_step_config, "%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(af_slew, imx111_af_get_slew,
	imx111_af_set_slew, "%llu\n");

static int cam_debug_stream_set(void *data, u64 val)
{
	int rc = 0;

	if (val) {
		imx111_start_stream();
		streaming = 1;
	} else {
		imx111_stop_stream();
		streaming = 0;
	}

	return rc;
}

static int cam_debug_stream_get(void *data, u64 *val)
{
	*val = streaming;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(cam_stream, cam_debug_stream_get,
			cam_debug_stream_set, "%llu\n");


static int cam_debug_init(void)
{
	struct dentry *cam_dir;
	debugfs_base = debugfs_create_dir("sensor", NULL);
	if (!debugfs_base)
		return -ENOMEM;

	cam_dir = debugfs_create_dir("imx111", debugfs_base);
	if (!cam_dir)
		return -ENOMEM;
	if (!debugfs_create_file("af_codeperstep", S_IRUGO | S_IWUSR, cam_dir,
			NULL, &af_codeperstep))
			return -ENOMEM;
	if (!debugfs_create_file("af_linear", S_IRUGO | S_IWUSR, cam_dir,
			NULL, &af_linear))
		return -ENOMEM;
	if (!debugfs_create_file("af_step", S_IRUGO | S_IWUSR, cam_dir,
			NULL, &af_step))
		return -ENOMEM;
	if (!debugfs_create_file("af_slew", S_IRUGO | S_IWUSR, cam_dir,
			NULL, &af_slew))
		return -ENOMEM;
	if (!debugfs_create_file("af_damping", S_IRUGO | S_IWUSR, cam_dir,
			NULL, &af_damping))
		return -ENOMEM;
	if (!debugfs_create_file("stream", S_IRUGO | S_IWUSR, cam_dir,
							 NULL, &cam_stream))
		return -ENOMEM;

	return 0;
}

