/*
 * LED Class Core
 *
 * Copyright (C) 2005 John Lenz <lenz@cs.wisc.edu>
 * Copyright (C) 2005-2007 Richard Purdie <rpurdie@openedhand.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <linux/leds.h>
#include "leds.h"
#if defined (CONFIG_BACKLIGHT_LM3639)
#include "../video/backlight/lm3639_bl.h"
#endif

#define LED_BUFF_SIZE 50

static struct class *leds_class;
/*                                                         */
#ifdef CONFIG_LGE_PM8038_KPJT
static int playing_pattern = 0; //                               

extern void change_led_pattern(int pattern, int pattern_r_on, int pattern_g_on, int pattern_b_on);	
extern void	make_blink_led_pattern(int red, int green, int blue, int delay_on,int delay_off,int on);
extern void pattern_test(int on);
extern void make_pwm_led_pattern(int patterns[]);
#else
#endif
/*                                                         */
static void led_update_brightness(struct led_classdev *led_cdev)
{
	if (led_cdev->brightness_get)
		led_cdev->brightness = led_cdev->brightness_get(led_cdev);
}


#if defined (CONFIG_BACKLIGHT_LM3639)
struct lm3639_reg_data lm3639_reg_d =
{
	.addr = 0x05,
	.value = 0x00,
};

static ssize_t led_reg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{

	lm3639_reg_read_(&lm3639_reg_d);

	return snprintf(buf, LED_BUFF_SIZE, "addr=0x%x,value=0x%x\n", lm3639_reg_d.addr, lm3639_reg_d.value);
}

static ssize_t led_reg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{

	unsigned int value, addr;
	sscanf(buf, "%x,%x", &addr, &value);

	if(addr == 0xFFFF)
	{
		lm3639_reg_d.addr = (char)value;
		printk("%s:%d, setting reg_addr=0x%x\n", __func__, __LINE__, lm3639_reg_d.addr);
		return size;
	}
	lm3639_reg_d.addr = (char)addr;
	lm3639_reg_d.value = (char)value;

	lm3639_reg_write_(&lm3639_reg_d);

	return size;
}
#endif

static ssize_t led_brightness_show(struct device *dev, 
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);

	/* no lock needed for this */
	led_update_brightness(led_cdev);

	return snprintf(buf, LED_BUFF_SIZE, "%u\n", led_cdev->brightness);
}

static ssize_t led_brightness_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	ssize_t ret = -EINVAL;
	char *after;
	unsigned long state = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;

	if (isspace(*after))
		count++;

	if (count == size) {
		ret = count;

		if (state == LED_OFF)
			led_trigger_remove(led_cdev);
		led_set_brightness(led_cdev, state);
	}

	return ret;
}

static ssize_t led_max_brightness_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	ssize_t ret = -EINVAL;
	unsigned long state = 0;
#ifdef CONFIG_LGE_PM //Control LCD brightness by one channel
	static int prev_max_brightness = 255;
#endif

	ret = strict_strtoul(buf, 10, &state);
	if (!ret) {
		ret = size;
		if (state > LED_FULL)
			state = LED_FULL;
		led_cdev->max_brightness = state;
#ifdef CONFIG_LGE_PM //Control LCD brightness by one channel
		if (prev_max_brightness != led_cdev->max_brightness)
		{
			 kobject_uevent(&led_cdev->dev->kobj, KOBJ_CHANGE);
		}
		prev_max_brightness = led_cdev->max_brightness;
#else // QCT
		led_set_brightness(led_cdev, led_cdev->brightness);
#endif
	}

	return ret;
}

static ssize_t led_max_brightness_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);

	return snprintf(buf, LED_BUFF_SIZE, "%u\n", led_cdev->max_brightness);
}

#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS)
static int lge_thm_status = 0;
static ssize_t thermald_status_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
        return sprintf(buf, "%d\n",lge_thm_status);

}

static ssize_t thermald_status_store(struct device *dev,
                struct device_attribute *attr, const char *buf, size_t size)
{
        struct led_classdev *led_cdev = dev_get_drvdata(dev);
        unsigned long state = 0;
        int rc = 1;
        if (strncmp(buf, "0", 1) == 0)
                lge_thm_status = 0;
        else if (strncmp(buf, "1", 1) == 0)
	{
                state = LED_FULL;
                led_cdev->max_brightness = state;
                led_set_brightness(led_cdev, led_cdev->brightness);

                lge_thm_status = 1;
	}
        return rc;
}
#endif

