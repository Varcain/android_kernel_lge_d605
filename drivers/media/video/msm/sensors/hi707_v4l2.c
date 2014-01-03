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
 
#include "msm_sensor.h"
/*                                                    */
#include <mach/board_lge.h>
#define SENSOR_NAME "hi707"

#define SENSOR_REG_PAGE_ADDR 0x03
#define SENSOR_REG_PAGE_0 0x00
#define SENSOR_REG_PAGE_20 0x20

#define SENSOR_PREVIEW_WIDTH 640
#define SENSOR_PREVIEW_HEIGHT 480

#define AWB_LOCK_ON 1	/*                                                                           */
#define AWB_LOCK_OFF 0
#define AEC_LOCK_ON 1/*                                                                           */
#define AEC_LOCK_OFF 0

DEFINE_MUTEX(hi707_mut);
static struct msm_sensor_ctrl_t hi707_s_ctrl;

static int PREV_EFFECT = -1;
static int PREV_EXPOSURE = -1;
static int PREV_WB = -1;
static int PREV_FPS = -1;
static int PREV_NIGHT_MODE = -1;
static int PREV_SOC_AEC_LOCK = -1;	/*                                                                               */
static int PREV_SOC_AWB_LOCK = -1;  /*                                                                               */

extern int32_t msm_sensor_enable_i2c_mux(struct msm_camera_i2c_conf *i2c_conf);
extern int32_t msm_sensor_disable_i2c_mux(struct msm_camera_i2c_conf *i2c_conf);


#if 0 //                           
extern void subpm_set_gpio(int onoff, int type);
extern int32_t msm_sensor_enable_i2c_mux(struct msm_camera_i2c_conf *i2c_conf);
extern int32_t msm_sensor_disable_i2c_mux(struct msm_camera_i2c_conf *i2c_conf);

extern unsigned int system_rev;

#endif


static int effect_value = CAMERA_EFFECT_OFF;



static struct msm_camera_i2c_reg_conf hi707_start_settings[] = {
	{0x03, 0x48},
	{0x16, 0xc0},
	{0x03, 0x00},
	{0x01, 0x70},
	{0x09, 0x00},	
	/*                                                              */
	{0x03, 0x20},
	{0x18, 0x30},	
	/*                                                              */
};

/*                                                                                                      */
static struct msm_camera_i2c_reg_conf hi707_entrance_start_settings[] = {
	{0x03, 0x48},
	{0x16, 0xc8},
	{0x03, 0x00},
	{0x01, 0x70},
	{0x09, 0x00},
	/*                                                              */
	{0x03, 0x20},
	{0x18, 0x30},	
	/*                                                              */
};
/*                                                                                                      */




static struct msm_camera_i2c_reg_conf hi707_stop_settings[] = {
	//{0x03, 0x00},
	//{0x01, 0x71},	
};

static struct msm_camera_i2c_reg_conf hi707_recommend_settings[] = {
	
//2013.02.06 업데이트
//V7 모델 셋팅 적용
//Ae target 0x34 -> 3a
//Video 30fps 모드 Max Exposure 1/40sec->1/30sec으로 변경함 -> 20130205
//Video 모드 셋팅 : sleep on/off 추가 ->20130206
//MIPI Error 개선 및 AE 오동작 개선
//DV2차 실외 AWB 이슈 개선+Fixed 30fps video 추가
//Manual 60Hz -> Auto flicker 변경
//AG max 0xa0 -> 0xb0
//BLC indoor 0x42 -> 0x44
//최종 업데이트 : 20130315

	{0x03, 0x00},
	{0x01, 0x71},  // reset op.
	{0x01, 0x73},
	{0x01, 0x71},
	{0x03, 0x20},  //page 20
	{0x10, 0x1c},  //ae off
	{0x03, 0x22},  //page 22
	{0x10, 0x7b},  //awb off
	{0x03, 0x00}, 
	{0x08, 0x0f}, //Parallel NO Output_PAD Out									   
	{0x10, 0x00},	//VDOCTL1 [5:4]subsample:1,1/2,1/4, [0]preview_en
#if defined(CONFIG_HI707_ROT_180) //180 degree rotation
	{0x11, 0x93},	//VDOCTL2 , 90 : FFR off, 94 : FFR on
#else
	{0x11, 0x90},	//VDOCTL2 , 90 : FFR off, 94 : FFR on
#endif
	{0x12, 0x00},	//CLK_CTL
	{0x14, 0x88},	//[7]fix_frm_spd:prevent hounting, [3]fix_frm_opt:inc. exp.time	
	{0x0b, 0xaa}, 
	{0x0c, 0xaa}, 
	{0x0d, 0xaa}, 	
	{0xc0, 0x95}, 
	{0xc1, 0x18}, 
	{0xc2, 0x91}, 
	{0xc3, 0x00}, 
	{0xc4, 0x01}, 

	{0x03, 0x00},  
	{0x20, 0x00},	//WINROW
	{0x21, 0x04}, // - VGA:04, QVGA,QQVGA:02
	{0x22, 0x00},	//WINCOL
	{0x23, 0x04}, // - VGA,QVGA:04, QQVGA:02
	{0x40, 0x00},	//HBLANK 
	{0x41, 0x90},	// - YUV422:0090, BAYER:0158
	{0x42, 0x00},	//VSYNCH
	{0x43, 0x04}, // - YUV422:0002, BAYER:0014
	
	{0x80, 0x2e}, //don't touch
	{0x81, 0x7e}, //don't touch
	{0x82, 0x90}, //don't touch
	{0x83, 0x30}, //don't touch
	{0x84, 0x2c}, //don't touch
	{0x85, 0x4b}, //don't touch
	{0x86, 0x01}, //don't touch
	{0x88, 0x47}, //don't touch
	{0x89, 0x48}, //BLC hold
	{0x90, 0x0c}, //BLC_TIME_TH_ON
	{0x91, 0x0c}, //BLC_TIME_TH_OFF 
	{0x92, 0xa8}, //98}, //BLC_AG_TH_ON
	{0x93, 0xa0}, //90}, //BLC_AG_TH_OFF
	{0x98, 0x38},
	{0x99, 0x00}, //41}, //Out BLC LHC
	{0xa0, 0x02}, //00}, //Dark BLC
	{0xa8, 0x44}, //42}, //40}, //Normal BLC LHC
	
	{0xc0, 0x95},	//PLL Mode									  
	{0xc1, 0x18},									  
	{0xc2, 0x91},	//[4]plldiv_en, [3:2]mipi4xclkdiv:bypass,1/2,1/4,1/8, [0]ispclkdiv:1/2,1/4
	{0xc3, 0x00},									  
	{0xc4, 0x01},	
	
	///////////////////////////// Page 2	-  Analog Circuit Control
	{0x03, 0x02},
	{0x10, 0x00},	//MODE_TEST
	{0x11, 0x00},	//MODE_DEAD_TEST
	{0x13, 0x40},	//MODE_ANA_TEST
	{0x14, 0x04},	//MODE_MEMORY	
	{0x18, 0x1c},	//Analog mode
	{0x19, 0x00},	//[0]pmos_off
	{0x1a, 0x00}, 
	{0x1b, 0x08},	
	{0x1c, 0x9c},	//DC-DC
	{0x1d, 0x03},
	{0x20, 0x33},	//PX bias
	{0x21, 0x77},	//ADC/ASP bias
	{0x22, 0xa7},	//Main bias
	{0x23, 0x32},	//Clamp 	
	{0x24, 0x33},
	{0x2b, 0x40},	//Fixed frame counter end
	{0x2d, 0x32},	//Fixed frame counter start
	{0x31, 0x99},	//shared control
	{0x32, 0x00},
	{0x33, 0x00},
	{0x34, 0x3c},
	{0x35, 0x0d},
	{0x3b, 0x80}, //SF 60
	{0x50, 0x21}, //timing control 1
	{0x51, 0x1C},
	{0x52, 0xaa},
	{0x53, 0x5a},
	{0x54, 0x30},
	{0x55, 0x10},
	{0x56, 0x0c},
	{0x58, 0x00}, 
	{0x59, 0x0F},
	{0x60, 0x34},	//addr_en - Exp. //Row Timing Control
	{0x61, 0x3a},
	{0x62, 0x34},	//rx1
	{0x63, 0x39},
	{0x64, 0x34},	//rx2
	{0x65, 0x39},
	{0x72, 0x35},	//tx1
	{0x73, 0x38},
	{0x74, 0x35},	//tx2
	{0x75, 0x38},
	{0x80, 0x02},	//addr_en - Read.
	{0x81, 0x2e},
	{0x82, 0x0d},	//rx1
	{0x83, 0x10},
	{0x84, 0x0d},	//rx2
	{0x85, 0x10},
	{0x92, 0x1d},	//tx1
	{0x93, 0x20},
	{0x94, 0x1d},	//tx2
	{0x95, 0x20},
	{0xa0, 0x03},	//sx
	{0xa1, 0x2d},
	{0xa4, 0x2d},	//sxb
	{0xa5, 0x03},
	{0xa8, 0x12},	//wrst
	{0xa9, 0x1b},
	{0xaa, 0x22},	//wsig
	{0xab, 0x2b},
	{0xac, 0x10},	//rx_off_rst
	{0xad, 0x0e},	//tx_off_rst
	{0xb8, 0x33},	//rx pwr - exp.
	{0xb9, 0x35},
	{0xbc, 0x0c},	//rx pwr - read
	{0xbd, 0x0e},
	{0xc0, 0x3a},	//addr_en1 - Fixed Exp.
	{0xc1, 0x3f},
	{0xc2, 0x3a},	//addr_en2
	{0xc3, 0x3f},
	{0xc4, 0x3a},	//sx1
	{0xc5, 0x3e},
	{0xc6, 0x3a},	//sx2
	{0xc7, 0x3e},
	{0xc8, 0x3a},	//rx1
	{0xc9, 0x3e},
	{0xca, 0x3a},	//rx2
	{0xcb, 0x3e},
	{0xcc, 0x3b},	//tx1
	{0xcd, 0x3d},
	{0xce, 0x3b},	//tx2
	{0xcf, 0x3d},	
	{0xd0, 0x33},	//Exposure domain valid
	{0xd1, 0x3f},

