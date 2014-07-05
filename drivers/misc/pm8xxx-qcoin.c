#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pm8xxx/direct_qcoin.h>

#include "../staging/android/timed_output.h"

#include <mach/board_lge.h>


struct timed_vibrator_data {
	struct timed_output_dev timed_dev;
	struct hrtimer timer;
	spinlock_t lock;
        struct device *dev;
        int max_timeout;
	atomic_t vibe_gain;				/* default max gain */
	struct direct_qcoin_platform_data *vibe_data;
	struct work_struct work_vibrator_off;
};

static enum hrtimer_restart set_power_off(struct hrtimer *timer)
{
	struct timed_vibrator_data *vib = container_of(timer, struct timed_vibrator_data, timer);

	vib->vibe_data->power_set(vib->dev->parent, 0);
	return HRTIMER_NORESTART;
}

/* LGPS use DC motor */
#if !defined CONFIG_MACH_MSM8930_LGPS9 && !defined CONFIG_MACH_MSM8930_FX3
static enum hrtimer_restart set_power_low(struct hrtimer *timer)
{
	struct timed_vibrator_data *vib = container_of(timer, struct timed_vibrator_data, timer);
	int gain = atomic_read(&vib->vibe_gain);

	vib->vibe_data->power_set(vib->dev->parent, gain);
	return HRTIMER_NORESTART;
}
#endif

static enum hrtimer_restart set_power_high(struct hrtimer *timer)
{
	struct timed_vibrator_data *vib = container_of(timer, struct timed_vibrator_data, timer);

	vib->vibe_data->power_set(vib->dev->parent, vib->vibe_data->high_max);
	return HRTIMER_NORESTART;
}

static int vibrator_get_time(struct timed_output_dev *timed_dev)
{
	struct timed_vibrator_data *vib = container_of(timed_dev, struct timed_vibrator_data, timed_dev);

	if (hrtimer_active(&vib->timer)) {
		ktime_t r = hrtimer_get_remaining(&vib->timer);
		return ktime_to_ms(r);
	} else
		return 0;
}

static void vibrator_enable(struct timed_output_dev *timed_dev, int value)
{
	struct timed_vibrator_data *vib = container_of(timed_dev, struct timed_vibrator_data, timed_dev);
	unsigned long flags;
	static int pre_value;
	spin_lock_irqsave(&vib->lock, flags);

	if (value > vib->max_timeout)
		value = vib->max_timeout;
	hrtimer_cancel(&vib->timer);

/* in case of DC motor */
#if defined(CONFIG_MACH_MSM8930_LGPS9) || defined(CONFIG_MACH_MSM8930_FX3)
	if (value <= 0) {
		if ((pre_value > 0) && (pre_value < 30))
			hrtimer_start(&vib->timer, ktime_set(20 / 1000,
				(20 % 1000) * 1000000), HRTIMER_MODE_REL);
		else
			set_power_off(&vib->timer);
	} else if (value < 60) {
		set_power_high(&vib->timer);
		hrtimer_start(&vib->timer, ktime_set(60 / 1000,
			(60 % 1000) * 1000000), HRTIMER_MODE_REL);
	} else {
		set_power_high(&vib->timer);
		hrtimer_start(&vib->timer, ktime_set(value / 1000,
			(value % 1000) * 1000000), HRTIMER_MODE_REL);
	}
#else
	if (value <= 0) {
		if ((pre_value > 0) && (pre_value < 30))
			hrtimer_start(&vib->timer, ktime_set(20 / 1000,
			(20 % 1000) * 1000000), HRTIMER_MODE_REL);
		else
			set_power_off(&vib->timer);
	} else if (value < 30) {
		set_power_high(&vib->timer);
		hrtimer_start(&vib->timer, ktime_set(30 / 1000,
		     (30 % 1000) * 1000000), HRTIMER_MODE_REL);
	} else if (value < 50) {
		set_power_high(&vib->timer);
		hrtimer_start(&vib->timer, ktime_set(value / 1000,
		     (value % 1000) * 1000000), HRTIMER_MODE_REL);
	} else if (value < 500) {
		set_power_high(&vib->timer);
		vib->timer.function = set_power_low;
		hrtimer_start(&vib->timer, ktime_set(50 / 1000,
		     (50 % 1000) * 1000000), HRTIMER_MODE_REL);
		hrtimer_cancel(&vib->timer);
		vib->timer.function = set_power_off;
		hrtimer_start(&vib->timer, ktime_set((value - 50) / 1000,
		     ((value - 50) % 1000) * 1000000), HRTIMER_MODE_REL);
	} else {
		set_power_low(&vib->timer);
		hrtimer_start(&vib->timer, ktime_set((value - 50) / 1000,
		     ((value - 50) % 1000) * 1000000), HRTIMER_MODE_REL);
	}
#endif
	pre_value = value;
	spin_unlock_irqrestore(&vib->lock, flags);
}

static ssize_t vibrator_amp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct timed_vibrator_data *vib = container_of(dev_, struct timed_vibrator_data, timed_dev);
	int gain = atomic_read(&(vib->vibe_gain));

	return sprintf(buf, "%d\n", gain);
}

static ssize_t vibrator_amp_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct timed_vibrator_data *vib = container_of(dev_, struct timed_vibrator_data, timed_dev);

	int gain;
	int level;
	int level_max = vib->vibe_data->low_max;
	int level_min = vib->vibe_data->low_min;

	sscanf(buf, "%d", &gain);



	if (gain >= GAIN_MAX)
		level = level_max;
	else if (gain < GAIN_MIN)
		level = 0;
	else
		level = (((level_max - level_min) * gain )/ GAIN_MAX) + level_min;

	atomic_set(&vib->vibe_gain, level);

	return size;
}

