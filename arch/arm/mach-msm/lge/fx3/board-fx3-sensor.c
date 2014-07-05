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

#include <linux/input.h>
#include <mach/gpio.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>

#include <linux/delay.h>
#include <mach/board_lge.h>
#include "board-fx3.h"

#if defined(CONFIG_MACH_LGE_FX3_TMUS) || defined(CONFIG_MACH_LGE_F6_TMUS) || defined(CONFIG_MACH_LGE_FX3_WCDMA_TRF_US) || defined(CONFIG_MACH_LGE_F6_VDF) || defined(CONFIG_MACH_LGE_F6_ORG)|| defined(CONFIG_MACH_LGE_F6_OPEN) || defined(CONFIG_MACH_LGE_F6_TMO)
#define SENSOR_VDD	    2900000
#elif defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS) 
#define SENSOR_VDD      2850000
#else
#define SENSOR_VDD      2800000
#endif

#define SENSOR_VIO 	    1800000

#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS) 
/* no use LDO_EN */
#else
#define SENSOR_LDO_EN   43
#endif

extern unsigned int system_rev;

/* acceleration platform data */
struct acceleration_platform_data {
	uint32_t version;
	int irq_num;
	int (*power)(unsigned char onoff);
};

/* ecompass platform data */
struct ecom_platform_data {
	uint32_t version;
	int irq_num;
	int (*power)(unsigned char onoff);
};

int sensor_power(int on)
{
	int rc = -EINVAL;
	static struct regulator *vreg_vdd;

#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS) 
	/*           
                                  
                           
  */
	static struct regulator *vreg_vio;

    vreg_vio = regulator_get(NULL, "8038_lvs2");

    if (IS_ERR(vreg_vio)) {
        pr_err("%s: regulator get of sensor_(%d) failed (%ld)\n"
        , __func__, SENSOR_VIO, PTR_ERR(vreg_vio));
        rc = PTR_ERR(vreg_vio);
        return rc;
    }

	rc = regulator_set_voltage(vreg_vio, SENSOR_VIO, SENSOR_VIO);
	rc = regulator_enable(vreg_vio);
#elif defined(CONFIG_MACH_LGE_F6_TMUS)|| defined(CONFIG_MACH_LGE_F6_VDF) || defined(CONFIG_MACH_LGE_F6_ORG)|| defined(CONFIG_MACH_LGE_F6_OPEN) || defined(CONFIG_MACH_LGE_F6_TMO)
	// empty section
#else
    /*
     * set gpio value to 1 to enable sensor ldo
     */
    rc = gpio_request(SENSOR_LDO_EN, "sensor_ldo");
    if(rc < 0) {
        pr_err("%s: SENSOR_LDO_EN(%d) request fail\n", __func__, SENSOR_LDO_EN);
        return rc;
    }
    gpio_direction_output(SENSOR_LDO_EN, 1);
    gpio_set_value(SENSOR_LDO_EN, on);
    pr_info("%s: SENSOR_LDO_EN(%d) is set to %d\n", __func__, SENSOR_LDO_EN, on);
    mdelay(5);
#endif

#if defined(CONFIG_MACH_LGE_FX3_SPCS) || defined(CONFIG_MACH_LGE_FX3_SPCSTRF)
    if(system_rev < HW_REV_B) {
        static struct regulator *vreg_l11;
        pr_info("%s: set vreg_l11 for HW_REV(%d)\n", __func__, system_rev);

        vreg_l11 = regulator_get(NULL, "8038_l11");
	    if (IS_ERR(vreg_l11)) {
		    pr_err("%s: regulator get of l11_msm_p3 failed (%ld)\n"
			    , __func__, PTR_ERR(vreg_l11));
		    rc = PTR_ERR(vreg_l11);
		    return rc;
	    }
        rc = regulator_set_voltage(vreg_l11, SENSOR_VIO, SENSOR_VIO);
        rc = regulator_enable(vreg_l11);
    }
#endif

#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS) 
    vreg_vdd = regulator_get(NULL, "8038_l9");
#else
    vreg_vdd = regulator_get(NULL, "8038_l10");
#endif
	if (IS_ERR(vreg_vdd)) {
        pr_err("%s: regulator get of sensor_2.8(VDD) failed (%ld)\n"
            , __func__, PTR_ERR(vreg_vdd));
        rc = PTR_ERR(vreg_vdd);
        return rc;
    }
    rc = regulator_set_voltage(vreg_vdd, SENSOR_VDD, SENSOR_VDD);
	rc = regulator_enable(vreg_vdd);

    pr_info("%s: %d\n", __func__, on);

	return rc;
}

struct bosch_sensor_specific{
    char *name;
    int place;
    int irq;
};

static struct bosch_sensor_specific bmm050_pdata = {
    .name = "bmm050",
#if defined(CONFIG_MACH_LGE_L9II_COMMON)
    .place = 4,
#elif defined(CONFIG_MACH_LGE_F6_TMUS) || defined(CONFIG_MACH_LGE_F6_VDF)
    .place = 3,
#else
    .place = 0,
#endif
    .irq = 46
};

