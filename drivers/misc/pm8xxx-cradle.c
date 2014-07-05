/*
 * Copyright (C) 2011-2012 LGE, Inc.
 *
 * Author: Fred Cho <fred.cho@lge.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pm8xxx/cradle.h>
#include <linux/gpio.h>
#include <linux/switch.h>
#include <linux/wakelock.h>

static int pre_set_flag;
struct pm8xxx_cradle {
	struct switch_dev sdev;
	struct work_struct work; //TEMP_FX3Q
	struct delayed_work delay_work; //TEMP_FX3Q
	struct device *dev;
	const struct pm8xxx_cradle_platform_data *pdata;
	int carkit;
	int pouch;
	spinlock_t lock;
	struct wake_lock wake_lock;
	int state;
};

static struct workqueue_struct *cradle_wq;
static struct pm8xxx_cradle *cradle;

static void pm8xxx_cradle_work_func(struct work_struct *work)
{
	int state;
	unsigned long flags;

	if (cradle->pdata->carkit_detect_pin)
		cradle->carkit = !gpio_get_value_cansleep(cradle->pdata->carkit_detect_pin);
	if (cradle->pdata->pouch_detect_pin)
		cradle->pouch = !gpio_get_value_cansleep(cradle->pdata->pouch_detect_pin);

	printk("carkit === > %d , pouch === > %d \n", cradle->carkit , cradle->pouch);

	spin_lock_irqsave(&cradle->lock, flags);
	if (cradle->carkit == 1)
		state = CRADLE_CARKIT;
	else if (cradle->pouch == 1)
		state = CRADLE_POUCH;
	else
		state = CRADLE_NO_DEV;
	printk("[Cradle] cradle value is %d\n", state);
	cradle->state = state;
	spin_unlock_irqrestore(&cradle->lock, flags);
	switch_set_state(&cradle->sdev, cradle->state);
}

void cradle_set_deskdock(int state)
{
	unsigned long flags;

	if (&cradle->sdev) {
		spin_lock_irqsave(&cradle->lock, flags);
		cradle->state = state;
		spin_unlock_irqrestore(&cradle->lock, flags);
		switch_set_state(&cradle->sdev, cradle->state);
	} else {
		pre_set_flag = state;
	}
}

int cradle_get_deskdock(void)
{
	if (!cradle)
		return pre_set_flag;

	return cradle->state;
}

#if defined(CONFIG_BU52031NVX_CARKITDETECT)
static irqreturn_t pm8xxx_carkit_irq_handler(int irq, void *handle)
{
	struct pm8xxx_cradle *cradle_handle = handle;
	printk("carkit irq!!!!!\n");
	queue_work(cradle_wq, &cradle_handle->work);
	return IRQ_HANDLED;
}
#endif

#if defined(CONFIG_BU52031NVX_POUCHDETECT)
static irqreturn_t pm8xxx_pouch_irq_handler(int irq, void *handle)
{
	struct pm8xxx_cradle *cradle_handle = handle;
	printk("pouch irq!!!!\n");
	queue_work(cradle_wq, &cradle_handle->work);
	return IRQ_HANDLED;
}
#endif

#if defined(CONFIG_BU52031NVX_CARKITDETECT)
static ssize_t
cradle_carkit_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len;
	struct pm8xxx_cradle *cradle = dev_get_drvdata(dev);
	len = snprintf(buf, PAGE_SIZE, "%d\n", !cradle->carkit);
	return len;
}
#endif

#if defined(CONFIG_BU52031NVX_POUCHDETECT)
static ssize_t
cradle_pouch_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len;
	struct pm8xxx_cradle *cradle = dev_get_drvdata(dev);
	len = snprintf(buf, PAGE_SIZE, "%d\n", !cradle->pouch);
	return len;

}
#endif

static ssize_t
cradle_sensing_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len;
	struct pm8xxx_cradle *cradle = dev_get_drvdata(dev);
	len = snprintf(buf, PAGE_SIZE, "%d\n", cradle->state);
	return len;

}

static ssize_t
cradle_send_event_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct pm8xxx_cradle *cradle = dev_get_drvdata(dev);
	return switch_get_state(&cradle->sdev);
}


static ssize_t
cradle_send_event_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct pm8xxx_cradle *cradle = dev_get_drvdata(dev);
	int cmd;

	if (sscanf(buf, "%d", &cmd) != 1)
		return -EINVAL;

	switch (cmd) {
	case 0: /* touch power on */
		switch_set_state(&cradle->sdev, 0);
		break;
	case 1: /*touch power off */
		switch_set_state(&cradle->sdev, 1);
		break;
	case 2:
		switch_set_state(&cradle->sdev, 2);
		break;
	case 256:
		switch_set_state(&cradle->sdev, 256);
		break;
	default:
		printk(KERN_INFO "ERROR  invalid value\n");
		break;
	}
	return count;
}


static struct device_attribute cradle_device_attrs[] = {
#if defined(CONFIG_BU52031NVX_CARKITDETECT)
	__ATTR(carkit,  S_IRUGO | S_IWUSR, cradle_carkit_show, NULL),
#endif
#if defined(CONFIG_BU52031NVX_POUCHDETECT)
	__ATTR(pouch, S_IRUGO | S_IWUSR, cradle_pouch_show, NULL),
#endif
	__ATTR(sensing,  S_IRUGO | S_IWUSR, cradle_sensing_show, NULL),
	__ATTR(send_event ,  S_IRUGO | S_IWUSR, cradle_send_event_show, cradle_send_event_store),
};