	///////////////////////////// Page 10
	{0x03, 0x10}, //Page 10 - Format, Image Effect
	{0x10, 0x03}, //ISPCTL - [7:4]0:YUV322, 6:BAYER, [1:0]VYUY, UYVY, YVYU, YUYV
	{0x11, 0x43}, // - [0x1010:1011]YUV422:0343, BAYER:6000
	{0x12, 0x30}, //Y offet, dy offseet enable          
	{0x40, 0x00}, 
	{0x41, 0x00}, //DYOFS  00->10-> 00  STEVE_130110(black scene face saturation)
	{0x48, 0x80}, //Contrast  88->84  _100318
	{0x50, 0xa0}, //e0}, //AGBRT
	{0x60, 0x0b},         
	{0x61, 0x00}, //default
	{0x62, 0x78}, //SATB  (1.4x)
	{0x63, 0x78}, //SATR  (1.2x)
	{0x64, 0x80}, //a0}, //AGSAT 20130205
	{0x66, 0x90}, //wht_th2
	{0x67, 0x36}, //wht_gain  Dark (0.4x), Normal (0.75x)

	///////////////////////////// Page 11
	{0x03, 0x11},
	{0x10, 0x25}, //LPF_CTL1 //0x01
	{0x11, 0x07}, //1f},	//Test Setting
	{0x20, 0x00}, //LPF_AUTO_CTL
	{0x21, 0x60}, //38},	//LPF_PGA_TH
	{0x23, 0x0a}, //LPF_TIME_TH
	{0x60, 0x13}, //ZARA_SIGMA_TH //40->10
	{0x61, 0x85},
	{0x62, 0x00},	//ZARA_HLVL_CTL
	{0x63, 0x00}, //83},//ZARA_LLVL_CTL
	{0x64, 0x00}, //83},//ZARA_DY_CTL
	{0x67, 0x70}, //60},//70}, //F0},	//Dark
	{0x68, 0x24}, //24},//30},	//Middle
	{0x69, 0x04}, //10},//High

	///////////////////////////// Page 12	
	{0x03, 0x12}, //Page 12 - 2D : YC1D,YC2D,DPC,Demosaic																				  
	{0x40, 0xd3}, //d6}, //d7},//YC2D_LPF_CTL1 //bc 															
	{0x41, 0x09},	//YC2D_LPF_CTL2 																 
	{0x50, 0x18}, //10}, //18}, //Test Setting																	 
	{0x51, 0x24},	//Test Setting																	 
	{0x70, 0x1f},	//GBGR_CTL1 //0x1f																 
	{0x71, 0x00},	//Test Setting																	 
	{0x72, 0x00},	//Test Setting																	 
	{0x73, 0x00},	//Test Setting																	 
	{0x74, 0x12},	//GBGR_G_UNIT_TH//12															 
	{0x75, 0x12},	//GBGR_RB_UNIT_TH//12															 
	{0x76, 0x20},	//GBGR_EDGE_TH																	 
	{0x77, 0x80},	//GBGR_HLVL_TH																	 
	{0x78, 0x88},	//GBGR_HLVL_COMP																 
	{0x79, 0x18},	//Test Setting 
	
	{0x90, 0x3d},																				   
	{0x91, 0x34},																				   
	{0x99, 0x28},																				   
	{0x9c, 0x05}, //14 For defect																   
	{0x9d, 0x08}, //15 For defect																   
	{0x9e, 0x28},																				   
	{0x9f, 0x28},																				   
																								   
	{0xb0, 0x7d}, //75 White Defect 															   
	{0xb5, 0x44},																				   
	{0xb6, 0x82},																				   
	{0xb7, 0x52},																				   
	{0xb8, 0x44},																				   
	{0xb9, 0x15},

	///////////////////////////// Page 13				
	{0x03, 0x13}, //Page 13 - Sharpness
	{0x10, 0x01},	
	{0x11, 0x8f}, //                                          
	{0x12, 0x14},	
	{0x13, 0x19},	
	{0x14, 0x08},	//Test Setting
	{0x20, 0x03},	//SHARP_Negative
	{0x21, 0x04}, //03},	//SHARP_Positive
	{0x23, 0x25},	//SHARP_DY_CTL
	{0x24, 0x21},	//40->33
	{0x25, 0x08},	//SHARP_PGA_TH
	{0x26, 0x40},	//Test Setting
	{0x27, 0x00},	//Test Setting
	{0x28, 0x08},	//Test Setting
	{0x29, 0x50},	//AG_TH
	{0x2a, 0xe0},	//region ratio
	{0x2b, 0x10},	//Test Setting
	{0x2c, 0x28},	//Test Setting
	{0x2d, 0x40},	//Test Setting
	{0x2e, 0x00},	//Test Setting
	{0x2f, 0x00},	//Test Setting
	{0x30, 0x11},	//Test Setting
	{0x80, 0x05},	//SHARP2D_CTL
	{0x81, 0x07},	//Test Setting
	{0x90, 0x04},	//SHARP2D_SLOPE
	{0x91, 0x05},	//SHARP2D_DIFF_CTL
	{0x92, 0x00},	//SHARP2D_HI_CLIP
	{0x93, 0x30},	//SHARP2D_DY_CTL
	{0x94, 0x30},	//Test Setting
	{0x95, 0x10},	//Test Setting 

	///////////////////////////// Page 14		
	{0x03, 0x14}, //Page 14 - Lens Shading Correction
	{0x10, 0x01},
	{0x20, 0x80}, //60},   //XCEN LHC
	{0x21, 0x80}, //YCEN
	{0x22, 0x88}, //7b}, //6a}, //50},
	{0x23, 0x5c}, //50}, //44}, //40},
	{0x24, 0x49}, //44}, //32}, //3d},
	
	//////////////////////////// 15page
	{0x03, 0x15}, 
	{0x10, 0x03},
	{0x14, 0x52},	//CMCOFSGM 
	{0x16, 0x3a},	//CMCOFSGL
	{0x17, 0x2f},	//CMC SIGN

	{0x30, 0xf1},
	{0x31, 0x71},
	{0x32, 0x00},
	{0x33, 0x1f},
	{0x34, 0xe1},
	{0x35, 0x42},
	{0x36, 0x01},
	{0x37, 0x31},
	{0x38, 0x72},
	
	{0x40, 0x90}, //CMC OFS
	{0x41, 0x82},
	{0x42, 0x12},
	{0x43, 0x86},
	{0x44, 0x92},
	{0x45, 0x18},
	{0x46, 0x84},
	{0x47, 0x02},
	{0x48, 0x02},
	//////////////////////////// 16page		
	{0x03, 0x16}, //gamma 															
	{0x30, 0x00},																				   
	{0x31, 0x08}, 																
	{0x32, 0x1c}, 											
	{0x33, 0x2f}, 																	
	{0x34, 0x53},																				   
	{0x35, 0x76},																				   
	{0x36, 0x93},																				   
	{0x37, 0xac},																				   
	{0x38, 0xc0},																				   
	{0x39, 0xd0},																				   
	{0x3a, 0xdc},																				   
	{0x3b, 0xed},																				   
	{0x3c, 0xf4}, //f7																			   
	{0x3d, 0xf6}, //fc																			   
	{0x3e, 0xfa}, //ff		
	//////////////////////////// 17page		   
	{0x03, 0x17},
	{0xc0, 0x01},     
	{0xc4, 0x4b}, //3c},
  	{0xc5, 0x3e}, //32},
	///////////////////////////// Page 20	- Auto Exposure 
	{0x03, 0x20},
	{0x10, 0x0c},	//AECTL
	{0x11, 0x04},
	{0x18, 0x31}, // 130521, Flicker Test
	{0x20, 0x01},	//FrameCTL
	{0x28, 0x27},	//FineCTL
	{0x29, 0xa1},
	{0x2a, 0xf0},
	{0x2b, 0x34},
	{0x2c, 0x2b},	 // 130521, Flicker Test	   
	{0x39, 0x22},
	{0x3a, 0xde},
	{0x3b, 0x23},
	{0x3c, 0xde},


