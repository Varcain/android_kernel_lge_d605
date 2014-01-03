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
 */

/*            */
#define DEBUG 1
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/mfd/pm8xxx/spk.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/jack.h>
#include <asm/mach-types.h>
#include <mach/socinfo.h>
#include "msm-pcm-routing.h"
#include "../codecs/wcd9304.h"

#ifdef CONFIG_LGE_AUDIO_TPA2028D
#include <sound/tpa2028d.h>
#endif

/* 8930 machine driver */
#if defined(CONFIG_SWITCH_FSA8008)
#include "../../../arch/arm/mach-msm/include/mach/board_lge.h"
#endif

#if defined(CONFIG_MACH_MSM8930_LGPS9)
#include "../../../arch/arm/mach-msm/lge/lgps9/board-lgps9.h"
#endif

#if defined(CONFIG_MACH_MSM8930_FX3)
#include "../../../arch/arm/mach-msm/lge/fx3/board-fx3.h"
#endif

#define MSM8930_SPK_ON 1
#define MSM8930_SPK_OFF 0

#define BTSCO_RATE_8KHZ 8000
#define BTSCO_RATE_16KHZ 16000

#define SPK_AMP_POS	0x1
#define SPK_AMP_NEG	0x2
#define SPKR_BOOST_GPIO 15
#define LEFT_SPKR_AMPL_GPIO 15
#define DEFAULT_PMIC_SPK_GAIN 0x0D
#define SITAR_EXT_CLK_RATE 12288000

#define SITAR_MBHC_DEF_BUTTONS 8
#define SITAR_MBHC_DEF_RLOADS 5
#ifdef CONFIG_FM_RADIO_MI2S_ENABLE
#define GPIO_MI2S_WS     47
#define GPIO_MI2S_SCLK   48
#define GPIO_MI2S_DOUT3  49
#define GPIO_MI2S_DOUT2  50
#define GPIO_MI2S_DOUT1  51
#define GPIO_MI2S_DOUT0  52
#define GPIO_MI2S_MCLK   53

struct request_gpio {
	unsigned gpio_no;
	char *gpio_name;
};
static struct request_gpio mi2s_gpio[] = {
	{
		.gpio_no = GPIO_MI2S_WS,
		.gpio_name = "MI2S_WS",
	},
	{
		.gpio_no = GPIO_MI2S_SCLK,
		.gpio_name = "MI2S_SCLK",
	},
	{
		.gpio_no = GPIO_MI2S_DOUT0,
		.gpio_name = "MI2S_DOUT0",
	},
};

static struct clk *mi2s_osr_clk;
static struct clk *mi2s_bit_clk;
static atomic_t mi2s_rsc_ref;
#endif

#define GPIO_AUX_PCM_DOUT 63
#define GPIO_AUX_PCM_DIN 64
#define GPIO_AUX_PCM_SYNC 65
#define GPIO_AUX_PCM_CLK 66

#ifdef CONFIG_LGE_AUDIO_TPA2028D
#define MSM8960_SPK_ON 1
#define MSM8960_SPK_OFF 0
#endif

//                                                                             
#define GPIO_MIC_FM_SW	51

#define MIC_ON	0
#define FM_ON	1
//                                                                             


static int msm8930_spk_control;
static int msm8930_slim_0_rx_ch = 1;
static int msm8930_slim_0_tx_ch = 1;
static int msm8930_pmic_spk_gain = DEFAULT_PMIC_SPK_GAIN;

#ifdef CONFIG_FM_RADIO_MI2S_ENABLE
static int msm_slim_3_rx_ch = 1;
#endif

#ifdef CONFIG_LGE_AUDIO_TPA2028D

#else
static int msm8930_ext_spk_pamp;
#endif
static int msm8930_btsco_rate = BTSCO_RATE_8KHZ;
static int msm8930_btsco_ch = 1;

static int msm8930_auxpcm_rate = BTSCO_RATE_8KHZ;

static struct clk *codec_clk;
static int clk_users;

static int msm8930_headset_gpios_configured;

static struct snd_soc_jack hs_jack;
static struct snd_soc_jack button_jack;
static atomic_t auxpcm_rsc_ref;

#ifdef CONFIG_ANDROID_SW_IRRC
#define EXT_AMP_MUTE 0
#define EXT_AMP_UNMUTE 1
struct timer_list EXT_AMP_ON_Timer;
unsigned long pm8xxx_spk_mute_data=1; // 0 is unmute
int PM8xxx_EXT_SPK_Force_Mute=0; // 0 mute, 1 unmute
#endif


//                                                                             
static int msm8930_mic_fm_sw_set = MIC_ON;
//                                                                             



static int msm8930_enable_codec_ext_clk(
		struct snd_soc_codec *codec, int enable,
		bool dapm);

static struct sitar_mbhc_config mbhc_cfg = {
	.headset_jack = &hs_jack,
	.button_jack = &button_jack,
	.read_fw_bin = false,
	.calibration = NULL,
	.micbias = SITAR_MICBIAS2,
	.mclk_cb_fn = msm8930_enable_codec_ext_clk,
	.mclk_rate = SITAR_EXT_CLK_RATE,
	.gpio = 0,
	.gpio_irq = 0,
	.gpio_level_insert = 1,
};

#ifdef CONFIG_FM_RADIO_MI2S_ENABLE
/* Shared channel numbers for Slimbus ports that connect APQ to MDM. */
enum {
	SLIM_1_RX_1 = 145, /* BT-SCO and USB TX */
	SLIM_1_TX_1 = 146, /* BT-SCO, USB, and MI2S RX */
	SLIM_3_RX_1 = 151, /* External echo-cancellation ref */
	SLIM_3_RX_2 = 152, /* External echo-cancellation ref */
	SLIM_3_TX_1 = 147, /* HDMI RX */
	SLIM_4_TX_1 = 148, /* In-call recording RX */
	SLIM_4_TX_2 = 149, /* In-call recording RX */
	SLIM_4_RX_1 = 150, /* In-call music delivery TX */
};
#endif

static void msm8930_ext_control(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	pr_debug("%s: msm8930_spk_control = %d", __func__, msm8930_spk_control);
#if defined(CONFIG_LGE_AUDIO_TPA2028D)
    if (msm8930_spk_control == MSM8960_SPK_ON)
         snd_soc_dapm_enable_pin(dapm, "Ext Spk");
    else
        snd_soc_dapm_disable_pin(dapm, "Ext Spk");
#else

    if (msm8930_spk_control == MSM8930_SPK_ON) {
		snd_soc_dapm_enable_pin(dapm, "Ext Spk Left Pos");
		snd_soc_dapm_enable_pin(dapm, "Ext Spk left Neg");
	} else {
		snd_soc_dapm_disable_pin(dapm, "Ext Spk Left Pos");
		snd_soc_dapm_disable_pin(dapm, "Ext Spk Left Neg");
	}
#endif
	snd_soc_dapm_sync(dapm);
}

static int msm8930_get_spk(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm8930_spk_control = %d", __func__, msm8930_spk_control);
	ucontrol->value.integer.value[0] = msm8930_spk_control;
	return 0;
}
static int msm8930_set_spk(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	pr_debug("%s()\n", __func__);
	if (msm8930_spk_control == ucontrol->value.integer.value[0])
		return 0;

	msm8930_spk_control = ucontrol->value.integer.value[0];
	msm8930_ext_control(codec);
	return 1;
}
#ifdef CONFIG_LGE_AUDIO_TPA2028D

#else
static int msm8930_cfg_spkr_gpio(int gpio,
		int enable, const char *gpio_label)
{
	int ret = 0;

	pr_debug("%s: Configure %s GPIO %u",
		__func__, gpio_label, gpio);
	ret = gpio_request(gpio, gpio_label);
	if (ret)
		return ret;

	pr_debug("%s: Enable %s gpio %u\n",
		__func__, gpio_label, gpio);
	gpio_direction_output(gpio, enable);

	return ret;
}

