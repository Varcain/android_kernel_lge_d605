#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <mach/board_lge.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/regulator/consumer.h>
#include "devices.h"
#include "board-fx3.h"

//#include CONFIG_BOARD_HEADER_FILE


/* Copyright(c) 2012, LGE Inc.
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

//#include "board-8930.h"

#include <linux/nfc/bcm43341.h>

#define NFC_GPIO_VEN            25
#define NFC_GPIO_IRQ            106
#define NFC_GPIO_FIRM           24

static struct bcm43341_platform_data bcm2079x_pdata = {
	.irq_gpio = NFC_GPIO_IRQ,
	.en_gpio = NFC_GPIO_VEN,
	.wake_gpio= NFC_GPIO_FIRM,
};

static struct i2c_board_info i2c_bcm43341[] __initdata = {
	{
	
		I2C_BOARD_INFO("bcm2079x-i2c", 0x77),

		//.flags = I2C_CLIENT_TEN,
		.irq = MSM_GPIO_TO_INT(NFC_GPIO_IRQ),
		.platform_data = &bcm2079x_pdata,
	},
};

static struct i2c_registry b2l_i2c_bcm_nfc_device __initdata = {
	I2C_SURF | I2C_FFA | I2C_FLUID | I2C_RUMI,
	MSM_8930_GSBI9_QUP_I2C_BUS_ID,
	i2c_bcm43341,
	ARRAY_SIZE(i2c_bcm43341),
};

static void __init lge_add_i2c_bcm2079x_device(void)
{
	i2c_register_board_info(b2l_i2c_bcm_nfc_device.bus, 
			       b2l_i2c_bcm_nfc_device.info,
			       b2l_i2c_bcm_nfc_device.len);
}


void __init lge_add_bcm43341_device(void)
{
	lge_add_i2c_bcm2079x_device();
}




#if defined(CONFIG_LGE_NFC)

#include <linux/nfc/pn544_lge.h>

//#define MSM_8930_GSBI8_QUP_I2C_BUS_ID 0

static struct pn544_i2c_platform_data nfc_platform_data[] = {
	{
		.ven_gpio 	= NFC_GPIO_VEN,
		.irq_gpio 	= NFC_GPIO_IRQ,
		.firm_gpio	= NFC_GPIO_FIRM,
		.sda_gpio   = NFC_GPIO_I2C_SDA,
		.scl_gpio   = NFC_GPIO_I2C_SCL ,
	},
};

static struct i2c_board_info msm_i2c_nxp_nfc_info[] = {
	{
		I2C_BOARD_INFO("pn544", NFC_I2C_SLAVE_ADDR),
		.platform_data = &nfc_platform_data,
		.irq = MSM_GPIO_TO_INT(NFC_GPIO_IRQ),
	}
};

static struct i2c_registry b2l_i2c_nfc_device __initdata = {
	I2C_SURF | I2C_FFA | I2C_FLUID | I2C_RUMI,
	MSM_8930_GSBI9_QUP_I2C_BUS_ID,
	msm_i2c_nxp_nfc_info,
	ARRAY_SIZE(msm_i2c_nxp_nfc_info),
};

static void __init lge_add_i2c_nfc_devices(void)
{
	//gpio_tlmm_config(GPIO_CFG(NFC_GPIO_FIRM, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
//	gpio_tlmm_config(GPIO_CFG(NFC_GPIO_I2C_SDA, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
//	gpio_tlmm_config(GPIO_CFG(NFC_GPIO_I2C_SCL, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
//	gpio_tlmm_config(GPIO_CFG(NFC_GPIO_VEN, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	//gpio_tlmm_config(GPIO_CFG(NFC_GPIO_IRQ, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);	
			
	i2c_register_board_info(b2l_i2c_nfc_device.bus,
				b2l_i2c_nfc_device.info,
				b2l_i2c_nfc_device.len);
}

void __init lge_add_nfc_devices(void)
{
	lge_add_i2c_nfc_devices();
}
#endif
