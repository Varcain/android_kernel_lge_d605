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

#include "msm_actuator.h"
#include <linux/debugfs.h>

#define IMX111_TOTAL_STEPS_NEAR_TO_FAR_MAX 29


//kyungh@qualcomm.com  2012.04.02 for AF calibration
#define IMX111_EEPROM_SADDR			0x50 >> 1
#define IMX111_ACT_START_ADDR		0x06
#define IMX111_ACT_MACRO_ADDR		0x08
#define IMX111_ACT_MARGIN			30
#define IMX111_ACT_MIN_MOVE_RANGE	200 // TBD


DEFINE_MUTEX(imx111_act_mutex);
static int imx111_actuator_debug_init(void);
static struct msm_actuator_ctrl_t imx111_act_t;

static int32_t imx111_wrapper_i2c_write(struct msm_actuator_ctrl_t *a_ctrl,
	int16_t next_lens_position, void *params)
{
	uint16_t msb = 0, lsb = 0;
	msb = (next_lens_position >> 4) & 0x3F;
	lsb = (next_lens_position << 4) & 0xF0;
	lsb |= (*(uint8_t *)params);
	CDBG("%s: Actuator MSB:0x%x, LSB:0x%x\n", __func__, msb, lsb);
	msm_camera_i2c_write(&a_ctrl->i2c_client,
		msb, lsb, MSM_CAMERA_I2C_BYTE_DATA);
	return next_lens_position;
}

static uint8_t imx111_hw_params[] = {
	0x0,
	0x5,
	0x6,
	0x8,
	0xB,
};

static uint16_t imx111_macro_scenario[] = {
	/* MOVE_NEAR dir*/
	4,
	29,
};

static uint16_t imx111_inf_scenario[] = {
	/* MOVE_FAR dir */
	8,
	22,
	29,
};

static struct region_params_t imx111_regions[] = {
	/* step_bound[0] - macro side boundary
	 * step_bound[1] - infinity side boundary
	 */
	/* Region 1 */
	{
		.step_bound = {2, 0},
		.code_per_step = 90,
	},
	/* Region 2 */
	{
		.step_bound = {29, 2},
		.code_per_step = 9,
	}
};

static struct damping_params_t imx111_macro_reg1_damping[] = {
	/* MOVE_NEAR Dir */
	/* Scene 1 => Damping params */
	{
		.damping_step = 0xFF,
		.damping_delay = 1500,
		.hw_params = &imx111_hw_params[0],
	},
	/* Scene 2 => Damping params */
	{
		.damping_step = 0xFF,
		.damping_delay = 1500,
		.hw_params = &imx111_hw_params[0],
	},
};

static struct damping_params_t imx111_macro_reg2_damping[] = {
	/* MOVE_NEAR Dir */
	/* Scene 1 => Damping params */
	{
		.damping_step = 0xFF,
		.damping_delay = 4500,
		.hw_params = &imx111_hw_params[4],
	},
	/* Scene 2 => Damping params */
	{
		.damping_step = 0xFF,
		.damping_delay = 4500,
		.hw_params = &imx111_hw_params[3],
	},
};

static struct damping_params_t imx111_inf_reg1_damping[] = {
	/* MOVE_FAR Dir */
	/* Scene 1 => Damping params */
	{
		.damping_step = 0xFF,
		.damping_delay = 450,
		.hw_params = &imx111_hw_params[0],
	},
	/* Scene 2 => Damping params */
	{
		.damping_step = 0xFF,
		.damping_delay = 450,
		.hw_params = &imx111_hw_params[0],
	},
	/* Scene 3 => Damping params */
	{
		.damping_step = 0xFF,
		.damping_delay = 450,
		.hw_params = &imx111_hw_params[0],
	},
};