static void msm8960_ext_spk_power_amp_on(u32 spk)
{
	int ret = 0;

	if (spk & (SPK_AMP_POS | SPK_AMP_NEG)) {
		if ((msm8930_ext_spk_pamp & SPK_AMP_POS) &&
			(msm8930_ext_spk_pamp & SPK_AMP_NEG)) {

			pr_debug("%s() External Bottom Speaker Ampl already "
				"turned on. spk = 0x%08x\n", __func__, spk);
			return;
		}

		msm8930_ext_spk_pamp |= spk;

		if ((msm8930_ext_spk_pamp & SPK_AMP_POS) &&
			(msm8930_ext_spk_pamp & SPK_AMP_NEG)) {

			if (socinfo_get_pmic_model() == PMIC_MODEL_PM8917) {
				ret = msm8930_cfg_spkr_gpio(
						LEFT_SPKR_AMPL_GPIO,
						1, "LEFT_SPKR_AMPL");
				if (ret) {
					pr_err("%s: Failed to config ampl gpio %u\n",
						__func__, LEFT_SPKR_AMPL_GPIO);
					return;
				}
			} else {

				/*
				 * 8930 CDP does not have a 5V speaker boost,
				 * hence the GPIO enable for speaker boost is
				 * only required for platforms other than CDP
				 */
//                                         
#if 0
				if (!machine_is_msm8930_cdp()) {
					ret = msm8930_cfg_spkr_gpio(
					  SPKR_BOOST_GPIO, 1, "SPKR_BOOST");
					if (ret) {
						pr_err("%s: Failure: spkr boost gpio %u\n",
						  __func__, SPKR_BOOST_GPIO);
						return;
					}
				}
#endif

#ifdef CONFIG_ANDROID_SW_IRRC
			if(PM8xxx_EXT_SPK_Force_Mute==1){
			printk("msm8960_ext_spk_power_amp_on Force_Mute, PM8xxx_EXT_SPK_Force_Mute = %d\n",PM8xxx_EXT_SPK_Force_Mute);
				pm8xxx_spk_enable(MSM8930_SPK_OFF);
//				pm8xxx_spk_mute(0);
			}
			else{
				pm8xxx_spk_enable(MSM8930_SPK_ON);
				}
#else
				pm8xxx_spk_enable(MSM8930_SPK_ON);
#endif


			}

			pr_debug("%s: sleeping 10 ms after turning on external "
				" Left Speaker Ampl\n", __func__);
			usleep_range(10000, 10000);
		}

	} else  {

		pr_err("%s: ERROR : Invalid External Speaker Ampl. spk = 0x%08x\n",
			__func__, spk);
		return;
	}
}

static void msm8960_ext_spk_power_amp_off(u32 spk)
{
	if (spk & (SPK_AMP_POS | SPK_AMP_NEG)) {
		if (!msm8930_ext_spk_pamp)
			return;

		if (socinfo_get_pmic_model() == PMIC_MODEL_PM8917) {
			gpio_free(LEFT_SPKR_AMPL_GPIO);
			msm8930_ext_spk_pamp = 0;
			return;
		}

//                                         
#if 0
		if (!machine_is_msm8930_cdp()) {
			pr_debug("%s: Free speaker boost gpio %u\n",
					__func__, SPKR_BOOST_GPIO);
			gpio_direction_output(SPKR_BOOST_GPIO, 0);
			gpio_free(SPKR_BOOST_GPIO);
		}
#endif
		pm8xxx_spk_enable(MSM8930_SPK_OFF);
		msm8930_ext_spk_pamp = 0;
		pr_debug("%s: slepping 10 ms after turning on external "
			" Left Speaker Ampl\n", __func__);
		usleep_range(10000, 10000);

	} else  {

		pr_err("%s: ERROR : Invalid External Speaker Ampl. spk = 0x%08x\n",
			__func__, spk);
		return;
	}
}
#endif

static int msm8930_spkramp_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *k, int event)
{
	pr_debug("%s() %x\n", __func__, SND_SOC_DAPM_EVENT_ON(event));
	
#ifdef CONFIG_LGE_AUDIO_TPA2028D
    printk(KERN_DEBUG "spk amp event\n");
    if (SND_SOC_DAPM_EVENT_ON(event))
        set_amp_gain(MSM8960_SPK_ON);
    else
        set_amp_gain(MSM8960_SPK_OFF);
    return 0;
#else

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		if (!strncmp(w->name, "Ext Spk Left Pos", 17))
			msm8960_ext_spk_power_amp_on(SPK_AMP_POS);
		else if (!strncmp(w->name, "Ext Spk Left Neg", 17))
			msm8960_ext_spk_power_amp_on(SPK_AMP_NEG);
		else {
			pr_err("%s() Invalid Speaker Widget = %s\n",
					__func__, w->name);
			return -EINVAL;
		}
	} else {
		if (!strncmp(w->name, "Ext Spk Left Pos", 17))
			msm8960_ext_spk_power_amp_off(SPK_AMP_POS);
		else if (!strncmp(w->name, "Ext Spk Left Neg", 17))
			msm8960_ext_spk_power_amp_off(SPK_AMP_NEG);
		else {
			pr_err("%s() Invalid Speaker Widget = %s\n",
					__func__, w->name);
			return -EINVAL;
		}
	}
	return 0;
#endif
}

static int msm8930_enable_codec_ext_clk(
		struct snd_soc_codec *codec, int enable,
		bool dapm)
{
	pr_debug("%s: enable = %d\n", __func__, enable);

#ifdef CONFIG_SWITCH_FSA8008
	if (enable == MCLK_ON_BANDGAP_ON) {
		clk_users++;
		pr_debug("%s: clk_users = %d\n", __func__, clk_users);
		if (clk_users != 1)
			return 0;

		if (codec_clk) {
			clk_set_rate(codec_clk, SITAR_EXT_CLK_RATE);
			clk_prepare_enable(codec_clk);
			sitar_mclk_enable(codec, enable, dapm);
		} else {
			pr_err("%s: Error setting Sitar MCLK\n", __func__);
			clk_users--;
			return -EINVAL;
		}
	} else if ((enable == MCLK_OFF_BANDGAP_OFF) ||
				(enable == MCLK_OF_BANDGAP_ON)){
		pr_debug("%s: clk_users = %d\n", __func__, clk_users);
		if (clk_users == 0)
			return 0;
		clk_users--;
		if (!clk_users) {
			pr_debug("%s: disabling MCLK. clk_users = %d\n",
					 __func__, clk_users);
			sitar_mclk_enable(codec, enable, dapm);
			clk_disable_unprepare(codec_clk);
		}
	}
#else
	if (enable) {
		clk_users++;
		pr_debug("%s: clk_users = %d\n", __func__, clk_users);
		if (clk_users != 1)
			return 0;

		if (codec_clk) {
			clk_set_rate(codec_clk, SITAR_EXT_CLK_RATE);
			clk_prepare_enable(codec_clk);
			sitar_mclk_enable(codec, 1, dapm);
		} else {
			pr_err("%s: Error setting Sitar MCLK\n", __func__);
			clk_users--;
			return -EINVAL;
		}
	} else {
		pr_debug("%s: clk_users = %d\n", __func__, clk_users);
		if (clk_users == 0)
			return 0;
		clk_users--;
		if (!clk_users) {
			pr_debug("%s: disabling MCLK. clk_users = %d\n",
					 __func__, clk_users);
			sitar_mclk_enable(codec, 0, dapm);
			clk_disable_unprepare(codec_clk);
		}
	}
#endif 	
	return 0;
}

static int msm8930_mclk_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	pr_debug("%s: event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		return msm8930_enable_codec_ext_clk(w->codec, 1, true);
	case SND_SOC_DAPM_POST_PMD:
		return msm8930_enable_codec_ext_clk(w->codec, 0, true);
	}
	return 0;
}

