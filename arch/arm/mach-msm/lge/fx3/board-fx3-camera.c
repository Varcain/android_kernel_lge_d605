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

#include <asm/mach-types.h>
#include <linux/gpio.h>
#include <mach/camera.h>
#include <mach/msm_bus_board.h>
#include <mach/gpiomux.h>
#include "devices.h"
#if !defined(CONFIG_MACH_MSM8930_FX3)
#include "board-8930.h"
#else /*               */
#include <mach/board_lge.h>
#include "board-fx3.h"
#endif

#ifdef CONFIG_LEDS_AS364X
#include <linux/leds-as364x.h>
#endif

#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3_TMUS) || defined(CONFIG_MACH_LGE_FX3_ATT) || defined(CONFIG_MACH_LGE_FX3_WCDMA_TRF_US) || defined(CONFIG_MACH_LGE_L9II_COMMON) \
	|| defined(CONFIG_F6_CAM8M) || defined(CONFIG_MACH_LGE_FX3Q_TMUS)
//                                                                                 
#if defined(CONFIG_MACH_LGE_L9II_COMMON) && defined(CONFIG_BACKLIGHT_LM3639)
#ifdef CONFIG_MACH_LGE_L9II_OPEN_EU_REV_A
#define GPIO_CAM_FLASH_LED_EN	(3)
#else
#define GPIO_CAM_FLASH_LED_EN	(2)
#endif
#else
#define GPIO_CAM_FLASH_AS2C	(2)
#define GPIO_CAM_FLASH_LED_EN	(3)
#endif
//                                                                                 
#define GPIO_CAM_VT_CAM_MCLK	(4)
#define GPIO_CAM_MAIN_CAM_MCLK	(5)
#define GPIO_CAM_I2C_SDA	(20)
#define GPIO_CAM_I2C_SCL	(21)
#define GPIO_CAM_AF_LDO_EN	(50)  /* REV.A change to CAM_SUBPM_EN */
#define GPIO_CAM_LDO_EN_1	(51)  /* REV.A Doesn't use */
#define GPIO_CAM_LDO_EN_2	(51)  // tied with EN1 (52)->(51)  /* REV.A Doesn't use */
#define GPIO_CAM_MAIN_CAM_PWDN	(53)  /* REV.A Doesn't use */
#define GPIO_CAM_VT_CAM_PWDN	(54)
#define GPIO_CAM_VCM_PWDN	(54) /* F6 EU */
#define GPIO_CAM_VT_CAM_RST_N	(76)
#define GPIO_CAM_MAIN_CAM_RST_N	(107)
/*                                                                 */
#if defined (CONFIG_IMX111)
#define I2C_SLAVE_ADDR_MAIN_CAM	(0x0D)
#elif defined (CONFIG_S5K4E5YA) //For F3Q TMUS
#define I2C_SLAVE_ADDR_MAIN_CAM	(0x20)
#else
#define I2C_SLAVE_ADDR_MAIN_CAM	(0x40)
#endif
/*                                                                 */
#define I2C_SLAVE_ADDR_ACTUATOR	(0x18)
/*                                                                */
#ifdef CONFIG_IMX119
#define I2C_SLAVE_ADDR_VT_CAM	(0x6e)
#else
#define I2C_SLAVE_ADDR_VT_CAM	(0x60)
#endif
#ifdef CONFIG_LEDS_AS364X
#define I2C_SLAVE_ADDR_AS3647   (0x30)
#endif
#ifdef CONFIG_IMX119
#define I2C_SLAVE_ADDR_VT_CAM	(0x6e)
#endif
/*                                                                */
/*                                                                 */
#if defined (CONFIG_MSM_CAMERA_FLASH_LM3559)
#define I2C_SLAVE_ADDR_LM3559 	(0x53)
#endif
/*                                                                 */
/*                                                                                   */
#if defined (CONFIG_MSM_CAMERA_FLASH_LM3639)
#define I2C_SLAVE_ADDR_LM3639 	(0x39)
#endif
/*                                                                                   */
#else
#define GPIO_CAM_FLASH_LED_EN	(2)
#define GPIO_CAM_FLASH_LED_STROBE	(3)
#define GPIO_CAM_VT_CAM_MCLK	(4)
#define GPIO_CAM_MAIN_CAM_MCLK	(5)
#define GPIO_CAM_I2C_SDA	(20)
#define GPIO_CAM_I2C_SCL	(21)
#define GPIO_CAM_VT_CAM_PWDN	(54)
#define GPIO_CAM_VT_CAM_RST_N	(76)
#define GPIO_CAM_MAIN_CAM_RST_N	(107)
#define I2C_SLAVE_ADDR_MAIN_CAM	(0x20)
#define I2C_SLAVE_ADDR_ACTUATOR	(0x18)
#define I2C_SLAVE_ADDR_EEPROM	(0x28) /*                                                                        */

/*                                                                */
#ifdef CONFIG_IMX119
#define I2C_SLAVE_ADDR_VT_CAM	(0x6e)
#else
#define I2C_SLAVE_ADDR_VT_CAM	(0x60)
#endif
/*                                                                */
#ifdef CONFIG_LEDS_AS364X
#define I2C_SLAVE_ADDR_AS3647   (0x30)
#endif
#endif

#ifdef CONFIG_MSM_CAMERA


