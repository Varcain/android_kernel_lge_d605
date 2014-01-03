/* arch/arm/mach-msm/qdsp5v2/tpa2055-amp.c
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

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <asm/system.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <asm/ioctls.h>
#include <mach/debug_mm.h>
#include <linux/slab.h>

#include <sound/tpa2028d.h>

#define MODULE_NAME	"tpa2028d_amp"

#define MSM8960_SPK_ON	1
#define MSM8960_SPK_OFF	0

/*                                                      */
#define	DEBUG_AMP_CTL	1
#define AMP_IOCTL_MAGIC 't'
/*                                                        */
/* MOD: add the get value for hiddenmenu */
#define AMP_SET_DATA	_IOW(AMP_IOCTL_MAGIC, 0, struct amp_cal *)
#define AMP_GET_DATA	_IOW(AMP_IOCTL_MAGIC, 1, struct amp_cal *)
/*                                                      */

/*                                                        */
/* MOD: add the call mode */
#define IN1_GAIN 0
#define IN2_GAIN 1
#define SPK_VOL  2
#define HP_LVOL  3
#define HP_RVOL	 4
#define SPK_LIM 5
#define HP_LIM 6
#if 0
struct amp_cal_type {
	u8 in1_gain;
	u8 in2_gain;
	u8 spk_vol;
	u8 hp_lvol;
	u8 hp_rvol;
	u8 mix_in1_gain;
	u8 mix_in2_gain;
	u8 mix_spk_vol;
	u8 mix_hp_lvol;
	u8 mix_hp_rvol;
	u8 spk_lim;
	u8 hp_lim;
	u8 mix_spk_lim;
	u8 mix_hp_lim;
	u8 call_in1_gain;
	u8 call_in2_gain;
	u8 call_spk_vol;
	u8 call_hp_lvol;
	u8 call_hp_rvol;
	u8 call_spk_lim;
	u8 call_hp_lim;
	u8 call_mix_spk_lim;
	u8 call_mix_hp_lim;
};


/*                                                                                            */
/*MOD:apply the audio calibration */
/*                                            */
/*MOD:apply the audio calibration for spkand spk&headset */
#if 0
struct amp_cal_type amp_cal_data = { IN1GAIN_0DB, IN2GAIN_0DB, SPK_VOL_M10DB, HPL_VOL_0DB, HPR_VOL_M60DB, IN1GAIN_0DB, IN2GAIN_0DB, SPK_VOL_M1DB, HPL_VOL_0DB, HPR_VOL_M60DB };
#else
/*
struct amp_cal_type amp_cal_data = { IN1GAIN_0DB, IN2GAIN_12DB, SPK_VOL_M9DB, HPL_VOL_M30DB, HPR_VOL_M30DB, IN1GAIN_0DB, IN2GAIN_12DB, SPK_VOL_M9DB, HPL_VOL_M30DB, HPR_VOL_M30DB };
struct amp_cal_type amp_cal_data = { IN1GAIN_6DB, IN2GAIN_12DB, SPK_VOL_M2DB, HPL_VOL_M10DB, HPR_VOL_M10DB, IN1GAIN_6DB, IN2GAIN_12DB, SPK_VOL_M2DB, HPL_VOL_M10DB, HPR_VOL_M10DB };
struct amp_cal_type amp_cal_data = { IN1GAIN_6DB, IN2GAIN_12DB, SPK_VOL_M2DB, HPL_VOL_M13DB, HPR_VOL_M13DB, IN1GAIN_6DB, IN2GAIN_12DB, SPK_VOL_M2DB, HPL_VOL_M21DB, HPR_VOL_M21DB };
*/
struct amp_cal_type amp_cal_data = {
	IN1GAIN_6DB, IN2GAIN_12DB, SPK_VOL_M2DB, HPL_VOL_M1DB, HPR_VOL_M1DB,
	IN1GAIN_6DB, IN2GAIN_12DB, SPK_VOL_M2DB, HPL_VOL_M21DB, HPR_VOL_M21DB,
	SLIMLVL_4P90V, HLIMLVL_1P15V, SLIMLVL_4P90V, HLIMLVL_1P15V,
	IN1GAIN_6DB, IN2GAIN_12DB, SPK_VOL_M2DB, HPL_VOL_M6DB, HPR_VOL_M6DB,
	SLIMLVL_4P90V, HLIMLVL_1P15V, SLIMLVL_4P90V, HLIMLVL_1P15V};