static const struct snd_soc_dapm_widget msm8930_dapm_widgets[] = {

	SND_SOC_DAPM_SUPPLY("MCLK",  SND_SOC_NOPM, 0, 0,
	msm8930_mclk_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

#ifdef CONFIG_LGE_AUDIO_TPA2028D
    SND_SOC_DAPM_SPK("Ext Spk", msm8930_spkramp_event),
#else
	SND_SOC_DAPM_SPK("Ext Spk Left Pos", msm8930_spkramp_event),
	SND_SOC_DAPM_SPK("Ext Spk Left Neg", msm8930_spkramp_event),
#endif
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Digital Mic1", NULL),
	SND_SOC_DAPM_MIC("ANCRight Headset Mic", NULL),
	SND_SOC_DAPM_MIC("ANCLeft Headset Mic", NULL),

	SND_SOC_DAPM_MIC("Digital Mic1", NULL),
	SND_SOC_DAPM_MIC("Digital Mic2", NULL),
	SND_SOC_DAPM_MIC("Digital Mic3", NULL),
	SND_SOC_DAPM_MIC("Digital Mic4", NULL),

};

static const struct snd_soc_dapm_route common_audio_map[] = {

	{"RX_BIAS", NULL, "MCLK"},
	{"LDO_H", NULL, "MCLK"},

	{"MIC BIAS1 Internal1", NULL, "MCLK"},
	{"MIC BIAS2 Internal1", NULL, "MCLK"},

	/* Speaker path */
#if defined(CONFIG_LGE_AUDIO_TPA2028D)
    {"Ext Spk", NULL, "LINEOUT1"},
#else
	{"Ext Spk Left Pos", NULL, "LINEOUT1"},
	{"Ext Spk Left Neg", NULL, "LINEOUT2"},
#endif

	/* Headset Mic */
#ifdef CONFIG_SWITCH_FSA8008
	{"AMIC2", NULL, "LDO_H"},
	{"MIC BIAS2 External", NULL, "Headset Mic"},
#else
	{"AMIC2", NULL, "MIC BIAS2 External"},
	{"MIC BIAS2 External", NULL, "Headset Mic"},
#endif

	/* Microphone path */
/*                                           */
	{"AMIC1", NULL, "MIC BIAS1 External"},
	{"MIC BIAS1 External", NULL, "ANCLeft Headset Mic"},

	{"AMIC3", NULL, "MIC BIAS1 External"},
	{"MIC BIAS1 External", NULL, "ANCRight Headset Mic"},
/*               */
	{"HEADPHONE", NULL, "LDO_H"},

	/**
	 * The digital Mic routes are setup considering
	 * fluid as default device.
	 */

	/**
	 * Digital Mic1. Front Bottom left Mic on Fluid and MTP.
	 * Digital Mic GM5 on CDP mainboard.
	 * Conncted to DMIC1 Input on Sitar codec.
	 */
#if 0
	{"DMIC1", NULL, "MIC BIAS1 External"},
	{"MIC BIAS1 External", NULL, "Digital Mic1"},

	/**
	 * Digital Mic2. Back top MIC on Fluid.
	 * Digital Mic GM6 on CDP mainboard.
	 * Conncted to DMIC2 Input on Sitar codec.
	 */
	{"DMIC2", NULL, "MIC BIAS1 External"},
	{"MIC BIAS1 External", NULL, "Digital Mic2"},
	/**
	 * Digital Mic3. Back Bottom Digital Mic on Fluid.
	 * Digital Mic GM1 on CDP mainboard.
	 * Conncted to DMIC4 Input on Sitar codec.
	 */
	{"DMIC3", NULL, "MIC BIAS1 External"},
	{"MIC BIAS1 External", NULL, "Digital Mic3"},

	/**
	 * Digital Mic4. Back top Digital Mic on Fluid.
	 * Digital Mic GM2 on CDP mainboard.
	 * Conncted to DMIC3 Input on Sitar codec.
	 */
	{"DMIC4", NULL, "MIC BIAS1 External"},
	{"MIC BIAS1 External", NULL, "Digital Mic4"},
#endif

};

static const char *spk_function[] = {"Off", "On"};
static const char *slim0_rx_ch_text[] = {"One", "Two"};
static const char *slim0_tx_ch_text[] = {"One", "Two", "Three", "Four"};

static const struct soc_enum msm8930_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, spk_function),
	SOC_ENUM_SINGLE_EXT(2, slim0_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(4, slim0_tx_ch_text),
};

static const char *btsco_rate_text[] = {"8000", "16000"};
static const struct soc_enum msm8930_btsco_enum[] = {
		SOC_ENUM_SINGLE_EXT(2, btsco_rate_text),
};
//                       
static const char *auxpcm_rate_text[] = {"rate_8000", "rate_16000"};
static const struct soc_enum msm8930_auxpcm_enum[] = {
    SOC_ENUM_SINGLE_EXT(2, auxpcm_rate_text),
};
//                       
#ifdef CONFIG_ANDROID_SW_IRRC
static const char *PMxxxx_SKP_Mute[] = {"Unmute","Mute"};
static const struct soc_enum PMxxxx_SPK_Mute_enum[] = {
    SOC_ENUM_SINGLE_EXT(2, PMxxxx_SKP_Mute),
};
#endif


//                                                                             
static const char* mic_fm_sw_text[] = {"mic_on", "fm_on"};
static const struct soc_enum msm8930_mic_fm_sw_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, mic_fm_sw_text),
};
//                                                                             



static int msm8930_slim_0_rx_ch_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm8930_slim_0_rx_ch  = %d\n", __func__,
		msm8930_slim_0_rx_ch);
	ucontrol->value.integer.value[0] = msm8930_slim_0_rx_ch - 1;
	return 0;
}

static int msm8930_slim_0_rx_ch_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	msm8930_slim_0_rx_ch = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: msm8930_slim_0_rx_ch = %d\n", __func__,
		msm8930_slim_0_rx_ch);
	return 1;
}

static int msm8930_slim_0_tx_ch_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm8930_slim_0_tx_ch  = %d\n", __func__,
		 msm8930_slim_0_tx_ch);
	ucontrol->value.integer.value[0] = msm8930_slim_0_tx_ch - 1;
	return 0;
}

static int msm8930_slim_0_tx_ch_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	msm8930_slim_0_tx_ch = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: msm8930_slim_0_tx_ch = %d\n", __func__,
		 msm8930_slim_0_tx_ch);
	return 1;
}

#ifdef CONFIG_FM_RADIO_MI2S_ENABLE
static int msm_slim_3_rx_ch_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_slim_3_rx_ch  = %d\n", __func__,
			msm_slim_3_rx_ch);
	ucontrol->value.integer.value[0] = msm_slim_3_rx_ch - 1;
	return 0;
}

static int msm_slim_3_rx_ch_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	msm_slim_3_rx_ch = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: msm_slim_3_rx_ch = %d\n", __func__,
			msm_slim_3_rx_ch);
	return 1;
}
#endif


static int msm8930_btsco_rate_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm8930_btsco_rate  = %d", __func__, msm8930_btsco_rate);
	ucontrol->value.integer.value[0] = msm8930_btsco_rate;
	return 0;
}

static int msm8930_btsco_rate_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{

	switch (ucontrol->value.integer.value[0]) {
	case 8000:
		msm8930_btsco_rate = BTSCO_RATE_8KHZ;
		break;
	case 16000:
		msm8930_btsco_rate = BTSCO_RATE_16KHZ;
		break;
	default:
		msm8930_btsco_rate = BTSCO_RATE_8KHZ;
		break;
	}
	pr_debug("%s: msm8930_btsco_rate = %d\n", __func__, msm8930_btsco_rate);
	return 0;
}

//                        
static int msm8930_auxpcm_rate_get(struct snd_kcontrol *kcontrol,
        struct snd_ctl_elem_value *ucontrol)
{
    pr_debug("%s: msm8930_auxpcm_rate  = %d", __func__,
            msm8930_auxpcm_rate);
    ucontrol->value.integer.value[0] = msm8930_auxpcm_rate;
    return 0;
}

static int msm8930_auxpcm_rate_put(struct snd_kcontrol *kcontrol,
        struct snd_ctl_elem_value *ucontrol)
{
    switch (ucontrol->value.integer.value[0]) {
        case 0:
            msm8930_auxpcm_rate = BTSCO_RATE_8KHZ;
            break;
        case 1:
            msm8930_auxpcm_rate = BTSCO_RATE_16KHZ;
            break;

        default:
            msm8930_auxpcm_rate = BTSCO_RATE_8KHZ;
            break;
    }
    pr_debug("%s: msm8930_auxpcm_rate = %d"
            "ucontrol->value.integer.value[0] = %d\n", __func__,
            msm8930_auxpcm_rate,
            (int)ucontrol->value.integer.value[0]);
    return 0;
}
//                       


//                                                                             
static int msm8930_mic_fm_sw_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm8930_mic_fm_sw_set = %d\n", __func__,
				msm8930_mic_fm_sw_set);
	//ucontrol->value.integer.value[0] = msm8930_mic_fm_sw_set;
	
	return 0;
}
//                                                                             



//                                                                             
static int msm8930_mic_fm_sw_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	switch(ucontrol->value.integer.value[0]) {
	case 0:
		gpio_direction_output(GPIO_MIC_FM_SW, MIC_ON);
		break;
	case 1:
		gpio_direction_output(GPIO_MIC_FM_SW, FM_ON);
		break;
		
	default:
		gpio_direction_output(GPIO_MIC_FM_SW, MIC_ON);
	}
	
	pr_debug("%s: ms8930_mic_fm_set = %d"
				"ucontrol->value.integer.value[0] = %d\n", __func__,
				msm8930_mic_fm_sw_set,
				(int)ucontrol->value.integer.value[0]);
				
	return 0;
}
//                                                                             