static struct gpiomux_setting cam_settings[] = {
	{
		.func = GPIOMUX_FUNC_GPIO, /*suspend*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_DOWN,
	},

	{
		.func = GPIOMUX_FUNC_1, /*active 1*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},

	{
		.func = GPIOMUX_FUNC_GPIO, /*active 2*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},

	{
		.func = GPIOMUX_FUNC_1, /*active 3*/
		.drv = GPIOMUX_DRV_8MA,
		.pull = GPIOMUX_PULL_NONE,
	},

	{
		.func = GPIOMUX_FUNC_5, /*active 4*/
		.drv = GPIOMUX_DRV_8MA,
		.pull = GPIOMUX_PULL_UP,
	},

	{
		.func = GPIOMUX_FUNC_6, /*active 5*/
		.drv = GPIOMUX_DRV_8MA,
		.pull = GPIOMUX_PULL_UP,
	},

	{
		.func = GPIOMUX_FUNC_2, /*active 6*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_UP,
	},

	{
		.func = GPIOMUX_FUNC_3, /*active 7*/
		.drv = GPIOMUX_DRV_8MA,
		.pull = GPIOMUX_PULL_UP,
	},

	{
		.func = GPIOMUX_FUNC_GPIO, /*i2c suspend*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_KEEPER,
	},
	{
		.func = GPIOMUX_FUNC_2, /*active 9*/
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},
/*                                                               */
	{
		.func = GPIOMUX_FUNC_2, /*active 10*/
		.drv = GPIOMUX_DRV_6MA,
		.pull = GPIOMUX_PULL_NONE,
	},

	{
		.func = GPIOMUX_FUNC_1, /*active 11*/
		.drv = GPIOMUX_DRV_4MA,
		.pull = GPIOMUX_PULL_NONE,
	},
/*                                                               */
};

#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS)
static struct msm_gpiomux_config msm8930_cam_common_configs[] = {
	{
		.gpio = GPIO_CAM_FLASH_AS2C, /* GPIO_2 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_CAM_FLASH_LED_EN, /* GPIO_3 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[1],
			//[GPIOMUX_SUSPENDED] = &cam_settings[0],
			[GPIOMUX_SUSPENDED] = &cam_settings[2],   //for sleep current , cam_settings[0], -> cam_settings[2],
		},
	},
	{
		.gpio = GPIO_CAM_VT_CAM_MCLK, /* GPIO_4 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[10],  /*to enhance VT MCLK, driving current should be increased. 2mA -> 6mA */
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_CAM_MAIN_CAM_MCLK, /* GPIO_5 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[11], /*                                                                                                      */
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_CAM_VT_CAM_RST_N,  /* GPIO_76 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_CAM_MAIN_CAM_RST_N, /* GPIO_107 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_CAM_LDO_EN_1,  /* GPIO_51 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_CAM_LDO_EN_2,  /* GPIO_52 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_CAM_MAIN_CAM_PWDN,  /* GPIO_53 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_CAM_VT_CAM_PWDN,  /* GPIO_54 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
};

static struct msm_gpiomux_config msm8930_cam_2d_configs[] = {
	{
		.gpio = GPIO_CAM_I2C_SDA,  /* GPIO_20 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			//[GPIOMUX_SUSPENDED] = &cam_settings[8],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],   // for sleep current, cam_settings[8] -> cam_settings[0]
		},
	},
	{
		.gpio = GPIO_CAM_I2C_SCL,  /* GPIO_21 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			//[GPIOMUX_SUSPENDED] = &cam_settings[8],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],   // for sleep current, cam_settings[8] -> cam_settings[0]
		},
	},
};
#elif defined(CONFIG_MACH_LGE_FX3_TMUS) || defined(CONFIG_MACH_LGE_FX3_ATT) || defined(CONFIG_MACH_LGE_FX3_WCDMA_TRF_US) || defined(CONFIG_MACH_LGE_L9II_COMMON) \
	|| defined(CONFIG_F6_CAM8M)
static struct msm_gpiomux_config msm8930_cam_common_configs[] = {
//                                                                                 
#if !(defined(CONFIG_MACH_LGE_L9II_COMMON) && defined(CONFIG_BACKLIGHT_LM3639))
	{
		.gpio = GPIO_CAM_FLASH_AS2C, /* GPIO_2 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_CAM_FLASH_LED_EN, /* GPIO_3 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[1],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
#endif
//                                                                                 
	{
		.gpio = GPIO_CAM_VT_CAM_MCLK, /* GPIO_4 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[9],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_CAM_MAIN_CAM_MCLK, /* GPIO_5 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[1],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_CAM_VT_CAM_RST_N,  /* GPIO_76 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_CAM_MAIN_CAM_RST_N, /* GPIO_107 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_CAM_LDO_EN_1,  /* GPIO_51 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
#if !defined(CONFIG_MACH_LGE_L9II_COMMON)
#if !defined(CONFIG_F6_CAM8M)
	{
		.gpio = GPIO_CAM_LDO_EN_2,  /* GPIO_52 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_CAM_MAIN_CAM_PWDN,  /* GPIO_53 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
#endif
	{
		.gpio = GPIO_CAM_VT_CAM_PWDN,  /* GPIO_54 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
#endif
};

static struct msm_gpiomux_config msm8930_cam_2d_configs[] = {
	{
		.gpio = GPIO_CAM_I2C_SDA,  /* GPIO_20 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[8],
		},
	},
	{
		.gpio = GPIO_CAM_I2C_SCL,  /* GPIO_21 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[8],
		},
	},
};
#else
static struct msm_gpiomux_config msm8930_cam_common_configs[] = {
	{
		.gpio = GPIO_CAM_FLASH_LED_EN, /* GPIO_2 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_CAM_FLASH_LED_STROBE, /* GPIO_3 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[1],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_CAM_VT_CAM_MCLK, /* GPIO_4 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[9],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_CAM_MAIN_CAM_MCLK, /* GPIO_5 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[1],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_CAM_VT_CAM_RST_N,  /* GPIO_76 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_CAM_MAIN_CAM_RST_N, /* GPIO_107 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
	{
		.gpio = GPIO_CAM_VT_CAM_PWDN,  /* GPIO_54 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[2],
			[GPIOMUX_SUSPENDED] = &cam_settings[0],
		},
	},
};

static struct msm_gpiomux_config msm8930_cam_2d_configs[] = {
	{
		.gpio = GPIO_CAM_I2C_SDA,  /* GPIO_20 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[8],
		},
	},
	{
		.gpio = GPIO_CAM_I2C_SCL,  /* GPIO_21 */
		.settings = {
			[GPIOMUX_ACTIVE]    = &cam_settings[3],
			[GPIOMUX_SUSPENDED] = &cam_settings[8],
		},
	},
};
#endif