				 // 130521, Flicker Test
	{0x60, 0x71}, //0x71}, //AE weight																	 
	{0x61, 0x00}, //0x11}, 																					   
	{0x62, 0x71}, //0x71}, 																		 
	{0x63, 0x00}, //0x11},  
	 
#if defined(CONFIG_HI707_ROT_180) 
				// 130521, Flicker Test
	{0x68, 0x36}, //30}, //AE_CEN
	{0x69, 0x70}, //6a},
	{0x6A, 0x35}, //27},
	{0x6B, 0xc9}, //bb},
#else  
				// 130521, Flicker Test
	{0x68, 0x30}, //30}, //AE_CEN
	{0x69, 0x6a}, //6a},
	{0x6A, 0x27}, //27},
	{0x6B, 0xbb}, //bb},
#endif

	{0x70, 0x36}, //34}, 20130524  //Y Targe 32 
	{0x76, 0x88}, //22}, // Unlock bnd1
	{0x77, 0xfe}, //02}, // Unlock bnd2
	{0x78, 0x23}, //22}, 20130524  //12}, // Yth 1
	{0x79, 0x26}, //Yth 2
	{0x7a, 0x23}, //Yth 3
	{0x7c, 0x1c}, //Yth 2 
	{0x7d, 0x22}, //Yth 4
	
	{0x83, 0x01}, //EXP Normal 20.00 fps 
	{0x84, 0x24}, 
	{0x85, 0xf8}, 
	{0x86, 0x00}, //EXPMin 7500.00 fps
	{0x87, 0xc8}, 
	{0x88, 0x02}, //EXP Max(120Hz) 10.00 fps 
	{0x89, 0x49}, 
	{0x8a, 0xf0}, 
	{0xa0, 0x02}, //EXP Max(100Hz) 10.00 fps 
	{0xa1, 0x49}, 
	{0xa2, 0xf0}, 
	{0x8B, 0x3a}, //EXP100 
	{0x8C, 0x98}, 
	{0x8D, 0x30}, //EXP120 
	{0x8E, 0xd4}, 
	{0x9c, 0x04}, //EXP Limit 1250.00 fps 
	{0x9d, 0xb0}, 
	{0x9e, 0x00}, //EXP Unit 
	{0x9f, 0xc8}, 
	
	{0x98, 0x8c}, //Outdoor BRIGHT_MEASURE_TH
	{0x99, 0x23},
	
	{0xb0, 0x1d},
	{0xb1, 0x14}, //14
	{0xb2, 0xb0}, //a0}, //80
	{0xb3, 0x17}, //AGLVL //17
	{0xb4, 0x17},
	{0xb5, 0x3e},
	{0xb6, 0x2b},
	{0xb7, 0x24},
	{0xb8, 0x21},
	{0xb9, 0x1f},
	{0xba, 0x1e},
	{0xbb, 0x1d},
	{0xbc, 0x1c},
	{0xbd, 0x1b},
	
	{0xc0, 0x1a}, //PGA_sky
	{0xc3, 0x48}, //PGA_dark_on
	{0xc4, 0x48}, //PGA_dark_off
	
	{0x03, 0x22}, //Page 22 AWB
	{0x10, 0xe2},
	{0x11, 0x2E}, //26},
	{0x20, 0x41}, //01 //69 
	{0x21, 0x40},
	{0x30, 0x80},
	{0x31, 0x80},
	{0x38, 0x12},
	{0x39, 0x33},
	{0x40, 0xf3}, //b8}, //93}, //f0},
	              // STEVE Yellowish	
	{0x41, 0x54}, //0x44}, //0x54},
	{0x42, 0x33}, //0x22}, //0x33},
	{0x43, 0xf3}, //0xf8}, //0xf3},
	{0x44, 0x55}, //0x55}, //0x55},
	{0x45, 0x44}, //0x44}, //0x44},
	{0x46, 0x02}, //0x08}, //0x02},

	{0x80, 0x40}, //3d}, // R
	{0x81, 0x20}, //20}, // G
	{0x82, 0x38}, //40}, // B

	{0x83, 0x59}, //58}, //5a}, //52}, //RMAX
	{0x84, 0x20}, //1d}, //RMIN
	{0x85, 0x53}, //BMAX 5a
	{0x86, 0x24}, //BMIN 
	
	{0x87, 0x49}, //48}, //4a}, //42
	{0x88, 0x3c},
	{0x89, 0x3e},
	{0x8a, 0x34},
  
	{0x8b, 0x00}, //02}, //0x08}, //02}, //OUT TH
	{0x8d, 0x24}, //3a}, //0x11}, //22},
	{0x8e, 0x61}, //b3}, //0x11}, //71}, 

	{0x8f, 0x63}, //65}, //0x63},
	{0x90, 0x60}, //61}, //0x60},
	{0x91, 0x5c}, //5C}, //0x5c},
	{0x92, 0x56}, //56}, //0x56},
	{0x93, 0x52}, //4E}, //0x52},
	{0x94, 0x4c}, //43}, //0x4c},
	{0x95, 0x3e}, //3A}, //0x3e},
	{0x96, 0x2f}, //32}, //0x2f},
	{0x97, 0x28}, //2A}, //0x28},
	{0x98, 0x23}, //24}, //0x23},
	{0x99, 0x21}, //20}, //0x21},
	{0x9a, 0x20}, //1C}, //0x20},
	{0x9b, 0x08}, //0a}, //0x07},

	/////////////////////////////
	// Page 48 MIPI /////////////
	/////////////////////////////													  
	{0x03, 0x48},
  //{0x10, 0x05},           //05},        
	{0x11, 0x0c}, //04}, //00}, con
	{0x16, 0xC8}, //88}, //c8}, //c4},  
	{0x1a, 0x00},                //00}, 
	{0x1b, 0x35},                //00}, 
	{0x1c, 0x03}, // HSP_LPX     //02},        
	{0x1d, 0x05}, // HSN_LPX     //04}, 
	{0x1e, 0x07}, // HS_ZERO     //07},  
	{0x1f, 0x03}, // HS_TRAIL    //05}, 
	{0x20, 0x00}, // HS_EXIT     //00}, 
	{0x21, 0xb8}, // HS_SYNC
	{0x28, 0x00}, // ADD
	{0x30, 0x05},                //05},
	{0x31, 0x00},                //00}, 
	{0x32, 0x07}, // CLK_ZERO    //06},                              
	{0x35, 0x01}, // CLK_TRAIL   //03},               
	{0x34, 0x01}, // CLK_PREPARE //02},               
	{0x36, 0x01}, // CLKP_LPX    //01},        
	{0x37, 0x03}, // CLKN_LPX    //03},                             
	{0x38, 0x02}, // CLK_EXIT    //00},        
	{0x39, 0xef},                //4a},  
	{0x3c, 0x00},                //00},        
	{0x3d, 0xfa},                //fa},                           
	{0x3f, 0x10},                //10},                                                                
	{0x40, 0x00},                //00},  
	{0x41, 0x20},                //20},        
	{0x42, 0x00},                //00}, 						  
	{0x10, 0x05},                //05},        
						 																	   
	{0x03, 0x20},
	{0x10, 0xec}, //cc}, //120hz first 20130524
	{0x03, 0x22}, //Page 22 AWB
	{0x10, 0xfb},
	
};

static struct msm_camera_i2c_reg_conf hi707_full_settings[] = {
};


static struct v4l2_subdev_info hi707_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
	/* more can be supported, to be added later */
};


static struct msm_camera_i2c_conf_array hi707_init_conf[] = {
	{&hi707_recommend_settings[0],
	ARRAY_SIZE(hi707_recommend_settings), 0, MSM_CAMERA_I2C_BYTE_DATA}
};