static const char *pmic_spk_gain_text[] = {
	"NEG_6_DB", "NEG_4_DB", "NEG_2_DB", "ZERO_DB", "POS_2_DB", "POS_4_DB",
	"POS_6_DB", "POS_8_DB", "POS_10_DB", "POS_12_DB", "POS_14_DB",
	"POS_16_DB", "POS_18_DB", "POS_20_DB", "POS_22_DB", "POS_24_DB"
};

static const struct soc_enum msm8960_pmic_spk_gain_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(pmic_spk_gain_text),
						pmic_spk_gain_text),
};

static int msm8930_pmic_gain_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm8930_pmic_spk_gain = %d\n", __func__,
			 msm8930_pmic_spk_gain);
	ucontrol->value.integer.value[0] = msm8930_pmic_spk_gain;
	return 0;
}

static int msm8930_pmic_gain_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	msm8930_pmic_spk_gain = ucontrol->value.integer.value[0];
/*            */
// MYUNGWON.KIM Block Duplicated Code 2012-11-22
//	if (socinfo_get_pmic_model() != PMIC_MODEL_PM8917)
//		ret = pm8xxx_spk_gain(msm8930_pmic_spk_gain);
    ret = pm8xxx_spk_gain(msm8930_pmic_spk_gain);
	pr_debug("%s: msm8930_pmic_spk_gain = %d"
			 " ucontrol->value.integer.value[0] = %d\n", __func__,
			 msm8930_pmic_spk_gain,
			 (int) ucontrol->value.integer.value[0]);
	return ret;
}

#ifdef CONFIG_ANDROID_SW_IRRC
void pm8xxx_spk_unmute_timer_function(unsigned long data)
{
	printk("pm8xxx_spk_unmute_timer_function data %d\n",(int)data);
//	pm8xxx_spk_mute(data);

	if((int)data==MSM8930_SPK_ON) //unmute case	
		{
			printk("pm8xxx_spk_unmute_timer_function data %d Unmute\n",(int)data);
			PM8xxx_EXT_SPK_Force_Mute=0;
			pm8xxx_spk_enable((int)data);
//			pm8xxx_spk_mute(1);

		}
	else{
			printk("pm8xxx_spk_unmute_timer_function data %d mute Case\n",(int)data);
			PM8xxx_EXT_SPK_Force_Mute=1;
			pm8xxx_spk_enable((int)data);
//			pm8xxx_spk_mute(0);
		}
}

static int msm8930_pmic_mute_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	int msm8930_pmic_mute_enable =0;
	int temp_pp = (int)ucontrol->value.integer.value[0];
	del_timer(&EXT_AMP_ON_Timer);
	init_timer(&EXT_AMP_ON_Timer);

	pm8xxx_spk_mute_data=MSM8930_SPK_ON;

	PM8xxx_EXT_SPK_Force_Mute=1;
	EXT_AMP_ON_Timer.expires = get_jiffies_64()+ (9*HZ/10);  //(700ms)
	EXT_AMP_ON_Timer.data= pm8xxx_spk_mute_data;
	EXT_AMP_ON_Timer.function = pm8xxx_spk_unmute_timer_function;
	add_timer(&EXT_AMP_ON_Timer);
	
	printk("msm8930_pmic_mute_put msm8930_pmic_mute_enable %d \n",temp_pp);

	if(ucontrol->value.integer.value[0]==1){
		msm8930_pmic_mute_enable=MSM8930_SPK_OFF;
		printk("msm8930_pmic_mute_put msm8930_pmic_mute_enable MUTE\n");
		}
	else{
		msm8930_pmic_mute_enable=MSM8930_SPK_ON;
		printk("msm8930_pmic_mute_put msm8930_pmic_mute_enable UNMUTE\n");
		}
	
	pm8xxx_spk_enable(msm8930_pmic_mute_enable);
//	pm8xxx_spk_mute(0);

	return ret;
}

static int msm8930_pmic_mute_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	int msm8930_pmic_mute_enable = ucontrol->value.integer.value[0];

	pr_debug("%s: msm8930_pmic_mute_enable = %d"
			 " ucontrol->value.integer.value[0] = %d\n", __func__,
			 msm8930_pmic_mute_enable,
			 (int) ucontrol->value.integer.value[0]);
	return ret;
}
#endif

static const struct snd_kcontrol_new sitar_msm8930_controls[] = {
	SOC_ENUM_EXT("Speaker Function", msm8930_enum[0], msm8930_get_spk,
		msm8930_set_spk),
	SOC_ENUM_EXT("SLIM_0_RX Channels", msm8930_enum[1],
		msm8930_slim_0_rx_ch_get, msm8930_slim_0_rx_ch_put),
	SOC_ENUM_EXT("SLIM_0_TX Channels", msm8930_enum[2],
		msm8930_slim_0_tx_ch_get, msm8930_slim_0_tx_ch_put),
	SOC_ENUM_EXT("PMIC SPK Gain", msm8960_pmic_spk_gain_enum[0],
		msm8930_pmic_gain_get, msm8930_pmic_gain_put),
	SOC_ENUM_EXT("Internal BTSCO SampleRate", msm8930_btsco_enum[0],
		msm8930_btsco_rate_get, msm8930_btsco_rate_put),
    /*            */
    SOC_ENUM_EXT("AUX PCM SampleRate", msm8930_auxpcm_enum[0],
            msm8930_auxpcm_rate_get, msm8930_auxpcm_rate_put),
#ifdef CONFIG_ANDROID_SW_IRRC
    SOC_ENUM_EXT("SPK Force Mute", PMxxxx_SPK_Mute_enum[0],
	        msm8930_pmic_mute_get, msm8930_pmic_mute_put),
#endif
	//                                                                             
	SOC_ENUM_EXT("Mic FM Switch", msm8930_mic_fm_sw_enum[0],
		msm8930_mic_fm_sw_get, msm8930_mic_fm_sw_put),
	//                                                                             
};

#ifdef CONFIG_FM_RADIO_MI2S_ENABLE
static const struct snd_kcontrol_new slim_3_mixer_controls[] = {
	SOC_ENUM_EXT("SLIM_3_RX Channels", msm8930_enum[1],  
		msm_slim_3_rx_ch_get, msm_slim_3_rx_ch_put),
};

static int msm_slim_3_init(struct snd_soc_pcm_runtime *rtd)
{
	int err = 0;
	struct snd_soc_platform *platform = rtd->platform;

	err = snd_soc_add_platform_controls(platform,
			slim_3_mixer_controls,
		ARRAY_SIZE(slim_3_mixer_controls));
	if (err < 0)
		return err;
	return 0;
}
#endif

static void *def_sitar_mbhc_cal(void)
{
	void *sitar_cal;
	struct sitar_mbhc_btn_detect_cfg *btn_cfg;
	u16 *btn_low, *btn_high;
	u8 *n_ready, *n_cic, *gain;

	sitar_cal = kzalloc(SITAR_MBHC_CAL_SIZE(SITAR_MBHC_DEF_BUTTONS,
				SITAR_MBHC_DEF_RLOADS),
				GFP_KERNEL);
	if (!sitar_cal) {
		pr_err("%s: out of memory\n", __func__);
		return NULL;
	}

#define S(X, Y) ((SITAR_MBHC_CAL_GENERAL_PTR(sitar_cal)->X) = (Y))
	S(t_ldoh, 100);
	S(t_bg_fast_settle, 100);
	S(t_shutdown_plug_rem, 255);
	S(mbhc_nsa, 4);
	S(mbhc_navg, 4);
#undef S
#define S(X, Y) ((SITAR_MBHC_CAL_PLUG_DET_PTR(sitar_cal)->X) = (Y))
	S(mic_current, SITAR_PID_MIC_5_UA);
	S(hph_current, SITAR_PID_MIC_5_UA);
	S(t_mic_pid, 100);
	S(t_ins_complete, 250);
	S(t_ins_retry, 200);
#undef S
#define S(X, Y) ((SITAR_MBHC_CAL_PLUG_TYPE_PTR(sitar_cal)->X) = (Y))
	S(v_no_mic, 30);
	S(v_hs_max, 1650);
#undef S
#define S(X, Y) ((SITAR_MBHC_CAL_BTN_DET_PTR(sitar_cal)->X) = (Y))
	S(c[0], 62);
	S(c[1], 124);
	S(nc, 1);
	S(n_meas, 3);
	S(mbhc_nsc, 11);
	S(n_btn_meas, 1);
	S(n_btn_con, 2);
	S(num_btn, SITAR_MBHC_DEF_BUTTONS);
	S(v_btn_press_delta_sta, 100);
	S(v_btn_press_delta_cic, 50);
#undef S
	btn_cfg = SITAR_MBHC_CAL_BTN_DET_PTR(sitar_cal);
	btn_low = sitar_mbhc_cal_btn_det_mp(btn_cfg, SITAR_BTN_DET_V_BTN_LOW);
	btn_high = sitar_mbhc_cal_btn_det_mp(btn_cfg, SITAR_BTN_DET_V_BTN_HIGH);
	btn_low[0] = -50;
	btn_high[0] = 10;
	btn_low[1] = 11;
	btn_high[1] = 38;
	btn_low[2] = 39;
	btn_high[2] = 64;
	btn_low[3] = 65;
	btn_high[3] = 91;
	btn_low[4] = 92;
	btn_high[4] = 115;
	btn_low[5] = 116;
	btn_high[5] = 141;
	btn_low[6] = 142;
	btn_high[6] = 163;
	btn_low[7] = 164;
	btn_high[7] = 250;
	n_ready = sitar_mbhc_cal_btn_det_mp(btn_cfg, SITAR_BTN_DET_N_READY);
	n_ready[0] = 48;
	n_ready[1] = 38;
	n_cic = sitar_mbhc_cal_btn_det_mp(btn_cfg, SITAR_BTN_DET_N_CIC);
	n_cic[0] = 60;
	n_cic[1] = 47;
	gain = sitar_mbhc_cal_btn_det_mp(btn_cfg, SITAR_BTN_DET_GAIN);
	gain[0] = 11;
	gain[1] = 9;

	return sitar_cal;
}

