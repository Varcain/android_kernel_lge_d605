/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <linux/module.h>
/*                                                    */
#include <mach/board_lge.h>
#include "msm_sensor.h"
#define SENSOR_NAME "hi543"
#define PLATFORM_DRIVER_NAME "msm_camera_hi543"
#define hi543_obj hi543_##obj
#define MSB                             1
#define LSB                             0

DEFINE_MUTEX(hi543_mut);

/*                                                      */
extern void subpm_set_gpio(int onoff, int type);
extern int32_t msm_sensor_enable_i2c_mux(struct msm_camera_i2c_conf *i2c_conf);
extern int32_t msm_sensor_disable_i2c_mux(struct msm_camera_i2c_conf *i2c_conf);

extern unsigned int system_rev;
/*                                                      */

static struct msm_sensor_ctrl_t hi543_s_ctrl;

static struct msm_camera_i2c_reg_conf hi543_start_settings[] = {
	{0x6010, 0x03}, 
	{0x5201, 0x02}, 
	{0x0100, 0x01}, 
	//{0x0100, 0x01},  
	//{0x0100, 0x01},  
	{0x0100, 0x00}, 
	{0x5201, 0x00}, 
	{0x6010, 0x02}, 
	{0x0100, 0x01}, 
	//{0x6012, 0x00}, 
};

/*                                                                                                */
static struct msm_camera_i2c_reg_conf hi543_entrance_start_settings[] = {
	{0x6010, 0x03}, 
	{0x5201, 0x02}, 
//	{0x6012, 0x08},
	{0x0100, 0x01}, 
	{0x0100, 0x01},  
	{0x0100, 0x00}, 
	{0x5201, 0x00}, 
	{0x6010, 0x02}, 
	{0x0100, 0x01}, 
	{0x6012, 0x00}, 	
};
/*                                                                                                */


static struct msm_camera_i2c_reg_conf hi543_stop_settings[] = {
	{0x0100, 0x00},
};

static struct msm_camera_i2c_reg_conf hi543_groupon_settings[] = {

};

static struct msm_camera_i2c_reg_conf hi543_groupoff_settings[] = {

};

//[non zsl]
static struct msm_camera_i2c_reg_conf hi543_prev_settings[] = {
	#if 0
	{0x0383, 0x03}, // x_odd_inc_l sub_1/2(0x03), sub_1/4(0x07), sub_1/8(0x0f)
	{0x0387, 0x03}, // y_odd_inc_l sub_1/2(0x03), sub_1/4(0x07), sub_1/8(0x0f)
	{0x0344, 0x00}, // x_start_addr
	{0x0345, 0x1a},
	{0x0346, 0x00}, // y_start_addr
	{0x0347, 0x1e}, 
	{0x0348, 0x0a}, // x_start_end
	{0x0349, 0x1d},
	{0x034a, 0x07}, // y_start_end
	{0x034b, 0xa1}, 
	{0x034C, 0x05}, // x_output_size_h  (width = 1280)
	{0x034D, 0x00}, // x_output_size_l   
	{0x034E, 0x03}, // y_output_size_h  (height = 960)
	{0x034F, 0xc0}, // y_output_size_l
	#else
	{0x0344, 0x00}, //X_addr_start_h
	{0x0345, 0x18}, //X_addr_start_l 24
	{0x0346, 0x00}, //Y_addr_start_h
	{0x0347, 0x1C}, //Y_addr_start_l 28
	{0x0348, 0x0A}, //X_addr_end_h
	{0x0349, 0x1F}, //X_addr_end_l 2591
	{0x034A, 0x07}, //Y_addr_end_h
	{0x034B, 0xA3}, //Y_addr_end_l 1955
	{0x034C, 0x05}, //X_output_size_h
	{0x034D, 0x00}, //X_output_size_l 1280
	{0x034E, 0x03}, //Y_output_size_h
	{0x034F, 0xC0}, //Y_output_size_l 960

	/*                                                            */
#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS)
	{0x4013, 0x07},
#else
	{0x4013, 0x03},
#endif
	/*                                                            */

//	{0x0380, 0x00}, 
//	{0x0381, 0x01}, 
//	{0x0382, 0x00}, 
	{0x0383, 0x03}, 
//	{0x0384, 0x00}, 
//	{0x0385, 0x01}, 
//	{0x0386, 0x00}, 
	{0x0387, 0x03}, 
//	{0x5000, 0x09}, 
//	{0x2300, 0x00}, 
//	{0xf010, 0x3f}, 
//	{0x6011, 0x0c}, 
//	{0x6010, 0x02}, 
//	{0x0100, 0x01}, 
	#endif
};