/*                                                           */
#if 0
#define VFE_CAMIF_TIMER1_GPIO 2
#define VFE_CAMIF_TIMER2_GPIO 3
#define VFE_CAMIF_TIMER3_GPIO_INT 4
static struct msm_camera_sensor_strobe_flash_data strobe_flash_xenon = {
	.flash_trigger = VFE_CAMIF_TIMER2_GPIO,
	.flash_charge = VFE_CAMIF_TIMER1_GPIO,
	.flash_charge_done = VFE_CAMIF_TIMER3_GPIO_INT,
	.flash_recharge_duration = 50000,
	.irq = MSM_GPIO_TO_INT(VFE_CAMIF_TIMER3_GPIO_INT),
};
#endif
/*                                                           */

static struct msm_bus_vectors cam_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
};

static struct msm_bus_vectors cam_preview_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 27648000,
		.ib  = 2656000000UL,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
};
/*                                                                                    */
#if defined(CONFIG_MACH_LGE_L9II_COMMON) || defined(CONFIG_F6_CAM8M)
static struct msm_bus_vectors cam_video_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 356728320, /* Change Arbitrated bandwidth 274406400*1.3*/
		.ib  = 2656000000UL,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 206807040,
		.ib  = 488816640,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
};

#else
static struct msm_bus_vectors cam_video_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 274406400,
		.ib  = 2656000000UL,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 206807040,
		.ib  = 488816640,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
};
#endif
/*                                                                                    */

static struct msm_bus_vectors cam_snapshot_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 600000000,
		.ib  = 2656000000UL,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 540000000,
		.ib  = 1350000000,
	},
};

static struct msm_bus_vectors cam_zsl_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 600000000,
		.ib  = 2656000000UL,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 540000000,
		.ib  = 1350000000,
	},
};

static struct msm_bus_vectors cam_video_ls_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 600000000,
		.ib  = 4264000000UL,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 206807040,
		.ib  = 488816640,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 540000000,
		.ib  = 1350000000,
	},
};

static struct msm_bus_vectors cam_dual_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 302071680,
		.ib  = 2656000000UL,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 206807040,
		.ib  = 488816640,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 540000000,
		.ib  = 1350000000,
	},
};

static struct msm_bus_vectors cam_adv_video_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 274406400,
		.ib  = 2656000000UL,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 206807040,
		.ib  = 488816640,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
};


static struct msm_bus_paths cam_bus_client_config[] = {
	{
		ARRAY_SIZE(cam_init_vectors),
		cam_init_vectors,
	},
	{
		ARRAY_SIZE(cam_preview_vectors),
		cam_preview_vectors,
	},
	{
		ARRAY_SIZE(cam_video_vectors),
		cam_video_vectors,
	},
	{
		ARRAY_SIZE(cam_snapshot_vectors),
		cam_snapshot_vectors,
	},
	{
		ARRAY_SIZE(cam_zsl_vectors),
		cam_zsl_vectors,
	},
	{
		ARRAY_SIZE(cam_video_ls_vectors),
		cam_video_ls_vectors,
	},
	{
		ARRAY_SIZE(cam_dual_vectors),
		cam_dual_vectors,
	},
	{
		ARRAY_SIZE(cam_adv_video_vectors),
		cam_adv_video_vectors,
	},

};

static struct msm_bus_scale_pdata cam_bus_client_pdata = {
		cam_bus_client_config,
		ARRAY_SIZE(cam_bus_client_config),
		.name = "msm_camera",
};

static struct msm_camera_device_platform_data msm_camera_csi_device_data[] = {
	{
		.csid_core = 0,
		.is_vpe    = 1,
		.cam_bus_scale_table = &cam_bus_client_pdata,
	},
	{
		.csid_core = 1,
		.is_vpe    = 1,
		.cam_bus_scale_table = &cam_bus_client_pdata,
	},
};

static struct camera_vreg_t msm_8930_back_cam_vreg[] = {
#if !(defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3_TMUS) || defined(CONFIG_MACH_LGE_FX3_WCDMA_TRF_US) || defined(CONFIG_MACH_LGE_L9II_COMMON) \
	|| defined(CONFIG_F6_CAM8M))
	{"cam_vdig", REG_LDO, 1580000, 1600000, 100000},  /* VDIG(L17): 1.8V */  /*                                                                                 */
	{"cam_vana", REG_LDO, 2800000, 2850000, 85600},  /* VANA(L9) : 2.8V */
	{"cam_vio", REG_VS, 0, 0, 0},  /* VDDIO(LVS2) : 1.8V */
