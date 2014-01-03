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
#include <linux/mpu.h>
#include <linux/akm8963.h>
#include "board-fx3.h"

#define SENSOR_VDD      3000000
#define SENSOR_VIO 	    1800000

#define SENSOR_LDO_EN   43
#define MPUIRQ_GPIO 46

extern unsigned int system_rev;

int sensor_LDO_control(int on)
{
	gpio_set_value(SENSOR_LDO_EN, 1);
    pr_info("%s: SENSOR_LDO_EN(%d) is %s, gpio value is %d\n", 
		__func__, SENSOR_LDO_EN, on ? "ON" : "OFF", gpio_get_value(SENSOR_LDO_EN));
   	mdelay(5);

	return 1;
}

int sensor_VDD_control(int on)
{
	static struct regulator *vreg_vdd;
	int rc = -EINVAL;

	vreg_vdd = regulator_get(NULL, "8038_l10");
	
	if (IS_ERR(vreg_vdd)) {
		pr_err("%s: regulator get of sensor_3(VDD) failed (%ld)\n"
			, __func__, PTR_ERR(vreg_vdd));
		rc = PTR_ERR(vreg_vdd);
		return rc;
	}
	pr_info("%s: regulator_get end\n", __func__);

	if(on == 1)
		rc = regulator_enable(vreg_vdd);
	else
		rc = regulator_disable(vreg_vdd);
	
	if(rc != 0){
		pr_err("%s: SENSOR_LDO_EN(%d) %s fail\n", __func__, SENSOR_LDO_EN, on ? "ON":"OFF");
		return -1;
	}
	mdelay(10);
	
	pr_info("%s: SENSOR_VDD(%d) is set to %d\n", __func__, SENSOR_VDD, regulator_get_voltage(vreg_vdd));

	return 1;
}
int sensor_power(int on)
{
	int rc = -EINVAL;
	static struct regulator *vreg_vdd;

	/*           
                                  
                           
  */
/*	static struct regulator *vreg_vio;

    vreg_vio = regulator_get(NULL, "8038_lvs2");

    if (IS_ERR(vreg_vio)) {
        pr_err("%s: regulator get of sensor_(%d) failed (%ld)\n"
        , __func__, SENSOR_VIO, PTR_ERR(vreg_vio));
        rc = PTR_ERR(vreg_vio);
        return rc;
    }

	rc = regulator_set_voltage(vreg_vio, SENSOR_VIO, SENSOR_VIO);
	rc = regulator_enable(vreg_vio);
*/
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
	pr_info("%s: SENSOR_LDO_EN(%d) is set and gpio_get_value is %d\n", __func__, SENSOR_LDO_EN, gpio_get_value(SENSOR_LDO_EN));
	
    vreg_vdd = regulator_get(NULL, "8038_l10");

	if (IS_ERR(vreg_vdd)) {
        pr_err("%s: regulator get of sensor_3(VDD) failed (%ld)\n"
            , __func__, PTR_ERR(vreg_vdd));
        rc = PTR_ERR(vreg_vdd);
        return rc;
    }
	pr_info("%s: regulator_get end\n", __func__);
	
    rc = regulator_set_voltage(vreg_vdd, SENSOR_VDD, SENSOR_VDD);
	pr_info("%s: SENSOR_VDD(%d) is set to %d\n", __func__, SENSOR_VDD, rc);
	rc = regulator_enable(vreg_vdd);
	pr_info("%s: SENSOR_VDD(%d) is set to %d, %d\n", __func__, SENSOR_VDD, rc, regulator_get_voltage(vreg_vdd));

	return rc;
}

static void mpuirq_init(void)
{
	int res;
	pr_info("*** MPU START *** mpuirq_init...\n");

	if ((res = gpio_request(MPUIRQ_GPIO, "mpuirq")))
		printk(KERN_ERR "myirqtest: gpio_request(%d) failed err=%d\n",	MPUIRQ_GPIO, res);
	
	if ((res = gpio_direction_input(MPUIRQ_GPIO)))
		printk(KERN_ERR "gpio_direction_input() failed err=%d\n", res);
	
	pr_info("*** MPU END *** mpuirq_init...\n");
}