static int msm8930_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;
	unsigned int rx_ch[SLIM_MAX_RX_PORTS], tx_ch[SLIM_MAX_TX_PORTS];
	unsigned int rx_ch_cnt = 0, tx_ch_cnt = 0;

	pr_debug("%s: ch=%d\n", __func__,
					msm8930_slim_0_rx_ch);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ret = snd_soc_dai_get_channel_map(codec_dai,
				&tx_ch_cnt, tx_ch, &rx_ch_cnt , rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to get codec chan map\n", __func__);
			goto end;
		}

		ret = snd_soc_dai_set_channel_map(cpu_dai, 0, 0,
				msm8930_slim_0_rx_ch, rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to set cpu chan map\n", __func__);
			goto end;
		}
		ret = snd_soc_dai_set_channel_map(codec_dai, 0, 0,
				msm8930_slim_0_rx_ch, rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to set codec channel map\n",
							       __func__);
			goto end;
		}
	} else {
		ret = snd_soc_dai_get_channel_map(codec_dai,
				&tx_ch_cnt, tx_ch, &rx_ch_cnt , rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to get codec chan map\n", __func__);
			goto end;
		}
		ret = snd_soc_dai_set_channel_map(cpu_dai,
				msm8930_slim_0_tx_ch, tx_ch, 0 , 0);
		if (ret < 0) {
			pr_err("%s: failed to set cpu chan map\n", __func__);
			goto end;
		}
		ret = snd_soc_dai_set_channel_map(codec_dai,
				msm8930_slim_0_tx_ch, tx_ch, 0, 0);
		if (ret < 0) {
			pr_err("%s: failed to set codec channel map\n",
							       __func__);
			goto end;
		}

	}
end:
	return ret;
}

#ifdef CONFIG_FM_RADIO_MI2S_ENABLE
static int msm_slimbus_1_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;
	unsigned int rx_ch = SLIM_1_RX_1, tx_ch = SLIM_1_TX_1;

	pr_debug("%s: rx_ch=%d,tx_ch=%d\n",__func__, rx_ch,tx_ch);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pr_debug("%s: APQ BT/USB TX -> SLIMBUS_1_RX -> MDM TX shared ch %d\n",
			__func__, rx_ch);

		ret = snd_soc_dai_set_channel_map(cpu_dai, 0, 0, 1, &rx_ch);
		if (ret < 0) {
			pr_err("%s: Erorr %d setting SLIM_1 RX channel map\n",
				__func__, ret);

			goto end;
		}
	} else {
		pr_debug("%s: MDM RX -> SLIMBUS_1_TX -> APQ BT/USB/MI2S Rx shared ch %d\n",
			__func__, tx_ch);

		ret = snd_soc_dai_set_channel_map(cpu_dai, 1, &tx_ch, 0, 0);
		if (ret < 0) {
			pr_err("%s: Erorr %d setting SLIM_1 TX channel map\n",
				__func__, ret);

			goto end;
		}
	}

end:
	return ret;
}

static int msm_slimbus_3_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;
	unsigned int rx_ch[2] = {SLIM_3_RX_1, SLIM_3_RX_2};

	pr_debug("%s: slim_3_rx_ch %d, sch %d %d\n",__func__, msm_slim_3_rx_ch,
			 rx_ch[0], rx_ch[1]);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pr_debug("%s: slim_3_rx_ch %d, sch %d %d\n",
			 __func__, msm_slim_3_rx_ch,
				 rx_ch[0], rx_ch[1]);

		ret = snd_soc_dai_set_channel_map(cpu_dai, 0, 0,
				msm_slim_3_rx_ch, rx_ch);
		if (ret < 0) {
			pr_err("%s: Erorr %d setting SLIM_3 RX channel map\n",
				__func__, ret);

			goto end;
		}
	} else {
		pr_err("%s: SLIMBUS_3_TX not defined for this DAI\n", __func__);
	}

end:
	return ret;
}
#endif

static int msm8930_audrx_init(struct snd_soc_pcm_runtime *rtd)
{
	int err;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	
	pr_debug("%s()\n", __func__);

	snd_soc_dapm_new_controls(dapm, msm8930_dapm_widgets,
				ARRAY_SIZE(msm8930_dapm_widgets));

	snd_soc_dapm_add_routes(dapm, common_audio_map,
		ARRAY_SIZE(common_audio_map));

#ifdef CONFIG_LGE_AUDIO_TPA2028D
    snd_soc_dapm_enable_pin(dapm, "Ext Spk");
#else
	snd_soc_dapm_enable_pin(dapm, "Ext Spk Left Pos");
	snd_soc_dapm_enable_pin(dapm, "Ext Spk Left Neg");
#endif

	snd_soc_dapm_sync(dapm);

	err = snd_soc_jack_new(codec, "Headset Jack",
		(SND_JACK_HEADSET | SND_JACK_OC_HPHL | SND_JACK_OC_HPHR),
		&hs_jack);
	if (err) {
		pr_err("failed to create new jack\n");
		return err;
	}

	err = snd_soc_jack_new(codec, "Button Jack",
				SITAR_JACK_BUTTON_MASK, &button_jack);
	if (err) {
		pr_err("failed to create new jack\n");
		return err;
	}
	codec_clk = clk_get(cpu_dai->dev, "osr_clk");
#ifdef CONFIG_SWITCH_FSA8008
	if (lge_get_board_usembhc()) {
		mbhc_cfg.gpio = 37;
		mbhc_cfg.gpio_irq = gpio_to_irq(mbhc_cfg.gpio);
		sitar_hs_detect(codec, &mbhc_cfg);
	} else {
		sitar_register_mclk_call_back(codec, msm8930_enable_codec_ext_clk);
	}
#else 
	mbhc_cfg.gpio = 37;
	mbhc_cfg.gpio_irq = gpio_to_irq(mbhc_cfg.gpio);
	sitar_hs_detect(codec, &mbhc_cfg);
#endif

	if (socinfo_get_pmic_model() != PMIC_MODEL_PM8917) {
		/* Initialize default PMIC speaker gain */
		pm8xxx_spk_gain(DEFAULT_PMIC_SPK_GAIN);
	}

	return 0;
}

/*            */
#if 0
static struct snd_soc_dsp_link lpa_fe_media = {
	.playback = true,
	.trigger = {
		SND_SOC_DSP_TRIGGER_POST,
		SND_SOC_DSP_TRIGGER_POST
	},
};

static struct snd_soc_dsp_link fe_media = {
	.playback = true,
	.capture = true,
	.trigger = {
		SND_SOC_DSP_TRIGGER_POST,
		SND_SOC_DSP_TRIGGER_POST
	},
};