static ssize_t cradle_print_name(struct switch_dev *sdev, char *buf)
{
	switch (switch_get_state(sdev)) {
	case 0:
		return sprintf(buf, "UNDOCKED\n");
	case 2:
		return sprintf(buf, "CARKIT\n");
	}
	return -EINVAL;
}

static int __devinit pm8xxx_cradle_probe(struct platform_device *pdev)
{
	int ret, i;
	const struct pm8xxx_cradle_platform_data *pdata =
						pdev->dev.platform_data;
	if (!pdata)
		return -EINVAL;

	cradle = kzalloc(sizeof(*cradle), GFP_KERNEL);
	if (!cradle)
		return -ENOMEM;

	cradle->pdata	= pdata;

	cradle->sdev.name = "dock";
	cradle->sdev.print_name = cradle_print_name;
	cradle->pouch = cradle->carkit = 0;

	spin_lock_init(&cradle->lock);

	ret = switch_dev_register(&cradle->sdev);
	if (ret < 0)
		goto err_switch_dev_register;

	if (pre_set_flag) {
		cradle_set_deskdock(pre_set_flag);
		cradle->state = pre_set_flag;
	}
	INIT_WORK(&cradle->work, pm8xxx_cradle_work_func);

	printk("%s : init cradle\n", __func__);

#if defined(CONFIG_BU52031NVX_CARKITDETECT)
	if (pdata->carkit_irq) {
		ret = request_irq(pdata->carkit_irq, pm8xxx_carkit_irq_handler, pdata->irq_flags, PM8XXX_CRADLE_DEV_NAME, cradle);
		if (ret > 0) {
			printk(KERN_ERR "%s: Can't allocate irq %d, ret %d\n", __func__, pdata->carkit_irq, ret);
			goto err_request_irq;
		}
	}

#endif

#if defined(CONFIG_BU52031NVX_POUCHDETECT)
	if (pdata->pouch_irq) {
		ret = request_irq(pdata->pouch_irq, pm8xxx_pouch_irq_handler, pdata->irq_flags, PM8XXX_CRADLE_DEV_NAME, cradle);
		if (ret > 0) {
			printk(KERN_ERR "%s: Can't allocate irq %d, ret %d\n", __func__, pdata->pouch_irq, ret);
			goto err_request_irq;
		}
	}
#endif

	for (i = 0; i < ARRAY_SIZE(cradle_device_attrs); i++) {
		ret = device_create_file(&pdev->dev, &cradle_device_attrs[i]);
		if (ret)
			goto err_request_irq;
	}

	platform_set_drvdata(pdev, cradle);
	return 0;

err_request_irq:
	if (pdata->carkit_irq)
		free_irq(pdata->carkit_irq, 0);
	if (pdata->pouch_irq)
		free_irq(pdata->pouch_irq, 0);

err_switch_dev_register:
#if defined(CONFIG_BU52031NVX_CARKITDETECT)
	if (pdata->carkit_irq)
		free_irq(pdata->carkit_irq, 0);
#endif
#if defined(CONFIG_BU52031NVX_POUCHDETECT)
	if (pdata->pouch_irq)
		free_irq(pdata->pouch_irq, 0);
#endif

	switch_dev_unregister(&cradle->sdev);
	kfree(cradle);
	return ret;
}

static int __devexit pm8xxx_cradle_remove(struct platform_device *pdev)
{
	struct pm8xxx_cradle *cradle = platform_get_drvdata(pdev);

	cancel_work_sync(&cradle->work);
	switch_dev_unregister(&cradle->sdev);
	platform_set_drvdata(pdev, NULL);
	kfree(cradle);

	return 0;
}

static int pm8xxx_cradle_suspend(struct device *dev)
{
	return 0;
}

static int pm8xxx_cradle_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops pm8xxx_cradle_pm_ops = {
	.suspend = pm8xxx_cradle_suspend,
	.resume = pm8xxx_cradle_resume,
};

static struct platform_driver pm8xxx_cradle_driver = {
	.probe		= pm8xxx_cradle_probe,
	.remove		= __devexit_p(pm8xxx_cradle_remove),
	.driver		= {
		.name	= PM8XXX_CRADLE_DEV_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &pm8xxx_cradle_pm_ops,
#endif
	},
};

static int __init pm8xxx_cradle_init(void)
{
	cradle_wq = create_singlethread_workqueue("cradle_wq");
	if (!cradle_wq)
		return -ENOMEM;
	return platform_driver_register(&pm8xxx_cradle_driver);
}
module_init(pm8xxx_cradle_init);

static void __exit pm8xxx_cradle_exit(void)
{
	if (cradle_wq)
		destroy_workqueue(cradle_wq);
	platform_driver_unregister(&pm8xxx_cradle_driver);
}
module_exit(pm8xxx_cradle_exit);

MODULE_ALIAS("platform:" PM8XXX_CRADLE_DEV_NAME);
MODULE_AUTHOR("LG Electronics Inc.");
MODULE_DESCRIPTION("pm8xxx cradle driver");
MODULE_LICENSE("GPL");