#endif
/*                                          */
/*                                                                                          */

/*                                                        */
/* MOD: modifiy to delete the first boot noise */
bool first_boot = 1;
/*                                                      */
/*                                                      */
#endif

static uint32_t msm_snd_debug = 1;
module_param_named(debug_mask, msm_snd_debug, uint, 0664);


#if DEBUG_AMP_CTL
#define D(fmt, args...) printk(fmt, ##args)
#else
#define D(fmt, args...) do {} while (0)
#endif

struct amp_data {
	struct i2c_client *client;
	struct audio_amp_platform_data *pdata;
};

static struct amp_data *_data = NULL;

int ReadI2C(char reg, char *ret)
{

	unsigned int err;
	unsigned char buf = reg;

	struct i2c_msg msg[2] = {
		{ _data->client->addr, 0, 1, &buf },
		{ _data->client->addr, I2C_M_RD, 1, ret}
	};
	err = i2c_transfer(_data->client->adapter, msg, 2);
	if (err < 0)
		D(KERN_ERR "%s():i2c read error:%x\n", __func__, reg);
	else
		D(KERN_INFO "%s():i2c read ok:%x\n", __func__, reg);
	return 0;

}

int WriteI2C(char reg, char val)
{

	int	err;
	unsigned char    buf[2];
	struct i2c_msg	msg = { _data->client->addr, 0, 2, buf };

	buf[0] = reg;
	buf[1] = val;
	err = i2c_transfer(_data->client->adapter, &msg, 1);

	if (err < 0) {
		D(KERN_ERR "%s():i2c write error:%x:%x\n", __func__, reg, val);
		return -EIO;
	} else {
		return 0;
	}
}

int tpa2028d_poweron(void)
{
	int fail = 0;
	char agc_compression_rate = 	_data->pdata->agc_compression_rate;
	char agc_output_limiter_disable = _data->pdata->agc_output_limiter_disable;
	char agc_fixed_gain = _data->pdata->agc_fixed_gain;

	agc_output_limiter_disable = (agc_output_limiter_disable<<7);

/*  fail |= WriteI2C(IC_CONTROL, 0xC2);	//Turn on */
	fail |= WriteI2C(IC_CONTROL, 0xE3); /*Tuen On*/
	fail |= WriteI2C(AGC_ATTACK_CONTROL, 0x05); /*Tuen On*/
	fail |= WriteI2C(AGC_RELEASE_CONTROL, 0x0B); /*Tuen On*/
	fail |= WriteI2C(AGC_HOLD_TIME_CONTROL, 0x00); /*Tuen On*/
	fail |= WriteI2C(AGC_FIXED_GAIN_CONTROL, agc_fixed_gain); /*Tuen On*/
	fail |= WriteI2C(AGC1_CONTROL, 0x3A|agc_output_limiter_disable); /*Tuen On*/
	fail |= WriteI2C(AGC2_CONTROL, 0xC0|agc_compression_rate); /*Tuen On*/
	fail |= WriteI2C(IC_CONTROL, 0xC3); /*Tuen On*/

    printk("%s %d\n",__func__,fail);
	return fail;
}

int tpa2028d_powerdown(void)
{
	int fail = 0;
//                                                                                                                                                                
	return fail;
}

inline void set_amp_gain(int amp_state)
{
	int fail = 0;

    printk("%s STATE : %d \n",__func__,amp_state);
    usleep(100);
    switch (amp_state) {
        case MSM8960_SPK_ON:
            /* if the delay time is need for chip ready,
             * should be added to each power or enable function.
             */
            fail = _data->pdata->power(1);
            /*need 5 msec for prevent mute issue*/
            msleep(5);
            fail = _data->pdata->enable(1);
            /*need 10 msec for chip ready*/
            msleep(10);
            fail = tpa2028d_poweron();
            break;
        case MSM8960_SPK_OFF:
            _data->pdata->enable(2);
            fail = tpa2028d_powerdown();
            _data->pdata->enable(0);
            _data->pdata->power(0);
            break;
        default:
		    printk(KERN_DEBUG "Amp_state [%d] does not support \n", amp_state);
	}
    printk("END %s \n",__func__);
}
EXPORT_SYMBOL(set_amp_gain);