static struct snd_soc_dsp_link slimbus0_hl_media = {
	.playback = true,
	.capture = true,
	.trigger = {
		SND_SOC_DSP_TRIGGER_POST,
		SND_SOC_DSP_TRIGGER_POST
	},
};
#endif

/*            */
#if 0
static struct snd_soc_dsp_link int_fm_hl_media = {
	.playback = true,
	.capture = true,
	.trigger = {
		SND_SOC_DSP_TRIGGER_POST,
		SND_SOC_DSP_TRIGGER_POST
	},
};

/* bi-directional media definition for hostless PCM device */
static struct snd_soc_dsp_link bidir_hl_media = {
	.playback = true,
	.capture = true,
	.trigger = {
		SND_SOC_DSP_TRIGGER_POST,
		SND_SOC_DSP_TRIGGER_POST
	},
};
#endif

/*            */
#if 0
static struct snd_soc_dsp_link hdmi_rx_hl = {
	.playback = true,
	.trigger = {
		SND_SOC_DSP_TRIGGER_POST,
		SND_SOC_DSP_TRIGGER_POST
	},
};
#endif

static int msm8930_slim_0_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
	SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s()\n", __func__);
	rate->min = rate->max = 48000;
	channels->min = channels->max = msm8930_slim_0_rx_ch;

	return 0;
}

static int msm8930_slim_0_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
	SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s()\n", __func__);
	rate->min = rate->max = 48000;
	channels->min = channels->max = msm8930_slim_0_tx_ch;

	return 0;
}

#ifdef CONFIG_FM_RADIO_MI2S_ENABLE
static int msm_slim_3_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
	SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s()\n", __func__);
	rate->min = rate->max = 48000;
	channels->min = channels->max = msm_slim_3_rx_ch;

	return 0;
}
#endif

static int msm8930_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
	SNDRV_PCM_HW_PARAM_RATE);

	pr_debug("%s()\n", __func__);
	rate->min = rate->max = 48000;

	return 0;
}
/*            */
#if 0
static int msm8930_hdmi_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = 48000;
	channels->min = channels->max = 2;

	return 0;
}
#endif
static int msm8930_btsco_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s()msm8930_btsco_rate=%d \n", __func__,msm8930_btsco_rate);
	rate->min = rate->max = msm8930_btsco_rate;
	channels->min = channels->max = msm8930_btsco_ch;

	return 0;
}

static int msm8930_auxpcm_be_params_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					 SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	/* PCM only supports mono output with 8khz sample rate */
/*            */
	pr_debug("%s()msm8930_auxpcm_rate=%d \n", __func__,msm8930_auxpcm_rate);
	rate->min = rate->max = msm8930_auxpcm_rate;
//                       
	channels->min = channels->max = 1;

	return 0;
}

static int msm8930_proxy_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
	SNDRV_PCM_HW_PARAM_RATE);

	pr_debug("%s()\n", __func__);
	rate->min = rate->max = 48000;

	return 0;
}

static int msm8930_aux_pcm_get_gpios(void)
{
	int ret = 0;

	pr_debug("%s\n", __func__);

	ret = gpio_request(GPIO_AUX_PCM_DOUT, "AUX PCM DOUT");
	if (ret < 0) {
		pr_err("%s: Failed to request gpio(%d): AUX PCM DOUT",
				__func__, GPIO_AUX_PCM_DOUT);

		goto fail_dout;
	}

	ret = gpio_request(GPIO_AUX_PCM_DIN, "AUX PCM DIN");
	if (ret < 0) {
		pr_err("%s: Failed to request gpio(%d): AUX PCM DIN",
				 __func__, GPIO_AUX_PCM_DIN);
		goto fail_din;
	}

	ret = gpio_request(GPIO_AUX_PCM_SYNC, "AUX PCM SYNC");
	if (ret < 0) {
		pr_err("%s: Failed to request gpio(%d): AUX PCM SYNC",
				__func__, GPIO_AUX_PCM_SYNC);
		goto fail_sync;
	}

	ret = gpio_request(GPIO_AUX_PCM_CLK, "AUX PCM CLK");
	if (ret < 0) {
		pr_err("%s: Failed to request gpio(%d): AUX PCM CLK",
				 __func__, GPIO_AUX_PCM_CLK);
		goto fail_clk;
	}

	return 0;

fail_clk:
	gpio_free(GPIO_AUX_PCM_SYNC);
fail_sync:
	gpio_free(GPIO_AUX_PCM_DIN);
fail_din:
	gpio_free(GPIO_AUX_PCM_DOUT);
fail_dout:

	return ret;
}

static int msm8930_aux_pcm_free_gpios(void)
{
	gpio_free(GPIO_AUX_PCM_DIN);
	gpio_free(GPIO_AUX_PCM_DOUT);
	gpio_free(GPIO_AUX_PCM_SYNC);
	gpio_free(GPIO_AUX_PCM_CLK);

	return 0;
}


#ifdef CONFIG_FM_RADIO_MI2S_ENABLE
static int msm8960_mi2s_free_gpios(void)
{
	int	i;
	for (i = 0; i < ARRAY_SIZE(mi2s_gpio); i++)
		gpio_free(mi2s_gpio[i].gpio_no);
	return 0;
}

static int msm8960_mi2s_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params)
{
	int rate = params_rate(params);
	int bit_clk_set = 0;

	bit_clk_set = 12288000/(rate * 2 * 16);

	pr_debug("%s[chapman]: rate = %d, bit_clk_set = %d\n", __func__,rate,bit_clk_set);

	clk_set_rate(mi2s_bit_clk, bit_clk_set);
	return 1;
}


static void msm8960_mi2s_shutdown(struct snd_pcm_substream *substream)
{

	pr_info("%s: free mi2s resources\n", __func__);
	if (atomic_dec_return(&mi2s_rsc_ref) == 0) {
		pr_info("%s: free mi2s resources\n", __func__);
		if (mi2s_bit_clk) {
			clk_disable_unprepare(mi2s_bit_clk);
			clk_put(mi2s_bit_clk);
			mi2s_bit_clk = NULL;
		}
       
		if (mi2s_osr_clk) {
			clk_disable_unprepare(mi2s_osr_clk);
			clk_put(mi2s_osr_clk);
			mi2s_osr_clk = NULL;
		}
	
		msm8960_mi2s_free_gpios();
	}
}

static int configure_mi2s_gpio(void)
{
	int	rtn;
	int	i;
	int	j;
	for (i = 0; i < ARRAY_SIZE(mi2s_gpio); i++) {
		rtn = gpio_request(mi2s_gpio[i].gpio_no,
						   mi2s_gpio[i].gpio_name);
		pr_debug("%s: gpio = %d, gpio name = %s, rtn = %d\n",
				 __func__,
				 mi2s_gpio[i].gpio_no,
				 mi2s_gpio[i].gpio_name,
				 rtn);
		if (rtn) {
			pr_err("%s: Failed to request gpio %d\n",
				   __func__,
				   mi2s_gpio[i].gpio_no);
			for (j = i; j >= 0; j--)
				gpio_free(mi2s_gpio[j].gpio_no);
			goto err;
		}
	}
err:
	return rtn;
}

static int msm8960_mi2s_startup(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;

	pr_debug("%s: dai name %s %p\n", __func__, cpu_dai->name, cpu_dai->dev);

	if (atomic_inc_return(&mi2s_rsc_ref) == 1) {
		pr_info("%s: acquire mi2s resources\n", __func__);
		configure_mi2s_gpio();
		mi2s_osr_clk = clk_get(cpu_dai->dev, "osr_clk");
		if (!IS_ERR(mi2s_osr_clk)) {
			clk_set_rate(mi2s_osr_clk, 12288000);
			clk_prepare_enable(mi2s_osr_clk);
		} else 
			pr_err("Failed to get mi2s_osr_clk\n");
		mi2s_bit_clk = clk_get(cpu_dai->dev, "bit_clk");
		if (IS_ERR(mi2s_bit_clk)) {
			pr_err("Failed to get mi2s_bit_clk\n");
			clk_disable_unprepare(mi2s_osr_clk);
			clk_put(mi2s_osr_clk);
			return PTR_ERR(mi2s_bit_clk);
		}
		clk_set_rate(mi2s_bit_clk, 8);
		ret = clk_prepare_enable(mi2s_bit_clk);
		if (ret != 0) {
			pr_err("Unable to enable mi2s_rx_bit_clk\n");
			clk_put(mi2s_bit_clk);
			clk_disable_unprepare(mi2s_osr_clk);
			clk_put(mi2s_osr_clk);
			return ret;
		}
		ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);
		if (ret < 0)
			ret = snd_soc_dai_set_fmt(codec_dai,
						  SND_SOC_DAIFMT_CBS_CFS);
		if (ret < 0)
			pr_err("set format for codec dai failed\n");
	}

	return ret;
}
#endif


