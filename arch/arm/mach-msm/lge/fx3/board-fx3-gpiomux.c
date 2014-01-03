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

#include <linux/gpio.h>
#include <asm/mach-types.h>
#include <mach/gpiomux.h>
#include <mach/socinfo.h>
#include "devices.h"

#include <mach/board_lge.h>
#include "board-fx3.h"

#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
#include <linux/skbuff.h>
#include <linux/wlan_plat.h>
#endif /* CONFIG_MMC_MSM_SDC4_SUPPORT */

#ifndef DISABLE_GSBI1_SPI
/* The SPI configurations apply to GSBI 1*/
static struct gpiomux_setting spi_active = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_12MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting spi_suspended_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};
#endif

static struct gpiomux_setting gsbi2_suspended_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_KEEPER,
};

static struct gpiomux_setting gsbi2_active_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gsbi3_suspended_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_KEEPER,
};

static struct gpiomux_setting gsbi3_active_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gsbi5 = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting gsbi9 = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

//                                                  
static struct gpiomux_setting gsbi6 = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};
static struct gpiomux_setting gsbi6_suspend = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};
#ifdef CONFIG_LGE_BLUESLEEP
static struct gpiomux_setting bt_host_wakeup_active_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting bt_host_wakeup_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting bt_wakeup_active_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting bt_wakeup_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};
#endif
//                                                  

static struct gpiomux_setting gsbi10 = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gsbi12 = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS) 
static struct gpiomux_setting proxi_int = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};
static struct gpiomux_setting gyro_int = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};
static struct gpiomux_setting sensor_en = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};
#endif

static struct gpiomux_setting cdc_mclk = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};
static struct gpiomux_setting audio_auxpcm[] = {
	/* Suspended state */
	{
		.func = GPIOMUX_FUNC_GPIO,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},
	/* Active state */
	{
		.func = GPIOMUX_FUNC_1,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},
};

#ifdef CONFIG_ANDROID_SW_IRRC
static struct gpiomux_setting irrc_tx[] = {
	{
		.func = GPIOMUX_FUNC_GPIO,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},
/*             
                                                     
*/
	{
//                                                             
#if defined(CONFIG_MACH_LGE_L9II_COMMON)
		.func = GPIOMUX_FUNC_2,
#else
		.func = GPIOMUX_FUNC_5,
#endif
//                                               
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},
};
#endif

/*                        
                                            
                           
                        
                           
  

                                                  
                           
                        
                           
  
*/
#if defined(CONFIG_KS8851) || defined(CONFIG_KS8851_MODULE)
static struct gpiomux_setting gpio_eth_config = {
	.pull = GPIOMUX_PULL_NONE,
	.drv = GPIOMUX_DRV_8MA,
	.func = GPIOMUX_FUNC_GPIO,
};
#endif

static struct gpiomux_setting slimbus = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_KEEPER,
};

//                                                  
static struct gpiomux_setting rfkill_active_bt_test = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_NONE,
};
static struct gpiomux_setting rfkill_suspend_bt_test = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};
//                                                  
#ifndef CONFIG_MMC_MSM_SDC4_SUPPORT

static struct gpiomux_setting wcnss_5wire_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting wcnss_5wire_active_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv  = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_DOWN,
};
#endif /* CONFIG_MMC_MSM_SDC4_SUPPORT */

#if defined (CONFIG_TOUCHSCREEN_ATMEL_TSADCC)
static struct gpiomux_setting atmel_resout_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting atmel_resout_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting atmel_ldo_en_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting atmel_ldo_en_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting atmel_int_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting atmel_int_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};
#endif //

#ifdef CONFIG_USB_OTG
#ifdef MSM8930_PHASE_2
static struct gpiomux_setting hsusb_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};
static struct msm_gpiomux_config msm8930_hsusb_configs[] = {
	{
		.gpio = 63,     /* HSUSB_EXTERNAL_5V_LDO_EN */
		.settings = {
			[GPIOMUX_SUSPENDED] = &hsusb_sus_cfg,
		},
	},
	{
		.gpio = 97,     /* HSUSB_5V_EN */
		.settings = {
			[GPIOMUX_SUSPENDED] = &hsusb_sus_cfg,
		},
	},
};
#endif
#endif /* CONFIG_USB_OTG */