static struct msm_camera_i2c_conf_array hi707_confs[] = {
	{&hi707_full_settings[0],
	ARRAY_SIZE(hi707_full_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};



static struct msm_sensor_output_info_t hi707_dimensions[] = {
	{
		.x_output = 0x280,
		.y_output = 0x1E0,
		.line_length_pclk = 0x031b,		//784	-->//144
		.frame_length_lines = 0x027b,   //484	--> //4
		.vt_pixel_clk = 15200000, //45600000,		
		.op_pixel_clk = 15200000, //45600000,
		.binning_factor = 1,
	},
};


static struct msm_camera_i2c_reg_conf hi707_exposure[][2] = {
	{{0x03, 0x10}, {0x40, 0xBC}}, /*EXPOSURECOMPENSATIONN6*/
	{{0x03, 0x10}, {0x40, 0xB2}}, /*EXPOSURECOMPENSATIONN5*/
	{{0x03, 0x10}, {0x40, 0xA8}}, /*EXPOSURECOMPENSATIONN4*/
	{{0x03, 0x10}, {0x40, 0x9E}}, /*EXPOSURECOMPENSATIONN3*/
	{{0x03, 0x10}, {0x40, 0x94}}, /*EXPOSURECOMPENSATIONN2*/
	{{0x03, 0x10}, {0x40, 0x8A}}, /*EXPOSURECOMPENSATIONN1*/
	{{0x03, 0x10}, {0x40, 0x80}}, /*EXPOSURECOMPENSATIOND*/
	{{0x03, 0x10}, {0x40, 0x0A}}, /*EXPOSURECOMPENSATIONP1*/
	{{0x03, 0x10}, {0x40, 0x14}}, /*EXPOSURECOMPENSATIONP2*/
	{{0x03, 0x10}, {0x40, 0x1E}}, /*EXPOSURECOMPENSATIONP3*/
	{{0x03, 0x10}, {0x40, 0x28}}, /*EXPOSURECOMPENSATIONP4*/
	{{0x03, 0x10}, {0x40, 0x32}}, /*EXPOSURECOMPENSATIONP5*/
	{{0x03, 0x10}, {0x40, 0x3C}}, /*EXPOSURECOMPENSATIONP6*/
};

static struct msm_camera_i2c_conf_array hi707_exposure_confs[][1] = {
	{{hi707_exposure[0],  ARRAY_SIZE(hi707_exposure[0]),  0,	MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hi707_exposure[1],  ARRAY_SIZE(hi707_exposure[1]),  0,	MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hi707_exposure[2],  ARRAY_SIZE(hi707_exposure[2]),  0,	MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hi707_exposure[3],  ARRAY_SIZE(hi707_exposure[3]),  0,	MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hi707_exposure[4],  ARRAY_SIZE(hi707_exposure[4]),  0,	MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hi707_exposure[5],  ARRAY_SIZE(hi707_exposure[5]),  0,	MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hi707_exposure[6],  ARRAY_SIZE(hi707_exposure[6]),  0,	MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hi707_exposure[7],  ARRAY_SIZE(hi707_exposure[7]),  0,	MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hi707_exposure[8],  ARRAY_SIZE(hi707_exposure[8]),  0,	MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hi707_exposure[9],  ARRAY_SIZE(hi707_exposure[9]),  0,	MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hi707_exposure[10], ARRAY_SIZE(hi707_exposure[10]), 0,	MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hi707_exposure[11], ARRAY_SIZE(hi707_exposure[11]), 0,	MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hi707_exposure[12], ARRAY_SIZE(hi707_exposure[12]), 0,	MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int hi707_exposure_enum_map[] = {
	MSM_V4L2_EXPOSURE_N6,
	MSM_V4L2_EXPOSURE_N5,
	MSM_V4L2_EXPOSURE_N4,
	MSM_V4L2_EXPOSURE_N3,
	MSM_V4L2_EXPOSURE_N2,
	MSM_V4L2_EXPOSURE_N1,
	MSM_V4L2_EXPOSURE_D,
	MSM_V4L2_EXPOSURE_P1,
	MSM_V4L2_EXPOSURE_P2,
	MSM_V4L2_EXPOSURE_P3,
	MSM_V4L2_EXPOSURE_P4,
	MSM_V4L2_EXPOSURE_P5,
	MSM_V4L2_EXPOSURE_P6,
};

static struct msm_camera_i2c_enum_conf_array hi707_exposure_enum_confs = {
	.conf = &hi707_exposure_confs[0][0],
	.conf_enum = hi707_exposure_enum_map,
	.num_enum = ARRAY_SIZE(hi707_exposure_enum_map),
	.num_index = ARRAY_SIZE(hi707_exposure_confs),
	.num_conf = ARRAY_SIZE(hi707_exposure_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};


static struct msm_camera_i2c_reg_conf hi707_no_effect[] = {
	{0x03, 0x10}, 
	{0x11, 0x43}, 
	{0x12, 0x30}, 
	{0x44, 0x80}, 
	{0x45, 0x80},
};

static struct msm_camera_i2c_conf_array hi707_no_effect_confs[] = {
	{&hi707_no_effect[0],	ARRAY_SIZE(hi707_no_effect), 0,	MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},
};

static struct msm_camera_i2c_reg_conf hi707_special_effect[][5] = {
	{
		/* MSM_V4L2_EFFECT_OFF */
		{0x03, 0x10}, 
		{0x11, 0x43}, 
		{0x12, 0x30}, 
		{0x44, 0x80}, 
		{0x45, 0x80},
	},
	{
		/* MSM_V4L2_EFFECT_MONO  */
		{0x03, 0x10}, 
		{0x11, 0x03}, 
		{0x12, 0x33}, 
		{0x44, 0x80}, 
		{0x45, 0x80},
	},
	{
		/* MSM_V4L2_EFFECT_NEGATIVE  */
		{0x03, 0x10}, 
		{0x11, 0x03}, 
		{0x12, 0x38}, 
		{0x44, 0x80}, 
		{0x45, 0x80},
	},
	{
		/* MSM_V4L2_EFFECT_SOLARIZE is not supported */
		{-1, -1, -1, -1, -1}, 
		{-1, -1, -1, -1, -1}, 
		{-1, -1, -1, -1, -1},	
		{-1, -1, -1, -1, -1},	
		{-1, -1, -1, -1, -1},
	},
	{
		/* MSM_V4L2_EFFECT_SEPIA */
		{0x03, 0x10}, 
		{0x11, 0x03}, 
		{0x12, 0x33}, 
		{0x44, 0x70}, 
		{0x45, 0x98},
	},
	{
		/* MSM_V4L2_EFFECT_POSTERAIZE is not supported */
		{-1, -1, -1, -1, -1}, 
		{-1, -1, -1, -1, -1}, 
		{-1, -1, -1, -1, -1},	
		{-1, -1, -1, -1, -1},	
		{-1, -1, -1, -1, -1},
	},
	{
		/* MSM_V4L2_EFFECT_WHITEBOARD is not supported */
		{-1, -1, -1, -1, -1}, 
		{-1, -1, -1, -1, -1}, 
		{-1, -1, -1, -1, -1},	
		{-1, -1, -1, -1, -1},	
		{-1, -1, -1, -1, -1},
	},
	{
		/* MSM_V4L2_EFFECT_BLACKBOARD is not supported */
		{-1, -1, -1, -1, -1}, 
		{-1, -1, -1, -1, -1}, 
		{-1, -1, -1, -1, -1},	
		{-1, -1, -1, -1, -1},	
		{-1, -1, -1, -1, -1},
	},
	{
		/* MSM_V4L2_EFFECT_AQUA is not supported */
		{-1, -1, -1, -1, -1}, 
		{-1, -1, -1, -1, -1}, 
		{-1, -1, -1, -1, -1},	
		{-1, -1, -1, -1, -1},	
		{-1, -1, -1, -1, -1},
	},
	{
		/* MSM_V4L2_EFFECT_EMBOSS is not supported */
		{-1, -1, -1, -1, -1}, 
		{-1, -1, -1, -1, -1}, 
		{-1, -1, -1, -1, -1},	
		{-1, -1, -1, -1, -1},	
		{-1, -1, -1, -1, -1},
	},
	{
		/* MSM_V4L2_EFFECT_SKETCH is not supported */
		{-1, -1, -1, -1, -1}, 
		{-1, -1, -1, -1, -1}, 
		{-1, -1, -1, -1, -1},	
		{-1, -1, -1, -1, -1},	
		{-1, -1, -1, -1, -1},
	},
	{
		/* MSM_V4L2_EFFECT_NEON is not supported */
		{-1, -1, -1, -1, -1}, 
		{-1, -1, -1, -1, -1}, 
		{-1, -1, -1, -1, -1},	
		{-1, -1, -1, -1, -1},	
		{-1, -1, -1, -1, -1},
	},
	{
		/* MSM_V4L2_EFFECT_MAX */
		{-1, -1, -1, -1, -1}, 
		{-1, -1, -1, -1, -1}, 
		{-1, -1, -1, -1, -1},	
		{-1, -1, -1, -1, -1},	
		{-1, -1, -1, -1, -1},
	},
};

static struct msm_camera_i2c_conf_array hi707_special_effect_confs[][1] = {
	{{hi707_special_effect[0],  ARRAY_SIZE(hi707_special_effect[0]),  0,	MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hi707_special_effect[1],  ARRAY_SIZE(hi707_special_effect[1]),  0,	MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hi707_special_effect[2],  ARRAY_SIZE(hi707_special_effect[2]),  0,	MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hi707_special_effect[3],  ARRAY_SIZE(hi707_special_effect[3]),  0,	MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hi707_special_effect[4],  ARRAY_SIZE(hi707_special_effect[4]),  0,	MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hi707_special_effect[5],  ARRAY_SIZE(hi707_special_effect[5]),  0,	MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hi707_special_effect[6],  ARRAY_SIZE(hi707_special_effect[6]),  0,	MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hi707_special_effect[7],  ARRAY_SIZE(hi707_special_effect[7]),  0,	MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hi707_special_effect[8],  ARRAY_SIZE(hi707_special_effect[8]),  0,	MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hi707_special_effect[9],  ARRAY_SIZE(hi707_special_effect[9]),  0,	MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hi707_special_effect[10], ARRAY_SIZE(hi707_special_effect[10]), 0,	MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hi707_special_effect[11], ARRAY_SIZE(hi707_special_effect[11]), 0,	MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hi707_special_effect[12], ARRAY_SIZE(hi707_special_effect[12]), 0,	MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int hi707_special_effect_enum_map[] = {
	MSM_V4L2_EFFECT_OFF,
	MSM_V4L2_EFFECT_MONO,
	MSM_V4L2_EFFECT_NEGATIVE,
	MSM_V4L2_EFFECT_SOLARIZE,
	MSM_V4L2_EFFECT_SEPIA,
	MSM_V4L2_EFFECT_POSTERAIZE,
	MSM_V4L2_EFFECT_WHITEBOARD,
	MSM_V4L2_EFFECT_BLACKBOARD,
	MSM_V4L2_EFFECT_AQUA,
	MSM_V4L2_EFFECT_EMBOSS,
	MSM_V4L2_EFFECT_SKETCH,
	MSM_V4L2_EFFECT_NEON,
	MSM_V4L2_EFFECT_MAX,
};

static struct msm_camera_i2c_enum_conf_array hi707_special_effect_enum_confs = {
	.conf = &hi707_special_effect_confs[0][0],
	.conf_enum = hi707_special_effect_enum_map,
	.num_enum = ARRAY_SIZE(hi707_special_effect_enum_map),
	.num_index = ARRAY_SIZE(hi707_special_effect_confs),
	.num_conf = ARRAY_SIZE(hi707_special_effect_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf hi707_wb_oem[][17] = {
	{
		/* MSM_V4L2_WB_OFF is not supported */
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
	},
	{
		/* MSM_V4L2_WB_AUTO */
		{0x03, 0x22},
		{0x10, 0x7b},
		{0x11, 0x2e},
		{0x80, 0x40}, //3d},
		{0x81, 0x20}, 
		{0x82, 0x38}, //40},
		{0x83, 0x59}, //58}, //RMAX  
		{0x84, 0x20}, //RMIN  
		{0x85, 0x53}, //BMAX  
		{0x86, 0x24}, //BMIN  
		{0x87, 0x49}, //48}, //RMAXB 
		{0x88, 0x3c}, //RMINB 
		{0x89, 0x3e}, //BMAXB 
		{0x8a, 0x34}, //BMINB                           
		{0x8d, 0x24}, //11,}, //IN/OUT slop R                          
		{0x8e, 0x61}, //11,}, //IN/OUT slop B                          
		{0x10, 0xfb},
	},
	{
		/* MSM_V4L2_WB_CUSTOM is not supported */
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
	},
	{
		/* MSM_V4L2_WB_INCANDESCENT */
		{0x03, 0x22},
		{0x10, 0x7b},
		{0x11, 0x26},
		//INCA
		{0x80, 0x20},
		{0x81, 0x20},
		{0x82, 0x60},
		{0x83, 0x2F},
		{0x84, 0x11},
		{0x85, 0x67},
		{0x86, 0x58},
		{0x87, 0x60}, //RMAX
		{0x88, 0x20}, //RMIN
		{0x89, 0x60}, //BMAX
		{0x8a, 0x20}, //BMIN
		{0x8d, 0x00}, //IN/OUT slop R
		{0x8e, 0x00}, //IN/OUT slop B
		{0x10, 0xfb},
	},
	{
		/* MSM_V4L2_WB_FLUORESCENT */
		{0x03, 0x22},
		{0x10, 0x7b},
		{0x11, 0x26},
		//TL84
		{0x80, 0x2d},
		{0x81, 0x20},
		{0x82, 0x50},
		{0x83, 0x37},
		{0x84, 0x23},
		{0x85, 0x55},
		{0x86, 0x4B},
		{0x87, 0x60}, //RMAX
		{0x88, 0x20}, //RMIN
		{0x89, 0x60}, //BMAX
		{0x8a, 0x20}, //BMIN
		{0x8d, 0x00}, //IN/OUT slop R
		{0x8e, 0x00}, //IN/OUT slop B
		{0x10, 0xfb},
	},
	{
		/* MSM_V4L2_WB_DAYLIGHT */
		{0x03, 0x22},
		{0x10, 0x7b},
		{0x11, 0x26},
		//D50
		{0x80, 0x42},
		{0x81, 0x20},
		{0x82, 0x3d},
		{0x83, 0x49},
		{0x84, 0x3A},
		{0x85, 0x47},
		{0x86, 0x33},
		{0x87, 0x60}, //RMAX
		{0x88, 0x20}, //RMIN
		{0x89, 0x60}, //BMAX
		{0x8a, 0x20}, //BMIN
		{0x8d, 0x00}, //IN/OUT slop R
		{0x8e, 0x00}, //IN/OUT slop B
		{0x10, 0xfb},
	},
	{
		/* MSM_V4L2_WB_CLOUDY_DAYLIGHT */
		{0x03, 0x22},
		{0x10, 0x7b},
		{0x11, 0x26},
		//Cloudy
		{0x80, 0x60},
		{0x81, 0x20},
		{0x82, 0x20},
		{0x83, 0x70},
		{0x84, 0x51},
		{0x85, 0x2A},
		{0x86, 0x20},
		{0x87, 0x60}, //RMAX
		{0x88, 0x20}, //RMIN
		{0x89, 0x60}, //BMAX
		{0x8a, 0x20}, //BMIN
		{0x8d, 0x00}, //IN/OUT slop R
		{0x8e, 0x00}, //IN/OUT slop B
		{0x10, 0xfb},
	},
};

static struct msm_camera_i2c_conf_array hi707_wb_oem_confs[][1] = {
	{{hi707_wb_oem[0], ARRAY_SIZE(hi707_wb_oem[0]),  0, MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hi707_wb_oem[1], ARRAY_SIZE(hi707_wb_oem[1]),  0, MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hi707_wb_oem[2], ARRAY_SIZE(hi707_wb_oem[2]),  0, MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hi707_wb_oem[3], ARRAY_SIZE(hi707_wb_oem[3]),  0, MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hi707_wb_oem[4], ARRAY_SIZE(hi707_wb_oem[4]),  0, MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hi707_wb_oem[5], ARRAY_SIZE(hi707_wb_oem[5]),  0, MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{hi707_wb_oem[6], ARRAY_SIZE(hi707_wb_oem[6]),  0, MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int hi707_wb_oem_enum_map[] = {
	MSM_V4L2_WB_OFF,
	MSM_V4L2_WB_AUTO ,
	MSM_V4L2_WB_CUSTOM,
	MSM_V4L2_WB_INCANDESCENT,
	MSM_V4L2_WB_FLUORESCENT,
	MSM_V4L2_WB_DAYLIGHT,
	MSM_V4L2_WB_CLOUDY_DAYLIGHT,
};

static struct msm_camera_i2c_enum_conf_array hi707_wb_oem_enum_confs = {
	.conf = &hi707_wb_oem_confs[0][0],
	.conf_enum = hi707_wb_oem_enum_map,
	.num_enum = ARRAY_SIZE(hi707_wb_oem_enum_map),
	.num_index = ARRAY_SIZE(hi707_wb_oem_confs),
	.num_conf = ARRAY_SIZE(hi707_wb_oem_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf hi707_night_mode[][21] = {
	/*NIGHT MODE OFF 10~30fps*/
	{
		{0x03, 0x20},
		{0x10, 0x0c},	
		{0x18, 0x38}, //AE Reset ON
		{0x03, 0x20},		
		{0x83, 0x01}, //EXP Normal 20.00 fps 
		{0x84, 0x24}, 
		{0x85, 0xf8}, 
		{0x88, 0x02}, //EXP Max(120Hz) 10.00 fps 
		{0x89, 0x49}, 
		{0x8a, 0xf0}, 
		{0xa0, 0x02}, //EXP Max(100Hz) 10.00 fps 
		{0xa1, 0x49}, 
		{0xa2, 0xf0},  
		{0x03, 0x00}, //PAGE 0
		{0x90, 0x0c}, //BLC_TIME_TH_ON
		{0x91, 0x0c}, //BLC_TIME_TH_OFF 
		{0x92, 0xa8}, //98}, //BLC_AG_TH_ON
		{0x93, 0xa0}, //90}, //BLC_AG_TH_OFF
		{0x03, 0x20},
		{0x10, 0xec}, //cc},20130524
		{0x18, 0x30}, //AE Reset OFF // 
	
	}, 
	/*NIGHT MODE ON 5~30 fps*/
	{
		{0x03, 0x20},
		{0x10, 0x0c},	
		{0x18, 0x38}, //AE Reset ON
		{0x03, 0x20},		
		{0x83, 0x04}, //EXP Normal 5.00 fps 
		{0x84, 0x93}, 
		{0x85, 0xe0},  
		{0x88, 0x04}, //EXP Max(120Hz) 5.00 fps 
		{0x89, 0x93}, 
		{0x8a, 0xe0}, 
		{0xa0, 0x04}, //EXP Max(100Hz) 5.00 fps 
		{0xa1, 0x93}, 
		{0xa2, 0xe0},   
		{0x03, 0x00}, //PAGE 0
		{0x90, 0x18}, //BLC_TIME_TH_ON
		{0x91, 0x18}, //BLC_TIME_TH_OFF 
		{0x92, 0xa8}, //98}, //BLC_AG_TH_ON
		{0x93, 0xa0}, //90}, //BLC_AG_TH_OFF
		{0x03, 0x20}, 
		{0x10, 0xec}, //cc}, 20130524
		{0x18, 0x30}, //AE Reset OFF // 
	},	
};

static struct msm_camera_i2c_conf_array hi707_night_mode_confs[][1] = {
	{{hi707_night_mode[0], ARRAY_SIZE(hi707_night_mode[0]),  0,	MSM_CAMERA_I2C_BYTE_DATA},},
	{{hi707_night_mode[1], ARRAY_SIZE(hi707_night_mode[1]),  0,	MSM_CAMERA_I2C_BYTE_DATA},},
};

static int hi707_night_mode_enum_map[] = {
	MSM_V4L2_NIGHT_MODE_OFF ,
	MSM_V4L2_NIGHT_MODE_ON,
};

static struct msm_camera_i2c_enum_conf_array hi707_night_mode_enum_confs = {
	.conf = &hi707_night_mode_confs[0][0],
	.conf_enum = hi707_night_mode_enum_map,
	.num_enum = ARRAY_SIZE(hi707_night_mode_enum_map),
	.num_index = ARRAY_SIZE(hi707_night_mode_confs),
	.num_conf = ARRAY_SIZE(hi707_night_mode_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

/*                                                                   */
static struct msm_camera_i2c_reg_conf hi707_fps_range[][45] = {
	{		
		/*HI707_FPS_1500_1500 28 */  
	 	{0x03, 0x00}, 
	  	{0x09, 0x01},		
		{0x03, 0x20},	
		{0x10, 0x0c},	
		{0x18, 0x38},
		{0x03, 0x00},
#if defined(CONFIG_HI707_ROT_180) //180 degree rotation
             {0x11, 0x97}, //90}, // Fixed On
#else
		{0x11, 0x94}, //90}, // Fixed On
#endif
		{0x03, 0x00}, //PAGE 0
		{0x90, 0x07}, //BLC_TIME_TH_ON
		{0x91, 0x07}, //BLC_TIME_TH_OFF 
		{0x92, 0xa8}, //98}, //BLC_AG_TH_ON
		{0x93, 0xa0}, //90}, //BLC_AG_TH_OFF
		{0x03, 0x20}, //Page 20
		{0x2a, 0xf0}, //90}, //F0
		{0x2b, 0x35}, //F5}, //34
		{0x83, 0x01}, //EXP Normal 20.00 fps 
		{0x84, 0x24}, 
		{0x85, 0xf8},   
		{0x88, 0x01}, //EXP Max(120Hz) 17.14 fps 
		{0x89, 0x55}, 
		{0x8a, 0xcc}, 
		{0xa0, 0x01}, //EXP Max(100Hz) 16.67 fps 
		{0xa1, 0x5f}, 
		{0xa2, 0x90}, 
		{0x91, 0x01}, //EXP Fix 15.00 fps
		{0x92, 0x86}, 
		{0x93, 0xa0}, 		
		{0x03, 0x20},
		{0x10, 0xec}, //cc}, 20130524	
		{0x18, 0x30},		
	}, 
	{		
		/*HI707_FPS_1000_3000*/	
	 	{0x03, 0x00}, 
	  	{0x09, 0x01},		
		{0x03, 0x20},
		{0x10, 0x0c},	
		{0x18, 0x38},
		{0x03, 0x00},
#if defined(CONFIG_HI707_ROT_180)  //180 degree rotation
	{0x11, 0x93},	//VDOCTL2 , 90 : FFR off, 94 : FFR on
#else
	{0x11, 0x90},	//VDOCTL2 , 90 : FFR off, 94 : FFR on
#endif
		{0x03, 0x00}, //PAGE 0
		{0x90, 0x0c}, //BLC_TIME_TH_ON
		{0x91, 0x0c}, //BLC_TIME_TH_OFF 
		{0x92, 0xa8}, //98}, //BLC_AG_TH_ON
		{0x93, 0xa0}, //90}, //BLC_AG_TH_OFF
		{0x03, 0x20}, //Page 20
		{0x2a, 0xf0}, //90}, //F0
		{0x2b, 0x34}, //F5}, //34
		{0x83, 0x01}, //EXP Normal 20.00 fps 
		{0x84, 0x24}, 
		{0x85, 0xf8},  
		{0x88, 0x02}, //EXP Max(120Hz) 10.00 fps 
		{0x89, 0x49}, 
		{0x8a, 0xf0}, 
		{0xa0, 0x02}, //EXP Max(100Hz) 10.00 fps 
		{0xa1, 0x49}, 
		{0xa2, 0xf0},
		{0x03, 0x20},
		{0x10, 0xec}, //cc}, 20130524
		{0x18, 0x30},		  		 	
		{0x03, 0x00}, //dummy
		{0x03, 0x00}, //dummy
		{0x03, 0x00}, //dummy	
	},	
	{		
		/*HI707_FPS_3000_3000*/	
	  {0x03, 0x00}, 
	  {0x09, 0x01},		
		{0x03, 0x20},
		{0x10, 0x0c},	
		{0x18, 0x38},
		{0x03, 0x00},
#if defined(CONFIG_HI707_ROT_180) //180 degree rotation
		{0x11, 0x97}, //Fixed Off
#else
		{0x11, 0x94}, //Fixed Off
#endif
		{0x03, 0x00}, //PAGE 0
		{0x90, 0x04}, //BLC_TIME_TH_ON
		{0x91, 0x04}, //BLC_TIME_TH_OFF 
		{0x92, 0xa8}, //98}, //BLC_AG_TH_ON
		{0x93, 0xa0}, //90}, //BLC_AG_TH_OFF
		{0x03, 0x20}, //Page 20
		{0x2a, 0xf0}, //90}, //F0
		{0x2b, 0x35}, //F5}, //34
		{0x83, 0x00}, //EXP Normal 40.00 fps 
		{0x84, 0x92}, 
		{0x85, 0x7c},   
		{0x88, 0x00}, //EXP Max(120Hz) 30.00 fps 
		{0x89, 0xc3}, 
		{0x8a, 0x50}, 
		{0xa0, 0x00}, //EXP Max(100Hz) 33.33 fps 
		{0xa1, 0xaf}, 
		{0xa2, 0xc8},
		{0x91, 0x00}, //EXP Fix 29.94 fps
		{0x92, 0xc3}, 
		{0x93, 0xb4}, 
		{0x03, 0x20},
		{0x10, 0xec}, //cc}, 20130524
		{0x18, 0x30},		  		 	
	},
};

static struct msm_camera_i2c_conf_array hi707_fps_range_confs[][1] = {
	{{hi707_fps_range[0], ARRAY_SIZE(hi707_fps_range[0]),  0,	MSM_CAMERA_I2C_BYTE_DATA},},
	{{hi707_fps_range[1], ARRAY_SIZE(hi707_fps_range[1]),  0,	MSM_CAMERA_I2C_BYTE_DATA},},
	{{hi707_fps_range[2], ARRAY_SIZE(hi707_fps_range[2]),  0,	MSM_CAMERA_I2C_BYTE_DATA},},
};

static int hi707_fps_range_enum_map[] = {
	MSM_V4L2_FPS_15_15, 
	MSM_V4L2_FPS_7P5_30,
	MSM_V4L2_FPS_30_30,
};

static struct msm_camera_i2c_enum_conf_array hi707_fps_range_enum_confs = {
	.conf = &hi707_fps_range_confs[0][0],
	.conf_enum = hi707_fps_range_enum_map,
	.num_enum = ARRAY_SIZE(hi707_fps_range_enum_map),
	.num_index = ARRAY_SIZE(hi707_fps_range_confs),
	.num_conf = ARRAY_SIZE(hi707_fps_range_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};
/*                                                                   */


int hi707_effect_msm_sensor_s_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;

	pr_err("%s is called effect=%d\n", __func__, value);
	
	effect_value = value;
	if (effect_value == CAMERA_EFFECT_OFF) {
		rc = msm_sensor_write_conf_array(
			s_ctrl->sensor_i2c_client,
			s_ctrl->msm_sensor_reg->no_effect_settings, 0);
		if (rc < 0) {
			CDBG("write faield\n");
			return rc;
		}
	} else {
		rc = msm_sensor_write_enum_conf_array(
			s_ctrl->sensor_i2c_client,
			ctrl_info->enum_cfg_settings, value);
	}
	return rc;
}

int hi707_msm_sensor_s_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;

	//pr_err("%s is called enum num: %d , value = %d\n", __func__, ctrl_info->ctrl_id, value);
	switch(ctrl_info->ctrl_id) {
		case V4L2_CID_EXPOSURE:
			printk("%s : PREV_EXPOSURE = %d, value = %d\n", __func__, PREV_EXPOSURE, value);
			if(PREV_EXPOSURE == value || PREV_EXPOSURE == -1) {
				PREV_EXPOSURE = value;
				return rc;
			}else {
				PREV_EXPOSURE = value;
			}
			break;
			
		case V4L2_CID_SPECIAL_EFFECT:
			printk("%s : PREV_EFFECT = %d, value = %d\n", __func__, PREV_EFFECT, value);
			if(PREV_EFFECT == value || PREV_EFFECT == -1) {
				PREV_EFFECT = value;
				return rc;
			}else {
				PREV_EFFECT = value;
			}
			break;
			
		case V4L2_CID_WHITE_BALANCE_TEMPERATURE:
			printk("%s : PREV_WB = %d, value = %d\n", __func__, PREV_WB, value);
			if(PREV_WB == value || PREV_WB == -1) {
				PREV_WB = value;
				return rc;
			}else {
				PREV_WB = value;
			}
			break;
			
		case V4L2_CID_NIGHT_MODE:
			printk("%s : PREV_NIGHT_MODE = %d, value = %d\n", __func__, PREV_NIGHT_MODE, value);
			if(PREV_NIGHT_MODE == value || PREV_NIGHT_MODE == -1) {
				PREV_NIGHT_MODE = value;
				return rc;
			}else {
				PREV_NIGHT_MODE = value;
			}
			break;
			
		case V4L2_CID_FPS_RANGE:
			printk("%s : PREV_FPS = %d, value = %d\n", __func__, PREV_FPS, value);
			if(PREV_FPS == value || PREV_FPS == -1) {
				PREV_FPS = value;
				return rc;
			}else {
				PREV_FPS = value;
			}
			break;
		
		default:
			break;
	}

	rc = msm_sensor_write_enum_conf_array(
		s_ctrl->sensor_i2c_client,
		ctrl_info->enum_cfg_settings, value);
	if (rc < 0) {
		CDBG("write faield\n");
		return rc;
	}
	return rc;
}


struct msm_sensor_v4l2_ctrl_info_t hi707_v4l2_ctrl_info[] = {
	{
		.ctrl_id = V4L2_CID_EXPOSURE,
		.min = MSM_V4L2_EXPOSURE_N6,
		.max = MSM_V4L2_EXPOSURE_P6,
		.step = 1,
		.enum_cfg_settings = &hi707_exposure_enum_confs,
		.s_v4l2_ctrl = hi707_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_SPECIAL_EFFECT,
		.min = MSM_V4L2_EFFECT_OFF,
		.max = MSM_V4L2_EFFECT_NEGATIVE,
		.step = 1,
		.enum_cfg_settings = &hi707_special_effect_enum_confs,
		.s_v4l2_ctrl = hi707_effect_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_WHITE_BALANCE_TEMPERATURE,
		.min = MSM_V4L2_WB_OFF,
		.max = MSM_V4L2_WB_CLOUDY_DAYLIGHT,
		.step = 1,
		.enum_cfg_settings = &hi707_wb_oem_enum_confs,
		.s_v4l2_ctrl = hi707_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_NIGHT_MODE,
		.min = MSM_V4L2_NIGHT_MODE_OFF,
		.max = MSM_V4L2_NIGHT_MODE_ON,
		.step = 1,
		.enum_cfg_settings = &hi707_night_mode_enum_confs,
		.s_v4l2_ctrl = hi707_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_FPS_RANGE,
		.min = MSM_V4L2_FPS_15_15,
		.max = MSM_V4L2_FPS_30_30,
		.step = 1,
		.enum_cfg_settings = &hi707_fps_range_enum_confs,
		.s_v4l2_ctrl = hi707_msm_sensor_s_ctrl_by_enum,
	},
};

static struct msm_sensor_output_reg_addr_t hi707_reg_addr = {
	.x_output = 0x20,	//should be changed
	.y_output = 0x24,	//
	.line_length_pclk = 0x20,
	.frame_length_lines = 0x24,
};

static struct msm_sensor_id_info_t hi707_id_info = {
	.sensor_id_reg_addr = 0x04,
	.sensor_id = 0xB8,
};

static const struct i2c_device_id hi707_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&hi707_s_ctrl},
	{ }
};


static struct i2c_driver hi707_i2c_driver = {
	.id_table = hi707_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client hi707_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
};

static int __init msm_sensor_init_module(void)
{
	int rc = 0;
	CDBG("HI707\n");
	rc = i2c_add_driver(&hi707_i2c_driver);
	return rc;
}

static struct v4l2_subdev_core_ops hi707_subdev_core_ops = {
	.s_ctrl = msm_sensor_v4l2_s_ctrl,
	.queryctrl = msm_sensor_v4l2_query_ctrl,
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops hi707_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops hi707_subdev_ops = {
	.core = &hi707_subdev_core_ops,
	.video  = &hi707_subdev_video_ops,
};

int32_t hi707_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;

	pr_err("[%s]\n", __func__);

	//for PAGE MODE
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x03, 0x00, MSM_CAMERA_I2C_BYTE_DATA);

	rc = msm_camera_i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_id_info->sensor_id_reg_addr, &chipid,
			MSM_CAMERA_I2C_BYTE_DATA);
			
	if (rc < 0) {
		pr_err("%s: %s: read id failed\n", __func__,
			s_ctrl->sensordata->sensor_name);
		return rc;
	}

	pr_err("msm_sensor id: %d\n", chipid);
	if (chipid != s_ctrl->sensor_id_info->sensor_id) {
		pr_err("msm_sensor_match_id chip id doesnot match\n");
		return -ENODEV;
	}
	return rc;
}

static struct msm_cam_clk_info cam_clk_info[] = {
	{"cam_clk", MSM_SENSOR_MCLK_24HZ},
};

int32_t hi707_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
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


int32_t hi707_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
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

	/*                                                                                                      */
	s_ctrl->hi707Initcheck = 1;
	pr_err("%s %d\n", __func__, s_ctrl->hi707Initcheck);
	
	pr_err("%s: X\n", __func__); /*                                                              */

	return 0;
}


/*                                                                   */
#define AEC_ROI_DX (192) // (128)
#define AEC_ROI_DY (192) // (128) // (96)
/*
// default 256 X 256
{0x68, 0x41}, //AE_CEN
{0x69, 0x81},
{0x6A, 0x38},
{0x6B, 0xb8},

{0x03, 0x20}, // register page 20
{0x68, (Xstartpoint/4)},
{0x69, (Xendpoint/4)},
{0x6a, (Ystartpoint/2)},
{0x6b, (Yendpoint/2)},
*/
static int8_t hi707_set_aec_roi(struct msm_sensor_ctrl_t *s_ctrl, int32_t aec_roi)
{
	int16_t coordinate_x, coordinate_y;
	int16_t x_start, x_end, y_start, y_end;
	int32_t rc = 0;

	coordinate_x = ((aec_roi >> 16)&0xFFFF);
	coordinate_y = aec_roi&0xFFFF;
	
//	printk("%s : coordinate (%d, %d), front camera mirroring\n", __func__, coordinate_x, coordinate_y);

	if(coordinate_x == -1 && coordinate_y == -1) {
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
						SENSOR_REG_PAGE_ADDR,
						SENSOR_REG_PAGE_20,
						MSM_CAMERA_I2C_BYTE_DATA);
		/* AE weight */
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x60, 0x70, MSM_CAMERA_I2C_BYTE_DATA);
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x61, 0x00, MSM_CAMERA_I2C_BYTE_DATA);
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x62, 0x70, MSM_CAMERA_I2C_BYTE_DATA);
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x63, 0x00, MSM_CAMERA_I2C_BYTE_DATA);

		/* AE window */
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x68, 0x30, MSM_CAMERA_I2C_BYTE_DATA);
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x69, 0x6a, MSM_CAMERA_I2C_BYTE_DATA);
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x6a, 0x27, MSM_CAMERA_I2C_BYTE_DATA);
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x6b, 0xbb, MSM_CAMERA_I2C_BYTE_DATA);	

		if (rc < 0) {
				pr_err("%s: %s: failed\n", __func__,
					s_ctrl->sensordata->sensor_name);
				return rc;
		}
	}
	else {	

#ifdef CONFIG_HI707_ROT_180
		coordinate_x = SENSOR_PREVIEW_WIDTH - coordinate_x; 
		coordinate_y = SENSOR_PREVIEW_HEIGHT -coordinate_y;
#endif

		x_start = ((coordinate_x - (AEC_ROI_DX/2) > 0)? coordinate_x - (AEC_ROI_DX/2) : 0)/4;
		x_end = ((coordinate_x + (AEC_ROI_DX/2) < SENSOR_PREVIEW_WIDTH)? coordinate_x + (AEC_ROI_DX/2) : SENSOR_PREVIEW_WIDTH)/4;

		y_start = ((coordinate_y - (AEC_ROI_DY/2) > 0)? coordinate_y - (AEC_ROI_DY/2) : 0)/2;
		y_end = ((coordinate_y + (AEC_ROI_DY/2) < SENSOR_PREVIEW_HEIGHT)? coordinate_y + (AEC_ROI_DY/2) : SENSOR_PREVIEW_HEIGHT)/2;
		
		printk("%s : (%d, %d), (%d, %d)\n", __func__, x_start, y_start, x_end, y_end);
		
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, SENSOR_REG_PAGE_ADDR, SENSOR_REG_PAGE_20, MSM_CAMERA_I2C_BYTE_DATA);

		/* AE weight */
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x60, 0x70, MSM_CAMERA_I2C_BYTE_DATA);
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x61, 0x00, MSM_CAMERA_I2C_BYTE_DATA);
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x62, 0x70, MSM_CAMERA_I2C_BYTE_DATA);
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x63, 0x00, MSM_CAMERA_I2C_BYTE_DATA);