//[zsl / snasphot]
static struct msm_camera_i2c_reg_conf hi543_snap_settings[] = {
	#if 0
	{0x0383,0x01},	//no subsampling
	{0x0387,0x01},	//no subsampling
	{0x0344,0x00}, // x_start_addr
	{0x0345,0x1a},
	{0x0346,0x00}, // y_start_addr
	{0x0347,0x1e}, 
	{0x0348,0x0a}, // x_start_end
	{0x0349,0x1d},
	{0x034a,0x07}, // y_start_end
	{0x034b,0xa1}, 
	#else
	{0x0344, 0x00}, //X_addr_start_h
	{0x0345, 0x1A}, //X_addr_start_l 26
	{0x0346, 0x00}, //Y_addr_start_h
	{0x0347, 0x1E}, //Y_addr_start_l 30
	{0x0348, 0x0A}, //X_addr_end_h
	{0x0349, 0x1D}, //X_addr_end_l 2589
	{0x034A, 0x07}, //Y_addr_end_h
	{0x034B, 0xA1}, //Y_addr_end_l 1953
	{0x034C, 0x0A}, //X_output_size_h
	{0x034D, 0x00}, //X_output_size_l 2560
	{0x034E, 0x07}, //Y_output_size_h
	{0x034F, 0x80}, //Y_output_size_l 1920

	/*                                                            */
	{0x4013, 0x03},
	/*                                                            */

//	{0x0380, 0x00}, 
//	{0x0381, 0x01}, 
//	{0x0382, 0x00}, 
	{0x0383, 0x01}, 
//	{0x0384, 0x00}, 
//	{0x0385, 0x01}, 
//	{0x0386, 0x00}, 
	{0x0387, 0x01}, 
//	{0x5000, 0x09}, 
//	{0x2300, 0x00}, 
//	{0xf010, 0x3f}, 
//	{0x6011, 0x0c}, 
//	{0x6010, 0x02}, 
//	{0x0100, 0x01}, 

	#endif
};

//[FHD]
static struct msm_camera_i2c_reg_conf hi543_video_settings[] = {
	#if 0
//	{0x0100,0x00}, 
//	{0x6010,0x03},
//	{0x0100,0x01}, 
//	{0x0100,0x00}, 
	{0x0383,0x01},	//no subsampling
	{0x0387,0x01},	//no subsampling
	{0x0344,0x01}, // x_start_addr
	{0x0345,0x52},
	{0x0346,0x01}, // y_start_addr
	{0x0347,0xbc}, 
	{0x0348,0x08}, // x_start_end
	{0x0349,0xe1},
	{0x034a,0x06}, // y_start_end
	{0x034b,0x03},	//0601 +16
	//{0x034c,0x07}, // x_output_size	1932
	//{0x034d,0x8c}, 
	//{0x034e,0x04}, // y_output_size	1092
	//{0x034f,0x44},
//	{0x6010,0x02}, 
//	{0x0100,0x01}, 
	#else 
	{0x0344, 0x01}, //X_addr_start_h
	{0x0345, 0x52}, //X_addr_start_l 338
	{0x0346, 0x01}, //Y_addr_start_h
	{0x0347, 0xBE}, //Y_addr_start_l 446
	{0x0348, 0x08}, //X_addr_end_h
	{0x0349, 0xE5}, //X_addr_end_l 2277
	{0x034A, 0x06}, //Y_addr_end_h
	{0x034B, 0x01}, //Y_addr_end_l 1537
	{0x034C, 0x07}, //X_output_size_h
	{0x034D, 0x90}, //X_output_size_l 1936
	{0x034E, 0x04}, //Y_output_size_h
	{0x034F, 0x40}, //Y_output_size_l 1088

	/*                                                            */
#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS)
	{0x4013, 0x07},
#else
	{0x4013, 0x03},
#endif
	/*                                                            */

//	{0x0380, 0x00}, 
//	{0x0381, 0x01}, 
//	{0x0382, 0x00}, 
	{0x0383, 0x01}, 
//	{0x0384, 0x00}, 
//	{0x0385, 0x01}, 
//	{0x0386, 0x00}, 
	{0x0387, 0x01}, 
//	{0x5000, 0x09}, 
//	{0x2300, 0x00}, 
//	{0xf010, 0x3f}, 
//	{0x6011, 0x0c}, 
//	{0x6010, 0x02}, 
//	{0x0100, 0x01},

	#endif
};