#ifdef CONFIG_FM_RADIO_MI2S_ENABLE
/*static struct gpiomux_setting hap_lvl_shft_suspended_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting hap_lvl_shft_active_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
	};*/
#else
static struct gpiomux_setting hap_lvl_shft_suspended_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting hap_lvl_shft_active_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};
#endif

static struct gpiomux_setting ap2mdm_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting mdm2ap_status_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting mdm2ap_errfatal_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting ap2mdm_kpdpwr_n_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

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

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
static struct gpiomux_setting hdmi_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting hdmi_active_1_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting hdmi_active_2_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting hdmi_active_3_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting hdmi_active_4_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_OUT_HIGH,
};

static struct gpiomux_setting hdmi_active_5_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_OUT_HIGH,
};

#endif

static struct gpiomux_setting sitar_reset = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};
#ifdef CONFIG_FM_RADIO_MI2S_ENABLE
static struct gpiomux_setting  mi2s_act_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting  mi2s_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};
#endif


#if defined(CONFIG_KS8851) || defined(CONFIG_KS8851_MODULE)
static struct msm_gpiomux_config msm8960_ethernet_configs[] = {
	{
		.gpio = 90,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_eth_config,
		}
	},
	{
		.gpio = 89,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_eth_config,
		}
	},
};
#endif

//                                                                             
#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS) 
static struct msm_gpiomux_config fx3q_sensor_configs[] = {
	{
		.gpio	   = 49,	/* PROXIMITY INT GPIO */
		.settings = {
			[GPIOMUX_ACTIVE] = &proxi_int,
		},
	},
	{
		.gpio	   = 46,	/* GYRO INT GPIO */
		.settings = {
			[GPIOMUX_ACTIVE] = &gyro_int,
			[GPIOMUX_SUSPENDED] = &gyro_int,
		},
	},
	{
		.gpio	   = 43,	/* SENSOR LDO EN GPIO */
		.settings = {
			[GPIOMUX_SUSPENDED] = &sensor_en,
		},
	},
};
#endif
//                                                                             

static struct msm_gpiomux_config msm8960_gsbi_configs[] __initdata = {
#ifndef DISABLE_GSBI1_SPI
	{
		.gpio      = 6,		/* GSBI1 QUP SPI_DATA_MOSI */
		.settings = {
			[GPIOMUX_SUSPENDED] = &spi_suspended_config,
			[GPIOMUX_ACTIVE] = &spi_active,
		},
	},
	{
		.gpio      = 7,		/* GSBI1 QUP SPI_DATA_MISO */
		.settings = {
			[GPIOMUX_SUSPENDED] = &spi_suspended_config,
			[GPIOMUX_ACTIVE] = &spi_active,
		},
	},
	{
		.gpio      = 8,		/* GSBI1 QUP SPI_CS_N */
		.settings = {
			[GPIOMUX_SUSPENDED] = &spi_suspended_config,
			[GPIOMUX_ACTIVE] = &spi_active,
		},
	},
	{
		.gpio      = 9,		/* GSBI1 QUP SPI_CLK */
		.settings = {
			[GPIOMUX_SUSPENDED] = &spi_suspended_config,
			[GPIOMUX_ACTIVE] = &spi_active,
		},
	},
#endif
	{
		.gpio      = 12,	/* GSBI2 I2C QUP SDA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi2_suspended_cfg,
			[GPIOMUX_ACTIVE] = &gsbi2_active_cfg,
		},
	},
	{
		.gpio      = 13,	/* GSBI2 I2C QUP SCL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi2_suspended_cfg,
			[GPIOMUX_ACTIVE] = &gsbi2_active_cfg,
		},
	},
	{
		.gpio      = 16,	/* GSBI3 I2C QUP SDA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi3_suspended_cfg,
			[GPIOMUX_ACTIVE] = &gsbi3_active_cfg,
		},
	},
	{
		.gpio      = 17,	/* GSBI3 I2C QUP SCL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi3_suspended_cfg,
			[GPIOMUX_ACTIVE] = &gsbi3_active_cfg,
		},
	},
	{
		.gpio      = 22,	/* GSBI5 UART2 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi5,
		},
	},
	{
		.gpio      = 23,	/* GSBI5 UART2 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi5,
		},
	},
	{
		.gpio      = 44,	/* GSBI12 I2C QUP SDA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi12,
		},
	},
	{
		.gpio      = 95,	/* GSBI9 I2C QUP SDA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi9,	//                  
			[GPIOMUX_ACTIVE] = &gsbi9,
		},
	},
	{
		.gpio      = 96,	/* GSBI12 I2C QUP SCL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi9,		//                  
			[GPIOMUX_ACTIVE] = &gsbi9,
		},
	},
	{
		.gpio      = 45,	/* GSBI12 I2C QUP SCL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi12,
		},
	},
	{
		.gpio      = 73,	/* GSBI10 I2C QUP SDA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi10,
		},
	},
	{
		.gpio      = 74,	/* GSBI10 I2C QUP SCL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi10,
		},
	},
};

//                                                  
static struct msm_gpiomux_config gsbi6_uart_configs[] __initdata = {
	{
		.gpio	   = 26,	/* GSBI6 UART */
		.settings = {
			[GPIOMUX_ACTIVE]	= &gsbi6,
			[GPIOMUX_SUSPENDED] = &gsbi6_suspend,
		},
	},
	{
		.gpio	   = 27,	/* GSBI6 UART */
		.settings = {
			[GPIOMUX_ACTIVE]	= &gsbi6,
			[GPIOMUX_SUSPENDED] = &gsbi6_suspend,
		},
	},
	{
		.gpio	   = 28,	/* GSBI6 UART */
		.settings = {
			[GPIOMUX_ACTIVE]	= &gsbi6,
			[GPIOMUX_SUSPENDED] = &gsbi6_suspend,
		},
	},
	{
		.gpio	   = 29,	/* GSBI6 UART */
		.settings = {
			[GPIOMUX_ACTIVE]	= &gsbi6,
			[GPIOMUX_SUSPENDED] = &gsbi6_suspend,
		},
	}
};			
//                                                  