		/* AE window */
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x68, x_start, MSM_CAMERA_I2C_BYTE_DATA);
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x69, x_end, MSM_CAMERA_I2C_BYTE_DATA);
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x6a, y_start, MSM_CAMERA_I2C_BYTE_DATA);
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x6b, y_end, MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0) {
				pr_err("%s: %s: failed\n", __func__,
					s_ctrl->sensordata->sensor_name);
				return rc;
		}
	}
	return rc;
}
/*                                                                   */

/*                                                                          */
int8_t hi707_get_snapshot_data(struct msm_sensor_ctrl_t *s_ctrl, struct snapshot_soc_data_cfg *snapshot_data) {
	int rc = 0;
	u16 analogGain = 0;
	u32 exposureTime = 0;
	u32 isoSpeed = 0;
	u16 Exposure1 = 0;
	u16 Exposure2 = 0;
	u16 Exposure3 = 0;
	u32 ExposureTotal = 0;
	
	//ISO Speed
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x03, 0x20, MSM_CAMERA_I2C_BYTE_DATA);	
	rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0xb0, &analogGain, MSM_CAMERA_I2C_BYTE_DATA);	

	if (rc < 0) {
		pr_err("%s: error to get analog & digital gain \n", __func__);
		return rc;
	}
	//Exposure Time
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x03, 0x20, MSM_CAMERA_I2C_BYTE_DATA);
	rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x80, &Exposure1, MSM_CAMERA_I2C_BYTE_DATA);
	rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x81, &Exposure2, MSM_CAMERA_I2C_BYTE_DATA);
	rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x82, &Exposure3, MSM_CAMERA_I2C_BYTE_DATA);

	
	if (rc < 0) {
		pr_err("%s: error to get exposure time \n", __func__);
		return rc;
	}

	if( analogGain <= 0x28 ){
		//printk("[CHECK]%s : iso speed - analogGain = 0x%x/n",  __func__, analogGain);
		analogGain = 0x28;  		//analogGain cannot move down than 0x28
	}
	//ISO speed
	if (analogGain > 0)
		isoSpeed = ((analogGain / 32) * 100);
	else
	      isoSpeed = 100;
