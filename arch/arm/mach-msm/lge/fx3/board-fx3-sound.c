#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <mach/board_lge.h>
#include <linux/mfd/pm8xxx/pm8038.h>
#include <linux/regulator/consumer.h>
#include "devices.h"
#include "board-fx3.h"

#ifdef CONFIG_SWITCH_FSA8008
#include "../../../../sound/soc/codecs/wcd9304.h" //                                                                        
#endif

#ifdef CONFIG_LGE_AUDIO_TPA2028D
#include <sound/tpa2028d.h>

/* Add the I2C driver for Audio Amp, ehgrace.kim@lge.cim, 06/13/2011 */
#define TPA2028D_ADDRESS (0xB0>>1)

#define PM8921_GPIO_BASE        NR_GPIO_IRQS
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)  (pm_gpio - 1 + PM8921_GPIO_BASE)
#define MSM_AMP_EN 7//(PM8921_GPIO_PM_TO_SYS(19))

int msm8921_l29_poweron(void)
{
    int rc = 0;
    struct regulator *audio_reg;
    printk("%s \n",__func__);

    audio_reg = regulator_get(NULL, "8921_l29");

    if (IS_ERR(audio_reg)) {
        rc = PTR_ERR(audio_reg);
        pr_err("%s:regulator get failed rc=%d\n", __func__, rc);
        return -1;
    }
    regulator_set_voltage(audio_reg, 0, 1800000);
    rc = regulator_enable(audio_reg);
    if (rc)
        pr_err("%s: regulator_enable failed rc =%d\n", __func__, rc);

    regulator_put(audio_reg);

    return rc;
}

int amp_power(bool on)
{
    printk("%s \n",__func__);
    return 0;
}

int amp_enable(int on_state)
{
    int err = 0;
    static int init_status = 0;

    /*
    struct pm_gpio param = {
        .direction      = PM_GPIO_DIR_OUT,
        .output_buffer  = PM_GPIO_OUT_BUF_CMOS,
        .output_value   = 1,
        .pull           = PM_GPIO_PULL_NO,
        .vin_sel    = PM_GPIO_VIN_S4,
        .out_strength   = PM_GPIO_STRENGTH_MED,
        .function       = PM_GPIO_FUNC_NORMAL,
    };
    */
    printk("%s State:%d\n",__func__,on_state);
    if (init_status == 0) {
        err = gpio_request(MSM_AMP_EN, "AMP_EN");
        if (err)
            pr_err("%s: Error requesting GPIO %d\n", __func__, MSM_AMP_EN);

//        err = pm8xxx_gpio_config(MSM_AMP_EN, &param);
//        if (err)
//            pr_err("%s: Failed to configure gpio %d\n", __func__, MSM_AMP_EN);
//        else
            init_status++;
    }

    switch (on_state) {
        case 0:
            err = gpio_direction_output(MSM_AMP_EN, 0);
            printk(KERN_INFO "%s: AMP_EN is set to 0\n", __func__);
            break;
        case 1:
            err = gpio_direction_output(MSM_AMP_EN, 1);
            printk(KERN_INFO "%s: AMP_EN is set to 1\n", __func__);
            break;
        case 2:
            printk(KERN_INFO "%s: amp enable bypass(%d)\n", __func__, on_state);
            err = 0;
            break;
        default:
            pr_err("amp enable fail\n");
            err = 0;//
            break;
    }
    return err;
}

static struct audio_amp_platform_data amp_platform_data =  {
    .enable = amp_enable,
    .power = amp_power,
    .agc_compression_rate = AGC_COMPRESIION_RATE,
    .agc_output_limiter_disable = AGC_OUTPUT_LIMITER_DISABLE,
    .agc_fixed_gain = AGC_FIXED_GAIN,
};

static struct i2c_board_info lge_i2c_amp_info[] = {
    {
        I2C_BOARD_INFO("tpa2028d_amp", TPA2028D_ADDRESS),
        .platform_data = &amp_platform_data,
    }
};

static struct i2c_registry l_dcm_i2c_audiosubsystem __initdata = {
    I2C_SURF | I2C_FFA | I2C_FLUID | I2C_RUMI,
    MSM_8960_GSBI2_QUP_I2C_BUS_ID,
    lge_i2c_amp_info,
    ARRAY_SIZE(lge_i2c_amp_info),
};

static void __init lge_add_i2c_audiosubsystem_devices(void)
{
    i2c_register_board_info(l_dcm_i2c_audiosubsystem.bus,
            l_dcm_i2c_audiosubsystem.info,
            l_dcm_i2c_audiosubsystem.len);
}
#endif





#ifdef CONFIG_SWITCH_FSA8008
static struct fsa8008_platform_data lge_hs_pdata = {
	.switch_name = "h2w",
	.keypad_name = "hs_detect",
	.key_code = KEY_MEDIA,
	.gpio_detect = GPIO_EAR_SENSE_N,
	.gpio_mic_en = GPIO_EAR_MIC_EN,
	.gpio_jpole  = GPIO_EARPOL_DETECT,
	.gpio_key    = GPIO_EAR_KEY_INT,
	.latency_for_detection = 10, /* 75 -> 10, 2012.03.23 donggyun.kim - spec : 4.5 ms */
	.set_headset_mic_bias = sitar_codec_micbias2_ctl, //                                                                        
};

static struct platform_device lge_hsd_device = {
   .name = "fsa8008",
   .id   = -1,
   .dev = {
      .platform_data = &lge_hs_pdata,
   },
};

static int __init lge_hsd_fsa8008_init(void)
{
	printk(KERN_INFO "lge_hsd_fsa8008_init\n");
	return platform_device_register(&lge_hsd_device);
}

static void __exit lge_hsd_fsa8008_exit(void)
{
	printk(KERN_INFO "lge_hsd_fsa8008_exit\n");
	platform_device_unregister(&lge_hsd_device);
}
#endif

void __init lge_add_sound_devices(void)
{
#ifdef CONFIG_LGE_AUDIO_TPA2028D
    lge_add_i2c_audiosubsystem_devices();
#endif
#ifdef CONFIG_SWITCH_FSA8008
	if(!lge_get_board_usembhc())
	{
		lge_hsd_fsa8008_init();
	}
#endif
}