static struct msm_gpiomux_config msm8960_slimbus_config[] __initdata = {
	{
		.gpio	= 60,		/* slimbus data */
		.settings = {
			[GPIOMUX_SUSPENDED] = &slimbus,
		},
	},
	{
		.gpio	= 61,		/* slimbus clk */
		.settings = {
			[GPIOMUX_SUSPENDED] = &slimbus,
		},
	},
};

#ifdef CONFIG_FM_RADIO_MI2S_ENABLE
static struct msm_gpiomux_config msm8960_mi2s_configs[] __initdata = {
	{
		.gpio	= 47,		/* mi2s ws */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mi2s_sus_cfg,
			[GPIOMUX_ACTIVE] = &mi2s_act_cfg,
		},
	},
	{
		.gpio	= 48,		/* mi2s sclk */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mi2s_sus_cfg,
			[GPIOMUX_ACTIVE] = &mi2s_act_cfg,
		},
	},
#if  0
	{
		.gpio	= 49,		/* mi2s dout3 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mi2s_sus_cfg,
			[GPIOMUX_ACTIVE] = &mi2s_act_cfg,
		},
	},
	{
		.gpio	= 50,		/* mi2s dout2 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mi2s_sus_cfg,
			[GPIOMUX_ACTIVE] = &mi2s_act_cfg,
		},
	},

	{
		.gpio	= 51,		/* mi2s dout1 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mi2s_sus_cfg,
			[GPIOMUX_ACTIVE] = &mi2s_act_cfg,
		},
	},
#endif	
	{
		.gpio	= 52,		/* mi2s dout0 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mi2s_sus_cfg,
			[GPIOMUX_ACTIVE] = &mi2s_act_cfg,
		},
	},
#if 0    /* QCT Point */
	{
		.gpio	= 53,		/* mi2s mclk */
		.settings = {
			[GPIOMUX_SUSPENDED] = &mi2s_sus_cfg,
			[GPIOMUX_ACTIVE] = &mi2s_act_cfg,
		},
	},
#endif

};
#endif

static struct msm_gpiomux_config msm8960_audio_codec_configs[] __initdata = {
	{
		.gpio = 59,
		.settings = {
			[GPIOMUX_SUSPENDED] = &cdc_mclk,
		},
	},
};

