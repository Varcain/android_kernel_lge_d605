#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <mach/board_lge.h>
#include <linux/mfd/pm8xxx/pm8038.h>
#include <linux/regulator/consumer.h>
#include "devices.h"
#include "board-lgps9.h"

#ifdef CONFIG_SWITCH_FSA8008
#include "../../../../sound/soc/codecs/wcd9304.h" //                                                                        
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
#ifdef CONFIG_SWITCH_FSA8008
	if(!lge_get_board_usembhc())
	{
		lge_hsd_fsa8008_init();
	}
#endif
}