static int msm8930_startup(struct snd_pcm_substream *substream)
{
	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);
	return 0;
}

static int msm8930_auxpcm_startup(struct snd_pcm_substream *substream)
{
	int ret = 0;

	pr_debug("%s(): substream = %s, auxpcm_rsc_ref counter = %d\n",
		__func__, substream->name, atomic_read(&auxpcm_rsc_ref));
	if (atomic_inc_return(&auxpcm_rsc_ref) == 1)
		ret = msm8930_aux_pcm_get_gpios();
	if (ret < 0) {
		pr_err("%s: Aux PCM GPIO request failed\n", __func__);
		return -EINVAL;
	}
	return 0;

}

static void msm8930_auxpcm_shutdown(struct snd_pcm_substream *substream)
{
	pr_debug("%s(): substream = %s, auxpcm_rsc_ref counter = %d\n",
		__func__, substream->name, atomic_read(&auxpcm_rsc_ref));
	if (atomic_dec_return(&auxpcm_rsc_ref) == 0)
		msm8930_aux_pcm_free_gpios();
}

static void msm8930_shutdown(struct snd_pcm_substream *substream)
{
	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);
}

static struct snd_soc_ops msm8930_be_ops = {
	.startup = msm8930_startup,
	.hw_params = msm8930_hw_params,
	.shutdown = msm8930_shutdown,
};

static struct snd_soc_ops msm8930_auxpcm_be_ops = {
	.startup = msm8930_auxpcm_startup,
	.shutdown = msm8930_auxpcm_shutdown,
};

#ifdef CONFIG_FM_RADIO_MI2S_ENABLE
static struct snd_soc_ops msm8960_mi2s_be_ops = {
	.startup = msm8960_mi2s_startup,
	.shutdown = msm8960_mi2s_shutdown,
	.hw_params = msm8960_mi2s_hw_params,
};

static struct snd_soc_ops msm_slimbus_1_be_ops = {
	.hw_params = msm_slimbus_1_hw_params,
};