static ssize_t vibrator_high_max_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct timed_vibrator_data *vib = container_of(dev_, struct timed_vibrator_data, timed_dev);

	return sprintf(buf, "%d\n", vib->vibe_data->high_max);
}

static ssize_t vibrator_high_max_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct timed_vibrator_data *vib = container_of(dev_, struct timed_vibrator_data, timed_dev);

	int gain;

	sscanf(buf, "%d", &gain);

	if (gain < 0)
		return -EINVAL;
	vib->vibe_data->high_max = gain;

	return size;
}

static ssize_t vibrator_low_max_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct timed_vibrator_data *vib = container_of(dev_, struct timed_vibrator_data, timed_dev);

	return sprintf(buf, "%d\n", vib->vibe_data->low_max);
}

static ssize_t vibrator_low_max_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct timed_vibrator_data *vib = container_of(dev_, struct timed_vibrator_data, timed_dev);

	int gain;

	sscanf(buf, "%d", &gain);

	if (gain < 0)
		return -EINVAL;
	vib->vibe_data->low_max = gain;

	return size;
}

static ssize_t vibrator_low_min_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct timed_vibrator_data *vib = container_of(dev_, struct timed_vibrator_data, timed_dev);

	return sprintf(buf, "%d\n", vib->vibe_data->low_min);
}

static ssize_t vibrator_low_min_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct timed_vibrator_data *vib = container_of(dev_, struct timed_vibrator_data, timed_dev);

	int gain;

	sscanf(buf, "%d", &gain);

	if (gain < 0)
		return -EINVAL;
	vib->vibe_data->low_min = gain;

	return size;
}

static struct device_attribute direct_qcoin_attrs[] = {
	__ATTR(amp, S_IRUGO | S_IWUSR, vibrator_amp_show, vibrator_amp_store),
	__ATTR(high_max, S_IRUGO | S_IWUSR, vibrator_high_max_show, vibrator_high_max_store),
	__ATTR(low_max, S_IRUGO | S_IWUSR, vibrator_low_max_show, vibrator_low_max_store),
	__ATTR(low_min, S_IRUGO | S_IWUSR, vibrator_low_min_show, vibrator_low_min_store),
};

struct timed_vibrator_data pm8xxx_qcoin_data = {
	.timed_dev.name = "vibrator",
	.timed_dev.enable = vibrator_enable,
	.timed_dev.get_time = vibrator_get_time,
	.max_timeout = 30000,
	.vibe_data = NULL,
};

static int pm8xxx_qcoin_probe(struct platform_device *pdev)
{
	int ret, i;
	struct timed_vibrator_data *vib;

	vib = kzalloc(sizeof(*vib), GFP_KERNEL);
	if (!vib)
		return -ENOMEM;

	platform_set_drvdata(pdev, &pm8xxx_qcoin_data);
	vib = (struct timed_vibrator_data *)platform_get_drvdata(pdev);

	vib->dev = &pdev->dev;
	vib->vibe_data = (struct direct_qcoin_platform_data *)pdev->dev.platform_data;

	atomic_set(&vib->vibe_gain, vib->vibe_data->amp);

	hrtimer_init(&vib->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vib->timer.function = set_power_off;
	spin_lock_init(&vib->lock);

	ret = timed_output_dev_register(&vib->timed_dev);
	if (ret < 0) {
		timed_output_dev_unregister(&vib->timed_dev);
		return -ENODEV;
	}

	for (i = 0; i < ARRAY_SIZE(direct_qcoin_attrs); i++) {
		ret = device_create_file(vib->timed_dev.dev, &direct_qcoin_attrs[i]);
		if (ret < 0) {
			timed_output_dev_unregister(&vib->timed_dev);
			device_remove_file(vib->timed_dev.dev, &direct_qcoin_attrs[i]);
			return -ENODEV;
		}
	}
	return 0;
}

static int pm8xxx_qcoin_remove(struct platform_device *pdev)
{
	struct timed_vibrator_data *vib = (struct timed_vibrator_data *)platform_get_drvdata(pdev);

	timed_output_dev_unregister(&vib->timed_dev);
	return 0;
}

#ifdef CONFIG_PM
static int pm8xxx_qcoin_suspend(struct device *dev)
{
	struct timed_vibrator_data *vib = dev_get_drvdata(dev);

	set_power_off(&vib->timer);
	return 0;
}

static const struct dev_pm_ops pm8xxx_qcoin_pm_ops = {
	.suspend = pm8xxx_qcoin_suspend,
};
#endif

static struct platform_driver pm8xxx_qcoin_driver = {
	.probe       = pm8xxx_qcoin_probe,
	.remove      = __devexit_p(pm8xxx_qcoin_remove),
	.driver      = {
		.name    = PM8XXX_VIBRATOR_DEV_NAME,
		.owner   = THIS_MODULE,
#ifdef CONFIG_PM
		.pm      = &pm8xxx_qcoin_pm_ops,
#endif
	},
};

static int __init pm8xxx_qcoin_init(void)
{
	return platform_driver_register(&pm8xxx_qcoin_driver);
}
static void __exit pm8xxx_qcoin_exit(void)
{
	platform_driver_unregister(&pm8xxx_qcoin_driver);
}

late_initcall_sync(pm8xxx_qcoin_init);
module_exit(pm8xxx_qcoin_exit);

MODULE_AUTHOR("LG Electronics Inc.");
MODULE_DESCRIPTION("PMIC Q-Coin Motor Driver");
MODULE_LICENSE("GPL");