static struct damping_params_t imx111_inf_reg2_damping[] = {
	/* MOVE_FAR Dir */
	/* Scene 1 => Damping params */
	{
		.damping_step = 0x1FF,
		.damping_delay = 4500,
		.hw_params = &imx111_hw_params[2],
	},
	/* Scene 2 => Damping params */
	{
		.damping_step = 0x1FF,
		.damping_delay = 4500,
		.hw_params = &imx111_hw_params[1],
	},
	/* Scene 3 => Damping params */
	{
		.damping_step = 27,
		.damping_delay = 2700,
		.hw_params = &imx111_hw_params[0],
	},
};

static struct damping_t imx111_macro_regions[] = {
	/* MOVE_NEAR dir */
	/* Region 1 */
	{
		.ringing_params = imx111_macro_reg1_damping,
	},
	/* Region 2 */
	{
		.ringing_params = imx111_macro_reg2_damping,
	},
};

static struct damping_t imx111_inf_regions[] = {
	/* MOVE_FAR dir */
	/* Region 1 */
	{
		.ringing_params = imx111_inf_reg1_damping,
	},
	/* Region 2 */
	{
		.ringing_params = imx111_inf_reg2_damping,
	},
};

//Start : kyungh@qualcomm.com  2012.04.02 for AF calibration
int32_t imx111_actuator_i2c_read_b_eeprom(struct msm_camera_i2c_client *dev_client, 
	unsigned char saddr, unsigned char *rxdata)
{
	int32_t rc = 0;
		
	struct i2c_msg msgs[] = {
		{
			.addr  = saddr << 1,
			.flags = 0,
			.len   = 1,
			.buf   = rxdata,
		},
		{
			.addr  = saddr << 1,
			.flags = I2C_M_RD,
			.len   = 1,
			.buf   = rxdata,
		},
	};
	rc = i2c_transfer(dev_client->client->adapter, msgs, 2);
	if (rc < 0)
		printk("imx111_i2c_rxdata failed 0x%x\n", saddr);
	return rc;
}

int32_t imx111_actuator_init_table(
                struct msm_actuator_ctrl_t *a_ctrl)
{
	int32_t rc = 0;
	uint16_t act_start = 0, act_macro = 0, move_range = 0;
	unsigned char buf;

	int16_t step_index = 0;
	printk("%s called\n", __func__);

	// read from eeprom     
	buf = IMX111_ACT_START_ADDR;
	rc = imx111_actuator_i2c_read_b_eeprom(&a_ctrl->i2c_client, 
                                IMX111_EEPROM_SADDR, &buf);
	if (rc < 0)
		goto act_cal_fail;
	
	act_start = (buf << 8) & 0xFF00;
	
	buf = IMX111_ACT_START_ADDR + 1;
	rc = imx111_actuator_i2c_read_b_eeprom(&a_ctrl->i2c_client, 
		IMX111_EEPROM_SADDR, &buf);
	
	if (rc < 0)
	goto act_cal_fail;
	
	act_start |= buf & 0xFF;
	printk("[QCTK_EEPROM] act_start = 0x%4x\n", act_start);

	buf = IMX111_ACT_MACRO_ADDR;
	rc = imx111_actuator_i2c_read_b_eeprom(&a_ctrl->i2c_client, 
		IMX111_EEPROM_SADDR, &buf);

	if (rc < 0)
		goto act_cal_fail;

	act_macro = (buf << 8) & 0xFF00;

	buf = IMX111_ACT_MACRO_ADDR + 1;
	rc = imx111_actuator_i2c_read_b_eeprom(&a_ctrl->i2c_client, 
		IMX111_EEPROM_SADDR, &buf);

	if (rc < 0)
		goto act_cal_fail;

	act_macro |= buf & 0xFF;
	printk("[QCTK_EEPROM] act_macro = 0x%4x\n", act_macro);
				
	if (a_ctrl->func_tbl.actuator_set_params)
		a_ctrl->func_tbl.actuator_set_params(a_ctrl);