#ifdef CONFIG_ANDROID_SW_IRRC
static struct msm_gpiomux_config msm8930_irrc_tx_configs[] __initdata = {
	{
//                                                             
#if defined(CONFIG_MACH_LGE_L9II_COMMON)
		.gpio = 3,
#else
		.gpio = 37,
#endif
//                                               
		.settings = {
			[GPIOMUX_SUSPENDED] = &irrc_tx[0],
			[GPIOMUX_ACTIVE] = &irrc_tx[1],
		},
	},
};
#endif

/*                        
                                                                            
  
             
               
                                     
    
   
  

                                                                            
  
             
               
                                           
    
   
  
*/
static struct msm_gpiomux_config msm8960_audio_auxpcm_configs[] __initdata = {
	{
		.gpio = 63,
		.settings = {
			[GPIOMUX_SUSPENDED] = &audio_auxpcm[0],
			[GPIOMUX_ACTIVE] = &audio_auxpcm[1],
		},
	},
	{
		.gpio = 64,
		.settings = {
			[GPIOMUX_SUSPENDED] = &audio_auxpcm[0],
			[GPIOMUX_ACTIVE] = &audio_auxpcm[1],
		},
	},
	{
		.gpio = 65,
		.settings = {
			[GPIOMUX_SUSPENDED] = &audio_auxpcm[0],
			[GPIOMUX_ACTIVE] = &audio_auxpcm[1],
		},
	},
	{
		.gpio = 66,
		.settings = {
			[GPIOMUX_SUSPENDED] = &audio_auxpcm[0],
			[GPIOMUX_ACTIVE] = &audio_auxpcm[1],
		},
	},
};

#ifndef CONFIG_MMC_MSM_SDC4_SUPPORT
static struct msm_gpiomux_config wcnss_5wire_interface[] = {
	{
		.gpio = 84,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5wire_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,
		},
	},
	{
		.gpio = 85,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5wire_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,
		},
	},
	{
		.gpio = 86,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5wire_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,
		},
	},
	{
		.gpio = 87,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5wire_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,
		},
	},
	{
		.gpio = 88,
		.settings = {
			[GPIOMUX_ACTIVE]    = &wcnss_5wire_active_cfg,
			[GPIOMUX_SUSPENDED] = &wcnss_5wire_suspend_cfg,
		},
	},
};
#endif /* CONFIG_MMC_MSM_SDC4_SUPPORT */

//                                                  
static struct msm_gpiomux_config bt_rfkill_interface[] = {
	{
		.gpio = 77,
		.settings = {
			[GPIOMUX_ACTIVE]    = &rfkill_active_bt_test,
			[GPIOMUX_SUSPENDED] = &rfkill_suspend_bt_test,
		},
	},
};

#ifdef CONFIG_LGE_BLUESLEEP
static struct msm_gpiomux_config msm8960_bt_host_wakeup_configs[] __initdata = {
	{
		.gpio = 78, // BT_HOST_WAKEUP
		.settings = {
			[GPIOMUX_ACTIVE]    = &bt_host_wakeup_active_cfg,
			[GPIOMUX_SUSPENDED] = &bt_host_wakeup_suspend_cfg,
		},
	},
};

static struct msm_gpiomux_config msm8960_bt_wakeup_configs[] __initdata = {
	{
		.gpio = 79,
		.settings = {
			[GPIOMUX_ACTIVE]    = &bt_wakeup_active_cfg,
			[GPIOMUX_SUSPENDED] = &bt_wakeup_suspend_cfg,
		},
	},
};
#endif //                     
//                                                  


/*                                   */
#if defined(CONFIG_LGE_NFC)
static struct gpiomux_setting nfc_pn544_ven_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting nfc_pn544_irq_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting nfc_pn544_firm_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

static struct msm_gpiomux_config msm8930_nfc_configs[] __initdata = {
	{
		.gpio = 24,
		.settings = {
			[GPIOMUX_SUSPENDED] = &nfc_pn544_firm_cfg,
		},
	},
	{
		.gpio = 106,
		.settings = {
			[GPIOMUX_SUSPENDED] = &nfc_pn544_irq_cfg,
		},
	},
	{
		.gpio = 25,
		.settings = {
			[GPIOMUX_SUSPENDED] = &nfc_pn544_ven_cfg,
		},
	},
};
#endif
/*                                 */ 