#else
/*                                                                 */
#if defined (CONFIG_IMX111)
	{"cam_vio", REG_VS, 0, 0, 0},
	{"cam_vana", REG_LDO, 2800000, 2850000, 85600},
#if !defined(CONFIG_F6_CAM8M_HW10) && defined(CONFIG_F6_CAM8M)
	{"cam_vdig", REG_LDO, 1200000, 1200000, 105000, 500},
#else
	{"cam_vdig", REG_LDO, 1200000, 1200000, 105000},
#endif
#if !defined (CONFIG_F6_CAM8M)
	{"cam_vaf", REG_LDO, 2800000, 2800000, 300000},
#endif
#else
	{"cam_vio", REG_VS, 0, 0, 0},  /* VDDIO(LVS2) : 1.8V */
	{"cam_vana", REG_LDO, 2800000, 2850000, 85600},  /* VANA(L9) : 2.8V */
	{"cam_vdig", REG_LDO, 1800000, 1800000, 120000},  /* VDIG(L17): 1.8V */	/*                                                                                 */
#endif
/*                                                                 */
#endif
};

static struct camera_vreg_t msm_8930_front_cam_vreg[] = {
/*                                                                */
	#ifdef CONFIG_IMX119
	{"cam_vio", REG_VS, 0, 0, 0},  /* VDDIO(LVS2) : 1.8V */
	{"cam_vana", REG_LDO, 2700000, 2700000, 85600},  /* VANA(L9) : 2.8V */
	{"cam_vdig", REG_LDO, 1200000, 1200000, 105000},  /* VDIG(L12): 1.2V */	
	#else
/*                                                                */
	{"cam_vio", REG_VS, 0, 0, 0},  /* VDDIO(LVS2) : 1.8V */
	{"cam_vana", REG_LDO, 2850000, 2850000, 85600},  /* VANA(L9) : 2.8V */
	{"cam_vdig", REG_LDO, 1800000, 1800000, 85600},  /* VDIG(L17): 1.8V */
	#endif

};

static struct gpio msm8930_common_cam_gpio[] = {
	{GPIO_CAM_VT_CAM_MCLK, GPIOF_DIR_IN, "CAMIF_MCLK"},
	{GPIO_CAM_MAIN_CAM_MCLK, GPIOF_DIR_IN, "CAMIF_MCLK"},
	{GPIO_CAM_I2C_SDA, GPIOF_DIR_IN, "CAMIF_I2C_DATA"},
	{GPIO_CAM_I2C_SCL, GPIOF_DIR_IN, "CAMIF_I2C_CLK"},
};

static struct gpio msm8930_front_cam_gpio[] = {
	{GPIO_CAM_VT_CAM_RST_N, GPIOF_DIR_OUT, "CAM_RESET"},
#ifndef CONFIG_IMX119  /*                                                                                                                 */
	{GPIO_CAM_VT_CAM_PWDN, GPIOF_DIR_OUT, "CAM_PWDN"},
#endif
};

static struct gpio msm8930_back_cam_gpio[] = {
	{GPIO_CAM_MAIN_CAM_RST_N, GPIOF_DIR_OUT, "CAM_RESET"},
};

static struct msm_gpio_set_tbl msm8930_front_cam_gpio_set_tbl[] = {
/*                                                                */
#ifdef CONFIG_IMX119
	{GPIO_CAM_VT_CAM_RST_N, GPIOF_OUT_INIT_LOW, 1000},
	{GPIO_CAM_VT_CAM_RST_N, GPIOF_OUT_INIT_HIGH, 2000},
#endif
/*                                                                */
};

static struct msm_gpio_set_tbl msm8930_back_cam_gpio_set_tbl[] = {
#if !(defined(CONFIG_MACH_LGE_FX3_VZW) /*                                     */ || defined (CONFIG_MACH_LGE_FX3_TMUS) || defined(CONFIG_MACH_LGE_FX3_WCDMA_TRF_US))
	{GPIO_CAM_MAIN_CAM_RST_N, GPIOF_OUT_INIT_LOW, 1000},
	{GPIO_CAM_MAIN_CAM_RST_N, GPIOF_OUT_INIT_HIGH, 2000},
#elif (defined(CONFIG_MACH_LGE_FX3Q_TMUS) || (defined(CONFIG_MACH_LGE_FX3_VZW) && defined(CONFIG_S5K4E5YA)))
	{GPIO_CAM_MAIN_CAM_RST_N, GPIOF_OUT_INIT_LOW, 1000},
	{GPIO_CAM_MAIN_CAM_RST_N, GPIOF_OUT_INIT_HIGH, 2000},
#endif
};

static struct msm_camera_gpio_conf msm_8930_front_cam_gpio_conf = {
	.cam_gpiomux_conf_tbl = msm8930_cam_2d_configs,
	.cam_gpiomux_conf_tbl_size = ARRAY_SIZE(msm8930_cam_2d_configs),
	.cam_gpio_common_tbl = msm8930_common_cam_gpio,
	.cam_gpio_common_tbl_size = ARRAY_SIZE(msm8930_common_cam_gpio),
	.cam_gpio_req_tbl = msm8930_front_cam_gpio,
	.cam_gpio_req_tbl_size = ARRAY_SIZE(msm8930_front_cam_gpio),
	.cam_gpio_set_tbl = msm8930_front_cam_gpio_set_tbl,
	.cam_gpio_set_tbl_size = ARRAY_SIZE(msm8930_front_cam_gpio_set_tbl),
};