static struct device_attribute led_class_attrs[] = {
#if defined(CONFIG_MACH_LGE_FX3_VZW) || defined(CONFIG_MACH_LGE_FX3Q_TMUS)
	__ATTR(thermald_status, 0644, thermald_status_show, thermald_status_store),
#endif
	__ATTR(brightness, 0644, led_brightness_show, led_brightness_store),
#if defined (CONFIG_BACKLIGHT_LM3639)
	__ATTR(reg, 0644, led_reg_show, led_reg_store),
#endif
	__ATTR(max_brightness, 0644, led_max_brightness_show,
			led_max_brightness_store),
#ifdef CONFIG_LEDS_TRIGGERS
	__ATTR(trigger, 0644, led_trigger_show, led_trigger_store),
#endif
	__ATTR_NULL,
};

static void led_timer_function(unsigned long data)
{
	struct led_classdev *led_cdev = (void *)data;
	unsigned long brightness;
	unsigned long delay;

	if (!led_cdev->blink_delay_on || !led_cdev->blink_delay_off) {
		led_set_brightness(led_cdev, LED_OFF);
		return;
	}

	brightness = led_get_brightness(led_cdev);
	if (!brightness) {
		/* Time to switch the LED on. */
		brightness = led_cdev->blink_brightness;
		delay = led_cdev->blink_delay_on;
	} else {
		/* Store the current brightness value to be able
		 * to restore it when the delay_off period is over.
		 */
		led_cdev->blink_brightness = brightness;
		brightness = LED_OFF;
		delay = led_cdev->blink_delay_off;
	}

	led_set_brightness(led_cdev, brightness);

	mod_timer(&led_cdev->blink_timer, jiffies + msecs_to_jiffies(delay));
}

/**
 * led_classdev_suspend - suspend an led_classdev.
 * @led_cdev: the led_classdev to suspend.
 */
void led_classdev_suspend(struct led_classdev *led_cdev)
{
	led_cdev->flags |= LED_SUSPENDED;
	led_cdev->brightness_set(led_cdev, 0);
}
EXPORT_SYMBOL_GPL(led_classdev_suspend);

/**
 * led_classdev_resume - resume an led_classdev.
 * @led_cdev: the led_classdev to resume.
 */
void led_classdev_resume(struct led_classdev *led_cdev)
{
	led_cdev->brightness_set(led_cdev, led_cdev->brightness);
	led_cdev->flags &= ~LED_SUSPENDED;
}
EXPORT_SYMBOL_GPL(led_classdev_resume);

static int led_suspend(struct device *dev, pm_message_t state)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);

	if (led_cdev->flags & LED_CORE_SUSPENDRESUME)
		led_classdev_suspend(led_cdev);

	return 0;
}

static int led_resume(struct device *dev)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);

	if (led_cdev->flags & LED_CORE_SUSPENDRESUME)
		led_classdev_resume(led_cdev);

	return 0;
}

/**
 * led_classdev_register - register a new object of led_classdev class.
 * @parent: The device to register.
 * @led_cdev: the led_classdev structure for this device.
 */
int led_classdev_register(struct device *parent, struct led_classdev *led_cdev)
{
	led_cdev->dev = device_create(leds_class, parent, 0, led_cdev,
				      "%s", led_cdev->name);
	if (IS_ERR(led_cdev->dev))
		return PTR_ERR(led_cdev->dev);

#ifdef CONFIG_LEDS_TRIGGERS
	init_rwsem(&led_cdev->trigger_lock);
#endif
	/* add to the list of leds */
	down_write(&leds_list_lock);
	list_add_tail(&led_cdev->node, &leds_list);
	up_write(&leds_list_lock);

	if (!led_cdev->max_brightness)
		led_cdev->max_brightness = LED_FULL;

	led_update_brightness(led_cdev);

	init_timer(&led_cdev->blink_timer);
	led_cdev->blink_timer.function = led_timer_function;
	led_cdev->blink_timer.data = (unsigned long)led_cdev;

#ifdef CONFIG_LEDS_TRIGGERS
	led_trigger_set_default(led_cdev);
#endif

	printk(KERN_DEBUG "Registered led device: %s\n",
			led_cdev->name);

	return 0;
}
EXPORT_SYMBOL_GPL(led_classdev_register);

/*                                                         */
#ifdef CONFIG_LGE_PM8038_KPJT
static ssize_t get_pattern(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", playing_pattern);
}