#if defined (CONFIG_TOUCHSCREEN_ATMEL_TSADCC)
static struct msm_gpiomux_config msm8960_atmel_configs[] __initdata = {
	{	/* TS INTERRUPT */
		.gpio = 11,
		.settings = {
			[GPIOMUX_ACTIVE]    = &atmel_int_act_cfg,
			[GPIOMUX_SUSPENDED] = &atmel_int_sus_cfg,
		},
	},
	{	/* TS LDO ENABLE */
		.gpio = 50,
		.settings = {
			[GPIOMUX_ACTIVE]    = &atmel_ldo_en_act_cfg,
			[GPIOMUX_SUSPENDED] = &atmel_ldo_en_sus_cfg,
		},
	},
	{	/* TS RESOUT */
		.gpio = 52,
		.settings = {
			[GPIOMUX_ACTIVE]    = &atmel_resout_act_cfg,
			[GPIOMUX_SUSPENDED] = &atmel_resout_sus_cfg,
		},
	},
};
#endif//

#ifdef CONFIG_FM_RADIO_MI2S_ENABLE
/*static struct msm_gpiomux_config hap_lvl_shft_config[] __initdata = {
	{
#ifdef NO_MI2S_GPIO_CONFLICT
		.gpio = 47,
#endif
		.settings = {
			[GPIOMUX_SUSPENDED] = &hap_lvl_shft_suspended_config,
			[GPIOMUX_ACTIVE] = &hap_lvl_shft_active_config,
		},
	},
	};*/
#else
static struct msm_gpiomux_config hap_lvl_shft_config[] __initdata = {
	{
		.gpio = 47,
		.settings = {
			[GPIOMUX_SUSPENDED] = &hap_lvl_shft_suspended_config,
			[GPIOMUX_ACTIVE] = &hap_lvl_shft_active_config,
		},
	},
};
#endif

static struct msm_gpiomux_config mdm_configs[] __initdata = {
	/* AP2MDM_STATUS */
	{
		.gpio = 94,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_cfg,
		}
	},
	/* MDM2AP_STATUS */
	{
		.gpio = 69,
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm2ap_status_cfg,
		}
	},
	/* MDM2AP_ERRFATAL */
	{
		.gpio = 70,
		.settings = {
			[GPIOMUX_SUSPENDED] = &mdm2ap_errfatal_cfg,
		}
	},
	/* AP2MDM_ERRFATAL */
	{
		.gpio = 95,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_cfg,
		}
	},
	/* AP2MDM_KPDPWR_N */
	{
		.gpio = 81,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_kpdpwr_n_cfg,
		}
	},
	/* AP2MDM_PMIC_RESET_N */
	{
		.gpio = 80,
		.settings = {
			[GPIOMUX_SUSPENDED] = &ap2mdm_kpdpwr_n_cfg,
		}
	}
};

static struct msm_gpiomux_config msm8960_mdp_vsync_configs[] __initdata = {
	{
		.gpio = 0,
		.settings = {
			[GPIOMUX_ACTIVE]    = &mdp_vsync_active_cfg,
			[GPIOMUX_SUSPENDED] = &mdp_vsync_suspend_cfg,
		},
	}
};

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
static struct msm_gpiomux_config msm8960_hdmi_configs[] __initdata = {
	{
		.gpio = 99,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_1_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
	{
		.gpio = 100,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_1_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
	{
		.gpio = 101,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_1_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
	{
		.gpio = 102,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_2_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},

};

static struct msm_gpiomux_config msm8930_mhl_configs[] __initdata = {
	{
		.gpio = 72,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_3_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
	{
		.gpio = 71,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_4_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},
	{
		.gpio = 73,
		.settings = {
			[GPIOMUX_ACTIVE]    = &hdmi_active_5_cfg,
			[GPIOMUX_SUSPENDED] = &hdmi_suspend_cfg,
		},
	},

};
#endif

static struct gpiomux_setting haptics_active_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};
static struct gpiomux_setting haptics_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config msm8930_haptics_configs[] __initdata = {
	{
		.gpio = 77,
		.settings = {
			[GPIOMUX_ACTIVE] = &haptics_active_cfg,
			[GPIOMUX_SUSPENDED] = &haptics_suspend_cfg,
		},
	},
	{
		.gpio = 78,
		.settings = {
			[GPIOMUX_ACTIVE] = &haptics_active_cfg,
			[GPIOMUX_SUSPENDED] = &haptics_suspend_cfg,
		},
	},
};
#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
static struct gpiomux_setting sdc4_suspend_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};


static struct gpiomux_setting sdc4_active_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv  = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config sdc4_interface[] = {
	{
		.gpio = 83,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc4_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc4_suspend_cfg,
		},
	},
	{
		.gpio = 84,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc4_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc4_suspend_cfg,
		},
	},
	{
		.gpio = 85,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc4_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc4_suspend_cfg,
		},
	},
	{
		.gpio = 86,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc4_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc4_suspend_cfg,
		},
	},
	{
		.gpio = 87,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc4_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc4_suspend_cfg,
		},
	},
	{
		.gpio = 88,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc4_active_cfg,
			[GPIOMUX_SUSPENDED] = &sdc4_suspend_cfg,
		},
	},
};
#endif /* CONFIG_MMC_MSM_SDC4_SUPPORT */