static struct msm_camera_gpio_conf msm_8930_back_cam_gpio_conf = {
	.cam_gpiomux_conf_tbl = msm8930_cam_2d_configs,
	.cam_gpiomux_conf_tbl_size = ARRAY_SIZE(msm8930_cam_2d_configs),
	.cam_gpio_common_tbl = msm8930_common_cam_gpio,
	.cam_gpio_common_tbl_size = ARRAY_SIZE(msm8930_common_cam_gpio),
	.cam_gpio_req_tbl = msm8930_back_cam_gpio,
	.cam_gpio_req_tbl_size = ARRAY_SIZE(msm8930_back_cam_gpio),
	.cam_gpio_set_tbl = msm8930_back_cam_gpio_set_tbl,
	.cam_gpio_set_tbl_size = ARRAY_SIZE(msm8930_back_cam_gpio_set_tbl),
};

static struct i2c_board_info msm_act_main_cam_i2c_info = {
	I2C_BOARD_INFO("msm_actuator", I2C_SLAVE_ADDR_ACTUATOR),
};

/*                                                                                 */
#if defined(CONFIG_HI543)
static struct msm_actuator_info msm_act_main_cam_0_info = {
	.board_info     = &msm_act_main_cam_i2c_info,
	.cam_name   = MSM_ACTUATOR_MAIN_CAM_0,
	.bus_id         = MSM_8930_GSBI4_QUP_I2C_BUS_ID,
	.vcm_pwd        = 0,
	.vcm_enable     = 1,
};
/*                                                                                */
#elif defined (CONFIG_IMX111)
static struct msm_actuator_info msm_act_main_cam_1_info = {
	.board_info     = &msm_act_main_cam_i2c_info,
	.cam_name   = MSM_ACTUATOR_MAIN_CAM_1,
	.bus_id         = MSM_8930_GSBI4_QUP_I2C_BUS_ID,
#if defined(CONFIG_F6_CAM8M)
	.vcm_pwd        = GPIO_CAM_VCM_PWDN,
#else
	.vcm_pwd        = 0,
#endif
	.vcm_enable     = 1,
};
/*                                                                                */
#else
static struct msm_actuator_info msm_act_main_cam_2_info = {
	.board_info     = &msm_act_main_cam_i2c_info,
	.cam_name   = MSM_ACTUATOR_MAIN_CAM_2,
	.bus_id         = MSM_8930_GSBI4_QUP_I2C_BUS_ID,
#if defined(CONFIG_F6_CAM8M)
	.vcm_pwd        = GPIO_CAM_VCM_PWDN,
#else
	.vcm_pwd        = 0,
#endif
	.vcm_enable     = 1,
};
#endif
/*                                                                                 */

#ifdef CONFIG_LEDS_AS364X
static struct msm_camera_sensor_flash_src led_flash_src = {
	.flash_sr_type = MSM_CAMERA_FLASH_SRC_CURRENT_DRIVER,
};

static struct as364x_platform_data as364x_flash_pdata = {
	.use_tx_mask = 1,
	.I_limit_mA = 2500,
	.txmasked_current_mA = 200,
	.vin_low_v_run_mV = 3300,
	.vin_low_v_mV = 3000,
	.strobe_type = 0, /* 0=edge */
#ifdef CONFIG_AS3648
	.max_peak_current_mA = 1000,
	.max_peak_duration_ms = 500,/* values for LUW FQ6N @1000mA */
#endif
#ifdef CONFIG_AS3647
	.max_peak_current_mA = 1600, /* LUW FQ6N can take up to 2000 */
	.max_peak_duration_ms = 100,/* value for LUW FQ6N @1600mA */
#endif
	.max_sustained_current_mA = 200,
	.min_current_mA = 50,
	.freq_switch_on = 0,
	.led_off_when_vin_low = 1, /* if 0 txmask current is used */
};
#endif

/*                                                                 */
#if defined (CONFIG_IMX111)
static struct msm_camera_sensor_flash_data flash_imx111 = {
	.flash_type	= MSM_CAMERA_FLASH_LED,
#ifdef CONFIG_MSM_CAMERA_FLASH
#ifdef CONFIG_F6_CAM8M
#ifdef CONFIG_LEDS_AS364X
	.flash_src	= &led_flash_src,
#else
	.flash_src	= &msm_flash_src
#endif
#endif
#endif
};

static struct msm_camera_i2c_conf msm8930_back_cam_i2c_conf = {
	.use_i2c_mux = 1,
	.mux_dev = &msm8960_device_i2c_mux_gsbi4,
	.i2c_mux_mode = MODE_L,
};

static struct msm_camera_csi_lane_params imx111_csi_lane_params = {
	.csi_lane_assign = 0xE4,
	.csi_lane_mask = 0xF,
};

static struct msm_camera_sensor_platform_info sensor_board_info_imx111 = {
	.mount_angle	= 90,
	.cam_vreg = msm_8930_back_cam_vreg,
	.num_vreg = ARRAY_SIZE(msm_8930_back_cam_vreg),
	.gpio_conf = &msm_8930_back_cam_gpio_conf,
	.i2c_conf = &msm8930_back_cam_i2c_conf,
	.csi_lane_params = &imx111_csi_lane_params,
};

static struct i2c_board_info imx111_eeprom_i2c_info = {
	I2C_BOARD_INFO("imx111_eeprom", 0x50), /* 0x50 */
};