//init setting
static struct msm_camera_i2c_reg_conf hi543_recommend_settings[] = {
/*                                                                              */
//start// 
	{0x0100, 0x00}, 
	{0x0100, 0x00}, 
	{0x0100, 0x00}, 
	{0x0103, 0x01}, 
	{0x0103, 0x00}, 
	{0x0100, 0x00},  
	{0xE048, 0x28}, 
	{0x0100, 0x00}, 
	{0x0100, 0x00}, 
	{0x0100, 0x00}, 
	{0x0106, 0x01}, 
#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS) 
	{0x0101, 0x00}, /*                                                                               */
#else
      {0x0101, 0x03}, //20130118 X-Y Flip 'on' for ADPC
#endif
	{0xf003, 0x3a}, 
	{0x0304, 0x00}, 
	{0x0305, 0x18}, 
	{0x0306, 0x03}, 
	{0x0307, 0x50}, 
	{0x5400, 0x04}, 
	{0x0b04, 0x01}, 
	{0xe039, 0x0F}, 
	{0xe04e, 0x22}, 
	{0xe051, 0x22}, 
	{0xe053, 0xFF}, 
	{0xe041, 0x2B}, 
	{0xe042, 0xF9}, 
	{0xe043, 0x7F}, 
	{0xe044, 0xFF}, 
	{0xe0a1, 0x03}, 
	{0xe0a2, 0x84}, //80}, 
	{0xe0ae, 0x08}, 
	{0xe054, 0x55}, 
	{0xe04b, 0x03}, 
	{0xe045, 0x7F}, 
	{0xe047, 0x00}, 
	{0xe196, 0x00}, 
	{0xe197, 0xD0}, 
	{0xe198, 0x00}, 
	{0xe199, 0xDF}, 
	{0xe19a, 0x00}, 
	{0xe19b, 0xD0}, 
	{0xe19c, 0x00}, 
	{0xe19d, 0xD1}, 
	{0xe186, 0x00}, 
	{0xe187, 0x40}, 
	{0xe188, 0x00}, 
	{0xe189, 0xEA}, 
	{0xe162, 0x20}, 
	{0xe163, 0x2D}, 
	{0xe14d, 0x7C}, 
	{0xe17a, 0x08}, 
	{0xe17d, 0x48}, 
	{0xe16e, 0x01}, 
	{0xe16f, 0xC2}, 
	{0xe170, 0x02}, 
	{0xe171, 0x0C}, 
	{0xe132, 0x01}, 
	{0xe133, 0xC2}, 
	{0xe138, 0x01}, 
	{0xe139, 0xC2}, 
	{0xe104, 0x01}, 
	{0xe105, 0xC2}, 
	{0xe10c, 0x01}, 
	{0xe10d, 0xBC}, 
	{0xe18a, 0x01}, 
	{0xe18b, 0x90}, 
	{0xe18c, 0x02}, 
	{0xe18d, 0x6c}, 
	{0xe18e, 0x00}, 
	{0xe18f, 0x08}, 
	{0xe190, 0x01}, 
	{0xe191, 0x50}, 
	{0xe05d, 0xff}, 
	{0xe05a, 0xbb}, 
	{0xe05b, 0xcc}, 
	{0x0205, 0xC3}, 
	{0xe0a0, 0x00}, //04}, 
	{0xe0b4, 0x09}, 
	{0xe0b5, 0x09}, 
	{0xe0b6, 0x09}, 
	{0xe0b7, 0x09}, 

#if 0	
	{0x0200, 0x05}, 
	{0x0201, 0x2f}, 
#else
	/*                                        */
	{0x0200, 0x01},    //0x05
	{0x0201, 0x34}, 	// 0x2f
#endif
	{0x0202, 0x07}, 
	{0x0203, 0xaf}, 
	{0x0008, 0x00}, 
	{0x0009, 0x00}, 
	{0xE00D, 0x88}, //08},2013.02.13 LHC 
	{0xE1BE, 0x05}, 
	{0xE1BF, 0x6A}, 
	{0xE001, 0x4A},
	{0xE002, 0x08}, 
	{0x4042, 0x07}, 
	{0x4043, 0x07}, 
	{0x4044, 0x07}, 
	{0x4013, 0x03}, 
	{0x4014, 0x03}, 
	{0x4015, 0x06}, 
	{0x4011, 0x00}, 
	{0x4010, 0x13}, 
	{0x7040, 0x59}, 
	{0x7047, 0x15}, 
	{0x5217, 0x0e}, // 20130118 ADPC Y offset 
	{0x5218, 0x06}, // 20130118 ADPC X offset
	
	/*                                                                      */
#if 0			   /*                                                      */
	{0x0340, 0x07}, //Frame_length_lines_h
	{0x0341, 0xC3}, //Frame_length_lines_l 1987
#else
	{0x0340, 0x07}, //Frame_length_lines_h
	{0x0341, 0xe5}, //Frame_length_lines_l 1987
#endif
	/*                                                                      */