#if 0
	//Exposure Time
	ExposureTotal = ((Exposure1<<16) * 524288)|((Exposure2<<8) * 2048)|((Exposure3<<2) * 8);

	if (ExposureTotal <= 0) {
		exposureTime = 600000;
	}else {
		exposureTime = ExposureTotal;
	}
#else
	ExposureTotal = ((Exposure1<<16))|((Exposure2<<8))|((Exposure3<<0));
	if (ExposureTotal > 0)
		ExposureTotal = ExposureTotal * 8 * 83 / 1000000;
	else
		ExposureTotal = 30;

	exposureTime = ExposureTotal;
#endif


	snapshot_data->iso_speed = isoSpeed;
	snapshot_data->exposure_time = exposureTime;
	printk("[CHECK]Camera Snapshot Data iso_speed = %d, exposure_time = %d \n", snapshot_data->iso_speed, snapshot_data->exposure_time); 

	return 0;
	
}
/*                                                                          */

#if 1
/*                                                                           */
static int8_t hi707_set_awb_lock(struct msm_sensor_ctrl_t *s_ctrl, int32_t soc_awb_lock)
{
	int32_t rc = 0;
	int16_t temp = 0;

	if(PREV_SOC_AWB_LOCK != soc_awb_lock)
		PREV_SOC_AWB_LOCK = soc_awb_lock;
	else if(PREV_SOC_AWB_LOCK == soc_awb_lock)
		return rc;

	if(PREV_SOC_AWB_LOCK == AWB_LOCK_ON){
		pr_err("[F3] [F3] %s soc_awb_lock %d\n", __func__, soc_awb_lock);

		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x03, 0x22, MSM_CAMERA_I2C_BYTE_DATA);	//page mode
		rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client,	0x10, &temp, MSM_CAMERA_I2C_BYTE_DATA);
		temp = temp & 0x7f; //[7]bit set as '0'
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x10, temp, MSM_CAMERA_I2C_BYTE_DATA);
	}
	else if(PREV_SOC_AWB_LOCK == AWB_LOCK_OFF){
		pr_err("[F3] [F3] %s soc_awb_unlock %d\n", __func__, soc_awb_lock);

		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x03, 0x22, MSM_CAMERA_I2C_BYTE_DATA);	//page mode
		rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client,	0x10, &temp, MSM_CAMERA_I2C_BYTE_DATA);
		temp = temp | 0x80; //[7]bit set as '1'
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x10, temp, MSM_CAMERA_I2C_BYTE_DATA);
	}
	
	return rc;
}
/*                                                                           */