/*                                                        */
/* MOD: add the call mode */
/*                                                        */
/* ADD: modifiy the amp for sonification mode for subsystem audio calibration */
/*                                                        */
/* MOD: add the get value for hiddenmenu */
static long amp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int rc = 0;
#if 0
	struct amp_cal amp_cal;

	switch (cmd) {
	case AMP_SET_DATA:
		if (copy_from_user(&amp_cal, (void __user *) arg, sizeof(amp_cal))) {
			MM_ERR("AMP_SET_DATA : invalid pointer\n");
			rc = -EFAULT;
			break;
		}
		switch (amp_cal.dev_type) {
		case ICODEC_HEADSET_SPK_RX:
			if (amp_cal.gain_type == IN1_GAIN)
				amp_cal_data.mix_in1_gain = amp_cal.data;
			else if (amp_cal.gain_type == IN2_GAIN)
				amp_cal_data.mix_in2_gain = amp_cal.data;
			else if (amp_cal.gain_type == SPK_VOL)
				amp_cal_data.mix_spk_vol = amp_cal.data;
			else if (amp_cal.gain_type == HP_LVOL)
				amp_cal_data.mix_hp_lvol = amp_cal.data;
			else if (amp_cal.gain_type == HP_RVOL)
				amp_cal_data.mix_hp_rvol = amp_cal.data;
			else if (amp_cal.gain_type == SPK_LIM)
				amp_cal_data.mix_spk_lim = amp_cal.data;
			else if (amp_cal.gain_type == HP_LIM)
				amp_cal_data.mix_hp_lim = amp_cal.data;
			else {
				MM_ERR("invalid set_gain_type for HP & SPK  %d\n", amp_cal.gain_type);
				rc = -EFAULT;
			}
			break;
		case ICODEC_HEADSET_ST_RX:
			if (amp_cal.gain_type == IN1_GAIN)
				amp_cal_data.in1_gain = amp_cal.data;
			else if (amp_cal.gain_type == HP_LVOL)
				amp_cal_data.hp_lvol = amp_cal.data;
			else if (amp_cal.gain_type == HP_RVOL)
				amp_cal_data.hp_rvol = amp_cal.data;
			else if (amp_cal.gain_type == HP_LIM)
				amp_cal_data.hp_lim = amp_cal.data;
			else {
				MM_ERR("invalid set_gain_type for HP %d\n", amp_cal.gain_type);
				rc = -EFAULT;
			}
			break;
		case ICODEC_SPEAKER_RX:
			if (amp_cal.gain_type == IN2_GAIN)
				amp_cal_data.in2_gain = amp_cal.data;
			else if (amp_cal.gain_type == SPK_VOL)
				amp_cal_data.spk_vol = amp_cal.data;
			else if (amp_cal.gain_type == SPK_LIM)
				amp_cal_data.spk_lim = amp_cal.data;
			else {
				MM_ERR("invalid set_gain_type for SPK %d\n", amp_cal.gain_type);
				rc = -EFAULT;
			}
			break;
		case ICODEC_HEADSET_PHONE_RX:
			if (amp_cal.gain_type == IN1_GAIN)
				amp_cal_data.call_in1_gain = amp_cal.data;
			else if (amp_cal.gain_type == HP_LVOL)
				amp_cal_data.call_hp_lvol = amp_cal.data;
			else if (amp_cal.gain_type == HP_RVOL)
				amp_cal_data.call_hp_rvol = amp_cal.data;
			else if (amp_cal.gain_type == HP_LIM)
				amp_cal_data.call_hp_lim = amp_cal.data;
			else {
				MM_ERR("invalid set_gain_type for HP_PHONE %d\n", amp_cal.gain_type);
				rc = -EFAULT;
			}
			break;
		case ICODEC_SPEAKER_PHONE_RX:
			if (amp_cal.gain_type == IN2_GAIN)
				amp_cal_data.call_in2_gain = amp_cal.data;
			else if (amp_cal.gain_type == SPK_VOL)
				amp_cal_data.call_spk_vol = amp_cal.data;
			else if (amp_cal.gain_type == SPK_LIM)
				amp_cal_data.call_spk_lim = amp_cal.data;
			else {
				MM_ERR("invalid set_gain_type for SPK_PHONE %d\n", amp_cal.gain_type);
				rc = -EFAULT;
			}
			break;

		default:
			MM_ERR("unknown dev type for setdata %d\n", amp_cal.dev_type);
			rc = -EFAULT;
 			break;
		}
		break;
	case AMP_GET_DATA:
		if (copy_from_user(&amp_cal, (void __user *) arg, sizeof(amp_cal))) {
			MM_ERR("AMP_GET_DATA : invalid pointer\n");
			rc = -EFAULT;
			break;
		}
		switch (amp_cal.dev_type) {
		case ICODEC_HEADSET_SPK_RX:
			if (amp_cal.gain_type == IN1_GAIN)
				amp_cal.data = amp_cal_data.mix_in1_gain;
			else if (amp_cal.gain_type == IN2_GAIN)
				amp_cal.data = amp_cal_data.mix_in2_gain;
			else if (amp_cal.gain_type == SPK_VOL)
				amp_cal.data = amp_cal_data.mix_spk_vol;
			else if (amp_cal.gain_type == HP_LVOL)
				amp_cal.data = amp_cal_data.mix_hp_lvol;
			else if (amp_cal.gain_type == HP_RVOL)
				amp_cal.data = amp_cal_data.mix_hp_rvol;
			else if (amp_cal.gain_type == SPK_LIM)
				amp_cal.data = amp_cal_data.mix_spk_lim;
			else if (amp_cal.gain_type == HP_LIM)
				amp_cal.data = amp_cal_data.mix_hp_lim;
			else {
				MM_ERR("invalid get_gain_type for HP & SPK %d\n", amp_cal.gain_type);
				rc = -EFAULT;
			}
			break;
		case ICODEC_HEADSET_ST_RX:
			if (amp_cal.gain_type == IN1_GAIN)
				amp_cal.data = amp_cal_data.in1_gain;
			else if (amp_cal.gain_type == HP_LVOL)
				amp_cal.data = amp_cal_data.hp_lvol;
			else if (amp_cal.gain_type == HP_RVOL)
				amp_cal.data = amp_cal_data.hp_rvol;
			else if (amp_cal.gain_type == HP_LIM)
				amp_cal.data = amp_cal_data.hp_lim;
			else {
				MM_ERR("invalid get_gain_type for HP %d\n", amp_cal.gain_type);
				rc = -EFAULT;
			}
			break;
		case ICODEC_SPEAKER_RX:
			if (amp_cal.gain_type == IN2_GAIN)
				amp_cal.data = amp_cal_data.in2_gain;
			else if (amp_cal.gain_type == SPK_VOL)
				amp_cal.data = amp_cal_data.spk_vol;
			else if (amp_cal.gain_type == SPK_LIM)
				amp_cal.data = amp_cal_data.spk_lim;
			else {
				MM_ERR("invalid get_gain_type for SPK %d\n", amp_cal.gain_type);
				rc = -EFAULT;
			}
			break;
		case ICODEC_HEADSET_PHONE_RX:
			if (amp_cal.gain_type == IN1_GAIN)
				amp_cal.data = amp_cal_data.call_in1_gain;
			else if (amp_cal.gain_type == HP_LVOL)
				amp_cal.data = amp_cal_data.call_hp_lvol;
			else if (amp_cal.gain_type == HP_RVOL)
				amp_cal.data = amp_cal_data.call_hp_rvol;
			else if (amp_cal.gain_type == HP_LIM)
				amp_cal.data = amp_cal_data.call_hp_lim;
			else {
				MM_ERR("invalid get_gain_type for HP_PHONE %d\n", amp_cal.gain_type);
				rc = -EFAULT;
			}
			break;
		case ICODEC_SPEAKER_PHONE_RX:
			if (amp_cal.gain_type == IN2_GAIN)
				amp_cal.data = amp_cal_data.call_in2_gain;
			else if (amp_cal.gain_type == SPK_VOL)
				amp_cal.data = amp_cal_data.call_spk_vol;
			else if (amp_cal.gain_type == SPK_LIM)
				amp_cal.data = amp_cal_data.call_spk_lim;
			else {
				MM_ERR("invalid get_gain_type for SPK_PHONE %d\n", amp_cal.gain_type);
				rc = -EFAULT;
			}
			break;
		default:
			MM_ERR("unknown dev type for getdata %d\n", amp_cal.dev_type);
			rc = -EFAULT;
			break;
		}
		MM_ERR("AMP_GET_DATA :dev %d, gain %d, data %d \n", amp_cal.dev_type, amp_cal.gain_type, amp_cal.data);
		if (copy_to_user((void __user *)arg, &amp_cal, sizeof(amp_cal))) {
			MM_ERR("AMP_GET_DATA : invalid pointer\n");
			rc = -EFAULT;
		}
		break;
	default:
		MM_ERR("unknown command\n");
		rc = -EINVAL;
		break;
	}