	{0x0342, 0x0B}, //Line_length_pck_h
	{0x0343, 0x1C}, //Line_length_pck_l 2844
	{0x0344, 0x00}, //X_addr_start_h
	{0x0345, 0x1A}, //X_addr_start_l 26
	{0x0346, 0x00}, //Y_addr_start_h
	{0x0347, 0x1E}, //Y_addr_start_l 30
	{0x0348, 0x0A}, //X_addr_end_h
	{0x0349, 0x1D}, //X_addr_end_l 2589
	{0x034A, 0x07}, //Y_addr_end_h
	{0x034B, 0xA1}, //Y_addr_end_l 1953
	{0x034C, 0x0A}, //X_output_size_h
	{0x034D, 0x00}, //X_output_size_l 2560
	{0x034E, 0x07}, //Y_output_size_h
	{0x034F, 0x80}, //Y_output_size_l 1920
	{0x0380, 0x00}, 
	{0x0381, 0x01}, 
	{0x0382, 0x00}, 
	{0x0383, 0x01}, 
	{0x0384, 0x00}, 
	{0x0385, 0x01}, 
	{0x0386, 0x00}, 
	{0x0387, 0x01}, 
	{0x5000, 0x0d}, //09}, //Dgain enable
	{0x1084, 0x01}, //Min Dgain
	{0x1085, 0x00},
	{0x1086, 0x02}, //Max Dgain
	{0x1087, 0x00},
	{0x2300, 0x00}, 
	{0xf010, 0x3f},	
	{0x6011, 0x0c}, //                                                                                                            
	{0x6013, 0x30}, //33},
	{0x0801, 0x37},
	{0x0805, 0x37},
	{0x0806, 0xff},
	{0x0807, 0x40},
	{0x0800, 0x7f},
	{0x0804, 0x3f},
	{0x0802, 0x53},
	{0x0803, 0x48},
	{0x6012, 0x08},	

/*                                                                           */
};

