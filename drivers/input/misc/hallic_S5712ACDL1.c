/* drivers/misc/hall_s5712ACDL1.c
 *
 * (C) 2010 LGE, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/module.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/gpio.h>
#include <asm/io.h>
#include <mach/msm_iomap.h>
#include <mach/board_lge.h>
#include <linux/slab.h>

enum {
	SLIDE_CLOSE = 0,
	SLIDE_OPEN,
};

static int hall_ic_enable;
int hall_ic_on = -1;
static int last_state;

struct hall_ic_device {
	struct input_dev *input_dev;
	struct timer_list timer;
	unsigned int irq_num;
	spinlock_t	lock;
};

static struct hall_ic_device *hallic_dev = NULL;
static struct s5712ACDL1_platform_data *s5712ACDL1_pdata = NULL;

static void hall_ic_report_event(int state)
{
	printk(KERN_INFO"%s start : state = %d\n",__func__, state);

	if (state == SLIDE_OPEN) {
		hall_ic_on = 0;
	} else {
		hall_ic_on = 1;
	}

	printk(KERN_INFO"%s : report event = %d\n",__func__,hall_ic_on);
	input_report_switch(hallic_dev->input_dev, SW_LID, hall_ic_on);
	input_sync(hallic_dev->input_dev);

	return;
}

static void s5712ACDL1_timer(unsigned long arg)
{
	struct hall_ic_device *dev = (struct hall_ic_device *)hallic_dev;
	int current_state;

	printk(KERN_INFO"%s start\n",__func__);

	spin_lock_irq(&dev->lock);
	if (hall_ic_enable)
		goto timer_end;

	current_state = gpio_get_value(s5712ACDL1_pdata->irq_pin);
	printk(KERN_INFO"%s : current_state = %d / last_state = %d\n",__func__,current_state, last_state);
	if (current_state == last_state) {
		hall_ic_enable = 1;
	} else {
		hall_ic_report_event(current_state);

		last_state = current_state;
		mod_timer(&dev->timer, jiffies + (s5712ACDL1_pdata->prohibit_time * HZ / 1000));
	}

timer_end:
	spin_unlock(&dev->lock);

	return;
}

//static int s5712ACDL1_irq_handler(int irq, void *dev_id)
static irqreturn_t s5712ACDL1_irq_handler(int irq, void *dev_id)
{
	struct hall_ic_device *dev = dev_id;

	printk(KERN_INFO"%s start : irq = %d, dev_id = 0x%x\n",__func__, irq, (unsigned int)dev_id);

	spin_lock_irq(&dev->lock);
	if (!hall_ic_enable)
		goto irq_handler_end;

	disable_irq_nosync(hallic_dev->irq_num);

	last_state = gpio_get_value(s5712ACDL1_pdata->irq_pin);
	printk(KERN_INFO"%s : read value = %d\n",__func__,last_state);

	hall_ic_report_event(last_state);

	hall_ic_enable = 0;
	mod_timer(&dev->timer, jiffies + (s5712ACDL1_pdata->prohibit_time * HZ / 1000));
	enable_irq(hallic_dev->irq_num);

irq_handler_end:
	spin_unlock(&dev->lock);

	return IRQ_HANDLED;
}

static ssize_t hall_ic_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (gpio_get_value(s5712ACDL1_pdata->irq_pin) == 1) {
		hall_ic_on = 0;
	} else {
		hall_ic_on = 1;
	}
	return sprintf(buf, "%d\n", hall_ic_on );
}

static ssize_t hall_ic_enable_store(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	int value;
	int current_state;

	sscanf(buf, "%d", &value);

	/* value == 0  : stop hall ic
	 * value == 1  : start hall ic
	 */
	value = !!value;
	spin_lock(&hallic_dev->lock);
	if (value == hall_ic_enable)
		goto enable_store_end;

	hall_ic_enable = value;
	current_state = gpio_get_value(s5712ACDL1_pdata->irq_pin);

	if (!hall_ic_enable) {
		last_state = current_state;
		printk(KERN_INFO"%s : hall_ic disabled\n", __func__);
	} else {
		if (current_state != last_state) {
			hall_ic_report_event(current_state);
			last_state = current_state;
			printk(KERN_INFO"%s : hall_ic enabled\n", __func__);
		}
	}

enable_store_end:
	spin_unlock(&hallic_dev->lock);

	return size;
}

static ssize_t hall_ic_show_timer(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", s5712ACDL1_pdata->prohibit_time);
}

static ssize_t hall_ic_set_timer(struct device *dev, struct device_attribute *attr,
								 const char *buf, size_t size)
{
	int value;

	sscanf(buf, "%d", &value);

	s5712ACDL1_pdata->prohibit_time = value;

	return size;
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR, hall_ic_enable_show, hall_ic_enable_store);
static DEVICE_ATTR(timer, S_IRUGO | S_IWUSR, hall_ic_show_timer , hall_ic_set_timer);