#ifdef CONFIG_MACH_LGE_FX3_VZW
static struct mpu_platform_data mpu6500_pdata = {
	.int_config = 0x10,
	.level_shifter = 0,
	.orientation = { -1, 0, 0,
					  0, 1, 0,
					  0, 0, -1, },
	.sec_slave_type = SECONDARY_SLAVE_TYPE_NONE,
	.key = {0xdd, 0x16, 0xcd, 0x07, 0xd9, 0xba, 0x97, 0x37,
			0xce, 0xfe, 0x23, 0x90, 0xe1, 0x66, 0x2f, 0x32},
};
#else
static struct mpu_platform_data mpu6500_pdata = {
	.int_config = 0x10,
	.level_shifter = 0,
	.orientation = {  0, 1, 0,
					  1, 0, 0,
					  0, 0, -1, },
	.sec_slave_type = SECONDARY_SLAVE_TYPE_NONE,
	.key = {0xdd, 0x16, 0xcd, 0x07, 0xd9, 0xba, 0x97, 0x37,
			0xce, 0xfe, 0x23, 0x90, 0xe1, 0x66, 0x2f, 0x32},
};
#endif
static struct prox_platform_data apds9190_pdata = {
	.irq_num = 49,
	.power = sensor_VDD_control,
};

static struct akm8963_platform_data akm_platform_data_8963 = {
	.gpio_DRDY = 0,
	.layout = 5,
	.outbit = 1,
};

static struct i2c_board_info mpu6500_i2c_bdinfo[] = {
	[0] = {
		I2C_BOARD_INFO("mpu6500", 0x68),
		.platform_data = &mpu6500_pdata,
		.irq = MSM_GPIO_TO_INT(MPUIRQ_GPIO),
	},
	[1] = {
		I2C_BOARD_INFO("mpu6050", 0x68),
		.platform_data = &mpu6500_pdata,
		.irq = MSM_GPIO_TO_INT(MPUIRQ_GPIO),
	},
};

static struct i2c_board_info apds9190_i2c_bdinfo[] = {
	[0] = {
		I2C_BOARD_INFO("apds9190", 0x39),
		.platform_data = &apds9190_pdata,
	},
};

static struct i2c_board_info akm8963_i2c_bdinfo[] = {
	[0] = {
		I2C_BOARD_INFO("akm8963", 0x0D),
		.flags = I2C_CLIENT_WAKE,
		.platform_data = &akm_platform_data_8963,
		.irq = 0,
	},
};

void __init lge_add_sensor_devices(void)
{
	gpio_tlmm_config(GPIO_CFG(MPUIRQ_GPIO, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE); //MPU_INT
	gpio_tlmm_config(GPIO_CFG(44, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE); //SDA
	gpio_tlmm_config(GPIO_CFG(45, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE); //SCL
	
	/* Sensor power on */
	sensor_power(1);

	/*MPU INT Init*/
	mpuirq_init();

	i2c_register_board_info(MSM_8930_GSBI12_QUP_I2C_BUS_ID,(&apds9190_i2c_bdinfo[0]), 1);
	i2c_register_board_info(MSM_8930_GSBI12_QUP_I2C_BUS_ID,(&akm8963_i2c_bdinfo[0]), 1);
#ifdef CONFIG_MACH_LGE_FX3_VZW
	if(system_rev < HW_REV_B)
		i2c_register_board_info(MSM_8930_GSBI12_QUP_I2C_BUS_ID,(&mpu6500_i2c_bdinfo[0]), 1);
	else
		i2c_register_board_info(MSM_8930_GSBI12_QUP_I2C_BUS_ID,(&mpu6500_i2c_bdinfo[1]), 1);
#else
	i2c_register_board_info(MSM_8930_GSBI12_QUP_I2C_BUS_ID,(&mpu6500_i2c_bdinfo[1]), 1);
#endif
}