static struct v4l2_subdev_info hi543_subdev_info[] = {
	{
#ifdef CONFIG_MACH_LGE
	.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
#else
	.code   = V4L2_MBUS_FMT_SGRBG10_1X10,
#endif
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_i2c_conf_array hi543_init_conf[] = {
	{&hi543_recommend_settings[0],
	ARRAY_SIZE(hi543_recommend_settings), 0, MSM_CAMERA_I2C_BYTE_DATA}
};

static struct msm_camera_i2c_conf_array hi543_confs[] = {
	{&hi543_snap_settings[0],
	ARRAY_SIZE(hi543_snap_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&hi543_prev_settings[0],
	ARRAY_SIZE(hi543_prev_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&hi543_video_settings[0],
	ARRAY_SIZE(hi543_video_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_sensor_output_info_t hi543_dimensions[] = {
	#if 0
	/*                                                                 */
	{
		.x_output = 0x0A00,	//2560
		.y_output = 0x0780,	//1920
		.line_length_pclk = 0x0B1C,
		.frame_length_lines = 0x07C3, //0x7A2, //0x7A8, //0x07B2,
		.vt_pixel_clk = 169600000,//169600000,
		.op_pixel_clk = 169600000,//169600000,
		.binning_factor = 1,
	}, 
	/*                                                                                 */
	{
		.x_output = 0x0500,		//1280
		.y_output = 0x03C0,		//960
		.line_length_pclk = 0x0B1C,
		.frame_length_lines = 0x7C3, //0x3E2,
		.vt_pixel_clk = 169600000,
		.op_pixel_clk = 169600000,
		.binning_factor = 1,
	},
	/*                                                                 */
	{	
		.x_output = 0x0790,	//1936
		.y_output = 0x0448, //1096 no mulitple of 16
		.line_length_pclk = 0x0B1C,
		.frame_length_lines = 0x07C3, //0x79B,//0x7A8, //0x07B2,
		.vt_pixel_clk = 169600000,
		.op_pixel_clk = 169600000,
		.binning_factor = 1,
	},
	#else

	/*                                                                 */
	/*                                                                      */
	{
		.x_output = 0x0A00,	//2560
		.y_output = 0x0780,	//1920
		.line_length_pclk = 0x0B1C,
		.frame_length_lines = 0x7e5, //                                                                                      
		.vt_pixel_clk = 169600000,//169600000,
		.op_pixel_clk = 169600000,//169600000,
		.binning_factor = 1,
	},

	/*                                                                                 */
	{
		.x_output = 0x0500,		//1280
		.y_output = 0x03C0,		//960
		.line_length_pclk = 0x0B1C,
		.frame_length_lines = 0x7e5, //                                                                                      
		.vt_pixel_clk = 169600000,
		.op_pixel_clk = 169600000,
		.binning_factor = 1,
	},
	/*                                                                 */
	{	
		.x_output = 0x0790,//0x0790, //1936
		.y_output = 0x0440,//0x0448, //1096 no mulitple of 16
		.line_length_pclk = 0x0B1C,
		.frame_length_lines = 0x7e5, //                                                                                      
		.vt_pixel_clk = 169600000,
		.op_pixel_clk = 169600000,
		.binning_factor = 1,
	},
	/*                                                                      */
	#endif
};

static struct msm_sensor_output_reg_addr_t hi543_reg_addr = {
	.x_output = 0x034C,
	.y_output = 0x034E,
	.line_length_pclk = 0x0342,
	.frame_length_lines = 0x0340,
};

static struct msm_sensor_id_info_t hi543_id_info = {
	.sensor_id_reg_addr = 0x0000,
	.sensor_id = 0x1F3C,
};

static enum msm_camera_vreg_name_t hi543_veg_seq[] = {
	CAM_VIO,
	CAM_VANA,
	CAM_VDIG,
//	CAM_VAF,
};


/*                                */
#if 1
/*                                                                 */
static struct msm_sensor_exp_gain_info_t hi543_exp_gain_info = {
	.coarse_int_time_addr = 0x0202,
	.global_gain_addr = 0x0204,	//0x0205
	.digital_gain_addr_gr = 0x020e,		
	.digital_gain_addr_r = 0x0210,
	.digital_gain_addr_gb = 0x0212,
	.digital_gain_addr_b = 0x0214,
	.vert_offset = 4,
};
/*                                                                 */

static inline uint8_t hi543_byte(uint16_t word, uint8_t offset)
{
	return word >> (offset * BITS_PER_BYTE);
}

/*                                                                 */
static int32_t hi543_sensor_write_exp_gain1(struct msm_sensor_ctrl_t *s_ctrl,
						uint16_t gain, uint32_t line)
{
        uint32_t fl_lines;
/*                                                                              */      
        uint16_t fl_lines_tmp[2];
        uint16_t ex_lines_tmp[2];
        uint16_t nDigitaGain = 0;
	 
//        uint8_t offset;
        uint8_t Again; 

        fl_lines = s_ctrl->curr_frame_length_lines;
        fl_lines = (fl_lines * s_ctrl->fps_divider) / Q10;	/*                                                                             */
//        offset = s_ctrl->sensor_exp_gain_info->vert_offset;

        msm_camera_i2c_read(s_ctrl->sensor_i2c_client, s_ctrl->sensor_output_reg_addr->frame_length_lines,&fl_lines_tmp[0], MSM_CAMERA_I2C_BYTE_DATA);
        msm_camera_i2c_read(s_ctrl->sensor_i2c_client, s_ctrl->sensor_output_reg_addr->frame_length_lines + 1,&fl_lines_tmp[1], MSM_CAMERA_I2C_BYTE_DATA);

        msm_camera_i2c_read(s_ctrl->sensor_i2c_client, s_ctrl->sensor_exp_gain_info->coarse_int_time_addr,&ex_lines_tmp[0], MSM_CAMERA_I2C_BYTE_DATA);
        msm_camera_i2c_read(s_ctrl->sensor_i2c_client, s_ctrl->sensor_exp_gain_info->coarse_int_time_addr + 1,&ex_lines_tmp[1], MSM_CAMERA_I2C_BYTE_DATA);

		fl_lines_tmp[1] = fl_lines_tmp[0] << 8 | fl_lines_tmp[1];
		ex_lines_tmp[1] = ex_lines_tmp[0] << 8 | ex_lines_tmp[1];

//		pr_err("%s read %d\n", __func__, fl_lines_tmp[1]);

        if(line < (fl_lines -6))
			fl_lines_tmp[1] = fl_lines + ((0x0001 & fl_lines)?(0x0001 & ~ ((ex_lines_tmp[1] ^ line))) : (0x0001 & ((ex_lines_tmp[1] ^ line))));
        else
			fl_lines_tmp[1] = line + 8 + (0x0001 &(line+8)?(0x0001 & ~ ((ex_lines_tmp[1] ^ line))) : (0x0001 & ((ex_lines_tmp[1] ^ line))));

        fl_lines = fl_lines_tmp[1];

	//	pr_err("%s write %d\n", __func__, fl_lines_tmp[1]);
/*                                                                              */
	
	if ((gain >> 8) == 0){
		Again = 0x00;
		nDigitaGain = gain | 0x0100;
	}
	else{
		Again = (gain >> 8) & 0xFF;
		nDigitaGain = 0x0100;
	} 
		
	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
	
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
	s_ctrl->sensor_output_reg_addr->frame_length_lines, hi543_byte(fl_lines, 1),
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
	s_ctrl->sensor_output_reg_addr->frame_length_lines + 1, hi543_byte(fl_lines, 0),
		MSM_CAMERA_I2C_BYTE_DATA);
	
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
	s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, hi543_byte(line, 1),
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
	s_ctrl->sensor_exp_gain_info->coarse_int_time_addr + 1, hi543_byte(line, 0),
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
	s_ctrl->sensor_exp_gain_info->global_gain_addr, hi543_byte(Again, 1),
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
	s_ctrl->sensor_exp_gain_info->global_gain_addr + 1, hi543_byte(Again, 0),
		MSM_CAMERA_I2C_BYTE_DATA);

	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
	s_ctrl->sensor_exp_gain_info->digital_gain_addr_r, hi543_byte(nDigitaGain,1),
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
	s_ctrl->sensor_exp_gain_info->digital_gain_addr_r + 1,hi543_byte(nDigitaGain,0), 
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
	s_ctrl->sensor_exp_gain_info->digital_gain_addr_gb, hi543_byte(nDigitaGain,1),
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
	s_ctrl->sensor_exp_gain_info->digital_gain_addr_gb + 1, hi543_byte(nDigitaGain,0),
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
	s_ctrl->sensor_exp_gain_info->digital_gain_addr_gr,hi543_byte(nDigitaGain,1),
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
	s_ctrl->sensor_exp_gain_info->digital_gain_addr_gr + 1, hi543_byte(nDigitaGain,0),
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
	s_ctrl->sensor_exp_gain_info->digital_gain_addr_b, hi543_byte(nDigitaGain,1),
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
	s_ctrl->sensor_exp_gain_info->digital_gain_addr_b + 1,hi543_byte(nDigitaGain,0),
		MSM_CAMERA_I2C_BYTE_DATA);	
	
	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	return 0;
}
/*                                                                 */


#if 0
static int32_t hi543_write_pict_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{
	uint16_t max_legal_gain = 0x0200;
	uint16_t min_ll_pck = 0x0AB2;
	uint32_t ll_pck, fl_lines;
	uint32_t ll_ratio;
	uint8_t gain_msb, gain_lsb;
	uint8_t intg_time_msb, intg_time_lsb;
	uint8_t ll_pck_msb, ll_pck_lsb;

	if (gain > max_legal_gain) {
		CDBG("Max legal gain Line:%d\n", __LINE__);
		gain = max_legal_gain;
	}

	pr_info("hi543_write_exp_gain : gain = %d line = %d\n", gain, line);
	line = (uint32_t) (line * s_ctrl->fps_divider);
	fl_lines = s_ctrl->curr_frame_length_lines * s_ctrl->fps_divider / Q10;
	ll_pck = s_ctrl->curr_line_length_pclk;

	if (fl_lines < (line / Q10))
		ll_ratio = (line / (fl_lines - 4));
	else
		ll_ratio = Q10;

	ll_pck = ll_pck * ll_ratio / Q10;
	line = line / ll_ratio;
	if (ll_pck < min_ll_pck)
		ll_pck = min_ll_pck;

	gain_msb = (uint8_t) ((gain & 0xFF00) >> 8);
	gain_lsb = (uint8_t) (gain & 0x00FF);

	intg_time_msb = (uint8_t) ((line & 0xFF00) >> 8);
	intg_time_lsb = (uint8_t) (line & 0x00FF);

	ll_pck_msb = (uint8_t) ((ll_pck & 0xFF00) >> 8);
	ll_pck_lsb = (uint8_t) (ll_pck & 0x00FF);

	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr,
		gain_msb,
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr + 1,
		gain_lsb,
		MSM_CAMERA_I2C_BYTE_DATA);

	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->line_length_pclk,
		ll_pck_msb,
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->line_length_pclk + 1,
		ll_pck_lsb,
		MSM_CAMERA_I2C_BYTE_DATA);