static struct msm_eeprom_info imx111_eeprom_info = {
	.board_info     = &imx111_eeprom_i2c_info,
	.bus_id         = MSM_8930_GSBI4_QUP_I2C_BUS_ID,
};

static struct msm_camera_sensor_info msm_camera_sensor_imx111_data = {
	.sensor_name	= "imx111",
	.pdata	= &msm_camera_csi_device_data[0],
	.flash_data	= &flash_imx111,
	.sensor_platform_info = &sensor_board_info_imx111,
	.csi_if	= 1,
	.camera_type = BACK_CAMERA_2D,
	.actuator_info = &msm_act_main_cam_1_info, /*                                                                              */
	.sensor_type = BAYER_SENSOR,
	.eeprom_info = &imx111_eeprom_info,
};
#endif /* CONFIG_IMX111 */
/*                                                                 */


#ifdef CONFIG_HI543
static struct msm_camera_sensor_flash_data flash_hi543 = {
	.flash_type	= MSM_CAMERA_FLASH_LED,
#ifdef CONFIG_LEDS_AS364X
	.flash_src  = &led_flash_src,
#endif
};


static int32_t hi543_ext_power_ctrl(int enable)
{
	pr_err("%s :E\n", __func__);

	if(enable) {
		pr_err("%s : HIGH\n", __func__);
		gpio_set_value_cansleep(GPIO_CAM_MAIN_CAM_RST_N, GPIOF_OUT_INIT_HIGH);
		usleep_range(3000, 4000);
	} else {
		pr_err("%s : LOW\n", __func__);

		usleep_range(3000, 4000);
		gpio_set_value_cansleep(GPIO_CAM_MAIN_CAM_RST_N, GPIOF_OUT_INIT_LOW);
	}

	usleep_range(5000, 10000);

	pr_err("%s :X\n", __func__);
	return 0;
}

static struct msm_camera_csi_lane_params hi543_csi_lane_params = {
	.csi_lane_assign = 0xE4,
	.csi_lane_mask = 0x3,
};


/*Start : tjeon@qualcomm.com - 20130319 */
//#ifdef CONFIG_HI543_EEPROM
static struct i2c_board_info hi543_eeprom_i2c_info = {
	I2C_BOARD_INFO("hi543_eeprom", 0x14), //                                                              
};

static struct msm_eeprom_info hi543_eeprom_info = {
	.board_info     = &hi543_eeprom_i2c_info,
	.bus_id         = MSM_8930_GSBI4_QUP_I2C_BUS_ID,
};
//#endif
/*End : tjeon@qualcomm.com - 20130319 */

static struct msm_camera_sensor_platform_info sensor_board_info_hi543 = {
	.mount_angle	= 90, //                                                                                              
	.cam_vreg = msm_8930_back_cam_vreg,
	.num_vreg = ARRAY_SIZE(msm_8930_back_cam_vreg),
	.gpio_conf = &msm_8930_back_cam_gpio_conf,
	.csi_lane_params = &hi543_csi_lane_params,
	.ext_power_ctrl = &hi543_ext_power_ctrl,
};

static struct msm_camera_sensor_info msm_camera_sensor_hi543_data = {
	.sensor_name	= "hi543",
	.pdata	= &msm_camera_csi_device_data[0],
	.flash_data	= &flash_hi543,
	//                                                                                                        
	.sensor_platform_info = &sensor_board_info_hi543,
	.csi_if	= 1,
	.camera_type = BACK_CAMERA_2D,
	.actuator_info = &msm_act_main_cam_0_info, /*                                                                                          */

/*Start : tjeon@qualcomm.com - 20130319 */
//#ifdef CONFIG_HI543_EEPROM
		.eeprom_info = &hi543_eeprom_info, /*                                                                        */
//#endif
/*End : tjeon@qualcomm.com - 20130319 */

	.sensor_type = BAYER_SENSOR,
};

#endif

#ifdef CONFIG_S5K4E5YA
static struct msm_camera_sensor_flash_data flash_s5k4e5ya = {
	.flash_type	= MSM_CAMERA_FLASH_LED,
#ifdef CONFIG_LEDS_AS364X
	.flash_src  = &led_flash_src,
#endif
};

static struct msm_camera_csi_lane_params s5k4e5ya_csi_lane_params = {
	.csi_lane_assign = 0xE4,
	.csi_lane_mask = 0x3,
};

static struct msm_camera_sensor_platform_info sensor_board_info_s5k4e5ya = {
	.mount_angle	= 90,
	.cam_vreg = msm_8930_back_cam_vreg,
	.num_vreg = ARRAY_SIZE(msm_8930_back_cam_vreg),
	.gpio_conf = &msm_8930_back_cam_gpio_conf,
	.csi_lane_params = &s5k4e5ya_csi_lane_params,
};

/*                                                                          */
#ifdef CONFIG_S5K4E5YA_EEPROM
static struct i2c_board_info s5k4e5ya_eeprom_i2c_info = {
	I2C_BOARD_INFO("s5k4e5ya_eeprom", 0x14), //                                                              
};

static struct msm_eeprom_info s5k4e5ya_eeprom_info = {
	.board_info     = &s5k4e5ya_eeprom_i2c_info,
	.bus_id         = MSM_8930_GSBI4_QUP_I2C_BUS_ID,
};
#endif
/*                                                                          */