static struct bosch_sensor_specific bma250_pdata = {
    .name = "bma2x2",
#if defined(CONFIG_MACH_LGE_L9II_COMMON)
    .place = 4,
#elif defined(CONFIG_MACH_LGE_F6_TMUS) || defined(CONFIG_MACH_LGE_F6_VDF)
    .place = 3,
#else
    .place = 0,
#endif
    .irq = 46
};

#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS) 
static struct prox_platform_data apds9190_pdata = {
	.irq_num = 49,
	.power = sensor_power,
};
#else
static struct prox_platform_data apds9130_pdata = {
	.irq_num = 49,
};
#endif

static struct i2c_board_info bmm050_i2c_bdinfo[] = {
	[0] = {
		I2C_BOARD_INFO("bmm050", 0x10),
		.platform_data = &bmm050_pdata,
	},
    [1] = {
        I2C_BOARD_INFO("bmm050", 0x12),
        .platform_data = &bmm050_pdata,
    },
};

static struct i2c_board_info bma250_i2c_bdinfo[] = {
	[0] = {
		I2C_BOARD_INFO("bma250", 0x18),
		.platform_data = &bma250_pdata,
	},
    [1] = {
        I2C_BOARD_INFO("bma2x2", 0x10),
        .platform_data = &bma250_pdata,
    },
};

#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS) 
static struct i2c_board_info apds9190_i2c_bdinfo[] = {
	[0] = {
		I2C_BOARD_INFO("apds9190", 0x39),
		.platform_data = &apds9190_pdata,
	},
};
#else
static struct i2c_board_info apds9130_i2c_bdinfo[] = {
	[0] = {
		I2C_BOARD_INFO("apds9130", 0x39),
		.platform_data = &apds9130_pdata,
	},
};
#endif

void __init lge_add_sensor_devices(void)
{
	gpio_tlmm_config(GPIO_CFG(44, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(45, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

#if !defined(CONFIG_MACH_LGE_FX3_VZW) && !defined(CONFIG_MACH_LGE_FX3Q_TMUS)
	gpio_tlmm_config(GPIO_CFG(49, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
#endif

	/* Sensor power on */
	sensor_power(1);

#if defined(CONFIG_MACH_LGE_FX3_SPCS) || defined(CONFIG_MACH_LGE_FX3_SPCSTRF)
    if(system_rev < HW_REV_C) {
    	i2c_register_board_info(MSM_8930_GSBI12_QUP_I2C_BUS_ID,(&bma250_i2c_bdinfo[0]), 1);
    	i2c_register_board_info(MSM_8930_GSBI12_QUP_I2C_BUS_ID,(&bmm050_i2c_bdinfo[0]), 1);
        printk("%s: bmc050 is enabled(system_rev : %d)\n", __func__, system_rev);
    } else {
    	i2c_register_board_info(MSM_8930_GSBI12_QUP_I2C_BUS_ID,(&bma250_i2c_bdinfo[1]), 1);
    	i2c_register_board_info(MSM_8930_GSBI12_QUP_I2C_BUS_ID,(&bmm050_i2c_bdinfo[1]), 1);
        printk("%s: bmc150 is enabled\n", __func__);
    }
#elif defined(CONFIG_MACH_LGE_FX3_MPCS) || defined(CONFIG_MACH_LGE_FX3_CRK)
    if(system_rev < HW_REV_B) {
        i2c_register_board_info(MSM_8930_GSBI12_QUP_I2C_BUS_ID,(&bma250_i2c_bdinfo[0]), 1);
        i2c_register_board_info(MSM_8930_GSBI12_QUP_I2C_BUS_ID,(&bmm050_i2c_bdinfo[0]), 1);
        printk("%s: bmc050 is enabled(system_rev : %d)\n", __func__, system_rev);
    } else {
        i2c_register_board_info(MSM_8930_GSBI12_QUP_I2C_BUS_ID,(&bma250_i2c_bdinfo[1]), 1);
        i2c_register_board_info(MSM_8930_GSBI12_QUP_I2C_BUS_ID,(&bmm050_i2c_bdinfo[1]), 1);
        printk("%s: bmc150 is enabled\n", __func__);
    }
#else // FX3_TMUS || FX3_VZW || FX3_WCDMA_TRF_US || L9II_OPEN_EU
	i2c_register_board_info(MSM_8930_GSBI12_QUP_I2C_BUS_ID,(&bma250_i2c_bdinfo[1]), 1);
	i2c_register_board_info(MSM_8930_GSBI12_QUP_I2C_BUS_ID,(&bmm050_i2c_bdinfo[1]), 1);
	printk("%s: bmc150 is enabled\n", __func__);
#endif

#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS) 
	i2c_register_board_info(MSM_8930_GSBI12_QUP_I2C_BUS_ID,(&apds9190_i2c_bdinfo[0]), 1);
#else
	i2c_register_board_info(MSM_8930_GSBI12_QUP_I2C_BUS_ID,(&apds9130_i2c_bdinfo[0]), 1);
#endif
}