	/* Coarse Integration Time */
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr,
		intg_time_msb,
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr + 1,
		intg_time_lsb,
		MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);

	return 0;
}
#endif
#endif

static int32_t hi543_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid[2];
	rc = msm_camera_i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_id_info->sensor_id_reg_addr, &chipid[0],
			MSM_CAMERA_I2C_BYTE_DATA);

		rc = msm_camera_i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_id_info->sensor_id_reg_addr | 0x0001, &chipid[1],
			MSM_CAMERA_I2C_BYTE_DATA);
	
	if (rc < 0) {
		pr_err("%s: %s: read id failed\n", __func__,
			s_ctrl->sensordata->sensor_name);
		return rc;
	}

	chipid[1] = chipid[0] << 8 | chipid[1];

	pr_err("hi543_sensor_match_id : %d\n", chipid[1]);
	if (chipid[1] != s_ctrl->sensor_id_info->sensor_id) {
		pr_err("msm_sensor_match_id chip id doesnot match\n");
		return -ENODEV;
	}
	return rc;
}


/*                                                    */
//delay * 1000
static struct msm_cam_clk_info cam_clk_info[] = {
	{"cam_clk", MSM_SENSOR_MCLK_24HZ, 12},
};

int32_t HI543_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	CDBG("%s: %d\n", __func__, __LINE__);
	pr_err("%s: E: %s\n", __func__, data->sensor_name); /*                                                              */
	s_ctrl->reg_ptr = kzalloc(sizeof(struct regulator *)
			* data->sensor_platform_info->num_vreg, GFP_KERNEL);
	if (!s_ctrl->reg_ptr) {
		pr_err("%s: could not allocate mem for regulators\n",	__func__);
		return -ENOMEM;
	}		
	rc = msm_camera_request_gpio_table(data, 1);
	if (rc < 0) {
		pr_err("%s: request gpio failed\n", __func__);
		goto request_gpio_failed;
	}

	rc = msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		s_ctrl->vreg_seq,
		s_ctrl->num_vreg_seq,
		s_ctrl->reg_ptr, 1);
	if (rc < 0) {
		pr_err("%s: regulator on failed\n", __func__);
		goto config_vreg_failed;
	}

	//[IOVDD] [AVDD] [DVDD]
	rc = msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		s_ctrl->vreg_seq,
		s_ctrl->num_vreg_seq,
		s_ctrl->reg_ptr, 1);
	if (rc < 0) {
		pr_err("%s: enable regulator failed\n", __func__);
		goto enable_vreg_failed;
	}

	//SKIP
	rc = msm_camera_config_gpio_table(data, 1);
	if (rc < 0) {
		pr_err("%s: config gpio failed\n", __func__);
		goto config_gpio_failed;
	}

	
	if (s_ctrl->clk_rate != 0)
		cam_clk_info->clk_rate = s_ctrl->clk_rate;


	//[CLK]
	rc = msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 1);
	if (rc < 0) {
		pr_err("%s: clk enable failed\n", __func__);
		goto enable_clk_failed;
	}
	
	pr_err("[clk : %ld]\n", s_ctrl->clk_rate);	/*                                                               */
	

	//[XSHUTDOWN]
	if (data->sensor_platform_info->ext_power_ctrl != NULL)
		data->sensor_platform_info->ext_power_ctrl(1);

	//[I2C]
	if (data->sensor_platform_info->i2c_conf &&
		data->sensor_platform_info->i2c_conf->use_i2c_mux)
		msm_sensor_enable_i2c_mux(data->sensor_platform_info->i2c_conf);
	
	pr_err("%s: X\n", __func__); /*                                                              */

	return rc;