static ssize_t set_pattern(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int pattern_num = 0;
	int pattern_r_on = 0;
	int pattern_g_on = 0;
	int pattern_b_on = 0;

    if(sscanf(buf, "%d,%d,%d,%d",&pattern_num,&pattern_r_on,&pattern_g_on,&pattern_b_on) !=4) {
		printk("[PMIC K-PJT] bad arguments ");
	}

	printk("[PMIC K-PJT] pattern_num=%d r=%d g=%d b=%d\n",pattern_num,pattern_r_on,pattern_g_on,pattern_b_on);
	change_led_pattern(pattern_num, pattern_r_on,pattern_g_on,pattern_b_on);

    playing_pattern = pattern_num;

	return size;
}

static DEVICE_ATTR(setting, S_IRUGO | S_IWUSR | S_IXOTH , get_pattern, set_pattern);

static ssize_t confirm_pwm_pattern(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count=0;

//	count = sprintf(buf,"%d %d\n", pattern_num,pattern_on );

	return count;
}


static ssize_t make_pwm_pattern(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{

	ssize_t ret = -EINVAL;
	int patterns[79];
	int i=0;

	if(sscanf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
		            &patterns[0], &patterns[1], &patterns[2], &patterns[3], &patterns[4], &patterns[5], &patterns[6], &patterns[7], &patterns[8], &patterns[9], &patterns[10],
		            &patterns[11],&patterns[12],&patterns[13],&patterns[14],&patterns[15],&patterns[16],&patterns[17],&patterns[18],&patterns[19],&patterns[20],&patterns[21],
		            &patterns[22],&patterns[23],&patterns[24],&patterns[25],&patterns[26],&patterns[27],&patterns[28],&patterns[29],&patterns[30],&patterns[31],&patterns[32],
		            &patterns[33],&patterns[34],&patterns[35],&patterns[36],&patterns[37],&patterns[38],&patterns[39],&patterns[40],&patterns[41],&patterns[42],&patterns[43],
		            &patterns[44],&patterns[45],&patterns[46],&patterns[47],&patterns[48],&patterns[49],&patterns[50],&patterns[51],&patterns[52],&patterns[53],&patterns[54],
		            &patterns[55],&patterns[56],&patterns[57],&patterns[58],&patterns[59],&patterns[60],&patterns[61],&patterns[62],
		            &patterns[63],&patterns[64],&patterns[65],&patterns[66],
		            &patterns[67],&patterns[68],&patterns[69],&patterns[70],
		            &patterns[71],&patterns[72],&patterns[73],&patterns[74],
		            &patterns[75],&patterns[76],&patterns[77],&patterns[78]) !=79){
		printk("[PMIC K-PJT] bad arguments ");
	}
	ret = size;

	printk("[PMIC K-PJT] LUT is \n");
	for(i=0;i<ARRAY_SIZE(patterns);i++){
		printk("%d ",patterns[i]);
	}

	make_pwm_led_pattern((int *)&patterns);
	return ret;
}

static DEVICE_ATTR(input_patterns, S_IRUGO | S_IWUSR | S_IXOTH , confirm_pwm_pattern, make_pwm_pattern);


static ssize_t confirm_blink_pattern(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count=0;

//	count = sprintf(buf,"%d %d\n", pattern_num,pattern_on );

	return count;
}