	/* Fill step position table */
	a_ctrl->step_position_table =
		kmalloc(sizeof(uint16_t) * (a_ctrl->set_info.total_steps + 1),
		GFP_KERNEL);

	//intial code
	a_ctrl->step_position_table[0] = a_ctrl->initial_code;

	// start code - by calibration data
	if (act_start > IMX111_ACT_MARGIN)
		a_ctrl->step_position_table[1] = act_start - IMX111_ACT_MARGIN;
	else
		a_ctrl->step_position_table[1] = act_start;

	move_range = act_macro - a_ctrl->step_position_table[1];
	printk("move_range %d ==============\n", move_range);

	if (move_range < IMX111_ACT_MIN_MOVE_RANGE)
		goto act_cal_fail;
                
	for (step_index = 2;step_index < a_ctrl->set_info.total_steps;step_index++) {
		a_ctrl->step_position_table[step_index] 
			= ((step_index - 1) * move_range + ((a_ctrl->set_info.total_steps - 1) >> 1))
			/ (a_ctrl->set_info.total_steps - 1) + a_ctrl->step_position_table[1];
	}

/*                                                                   */
	printk("Actuator Clibration table: start(%d),macro(%d) ==============\n",
	act_start, act_macro);

	for (step_index = 0; step_index < a_ctrl->set_info.total_steps; step_index++)
		printk("step_position_table[%d]= %d\n",step_index,
		a_ctrl->step_position_table[step_index]);
/*                                                                   */

	a_ctrl->curr_step_pos = 0;
	a_ctrl->curr_region_index = 0;

	return rc;             

	act_cal_fail:
		pr_err(" imx111_actuator_init_table: calibration fails\n");
		rc = msm_actuator_init_table(a_ctrl);

return rc;
}
//End : kyungh@qualcomm.com  2012.04.02 for AF calibration

static int32_t imx111_set_params(struct msm_actuator_ctrl_t *a_ctrl)
{
	return 0;
}

static const struct i2c_device_id imx111_act_i2c_id[] = {
	{"imx111_act", (kernel_ulong_t)&imx111_act_t},
	{ }
};

static int imx111_act_config(
	void __user *argp)
{
	LINFO("%s called\n", __func__);
	return (int) msm_actuator_config(&imx111_act_t, argp);
}

static int imx111_i2c_add_driver_table(
	void)
{
	LINFO("%s called\n", __func__);
	return (int) msm_actuator_init_table(&imx111_act_t);
}

static struct i2c_driver imx111_act_i2c_driver = {
	.id_table = imx111_act_i2c_id,
	.probe  = msm_actuator_i2c_probe,
	.remove = __exit_p(imx111_act_i2c_remove),
	.driver = {
		.name = "imx111_act",
	},
};

static int __init imx111_i2c_add_driver(
	void)
{
	LINFO("%s called\n", __func__);
	return i2c_add_driver(imx111_act_t.i2c_driver);
}

static struct v4l2_subdev_core_ops imx111_act_subdev_core_ops;

static struct v4l2_subdev_ops imx111_act_subdev_ops = {
	.core = &imx111_act_subdev_core_ops,
};

static int32_t imx111_act_probe(
	void *board_info,
	void *sdev)
{
	LINFO("%s called\n", __func__);
	imx111_actuator_debug_init();

	return (int) msm_actuator_create_subdevice(&imx111_act_t,
		(struct i2c_board_info const *)board_info,
		(struct v4l2_subdev *)sdev);
}

/*                                                                        */
int32_t imx111_act_power_down(void *a_info)
{
	int32_t rc = 0;
	rc = imx111_act_t.func_tbl.actuator_power_down(&imx111_act_t);
	return rc;
}
/*                                                                        */

/*                                                                       */
#define IMX111_ACT_STOP_POS 10
int32_t imx111_actuator_af_power_down(
	struct msm_actuator_ctrl_t *a_ctrl)
{
	int32_t rc = 0;
	LINFO("%s called\n", __func__);