#endif
	return rc;
}
/*                                                      */
/*                                                      */

static int amp_open(struct inode *inode, struct file *file)
{
	int rc = 0;
	return rc;
}

static int amp_release(struct inode *inode, struct file *file)
{
	int rc = 0;
	return rc;
}

static const struct file_operations tpa_fops = {
	.owner		= THIS_MODULE,
	.open		= amp_open,
	.release	= amp_release,
	.unlocked_ioctl	= amp_ioctl,
};

struct miscdevice amp_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "tpa_amp",
	.fops = &tpa_fops,
};
/*                                                      */

static ssize_t
tpa2028d_comp_rate_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct audio_amp_platform_data *pdata = _data->pdata;
	int val;

	if (sscanf(buf, "%d", &val) != 1)
		return -EINVAL;
	if (val > 3 || val < 0)
		return -EINVAL;

	pdata->agc_compression_rate = val;
	return count;
}

static ssize_t
tpa2028d_out_lim_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct audio_amp_platform_data *pdata = _data->pdata;
	int val;

	if (sscanf(buf, "%d", &val) != 1)
		return -EINVAL;
	pdata->agc_output_limiter_disable = val&0x0001;
	return count;
}

static ssize_t
tpa2028d_fixed_gain_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct audio_amp_platform_data *pdata = _data->pdata;
	int val;

	if (sscanf(buf, "%d", &val) != 1)
		return -EINVAL;

	pdata->agc_fixed_gain = val;
	return count;
}