static struct msm_camera_sensor_info msm_camera_sensor_s5k4e5ya_data = {
	.sensor_name	= "s5k4e5ya",
	.pdata	= &msm_camera_csi_device_data[0],
	.flash_data	= &flash_s5k4e5ya,
	//                                                                                                       
	.sensor_platform_info = &sensor_board_info_s5k4e5ya,
	.csi_if	= 1,
	.camera_type = BACK_CAMERA_2D,
	.actuator_info = &msm_act_main_cam_2_info,
#ifdef CONFIG_S5K4E5YA_EEPROM
	.eeprom_info = &s5k4e5ya_eeprom_info, /*                                                                        */
#endif
	.sensor_type = BAYER_SENSOR,
};
#endif  /* CONFIG_S5K4E5YA */

#ifdef CONFIG_HI707
static struct msm_camera_sensor_flash_data flash_hi707 = {
	.flash_type = MSM_CAMERA_FLASH_NONE
};

static struct msm_camera_csi_lane_params hi707_csi_lane_params = {
	.csi_lane_assign = 0xE4,
	.csi_lane_mask = 0x1,
};

static int32_t hi707_ext_power_ctrl(int enable)
{
	pr_err("%s :E\n", __func__);

	usleep_range(2000, 3000);
	if(enable) {
		pr_err("%s : GPIO_CAM_VT_CAM_PWDN H\n", __func__);
		gpio_set_value_cansleep(GPIO_CAM_VT_CAM_PWDN, GPIOF_OUT_INIT_HIGH);
		usleep_range(30000, 35000);
		pr_err("%s : GPIO_CAM_VT_CAM_RST_N H\n", __func__);
		gpio_set_value_cansleep(GPIO_CAM_VT_CAM_RST_N, GPIOF_OUT_INIT_HIGH);
//		usleep_range(10000, 15000);
	} else {
//		usleep_range(10000, 15000);
		pr_err("%s : GPIO_CAM_VT_CAM_RST_N L\n", __func__);
		gpio_set_value_cansleep(GPIO_CAM_VT_CAM_RST_N, GPIOF_OUT_INIT_LOW);
		usleep_range(30000, 35000);
		pr_err("%s : GPIO_CAM_VT_CAM_PWDN L\n", __func__);
		gpio_set_value_cansleep(GPIO_CAM_VT_CAM_PWDN, GPIOF_OUT_INIT_LOW);
	}

	usleep_range(5000, 10000);

	pr_err("%s :X\n", __func__);
	return 0;
}

static struct msm_camera_sensor_platform_info sensor_board_info_hi707 = {
	.mount_angle	= 270,   /*                                                                                */
	.cam_vreg = msm_8930_front_cam_vreg,
	.num_vreg = ARRAY_SIZE(msm_8930_front_cam_vreg),
	.gpio_conf = &msm_8930_front_cam_gpio_conf,
	.csi_lane_params = &hi707_csi_lane_params,
	.ext_power_ctrl = &hi707_ext_power_ctrl,
};

static struct msm_camera_sensor_info msm_camera_sensor_hi707_data = {
	.sensor_name	= "hi707",
	.pdata	= &msm_camera_csi_device_data[1],
	.flash_data = &flash_hi707,
	.sensor_platform_info = &sensor_board_info_hi707,
	.csi_if	= 1,
	.camera_type = FRONT_CAMERA_2D,
	.sensor_type = YUV_SENSOR,
};
#endif /* CONFIG_HI707 */

/*                                                                */
#ifdef CONFIG_IMX119
static struct msm_camera_i2c_conf msm8930_front_cam_i2c_conf = {
	.use_i2c_mux = 1,
	.mux_dev = &msm8960_device_i2c_mux_gsbi4,
	.i2c_mux_mode = MODE_L,
};

static struct msm_camera_sensor_flash_data flash_imx119 = {
	.flash_type	= MSM_CAMERA_FLASH_NONE,
};

static struct msm_camera_csi_lane_params imx119_csi_lane_params = {
	.csi_lane_assign = 0xE4,
	.csi_lane_mask = 0x1,
};

static struct msm_camera_sensor_platform_info sensor_board_info_imx119 = {
	.mount_angle	= 270,
	.cam_vreg = msm_8930_front_cam_vreg,
	.num_vreg = ARRAY_SIZE(msm_8930_front_cam_vreg),
	.gpio_conf = &msm_8930_front_cam_gpio_conf,
	.i2c_conf = &msm8930_front_cam_i2c_conf,
	.csi_lane_params = &imx119_csi_lane_params,
};

static struct msm_camera_sensor_info msm_camera_sensor_imx119_data = {
	.sensor_name	= "imx119",
	.pdata	= &msm_camera_csi_device_data[1],
	.flash_data	= &flash_imx119,
	.sensor_platform_info = &sensor_board_info_imx119,
	.csi_if	= 1,
	.camera_type = FRONT_CAMERA_2D,
	.sensor_type = BAYER_SENSOR,
};
#endif
/*                                                                */


static struct platform_device msm_camera_server = {
	.name = "msm_cam_server",
	.id = 0,
};


void __init msm8930_init_cam(void)
{
	msm_gpiomux_install(msm8930_cam_common_configs,
			ARRAY_SIZE(msm8930_cam_common_configs));

	platform_device_register(&msm_camera_server);
	platform_device_register(&msm8960_device_csiphy0);
	platform_device_register(&msm8960_device_csiphy1);
	platform_device_register(&msm8960_device_csid0);
	platform_device_register(&msm8960_device_csid1);
	platform_device_register(&msm8960_device_ispif);
	platform_device_register(&msm8960_device_vfe);
	platform_device_register(&msm8960_device_vpe);
	platform_device_register(&msm8960_device_i2c_mux_gsbi4); /*                                                              */
}