enable_clk_failed:
		msm_camera_config_gpio_table(data, 0);
config_gpio_failed:
	msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			s_ctrl->vreg_seq,
			s_ctrl->num_vreg_seq,
			s_ctrl->reg_ptr, 0);

enable_vreg_failed:
	msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		s_ctrl->vreg_seq,
		s_ctrl->num_vreg_seq,
		s_ctrl->reg_ptr, 0);
config_vreg_failed:
	msm_camera_request_gpio_table(data, 0);
request_gpio_failed:
	kfree(s_ctrl->reg_ptr);

	return rc;
}


int32_t HI543_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	CDBG("%s\n", __func__);
	pr_err("%s: E: %s\n", __func__, data->sensor_name); /*                                                              */

	//[i2c]
	if (data->sensor_platform_info->i2c_conf &&
		data->sensor_platform_info->i2c_conf->use_i2c_mux)
		msm_sensor_disable_i2c_mux(
			data->sensor_platform_info->i2c_conf);

	//[XSHUTDOWN]
	if (data->sensor_platform_info->ext_power_ctrl != NULL)
		data->sensor_platform_info->ext_power_ctrl(0);
	
	msleep(15); /*                                                               */
	
	msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 0);
	msm_camera_config_gpio_table(data, 0);

	//[IOVDD, AVDD, DVDD]
	msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			s_ctrl->vreg_seq,
			s_ctrl->num_vreg_seq,
			s_ctrl->reg_ptr, 0);
	
	msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			s_ctrl->vreg_seq,
			s_ctrl->num_vreg_seq,
			s_ctrl->reg_ptr, 0);

	//[SKIP]
	msm_camera_request_gpio_table(data, 0);
	kfree(s_ctrl->reg_ptr);

	/*                                                                                                */
	s_ctrl->hi543Initcheck = 1;
	pr_err("%s %d\n", __func__, s_ctrl->hi543Initcheck);
	/*                                                                                                */
	
	pr_err("%s: X\n", __func__); /*                                                              */

	return 0;
}
/*                                                      */