static struct device_attribute tpa2028d_device_attrs[] = {
	__ATTR(comp_rate,  S_IRUGO | S_IWUSR, NULL, tpa2028d_comp_rate_store),
	__ATTR(out_lim, S_IRUGO | S_IWUSR, NULL, tpa2028d_out_lim_store),
	__ATTR(fixed_gain, S_IRUGO | S_IWUSR, NULL, tpa2028d_fixed_gain_store),
};

static int tpa2028d_amp_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct amp_data *data;
	struct audio_amp_platform_data *pdata;
	struct i2c_adapter *adapter = client->adapter;
	int err, i;

	printk("MYUNGWON %s \n", __func__);
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA)) {
		err = -EOPNOTSUPP;
		return err;
	}

	if (msm_snd_debug & 1)
		printk(KERN_INFO "%s()\n", __func__);

	pdata = client->dev.platform_data;
	if (pdata == NULL) {
		printk(KERN_DEBUG "%s platform data is null\n", __func__);
		return -ENODEV;
	}

	data = kzalloc(sizeof(struct amp_data), GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;

	_data = data;
	_data->pdata = pdata;
	data->client = client;
	i2c_set_clientdata(client, data);

	if (msm_snd_debug & 1)
		printk(KERN_INFO "%s chip found\n", client->name);

	set_amp_gain(MSM8960_SPK_OFF);

/*                                                        */
/* ADD: modifiy the amp for sonification mode for subsystem audio calibration */
	err = misc_register(&amp_misc);
/*                                                      */

	for (i = 0; i < ARRAY_SIZE(tpa2028d_device_attrs); i++) {
		err = device_create_file(&client->dev, &tpa2028d_device_attrs[i]);
		if (err)
			return err;
	}
	return 0;
}

static int tpa2028d_amp_remove(struct i2c_client *client)
{
	struct amp_data *data = i2c_get_clientdata(client);
	kfree(data);

	printk(KERN_INFO "%s()\n", __func__);
	i2c_set_clientdata(client, NULL);
	return 0;
}


static struct i2c_device_id tpa2028d_amp_idtable[] = {
	{ "tpa2028d_amp", 1 },
};

static struct i2c_driver tpa2028d_amp_driver = {
	.probe = tpa2028d_amp_probe,
	.remove = tpa2028d_amp_remove,
	.id_table = tpa2028d_amp_idtable,
	.driver = {
		.name = MODULE_NAME,
	},
};


static int __init tpa2028d_amp_init(void)
{
    printk("MYUNGWON %s\n",__func__);
	return i2c_add_driver(&tpa2028d_amp_driver);
}

static void __exit tpa2028d_amp_exit(void)
{
	return i2c_del_driver(&tpa2028d_amp_driver);
}

module_init(tpa2028d_amp_init);
module_exit(tpa2028d_amp_exit);

MODULE_DESCRIPTION("TPA2028D Amp Control");
MODULE_AUTHOR("Kim EunHye <ehgrace.kim@lge.com>");
MODULE_LICENSE("GPL");