static struct snd_soc_ops msm_slimbus_3_be_ops = {
	.hw_params = msm_slimbus_3_hw_params,
};
#endif

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link msm8930_dai[] = {
	/* FrontEnd DAI Links */
	{
		.name = "MSM8930 Media1",
		.stream_name = "MultiMedia1",
		.cpu_dai_name	= "MultiMedia1",
		.platform_name  = "msm-pcm-dsp",
		.dynamic = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA1
	},
	{
		.name = "MSM8930 Media2",
		.stream_name = "MultiMedia2",
		.cpu_dai_name	= "MultiMedia2",
		.platform_name  = "msm-multi-ch-pcm-dsp",
		.dynamic = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA2,
	},
	{
		.name = "Circuit-Switch Voice",
		.stream_name = "CS-Voice",
		.cpu_dai_name   = "CS-VOICE",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.be_id = MSM_FRONTEND_DAI_CS_VOICE,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
	},
	{
		.name = "MSM VoIP",
		.stream_name = "VoIP",
		.cpu_dai_name	= "VoIP",
		.platform_name  = "msm-voip-dsp",
		.dynamic = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_VOIP,
	},
	{
		.name = "MSM8930 LPA",
		.stream_name = "LPA",
		.cpu_dai_name	= "MultiMedia3",
		.platform_name  = "msm-pcm-lpa",
		.dynamic = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA3,
	},
	/* Hostless PMC purpose */
	{
		.name = "SLIMBUS_0 Hostless",
		.stream_name = "SLIMBUS_0 Hostless",
		.cpu_dai_name	= "SLIMBUS0_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
		/* .be_id = do not care */
	},
	{
		.name = "INT_FM Hostless",
		.stream_name = "INT_FM Hostless",
		.cpu_dai_name	= "INT_FM_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
		/* .be_id = do not care */
	},
	{
		.name = "MSM AFE-PCM RX",
		.stream_name = "AFE-PROXY RX",
		.cpu_dai_name = "msm-dai-q6.241",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.platform_name  = "msm-pcm-afe",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
	},
	{
		.name = "MSM AFE-PCM TX",
		.stream_name = "AFE-PROXY TX",
		.cpu_dai_name = "msm-dai-q6.240",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.platform_name  = "msm-pcm-afe",
		.ignore_suspend = 1,
	},
	{
		.name = "MSM8930 Compr",
		.stream_name = "COMPR",
		.cpu_dai_name	= "MultiMedia4",
		.platform_name  = "msm-compr-dsp",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA4,
	},
	 {
		.name = "AUXPCM Hostless",
		.stream_name = "AUXPCM Hostless",
		.cpu_dai_name   = "AUXPCM_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
/*            */
#if 0
	/* HDMI Hostless */
	{
		.name = "HDMI_RX_HOSTLESS",
		.stream_name = "HDMI_RX_HOSTLESS",
		.cpu_dai_name = "HDMI_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
#endif
	{
		.name = "VoLTE",
		.stream_name = "VoLTE",
		.cpu_dai_name   = "VoLTE",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_VOLTE,
	},
	{
		.name = "SGLTE",
		.stream_name = "SGLTE",
		.cpu_dai_name   = "SGLTE",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_SGLTE,
	},
	{
		.name = "MSM8960 LowLatency",
		.stream_name = "MultiMedia5",
		.cpu_dai_name	= "MultiMedia5",
		.platform_name  = "msm-lowlatency-pcm-dsp",
		.dynamic = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA5,
	},
	/* Backend DAI Links */
	{
		.name = LPASS_BE_SLIMBUS_0_RX,
		.stream_name = "Slimbus Playback",
		.cpu_dai_name = "msm-dai-q6.16384",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "sitar_codec",
		.codec_dai_name	= "sitar_rx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_0_RX,
		.init = &msm8930_audrx_init,
		.be_hw_params_fixup = msm8930_slim_0_rx_be_hw_params_fixup,
		.ops = &msm8930_be_ops,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
	},
	{
		.name = LPASS_BE_SLIMBUS_0_TX,
		.stream_name = "Slimbus Capture",
		.cpu_dai_name = "msm-dai-q6.16385",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "sitar_codec",
		.codec_dai_name	= "sitar_tx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_0_TX,
		.be_hw_params_fixup = msm8930_slim_0_tx_be_hw_params_fixup,
		.ops = &msm8930_be_ops,
	},
#ifdef CONFIG_FM_RADIO_MI2S_ENABLE
	{
		.name = "Voice Stub",
		.stream_name = "Voice Stub",
		.cpu_dai_name = "VOICE_STUB",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1, /* Playback support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_VOICE_STUB,
	},
#endif	
	/* Backend BT/FM DAI Links */
	{
		.name = LPASS_BE_INT_BT_SCO_RX,
		.stream_name = "Internal BT-SCO Playback",
		.cpu_dai_name = "msm-dai-q6.12288",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name	= "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_INT_BT_SCO_RX,
		.be_hw_params_fixup = msm8930_btsco_be_hw_params_fixup,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
	},
	{
		.name = LPASS_BE_INT_BT_SCO_TX,
		.stream_name = "Internal BT-SCO Capture",
		.cpu_dai_name = "msm-dai-q6.12289",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name	= "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_INT_BT_SCO_TX,
		.be_hw_params_fixup = msm8930_btsco_be_hw_params_fixup,
	},
	{
		.name = LPASS_BE_INT_FM_RX,
		.stream_name = "Internal FM Playback",
		.cpu_dai_name = "msm-dai-q6.12292",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_INT_FM_RX,
		.be_hw_params_fixup = msm8930_be_hw_params_fixup,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
	},
	{
		.name = LPASS_BE_INT_FM_TX,
		.stream_name = "Internal FM Capture",
		.cpu_dai_name = "msm-dai-q6.12293",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_INT_FM_TX,
		.be_hw_params_fixup = msm8930_be_hw_params_fixup,
	},
/*            */
#if 0
	/* HDMI BACK END DAI Link */
	{
		.name = LPASS_BE_HDMI,
		.stream_name = "HDMI Playback",
		.cpu_dai_name = "msm-dai-q6-hdmi.8",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_HDMI_RX,
		.be_hw_params_fixup = msm8930_hdmi_be_hw_params_fixup,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
	},
#endif

#ifdef CONFIG_FM_RADIO_MI2S_ENABLE
	{
		.name = LPASS_BE_MI2S_RX,
		.stream_name = "MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s",
		.platform_name = "msm-pcm-routing",
		.codec_name 	= "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_MI2S_RX,
		.be_hw_params_fixup = msm8930_be_hw_params_fixup,
		.ops = &msm8960_mi2s_be_ops,
		.ignore_pmdown_time = 1, /* Playback support */
	},
	{
		.name = "MI2S_TX Hostless",
		.stream_name = "MI2S_TX Hostless",
		.cpu_dai_name	= "MI2S_TX_HOSTLESS",
		.platform_name	= "msm-pcm-hostless",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,  /* this dainlink has playback support */
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = LPASS_BE_MI2S_TX,
		.stream_name = "MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s",
		.platform_name = "msm-pcm-routing",
		.codec_name 	= "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_MI2S_TX,
		.ops = &msm8960_mi2s_be_ops,
		},
#endif
	/* Backend AFE DAI Links */
	{
		.name = LPASS_BE_AFE_PCM_RX,
		.stream_name = "AFE Playback",
		.cpu_dai_name = "msm-dai-q6.224",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_AFE_PCM_RX,
		.be_hw_params_fixup = msm8930_proxy_be_hw_params_fixup,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
	},
	{
		.name = LPASS_BE_AFE_PCM_TX,
		.stream_name = "AFE Capture",
		.cpu_dai_name = "msm-dai-q6.225",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_AFE_PCM_TX,
		.be_hw_params_fixup = msm8930_proxy_be_hw_params_fixup,
	},
	/* AUX PCM Backend DAI Links */
	{
		.name = LPASS_BE_AUXPCM_RX,
		.stream_name = "AUX PCM Playback",
		.cpu_dai_name = "msm-dai-q6.2",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_AUXPCM_RX,
		.be_hw_params_fixup = msm8930_auxpcm_be_params_fixup,
		.ops = &msm8930_auxpcm_be_ops,
	},
	{
		.name = LPASS_BE_AUXPCM_TX,
		.stream_name = "AUX PCM Capture",
		.cpu_dai_name = "msm-dai-q6.3",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_AUXPCM_TX,
		.be_hw_params_fixup = msm8930_auxpcm_be_params_fixup,
		.ops = &msm8930_auxpcm_be_ops,
	},
	/* Incall Music BACK END DAI Link */
	{
		.name = LPASS_BE_VOICE_PLAYBACK_TX,
		.stream_name = "Voice Farend Playback",
		.cpu_dai_name = "msm-dai-q6.32773",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_VOICE_PLAYBACK_TX,
		.be_hw_params_fixup = msm8930_be_hw_params_fixup,
	},
	/* Incall Record Uplink BACK END DAI Link */
	{
		.name = LPASS_BE_INCALL_RECORD_TX,
		.stream_name = "Voice Uplink Capture",
		.cpu_dai_name = "msm-dai-q6.32772",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_INCALL_RECORD_TX,
		.be_hw_params_fixup = msm8930_be_hw_params_fixup,
	},
	/* Incall Record Downlink BACK END DAI Link */
	{
		.name = LPASS_BE_INCALL_RECORD_RX,
		.stream_name = "Voice Downlink Capture",
		.cpu_dai_name = "msm-dai-q6.32771",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_INCALL_RECORD_RX,
		.be_hw_params_fixup = msm8930_be_hw_params_fixup,
		.ignore_pmdown_time = 1, /* this dainlink has playback support */
	},
#ifdef CONFIG_FM_RADIO_MI2S_ENABLE
	{
		.name = LPASS_BE_SLIMBUS_1_TX,
		.stream_name = "Slimbus1 Capture",
		.cpu_dai_name = "msm-dai-q6.16387",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_1_TX,
		.be_hw_params_fixup =  msm8930_btsco_be_hw_params_fixup,
		.ops = &msm_slimbus_1_be_ops,
	},
	{
		.name = LPASS_BE_SLIMBUS_3_RX,
		.stream_name = "Slimbus3 Playback",
		.cpu_dai_name = "msm-dai-q6.16390",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.init = &msm_slim_3_init,
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_3_RX,
		.be_hw_params_fixup = msm_slim_3_rx_be_hw_params_fixup,
		.ops = &msm_slimbus_3_be_ops,
	},
#endif

};

struct snd_soc_card snd_soc_card_msm8930 = {
	.name		= "msm8930-sitar-snd-card",
	.dai_link	= msm8930_dai,
	.num_links	= ARRAY_SIZE(msm8930_dai),
	.controls	= sitar_msm8930_controls,
	.num_controls	= ARRAY_SIZE(sitar_msm8930_controls),
};

static struct platform_device *msm8930_snd_device;

static int msm8930_configure_headset_mic_gpios(void)
{
	int ret;
	ret = gpio_request(80, "US_EURO_SWITCH");
	if (ret) {
		pr_err("%s: Failed to request gpio 80\n", __func__);
		return ret;
	}
	ret = gpio_direction_output(80, 0);
	if (ret) {
		pr_err("%s: Unable to set direction\n", __func__);
		gpio_free(80);
	}
	msm8930_headset_gpios_configured = 0;
	return 0;
}
static void msm8930_free_headset_mic_gpios(void)
{
	if (msm8930_headset_gpios_configured){
#ifdef CONFIG_SWITCH_FSA8008
		if(lge_get_board_usembhc()) {
			gpio_free(80);
		}
#else
		gpio_free(80);
#endif
	}
}

static int __init msm8930_audio_init(void)
{
	int ret;

	if (!cpu_is_msm8930() && !cpu_is_msm8930aa() && !cpu_is_msm8627()) {
		pr_err("%s: Not the right machine type\n", __func__);
		return -ENODEV ;
	}
	mbhc_cfg.calibration = def_sitar_mbhc_cal();
	if (!mbhc_cfg.calibration) {
		pr_err("Calibration data allocation failed\n");
		return -ENOMEM;
	}

	msm8930_snd_device = platform_device_alloc("soc-audio", 0);
	if (!msm8930_snd_device) {
		pr_err("Platform device allocation failed\n");
		kfree(mbhc_cfg.calibration);
		return -ENOMEM;
	}

#ifdef CONFIG_MACH_MSM8930_LGPS9

	ret = gpio_request(GPIO_EXT_BOOST_EN, "ext_boost_en");
	if (unlikely(ret < 0))
		printk(KERN_INFO"%s not able to get gpio\n", __func__);

	if (gpio_get_value(GPIO_EXT_BOOST_EN) != 1) {
		gpio_set_value(GPIO_EXT_BOOST_EN, 1);
		pr_info("The ext_bootst_en gpio value is =%d\n",
					gpio_get_value(GPIO_EXT_BOOST_EN));
	}
#endif

	platform_set_drvdata(msm8930_snd_device, &snd_soc_card_msm8930);
	ret = platform_device_add(msm8930_snd_device);
	if (ret) {
		platform_device_put(msm8930_snd_device);
		kfree(mbhc_cfg.calibration);
		return ret;
	}

#ifdef CONFIG_SWITCH_FSA8008
	if (lge_get_board_usembhc()) {
		if (msm8930_configure_headset_mic_gpios()) {
			pr_err("%s Fail to configure headset mic gpios\n", __func__);
			msm8930_headset_gpios_configured = 0;
		} else
			msm8930_headset_gpios_configured = 1;
	} else {
		msm8930_headset_gpios_configured = 1;
	}
#else 
	if (msm8930_configure_headset_mic_gpios()) {
		pr_err("%s Fail to configure headset mic gpios\n", __func__);
		msm8930_headset_gpios_configured = 0;
	} else
		msm8930_headset_gpios_configured = 1;
#endif


	atomic_set(&auxpcm_rsc_ref, 0);
#ifdef CONFIG_FM_RADIO_MI2S_ENABLE
	atomic_set(&mi2s_rsc_ref, 0);
#endif
	return ret;

}
module_init(msm8930_audio_init);

static void __exit msm8930_audio_exit(void)
{
	if (!cpu_is_msm8930() && !cpu_is_msm8930aa() && !cpu_is_msm8627()) {
		pr_err("%s: Not the right machine type\n", __func__);
		return ;
	}
	msm8930_free_headset_mic_gpios();
	platform_device_unregister(msm8930_snd_device);
	kfree(mbhc_cfg.calibration);
}
module_exit(msm8930_audio_exit);

MODULE_DESCRIPTION("ALSA SoC MSM8930");
MODULE_LICENSE("GPL v2");