int32_t hi543_sensor_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	struct msm_camera_sensor_info *s_info;

	pr_err("jisun.no - hi543 device driver probing !!!");

	rc = msm_sensor_i2c_probe(client, id);

	s_info = client->dev.platform_data;
	if (s_info == NULL) {
		pr_err("%s %s NULL sensor data\n", __func__, client->name);
		return -EFAULT;
	}

	if (s_info->actuator_info->vcm_enable) {
		rc = gpio_request(s_info->actuator_info->vcm_pwd,
				"msm_actuator");
		if (rc < 0)
			pr_err("%s: gpio_request:msm_actuator %d failed\n",
				__func__, s_info->actuator_info->vcm_pwd);
		rc = gpio_direction_output(s_info->actuator_info->vcm_pwd, 0);
		if (rc < 0)
			pr_err("%s: gpio:msm_actuator %d direction can't be set\n",
				__func__, s_info->actuator_info->vcm_pwd);
		gpio_free(s_info->actuator_info->vcm_pwd);
	}

	return rc;
}

static const struct i2c_device_id hi543_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&hi543_s_ctrl},
	{ }
};

static struct i2c_driver hi543_i2c_driver = {
	.id_table = hi543_i2c_id,
	.probe  = hi543_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client hi543_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int __init msm_sensor_init_module(void)
{
	pr_err("- hi543 module init !!!");
	return i2c_add_driver(&hi543_i2c_driver);
}

static struct v4l2_subdev_core_ops hi543_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops hi543_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops hi543_subdev_ops = {
	.core = &hi543_subdev_core_ops,
	.video  = &hi543_subdev_video_ops,
};

static struct msm_sensor_fn_t hi543_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = msm_sensor_set_fps,
	.sensor_write_exp_gain = hi543_sensor_write_exp_gain1,	//                             
	.sensor_write_snapshot_exp_gain = hi543_sensor_write_exp_gain1,	//                             
	.sensor_setting = msm_sensor_setting,
	.sensor_csi_setting = msm_sensor_setting1,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = HI543_sensor_power_up, //                                                      
	.sensor_power_down = HI543_sensor_power_down, //                                                       
	.sensor_adjust_frame_lines = msm_sensor_adjust_frame_lines1,
	.sensor_get_csi_params = msm_sensor_get_csi_params,
	.sensor_match_id = hi543_sensor_match_id,				//                             
};

static struct msm_sensor_reg_t hi543_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = hi543_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(hi543_start_settings),
	.entrance_start_stream_conf = hi543_entrance_start_settings,					/*                                                                                              */
	.entrance_start_stream_conf_size = ARRAY_SIZE(hi543_entrance_start_settings),   /*                                                                                              */
	.stop_stream_conf = hi543_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(hi543_stop_settings),
	.group_hold_on_conf = hi543_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(hi543_groupon_settings),
	.group_hold_off_conf = hi543_groupoff_settings,
	.group_hold_off_conf_size =
		ARRAY_SIZE(hi543_groupoff_settings),
	.init_settings = &hi543_init_conf[0],
	.init_size = ARRAY_SIZE(hi543_init_conf),
	.mode_settings = &hi543_confs[0],
	.output_settings = &hi543_dimensions[0],
	.num_conf = ARRAY_SIZE(hi543_confs),
};

static struct msm_sensor_ctrl_t hi543_s_ctrl = {
	.hi543Initcheck = 1,					/*                                                                                              */
	.msm_sensor_reg = &hi543_regs,
	.sensor_i2c_client = &hi543_sensor_i2c_client,
	.sensor_i2c_addr = 0x40,
	.sensor_output_reg_addr = &hi543_reg_addr,
	.sensor_id_info = &hi543_id_info,
	.vreg_seq = hi543_veg_seq,
	.num_vreg_seq = ARRAY_SIZE(hi543_veg_seq),
	.sensor_exp_gain_info = &hi543_exp_gain_info,		//                             
	.cam_mode = MSM_SENSOR_MODE_INVALID,
//	.csic_params = &hi543_csi_params_array[0],
//	.csi_params = &hi543_csi2_params_array[0],
	.msm_sensor_mutex = &hi543_mut,
	.sensor_i2c_driver = &hi543_i2c_driver,
	.sensor_v4l2_subdev_info = hi543_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(hi543_subdev_info),
	.sensor_v4l2_subdev_ops = &hi543_subdev_ops,
	.func_tbl = &hi543_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Samsung 5MP Bayer sensor driver");
MODULE_LICENSE("GPL v2");