static int s5712ACDL1_probe(struct platform_device *pdev)
{
	struct hall_ic_device *dev;
	struct input_dev *input_dev;
	int ret, err;

	printk(KERN_INFO"%s : start\n",__func__);

	if (!pdev || !pdev->dev.platform_data) {
		printk(KERN_ERR"%s : pdev or platform data is null\n", __func__);
		return -ENODEV;
	}
	s5712ACDL1_pdata = pdev->dev.platform_data;

	printk(KERN_INFO"%s : irq = %d, prohibit_time = %d\n",__func__,s5712ACDL1_pdata->irq_pin,s5712ACDL1_pdata->prohibit_time);

	if (!s5712ACDL1_pdata->irq_pin) {
		printk(KERN_ERR"%s : platform data is invalid\n", __func__);
		return -ENODEV;
	}

	dev = kzalloc(sizeof(struct hall_ic_device), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!dev || !input_dev) {
		ret = -ENOMEM;
		printk(KERN_ERR"%s : input_allocate_device failed\n", __func__);
		goto err_input_register_device;
	}

	platform_set_drvdata(pdev, dev);
	hallic_dev = dev; /* for miscdevice */

	input_dev->name = "hall-ic";
	input_dev->phys = "hallic/input";
	input_dev->dev.parent = &pdev->dev;
	input_dev->evbit[0] = BIT_MASK(EV_SW);

	set_bit(SW_LID, input_dev->swbit);

	ret = input_register_device(input_dev);
	if (ret) {
		printk(KERN_ERR"%s : input_register_device failed\n", __func__);
		goto err_input_register_device;
	}
	dev->input_dev = input_dev;
	spin_lock_init(&dev->lock);

	setup_timer(&dev->timer, s5712ACDL1_timer, (unsigned long)dev);

	hall_ic_enable = 1;
	dev->irq_num = gpio_to_irq(s5712ACDL1_pdata->irq_pin);

	printk(KERN_INFO"%s : dev->irq_num = %d, hallic_dev->irq_num = %d\n",__func__,dev->irq_num,hallic_dev->irq_num);

	ret = request_irq(hallic_dev->irq_num, s5712ACDL1_irq_handler,
			IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING,
			"hall-ic", dev);
	if (ret) {
		printk(KERN_ERR"%s : request_irq failed\n", __func__);
		goto err_request_irq;
	}

//	err = set_irq_wake(hallic_dev->irq_num, 1);
	err = irq_set_irq_wake(hallic_dev->irq_num, 1);
	if (err)
		printk(KERN_ERR"%s : set_irq_wake failed\n", __func__);

	ret = device_create_file(&pdev->dev, &dev_attr_enable);
	if (ret) {
		printk(KERN_ERR"%s : device_create_file failed\n", __func__);
		goto err_request_irq;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_timer);
	if (ret) {
		printk(KERN_ERR"%s : device_create_file failed\n", __func__);
		device_remove_file(&pdev->dev, &dev_attr_enable);
		goto err_request_irq;
	}

	last_state = gpio_get_value(s5712ACDL1_pdata->irq_pin);
	printk(KERN_INFO"%s : read value = %d\n",__func__, last_state);

	if (last_state == SLIDE_OPEN) {
		hall_ic_on = 0;
		input_report_switch(dev->input_dev, SW_LID, 0);
		input_sync(dev->input_dev);
	} else {
		hall_ic_on = 1;
		input_report_switch(dev->input_dev, SW_LID, 1);
		input_sync(dev->input_dev);
	}

	printk(KERN_ERR "android-hall_ic: hall_ic_probe: Done\n");
	return 0;

err_request_irq:
	input_unregister_device(input_dev);
err_input_register_device:
	input_free_device(input_dev);
	kfree(dev);
	return ret;
}

static int s5712ACDL1_remove(struct platform_device *pdev)
{
	struct hall_ic_device *dev = (struct hall_ic_device *)hallic_dev;

	device_remove_file(&pdev->dev, &dev_attr_enable);
	device_remove_file(&pdev->dev, &dev_attr_timer);
	del_timer_sync(&dev->timer);

	return 0;
}

static struct platform_driver s5712ACDL1_driver = {
	.probe		= s5712ACDL1_probe,
	.remove		= s5712ACDL1_remove,
	.driver		= {
		.name		= "hall-ic",
		.owner		= THIS_MODULE,
	},
};

static int __init s5712ACDL1_init(void)
{
	return platform_driver_register(&s5712ACDL1_driver);
}

static void __exit s5712ACDL1_exit(void)
{
	platform_driver_unregister(&s5712ACDL1_driver);
}

module_init(s5712ACDL1_init);
module_exit(s5712ACDL1_exit);

MODULE_DESCRIPTION("s5712ACDL1 hall ic driver for Android");
MODULE_LICENSE("GPL");