	if (a_ctrl->curr_step_pos != 0) {
		uint8_t hw_damping = 0xA;
		int delay = 0;
		int cur_pos = a_ctrl->curr_step_pos;
		int init_pos = a_ctrl->initial_code;
		int cur_dac = a_ctrl->step_position_table[a_ctrl->curr_step_pos];
		int mid_dac = a_ctrl->step_position_table[IMX111_ACT_STOP_POS];

		if(cur_pos > IMX111_ACT_STOP_POS) {
			imx111_wrapper_i2c_write(a_ctrl, mid_dac, &hw_damping);
			delay = (cur_dac - mid_dac) * 2 / 3;
			CDBG("%s: To mid - step = %d, position = %d, delay = %d", __func__, cur_pos, cur_dac, delay);
			mdelay(delay);
		} else
			mid_dac = cur_dac;

		hw_damping = 0x7;
		delay = mid_dac / 2;
		imx111_wrapper_i2c_write(a_ctrl, init_pos + 10, &hw_damping);
		CDBG("%s: To end - step = 10, position = %d, delay = %d", __func__, mid_dac, delay);
		mdelay(delay); 	// delay can be changed but put max due to it's small enough
		a_ctrl->curr_step_pos = 0;
	}

	kfree(a_ctrl->step_position_table);
	return rc;
}

int32_t imx111_actuator_set_default_focus(
	struct msm_actuator_ctrl_t *a_ctrl)
{
	int32_t rc = 0;
	if (!a_ctrl->step_position_table)
		a_ctrl->func_tbl.actuator_init_table(a_ctrl);

	if (a_ctrl->curr_step_pos != 0) {
		uint8_t hw_damping = 0x9;
		imx111_wrapper_i2c_write(a_ctrl, a_ctrl->initial_code, &hw_damping);
		mdelay(50); 	// delay can be changed but put max due to it's small enough
		a_ctrl->curr_step_pos = 0;
	}

	return rc;
}

int32_t imx111_actuator_move_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	int dir,
	int32_t num_steps)
{
	int8_t sign_dir = 0;
	int16_t dest_step_pos = 0;

	/* Determine sign direction */
	if (dir == MOVE_NEAR)
		sign_dir = 1;
	else if (dir == MOVE_FAR)
		sign_dir = -1;
	else {
		pr_err("Illegal focus direction\n");
		return -EINVAL;
	}

	/* Determine destination step position */
	dest_step_pos = a_ctrl->curr_step_pos +
		(sign_dir * num_steps);

	if(dest_step_pos == 0)
		return imx111_actuator_set_default_focus(a_ctrl);
	else
		return msm_actuator_move_focus(a_ctrl, dir, num_steps);
}
/*                                                                       */

static struct msm_actuator_ctrl_t imx111_act_t = {
	.i2c_driver = &imx111_act_i2c_driver,
	.i2c_addr = 0x18,
	.act_v4l2_subdev_ops = &imx111_act_subdev_ops,
	.actuator_ext_ctrl = {
		.a_init_table = imx111_i2c_add_driver_table,
		.a_create_subdevice = imx111_act_probe,
		.a_config = imx111_act_config,
/*                                                                        */
		.a_power_down = imx111_act_power_down,
/*                                                                        */
	},

	.i2c_client = {
		.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	},

	.set_info = {
		.total_steps = IMX111_TOTAL_STEPS_NEAR_TO_FAR_MAX,
	},

	.curr_step_pos = 0,
	.curr_region_index = 0,
	.initial_code = 0,
	.actuator_mutex = &imx111_act_mutex,

	/* Initialize scenario */
	.ringing_scenario[MOVE_NEAR] = imx111_macro_scenario,
	.scenario_size[MOVE_NEAR] = ARRAY_SIZE(imx111_macro_scenario),
	.ringing_scenario[MOVE_FAR] = imx111_inf_scenario,
	.scenario_size[MOVE_FAR] = ARRAY_SIZE(imx111_inf_scenario),

