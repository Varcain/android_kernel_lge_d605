/*  arch/arm/mach-msm/qdsp5v2/lge_audio_misc_ctl.c
 *
 * Copyright (C) 2009 LGE, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <mach/board_lge.h>
#include <mach/board.h>

#define IOCTL_READ_REG _IOW('a', 0, int)
#define IOCTL_WRITE_REG _IOW('a', 1, int)
#define IOCTL_READ_PORCH _IOW('a', 2, int)
#define IOCTL_WRITE_PORCH _IOW('a', 3, int)
#define IOCTL_READ_LUT _IOW('a', 4, int)
#define IOCTL_WRITE_LUT _IOW('a', 5, int)

#define LUT_SIZE 256
struct msm_panel_common_pdata *lcdc_pdata;
#ifdef CONFIG_LGE_QC_LCDC_LUT
extern int lge_set_qlut(void);
extern unsigned int p_lg_qc_lcdc_lut[];

static int tuning_read_lut(unsigned long tmp)
{
	int size = LUT_SIZE*4;
	printk(KERN_INFO "read_lut_table in misc driver\n");

	if (copy_to_user((unsigned int *)tmp, p_lg_qc_lcdc_lut,
				size)) {
		printk(KERN_ERR "read_file : error of copy_to_user_buff\n");
		return -EFAULT;
	}

	return 0;
}

static int tuning_write_lut(unsigned long tmp)
{
	u32 *buf;
	int size = LUT_SIZE*4;

	printk(KERN_INFO "write lut file\n");

	buf = kmalloc(size, GFP_KERNEL);
	if (copy_from_user(buf, (unsigned int *)tmp, size)) {
		printk(KERN_ERR "write_file : error of copy_from_user\n");
		return -EFAULT;
	}

	memcpy(p_lg_qc_lcdc_lut, buf, size);
	kfree(buf);
	return 0;
}
#endif
long device_ioctl(struct file *file, unsigned int ioctl_num,
		unsigned long ioctl_param)
{
	switch (ioctl_num) {

	case IOCTL_READ_REG:
		printk(KERN_INFO "IOCTL_READ_1ST\n");
		lcdc_pdata->read_regset(ioctl_param);
		break;
	case IOCTL_WRITE_REG:
		printk(KERN_INFO "IOCTL_WRITE_1ST\n");
		lcdc_pdata->write_regset(ioctl_param);
		break;
	case IOCTL_READ_PORCH:
		printk(KERN_INFO "IOCTL_READ_PORCH\n");
		lcdc_pdata->read_porch(ioctl_param);
		break;
	case IOCTL_WRITE_PORCH:
		printk(KERN_INFO "IOCTL_WRITE_PORCH\n");
		lcdc_pdata->write_porch(ioctl_param);
		break;
	case IOCTL_READ_LUT:
		printk(KERN_INFO "IOCTL_READ_LUT\n");
#ifdef CONFIG_LGE_QC_LCDC_LUT
		tuning_read_lut(ioctl_param);
#else
		printk(" In order to write LUT, CONFIG_LGE_QC_LCDC_LUT should be enabled!");
#endif
		break;
	case IOCTL_WRITE_LUT:
		printk(KERN_INFO "IOCTL_WRITE_LUT\n");
#ifdef CONFIG_LGE_QC_LCDC_LUT
		tuning_write_lut(ioctl_param);
		lge_set_qlut();
#else
		printk(" In order to write LUT, CONFIG_LGE_QC_LCDC_LUT should be enabled!");
#endif
		break;
	}
	return 0;
}

static const struct file_operations lcd_misc_fops = {
	.owner	= THIS_MODULE,
	.unlocked_ioctl = device_ioctl
};

struct miscdevice lcd_misc_dev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "lcd_misc",
	.fops	= &lcd_misc_fops
};

static int lcd_misc_probe(struct platform_device *pdev)
{
	lcdc_pdata = pdev->dev.platform_data;
	return misc_register(&lcd_misc_dev);
}
static struct platform_driver this_driver = {
	.probe  = lcd_misc_probe,
	.driver = {
		.name   = "lcd_misc_msm",
	},
};

int __init lcd_misc_init(void)
{
	printk(KERN_INFO "lcd_misc_init \n");
	return platform_driver_register(&this_driver);
}

device_initcall(lcd_misc_init);

MODULE_DESCRIPTION("MSM MISC driver");
MODULE_LICENSE("GPL v2");
