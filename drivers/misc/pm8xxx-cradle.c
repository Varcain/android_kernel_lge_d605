/*
 * Copyright LG Electronics (c) 2011
 * All rights reserved.
 * Author: Fred Cho <fred.cho@lge.com>
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
	struct delayed_work work;
	struct device *dev;
	const struct pm8xxx_cradle_platform_data *pdata;
	int carkit;
	int pouch;
	int carkit_pre;
	int pouch_pre;
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

	if (cradle->pdata->carkit_detect_pin) {
		cradle->carkit = !gpio_get_value_cansleep(cradle->pdata->carkit_detect_pin);
		if (cradle->carkit_pre != cradle->carkit) {
			cradle->carkit_pre = cradle->carkit;
			queue_delayed_work(cradle_wq, &cradle->work, msecs_to_jiffies(100));
			return;
		}
	}

	if (cradle->pdata->pouch_detect_pin) {
		cradle->pouch = !gpio_get_value_cansleep(cradle->pdata->pouch_detect_pin);
		if (cradle->pouch_pre != cradle->pouch) {
			cradle->pouch_pre = cradle->pouch;
			queue_delayed_work(cradle_wq, &cradle->work, msecs_to_jiffies(100));
			return;
		}
	}

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
	wake_lock_timeout(&cradle->wake_lock, 5 * HZ);
	if (cradle->pdata->carkit_detect_pin) {
		cradle->carkit = !gpio_get_value_cansleep(cradle->pdata->carkit_detect_pin);
		if (cradle->carkit_pre != cradle->carkit) {
			cradle->carkit_pre = cradle->carkit;
			queue_delayed_work(cradle_wq, &cradle_handle->work, msecs_to_jiffies(500));
		}
	}
	return IRQ_HANDLED;
}
#endif

#if defined(CONFIG_BU52031NVX_POUCHDETECT)
static irqreturn_t pm8xxx_pouch_irq_handler(int irq, void *handle)
{
	struct pm8xxx_cradle *cradle_handle = handle;
	printk("pouch irq!!!!\n");
	wake_lock_timeout(&cradle->wake_lock, 5 * HZ);
	if (cradle->pdata->pouch_detect_pin) {
		cradle->pouch = !gpio_get_value_cansleep(cradle->pdata->pouch_detect_pin);
		if (cradle->pouch_pre != cradle->pouch) {
			cradle->pouch_pre = cradle->pouch;
			queue_delayed_work(cradle_wq, &cradle_handle->work, msecs_to_jiffies(500));
		}
	}
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
	cradle->pouch = cradle->pouch_pre = 0;
	cradle->carkit = cradle->carkit_pre = 0;
	spin_lock_init(&cradle->lock);
	wake_lock_init(&cradle->wake_lock, WAKE_LOCK_SUSPEND, dev_name(&pdev->dev));

	ret = switch_dev_register(&cradle->sdev);
	if (ret < 0)
		goto err_switch_dev_register;

	if (pre_set_flag) {
		cradle_set_deskdock(pre_set_flag);
		cradle->state = pre_set_flag;
	}
	INIT_DELAYED_WORK(&cradle->work, pm8xxx_cradle_work_func);

	printk("%s : init cradle\n", __func__);
#if defined(CONFIG_BU52031NVX_CARKITDETECT)
	if (pdata->carkit_irq) {
		ret = request_irq(pdata->carkit_irq, pm8xxx_carkit_irq_handler, pdata->irq_flags, PM8XXX_CRADLE_DEV_NAME, cradle);
		if (ret > 0) {
			printk(KERN_ERR "%s: Can't allocate irq %d, ret %d\n", __func__, pdata->carkit_irq, ret);
			goto err_request_irq;
		}
		ret = irq_set_irq_wake(pdata->carkit_irq, 1);
		if (ret > 0) {
			printk(KERN_ERR "%s: Can't set irq wake %d, ret %d\n", __func__, pdata->carkit_irq, ret);
			goto err_request_irq;
		}

	}
	if (cradle->pdata->carkit_detect_pin) {
		cradle->carkit = !gpio_get_value_cansleep(cradle->pdata->carkit_detect_pin);
		if (cradle->carkit_pre != cradle->carkit) {
			cradle->carkit_pre = cradle->carkit;
			queue_delayed_work(cradle_wq, &cradle->work, 0);
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
		ret = irq_set_irq_wake(pdata->pouch_irq, 1);
		if (ret > 0) {
			printk(KERN_ERR "%s: Can't set irq wake %d, ret %d\n", __func__, pdata->pouch_irq, ret);
			goto err_request_irq;
		}
	}
	if (cradle->pdata->pouch_detect_pin) {
		cradle->pouch = !gpio_get_value_cansleep(cradle->pdata->pouch_detect_pin);
		if (cradle->pouch_pre != cradle->pouch) {
			cradle->pouch_pre = cradle->pouch;
			queue_delayed_work(cradle_wq, &cradle->work, 0);
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
#if defined(CONFIG_BU52031NVX_CARKITDETECT)
	if (pdata->carkit_irq)
		free_irq(pdata->carkit_irq, 0);
#endif
#if defined(CONFIG_BU52031NVX_POUCHDETECT)
	if (pdata->pouch_irq)
		free_irq(pdata->pouch_irq, 0);
#endif

err_switch_dev_register:
	wake_lock_destroy(&cradle->wake_lock);
	switch_dev_unregister(&cradle->sdev);
	kfree(cradle);
	return ret;
}

static int __devexit pm8xxx_cradle_remove(struct platform_device *pdev)
{
	struct pm8xxx_cradle *cradle = platform_get_drvdata(pdev);

	wake_lock_destroy(&cradle->wake_lock);
	cancel_delayed_work_sync(&cradle->work);
	switch_dev_unregister(&cradle->sdev);
	platform_set_drvdata(pdev, NULL);
	kfree(cradle);

	return 0;
}

static struct platform_driver pm8xxx_cradle_driver = {
	.probe		= pm8xxx_cradle_probe,
	.remove		= __devexit_p(pm8xxx_cradle_remove),
	.driver		= {
		.name	= PM8XXX_CRADLE_DEV_NAME,
		.owner	= THIS_MODULE,
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