	/* Initialize region params */
	.region_params = imx111_regions,
	.region_size = ARRAY_SIZE(imx111_regions),

	/* Initialize damping params */
	.damping[MOVE_NEAR] = imx111_macro_regions,
	.damping[MOVE_FAR] = imx111_inf_regions,

	.func_tbl = {
		.actuator_set_params = imx111_set_params,
		.actuator_init_focus = NULL,
		//.actuator_init_table = msm_actuator_init_table,
		.actuator_init_table = imx111_actuator_init_table, //kyungh@qualcomm.com  2012.04.02 for AF calibration
		.actuator_move_focus = imx111_actuator_move_focus,		/*                                                                     */
		.actuator_write_focus = msm_actuator_write_focus,
		.actuator_i2c_write = imx111_wrapper_i2c_write,
		.actuator_set_default_focus = imx111_actuator_set_default_focus,	/*                                                                     */
		.actuator_power_down = imx111_actuator_af_power_down,	/*                                                                     */
	},

/*                                                                      */
	.get_info = {
		.focal_length_num = 320,
		.focal_length_den = 100,
		.f_number_num = 240,
		.f_number_den = 100,
		.f_pix_num = 112,
		.f_pix_den = 100,
		.total_f_dist_num = 171016,
		.total_f_dist_den = 100,
		.hor_view_angle_num = 598,
		.hor_view_angle_den = 10,
		.ver_view_angle_num = 467,
		.ver_view_angle_den = 10,
	},
/*                                                                      */

};

static int imx111_actuator_set_delay(void *data, u64 val)
{
	imx111_inf_reg2_damping[1].damping_delay = val;
	return 0;
}

static int imx111_actuator_get_delay(void *data, u64 *val)
{
	*val = imx111_inf_reg2_damping[1].damping_delay;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(imx111_delay,
	imx111_actuator_get_delay,
	imx111_actuator_set_delay,
	"%llu\n");

static int imx111_actuator_set_jumpparam(void *data, u64 val)
{
	imx111_inf_reg2_damping[1].damping_step = val & 0xFFF;
	return 0;
}

static int imx111_actuator_get_jumpparam(void *data, u64 *val)
{
	*val = imx111_inf_reg2_damping[1].damping_step;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(imx111_jumpparam,
	imx111_actuator_get_jumpparam,
	imx111_actuator_set_jumpparam,
	"%llu\n");

static int imx111_actuator_set_hwparam(void *data, u64 val)
{
	imx111_hw_params[2] = val & 0xFF;
	return 0;
}

static int imx111_actuator_get_hwparam(void *data, u64 *val)
{
	*val = imx111_hw_params[2];
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(imx111_hwparam,
	imx111_actuator_get_hwparam,
	imx111_actuator_set_hwparam,
	"%llu\n");

static int imx111_actuator_debug_init(void)
{
	struct dentry *debugfs_base = debugfs_create_dir("imx111_actuator", NULL);
	if (!debugfs_base)
		return -ENOMEM;

	if (!debugfs_create_file("imx111_delay",
		S_IRUGO | S_IWUSR, debugfs_base, NULL, &imx111_delay))
		return -ENOMEM;

	if (!debugfs_create_file("imx111_jumpparam",
		S_IRUGO | S_IWUSR, debugfs_base, NULL, &imx111_jumpparam))
		return -ENOMEM;

	if (!debugfs_create_file("imx111_hwparam",
		S_IRUGO | S_IWUSR, debugfs_base, NULL, &imx111_hwparam))
		return -ENOMEM;

	return 0;
}
subsys_initcall(imx111_i2c_add_driver);
MODULE_DESCRIPTION("IMX111 actuator");
MODULE_LICENSE("GPL v2");