/*                                                                           */
static int8_t hi707_set_aec_lock(struct msm_sensor_ctrl_t *s_ctrl, int32_t soc_aec_lock)
{
	int32_t rc = 0;
	int16_t temp = 0;

	if(PREV_SOC_AEC_LOCK != soc_aec_lock)
		PREV_SOC_AEC_LOCK = soc_aec_lock;
	else if(PREV_SOC_AEC_LOCK == soc_aec_lock)
		return rc;

	if(PREV_SOC_AEC_LOCK == AEC_LOCK_ON){
		pr_err("[F3] [F3] %s soc_aec_lock %d\n", __func__, soc_aec_lock);

		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x03, 0x20, MSM_CAMERA_I2C_BYTE_DATA);	//page mode
		rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client,	0x10, &temp, MSM_CAMERA_I2C_BYTE_DATA);
		temp = temp & 0x7f; //[7]bit set as '0'
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x10, temp, MSM_CAMERA_I2C_BYTE_DATA);
	}
	else if(PREV_SOC_AEC_LOCK == AEC_LOCK_OFF){
		pr_err("[F3] [F3] %s soc_aec_unlock %d\n", __func__, soc_aec_lock);

		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x03, 0x20, MSM_CAMERA_I2C_BYTE_DATA);	//page mode
		rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client,	0x10, &temp, MSM_CAMERA_I2C_BYTE_DATA);
		temp = temp | 0x80; //[7]bit set as '1'
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x10, temp, MSM_CAMERA_I2C_BYTE_DATA);
	}
	
	return rc;
}
/*                                                                           */
#endif