static ssize_t make_blink_pattern(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{

	ssize_t ret = -EINVAL;
	int delay_on;
	int delay_off;
	int on;

	int red;
	int green;
	int blue;

    if(sscanf(buf, "%d,%d,%d,%d,%d,%d",&red,&green,&blue,&delay_on,&delay_off,&on) !=6){
		printk("[PMIC K-PJT] bad arguments ");
	}
		ret = size;
	


	printk("\n");
	printk("[PMIC K-PJT] led-class.c Original red is %d, green is %d, blue is %d\n",red,green,blue);
	red = red*100/255;
	green = green*100/255;
	blue = blue*100/255;
	printk("[PMIC K-PJT] led-class.c Translated red is %d, green is %d, blue is %d\n",red,green,blue);
	printk("[PMIC K-PJT] led-class.c delay_on is %d\n",delay_on);
	printk("[PMIC K-PJT] led-class.c delay_off %d\n",delay_off);
	printk("[PMIC K-PJT] led-class.c on %d\n",on);
	make_blink_led_pattern(red,green,blue,delay_on,delay_off,on);
	
	//led_blink_set(

	return ret;
}

static DEVICE_ATTR(blink_patterns, S_IRUGO | S_IWUSR | S_IXOTH , confirm_blink_pattern, make_blink_pattern);


static ssize_t get_test_pattern(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count=0;

//	count = sprintf(buf,"%d %d\n", pattern_num,pattern_on );

	return count;
}


static ssize_t set_test_pattern(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{

	ssize_t ret = -EINVAL;
	int test;
    if(sscanf(buf, "%d",&test) !=1){
		printk("[PMIC K-PJT] bad arguments ");
	}
		ret = size;
	

	pattern_test(test);
	return ret;
}

static DEVICE_ATTR(test_patterns, S_IRUGO | S_IWUSR | S_IXOTH , get_test_pattern, set_test_pattern);


int led_pattern_sysfs_register()
{
	struct class *kpjt;
	struct device *pattern_sysfs_dev;
	struct device *make_pattern_sysfs_dev;
	kpjt = class_create(THIS_MODULE, "pmic-kpjt");
	if (IS_ERR(kpjt))
	{
		printk("Failed to create class(pmic-kpjt)!\n");
	}
	pattern_sysfs_dev = device_create(kpjt, NULL, 0, NULL, "use_patterns");
	if (IS_ERR(pattern_sysfs_dev))
		return PTR_ERR(pattern_sysfs_dev);

	if (device_create_file(pattern_sysfs_dev, &dev_attr_setting) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_setting.attr.name);


	make_pattern_sysfs_dev = device_create(kpjt, NULL, 0, NULL, "make_patterns");
	if (IS_ERR(make_pattern_sysfs_dev))
		return PTR_ERR(make_pattern_sysfs_dev);

	if (device_create_file(make_pattern_sysfs_dev, &dev_attr_input_patterns) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_input_patterns.attr.name);

	if (device_create_file(make_pattern_sysfs_dev, &dev_attr_blink_patterns) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_blink_patterns.attr.name);

	if (device_create_file(make_pattern_sysfs_dev, &dev_attr_test_patterns) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_test_patterns.attr.name);

	return 0;
}
EXPORT_SYMBOL_GPL(led_pattern_sysfs_register);
#else
#endif
/*                                                         */

/**
 * led_classdev_unregister - unregisters a object of led_properties class.
 * @led_cdev: the led device to unregister
 *
 * Unregisters a previously registered via led_classdev_register object.
 */
void led_classdev_unregister(struct led_classdev *led_cdev)
{
#ifdef CONFIG_LEDS_TRIGGERS
	down_write(&led_cdev->trigger_lock);
	if (led_cdev->trigger)
		led_trigger_set(led_cdev, NULL);
	up_write(&led_cdev->trigger_lock);
#endif

	/* Stop blinking */
	led_brightness_set(led_cdev, LED_OFF);

	device_unregister(led_cdev->dev);

	down_write(&leds_list_lock);
	list_del(&led_cdev->node);
	up_write(&leds_list_lock);
}
EXPORT_SYMBOL_GPL(led_classdev_unregister);

#ifdef CONFIG_LGE_PM //Control LCD brightness by one channel
int leds_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	int ret = 0;
	static int prev_max_brightness = 255;
	char up[] = "UP";
	char down[] = "DOWN";

	ret = add_uevent_var(env, "DEV_NAME=%s", "lcd-backlight");
	if (ret)
		return ret;
	ret = add_uevent_var(env, "DIRECTION=%s",
		(prev_max_brightness > led_cdev->max_brightness) ? up : down);
	if (ret)
		return ret;
	ret = add_uevent_var(env, "MAX=%d", led_cdev->max_brightness);
	prev_max_brightness = led_cdev->max_brightness;
	if (ret)
		return ret;
	ret = add_uevent_var(env, "CURRENT=%d", led_cdev->brightness);
	if (ret)
		return ret;

  return 0;
}
#endif

static int __init leds_init(void)
{
	leds_class = class_create(THIS_MODULE, "leds");
	if (IS_ERR(leds_class))
		return PTR_ERR(leds_class);
	leds_class->suspend = led_suspend;
	leds_class->resume = led_resume;
#ifdef CONFIG_LGE_PM
	leds_class->dev_uevent = leds_uevent;
#endif
	leds_class->dev_attrs = led_class_attrs;
	return 0;
}

static void __exit leds_exit(void)
{
	class_destroy(leds_class);
}

subsys_initcall(leds_init);
module_exit(leds_exit);

MODULE_AUTHOR("John Lenz, Richard Purdie");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LED Class Interface");