#ifdef CONFIG_I2C
struct i2c_board_info msm8930_camera_i2c_boardinfo[] = {
#ifdef CONFIG_HI543
	{
		I2C_BOARD_INFO("hi543", I2C_SLAVE_ADDR_MAIN_CAM),
		.platform_data = &msm_camera_sensor_hi543_data,
	},
#endif
#ifdef CONFIG_S5K4E5YA
	{
		I2C_BOARD_INFO("s5k4e5ya", I2C_SLAVE_ADDR_MAIN_CAM),
		.platform_data = &msm_camera_sensor_s5k4e5ya_data,
	},
#endif
/*                                                                 */
#if defined (CONFIG_IMX111)
	{
		I2C_BOARD_INFO("imx111", I2C_SLAVE_ADDR_MAIN_CAM),
		.platform_data = &msm_camera_sensor_imx111_data,
	},
#endif
/*                                                                 */
#ifdef CONFIG_HI707
	{
		I2C_BOARD_INFO("hi707", I2C_SLAVE_ADDR_VT_CAM),
		.platform_data = &msm_camera_sensor_hi707_data,
	},
#endif
/*                                                                */
#ifdef CONFIG_IMX119
	{
		I2C_BOARD_INFO("imx119", I2C_SLAVE_ADDR_VT_CAM),
		.platform_data = &msm_camera_sensor_imx119_data,
	},
#endif
/*                                                                */

};

struct msm_camera_board_info msm8930_camera_board_info = {
	.board_info = msm8930_camera_i2c_boardinfo,
	.num_i2c_board_info = ARRAY_SIZE(msm8930_camera_i2c_boardinfo),
};

/*                                                                 */
#if defined (CONFIG_MSM_CAMERA_FLASH_LM3559)

struct led_flash_platform_data {
	unsigned gpio_en;
	unsigned scl_gpio;
	unsigned sda_gpio;
};

static struct led_flash_platform_data lm3559_flash_pdata[] = {
	{
		.scl_gpio = GPIO_CAM_I2C_SCL,
		.sda_gpio = GPIO_CAM_I2C_SDA,
		.gpio_en = GPIO_CAM_FLASH_LED_EN,
	}
};

static struct i2c_board_info cam_i2c_flash_info[] = {
	{
	I2C_BOARD_INFO("lm3559", I2C_SLAVE_ADDR_LM3559), /* 0x53 */
	.type = "lm3559",
	.platform_data = &lm3559_flash_pdata,
	}
};
#endif /* CONFIG_MSM_CAMERA_FLASH_LM3559 */

#if defined(CONFIG_LEDS_AS364X)
static struct i2c_board_info cam_i2c_flash_info[] = {
	{
		I2C_BOARD_INFO("as364x", I2C_SLAVE_ADDR_AS3647),
		.type = "as364x",
		.platform_data = &as364x_flash_pdata,
	},
};
#endif /* CONFIG_LEDS_AS364X */
/*                                                                 */

#endif

#endif  /* CONFIG_MSM_CAMERA at line 40 */


/*
 * The body of lge_add_camera_devices() is from
 * register_i2c_devices(void) function,
 * which is defined in board-8960.c file of QCT original.
 *
 */
void __init lge_add_camera_devices(void)
{
#if defined(CONFIG_MSM_CAMERA)
	struct i2c_registry msm8930_camera_i2c_devices = {
		I2C_SURF | I2C_FFA | I2C_FLUID | I2C_RUMI,
		MSM_8930_GSBI4_QUP_I2C_BUS_ID,
		msm8930_camera_board_info.board_info,
		msm8930_camera_board_info.num_i2c_board_info,
	};
#if defined (CONFIG_MSM_CAMERA_FLASH_LM3559)
	struct i2c_registry msm8930_camera_flash_i2c_devices = {
		I2C_SURF | I2C_FFA | I2C_FLUID,
		MSM_8930_GSBI4_QUP_I2C_BUS_ID,
		cam_i2c_flash_info,
		ARRAY_SIZE(cam_i2c_flash_info),
	};
#elif defined (CONFIG_LEDS_AS364X)
	struct i2c_registry msm8930_camera_flash_i2c_devices = {
		I2C_SURF | I2C_FFA | I2C_FLUID,
		2, /* MSM_8930_GSBI2_QUP_I2C_BUS_ID */
		cam_i2c_flash_info,
		ARRAY_SIZE(cam_i2c_flash_info),
	};
#endif
	pr_err("%s: E\n", __func__);
	i2c_register_board_info(msm8930_camera_i2c_devices.bus,
		msm8930_camera_i2c_devices.info,
		msm8930_camera_i2c_devices.len);

#ifdef CONFIG_LEDS_AS364X
	i2c_register_board_info(msm8930_camera_flash_i2c_devices.bus,
		msm8930_camera_flash_i2c_devices.info,
		msm8930_camera_flash_i2c_devices.len);
#endif

/*                                                                 */
#if defined (CONFIG_MSM_CAMERA_FLASH_LM3559)
	i2c_register_board_info(msm8930_camera_flash_i2c_devices.bus,
		msm8930_camera_flash_i2c_devices.info,
		msm8930_camera_flash_i2c_devices.len);
#endif
/*                                                                 */

	pr_err("%s: X\n", __func__);
#endif
}