static struct gpiomux_setting sd_det_line = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct msm_gpiomux_config msm8930_sd_det_config[] __initdata = {
	{
		.gpio = 94,	/* SD Card Detect Line */
		.settings = {
			[GPIOMUX_SUSPENDED] = &sd_det_line,
			[GPIOMUX_ACTIVE] = &sd_det_line,
		},
	},
};

static struct msm_gpiomux_config msm_sitar_config[] __initdata = {
	{
		.gpio   = 42,           /* SYS_RST_N */
		.settings = {
			[GPIOMUX_SUSPENDED] = &sitar_reset,
		},
	}
};


#if defined (CONFIG_NFC_BCM43341) /*                    */
static struct gpiomux_setting nfc_bcm43341_ven_cfg = { // REG_PU
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting nfc_bcm43341_irq_cfg = { // NFC_IRQ
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN, // BCM2079X should be PULL_NONE? MDM to confirm.
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting nfc_bcm43341_wake_cfg = { // NFC_WAKE
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_HIGH,
};

static struct msm_gpiomux_config apq8064_bcm43341_nfc_configs[] __initdata = {
	{
		.gpio = 24,
		.settings = {
			[GPIOMUX_ACTIVE] = &nfc_bcm43341_wake_cfg,
			[GPIOMUX_SUSPENDED] = &nfc_bcm43341_wake_cfg,
		},
	},
	{
		.gpio = 106,
		.settings = {
			[GPIOMUX_ACTIVE] = &nfc_bcm43341_irq_cfg,
			[GPIOMUX_SUSPENDED] = &nfc_bcm43341_irq_cfg,
		},
	},
	{
		.gpio = 25, 
		.settings = {
			[GPIOMUX_ACTIVE] = &nfc_bcm43341_ven_cfg,
			[GPIOMUX_SUSPENDED] = &nfc_bcm43341_ven_cfg,
		},
	},
	
};
#endif

//                                                                                     
#if (defined (CONFIG_MACH_LGE_FX3_VZW) && defined (CONFIG_SWITCH_FSA8008)) \
	|| ( defined(CONFIG_MACH_LGE_FX3Q_TMUS) && defined(CONFIG_SWITCH_FSA8008))
static struct gpiomux_setting fsa8008_earpol_detect = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting fsa8008_ear_key_int = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};

static struct msm_gpiomux_config fsa8008_earpol_detect_config[] __initdata = {
	{
		.gpio   = 48,
		.settings = {
			[GPIOMUX_SUSPENDED] = &fsa8008_earpol_detect,
		},
	}
};

static struct msm_gpiomux_config fsa8008_ear_key_int_config[] __initdata = {
	{
		.gpio   = 55,
		.settings = {
			[GPIOMUX_SUSPENDED] = &fsa8008_ear_key_int,
		},
	}
};
#endif
//                                                                                     

//                                                             
#if defined(CONFIG_FB_MSM_MIPI_TX13D107VM_NT35521_VIDEO_HD_PT)
static struct gpiomux_setting dsv_en_cfg = {
		.func = GPIOMUX_FUNC_GPIO,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
};

static struct msm_gpiomux_config msm_dsv_en_config[] __initdata = {
	{
		.gpio      = 56,
		.settings = {
			[GPIOMUX_SUSPENDED] = &dsv_en_cfg,
		},
	}
};
#endif	
//                                                             
//                                                       
#if defined (CONFIG_BACKLIGHT_LM3639)
static struct gpiomux_setting lcd_bl_en_cfg = {
		.func = GPIOMUX_FUNC_GPIO,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
};

static struct msm_gpiomux_config msm_lcd_bl_en_config[] __initdata = {
	{
//                                                                
#ifdef CONFIG_MACH_LGE_L9II_OPEN_EU_REV_A
		.gpio      = 3, 
#else
		.gpio      = 2, 
#endif		
//                                                                  
		.settings = {
			[GPIOMUX_SUSPENDED] = &lcd_bl_en_cfg,
		},
	}
};
#endif
//                                                       


#if defined (CONFIG_MACH_LGE_L9II_COMMON)
static struct gpiomux_setting vibrator_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
    .dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting vibrator_active_cfg = {
	.func = GPIOMUX_FUNC_5,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting vibrator_ldo_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting vibrator_ldo_active_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct msm_gpiomux_config gpio2_vibrator_configs[] = {
	{
		.gpio = 37,
		.settings = {
			[GPIOMUX_ACTIVE]	= &vibrator_active_cfg,
			[GPIOMUX_SUSPENDED] = &vibrator_suspend_cfg,
		},
	},
	{
		.gpio = 54,  
		.settings = {
			[GPIOMUX_ACTIVE]    = &vibrator_ldo_active_cfg,
			[GPIOMUX_SUSPENDED] = &vibrator_ldo_suspend_cfg,
		},
	},
};
#endif

int __init msm8930_init_gpiomux(void)
{
	int rc = msm_gpiomux_init(NR_GPIO_IRQS);
	if (rc) {
		pr_err(KERN_ERR "msm_gpiomux_init failed %d\n", rc);
		return rc;
	}

#if defined(CONFIG_KS8851) || defined(CONFIG_KS8851_MODULE)
	msm_gpiomux_install(msm8960_ethernet_configs,
			ARRAY_SIZE(msm8960_ethernet_configs));
#endif

	msm_gpiomux_install(msm8960_gsbi_configs,
			ARRAY_SIZE(msm8960_gsbi_configs));

#if defined (CONFIG_TOUCHSCREEN_ATMEL_TSADCC)
	msm_gpiomux_install(msm8960_atmel_configs,
			ARRAY_SIZE(msm8960_atmel_configs));
#endif//

#ifdef CONFIG_FM_RADIO_MI2S_ENABLE
	msm_gpiomux_install(msm8960_mi2s_configs,
			ARRAY_SIZE(msm8960_mi2s_configs));
#endif

	msm_gpiomux_install(msm8960_slimbus_config,
			ARRAY_SIZE(msm8960_slimbus_config));

	msm_gpiomux_install(msm8960_audio_codec_configs,
			ARRAY_SIZE(msm8960_audio_codec_configs));

//                                               
#ifdef CONFIG_ANDROID_SW_IRRC
	msm_gpiomux_install(msm8930_irrc_tx_configs,
			ARRAY_SIZE(msm8930_irrc_tx_configs));
#endif
//                                               

//                                          
//	msm_gpiomux_install(msm8960_audio_mbhc_configs,
//			ARRAY_SIZE(msm8960_audio_mbhc_configs));
//                                                

//                                               
//	msm_gpiomux_install(msm8960_audio_spkr_configs,
//			ARRAY_SIZE(msm8960_audio_spkr_configs));
//                                             

	msm_gpiomux_install(msm8960_audio_auxpcm_configs,
			ARRAY_SIZE(msm8960_audio_auxpcm_configs));

#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
	msm_gpiomux_install(sdc4_interface,
			ARRAY_SIZE(sdc4_interface));
#else /* CONFIG_MMC_MSM_SDC4_SUPPORT */
	msm_gpiomux_install(wcnss_5wire_interface,
			ARRAY_SIZE(wcnss_5wire_interface));
#endif /* CONFIG_MMC_MSM_SDC4_SUPPORT */

//                                                  
  	msm_gpiomux_install(bt_rfkill_interface,
		  	ARRAY_SIZE(bt_rfkill_interface));
	msm_gpiomux_install(gsbi6_uart_configs,
		 	ARRAY_SIZE(gsbi6_uart_configs));
	
#ifdef CONFIG_LGE_BLUESLEEP
	msm_gpiomux_install(msm8960_bt_host_wakeup_configs,
			ARRAY_SIZE(msm8960_bt_host_wakeup_configs));
	
	msm_gpiomux_install(msm8960_bt_wakeup_configs,
			ARRAY_SIZE(msm8960_bt_wakeup_configs));
#endif
//                                                  

#ifndef CONFIG_FM_RADIO_MI2S_ENABLE
	if (machine_is_msm8930_mtp() || machine_is_msm8930_fluid() ||
		machine_is_msm8930_cdp()) {
		msm_gpiomux_install(hap_lvl_shft_config,
			ARRAY_SIZE(hap_lvl_shft_config));
#endif
#ifdef CONFIG_USB_OTG
#ifdef MSM8930_PHASE_2
		msm_gpiomux_install(msm8930_hsusb_configs,
			ARRAY_SIZE(msm8930_hsusb_configs));
#endif
#endif /* CONFIG_USB_OTG */

#ifndef CONFIG_FM_RADIO_MI2S_ENABLE
	}
#endif

	if (PLATFORM_IS_CHARM25())
		msm_gpiomux_install(mdm_configs,
			ARRAY_SIZE(mdm_configs));

	if (machine_is_msm8930_cdp() || machine_is_msm8930_mtp()
		|| machine_is_msm8930_fluid())
		msm_gpiomux_install(msm8930_haptics_configs,
			ARRAY_SIZE(msm8930_haptics_configs));

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
	msm_gpiomux_install(msm8960_hdmi_configs,
			ARRAY_SIZE(msm8960_hdmi_configs));
	if (machine_is_msm8930_fluid())
		msm_gpiomux_install(msm8930_mhl_configs,
				ARRAY_SIZE(msm8930_mhl_configs));
#endif

/*                                   */
#if defined(CONFIG_LGE_NFC)
	msm_gpiomux_install(msm8930_nfc_configs,
			ARRAY_SIZE(msm8930_nfc_configs));
#endif
/*                                 */ 

#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS) 
	msm_gpiomux_install(fx3q_sensor_configs,
					ARRAY_SIZE(fx3q_sensor_configs));
#endif

#if defined (CONFIG_NFC_BCM43341) /*                    */
		msm_gpiomux_install(apq8064_bcm43341_nfc_configs,
				ARRAY_SIZE(apq8064_bcm43341_nfc_configs));
		pr_err(KERN_ERR "apq8064_bcm43341_nfc_configs CONFIG_NFC_BCM43341\n");
#endif

//                                                                                     
#if (defined (CONFIG_MACH_LGE_FX3_VZW) && defined (CONFIG_SWITCH_FSA8008) ) \
	|| ( defined(CONFIG_MACH_LGE_FX3Q_TMUS) && defined(CONFIG_SWITCH_FSA8008))
	msm_gpiomux_install(fsa8008_earpol_detect_config,
			ARRAY_SIZE(fsa8008_earpol_detect_config));
	msm_gpiomux_install(fsa8008_ear_key_int_config,
			ARRAY_SIZE(fsa8008_ear_key_int_config));
#endif
//                                                                                     

	msm_gpiomux_install(msm8960_mdp_vsync_configs,
			ARRAY_SIZE(msm8960_mdp_vsync_configs));

	msm_gpiomux_install(msm8930_sd_det_config,
			ARRAY_SIZE(msm8930_sd_det_config));

	msm_gpiomux_install(msm_sitar_config, ARRAY_SIZE(msm_sitar_config));

//                                                             
#if defined(CONFIG_FB_MSM_MIPI_TX13D107VM_NT35521_VIDEO_HD_PT)
	msm_gpiomux_install(msm_dsv_en_config, ARRAY_SIZE(msm_dsv_en_config));
#endif        
//                                                             
//                                                       
#if defined (CONFIG_BACKLIGHT_LM3639)
	msm_gpiomux_install(msm_lcd_bl_en_config, ARRAY_SIZE(msm_lcd_bl_en_config));
#endif
//                                                       


#if defined (CONFIG_MACH_LGE_L9II_COMMON)
	msm_gpiomux_install(gpio2_vibrator_configs, ARRAY_SIZE(gpio2_vibrator_configs));
#endif

	return 0;
}