static struct msm_sensor_fn_t hi707_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
#if 1  /* VFE 3.X */
	.sensor_setting = msm_sensor_setting,
#else   /* VFE 2.X */
	.sensor_csi_setting = msm_sensor_setting1,
#endif
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = hi707_sensor_power_up,
	.sensor_power_down = hi707_sensor_power_down,
	.sensor_get_csi_params = msm_sensor_get_csi_params,
	.sensor_match_id = hi707_sensor_match_id,
	.sensor_set_aec_roi = hi707_set_aec_roi,
	.sensor_get_soc_snapshotdata = hi707_get_snapshot_data,	/*                                                                          */
    .sensor_set_soc_awb_lock = hi707_set_awb_lock,	/*                                                                         */
    .sensor_set_soc_aec_lock = hi707_set_aec_lock,	/*                                                                         */
};

static struct msm_sensor_reg_t hi707_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = hi707_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(hi707_start_settings),
	.entrance_start_stream_conf = hi707_entrance_start_settings,					/*                                                                                                      */
	.entrance_start_stream_conf_size = ARRAY_SIZE(hi707_entrance_start_settings),   /*                                                                                                      */
	.stop_stream_conf = hi707_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(hi707_stop_settings),
	.init_settings = &hi707_init_conf[0],
	.init_size = ARRAY_SIZE(hi707_init_conf),
	.mode_settings = &hi707_confs[0],
	.no_effect_settings = &hi707_no_effect_confs[0],
	.output_settings = &hi707_dimensions[0],
	.num_conf = ARRAY_SIZE(hi707_confs),
};

static struct msm_sensor_ctrl_t hi707_s_ctrl = {
	.hi707Initcheck = 1,		/*                                                                                                      */
	.msm_sensor_reg = &hi707_regs,
	.msm_sensor_v4l2_ctrl_info = hi707_v4l2_ctrl_info,
	.num_v4l2_ctrl = ARRAY_SIZE(hi707_v4l2_ctrl_info),
	.sensor_i2c_client = &hi707_sensor_i2c_client,
	.sensor_i2c_addr = 0x60,
	.sensor_output_reg_addr = &hi707_reg_addr,
	.sensor_id_info = &hi707_id_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.msm_sensor_mutex = &hi707_mut,
	.sensor_i2c_driver = &hi707_i2c_driver,
	.sensor_v4l2_subdev_info = hi707_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(hi707_subdev_info),
	.sensor_v4l2_subdev_ops = &hi707_subdev_ops,
	.func_tbl = &hi707_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("hi707 VT sensor driver");
MODULE_LICENSE("GPL v2");
