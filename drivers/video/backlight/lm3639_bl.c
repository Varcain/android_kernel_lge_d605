/* drivers/video/backlight/lm3639_bl.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/backlight.h>
#include <linux/leds.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <mach/board.h>
#include <mach/camera.h>
#include <linux/mfd/pm8xxx/pm8921-charger.h>

#include <mach/board_lge.h>
#include <linux/earlysuspend.h>

#include "lm3639_bl.h"

#define I2C_BL_NAME "lm3639_bled"
#define I2C_FLASH_NAME "lm3639_flash"
#define I2C_TORCH_NAME "lm3639_torch"

//                                                                   
/* Flash control for L9II */
/*                                */

/*	 Flash Current
	0000 : 46.875 mA		1000 : 421.875 mA
	0001 : 93.75 mA 		1001 : 468.25 mA
	0010 : 140.625 mA 	1010 : 515.625 mA
	0011 : 187.5 mA  		1011 : 562.5 mA
	0100 : 234.375 mA	1100 : 609.375 mA
	0101 : 281.25 mA		1101 : 659.25 mA
	0110 : 328.125 mA	1110 : 703.125 mA
	0111 : 375 mA		1111 : 750 mA

	Torch Current
	000 : 28.125 mA		100 : 140.625 mA
	001 : 56.25 mA		101 : 168.75 mA
	010 : 84.375 mA		110 : 196.875 mA
	011 : 112.5 mA		111 : 225mA

	Frequency [7]
	0 : 2 Mhz (default)
	1 : 4 Mhz

	Current Limit [6:5]
	00 : 1.7 A			10 : 2.5 A (default)
	01 : 1.9 A			11 : 3.1 A

	Time out [4:0]
	00000 : 32 ms
	01111 : 512 ms (default)
	11111 : 1024 ms
*/

#define LM3639_POWER_OFF		0x00
#define LM3639_POWER_ON_TEST	0xFF
#define LM3639_BL_ON			0xF0
#define LM3639_FLASH_ON			0x0F

//REG address S
#define REG_BL_CONF_2			(unsigned char)0x02
#define REG_BL_CONF_3			(unsigned char)0x03
#define REG_BL_CONF_5			(unsigned char)0x05

#define REG_FL_CONF_1			(unsigned char)0x06
#define REG_FL_CONF_2			(unsigned char)0x07
#define REG_IO_CTRL				(unsigned char)0x09
#define REG_ENABLE				(unsigned char)0x0A
//REG address E

static int lm3639_onoff_state = LM3639_POWER_OFF;

static void lm3639_config_gpio_on(void);
static void lm3639_config_gpio_off(void);

enum led_status {
	LM3639_LED_OFF,
	LM3639_LED_LOW,
	LM3639_LED_HIGH,
	LM3639_LED_MAX
};
//                                                                   

static struct i2c_client *lm3639_i2c_client;
//                                                  
#if 0//def CONFIG_HAS_EARLYSUSPEND
static struct early_suspend early_suspend;
#endif
//                                                  

unsigned char strobe_ctrl;
unsigned char flash_ctrl;


struct backlight_platform_data {
	void (*platform_init)(void);
	int gpio;
	unsigned int mode;
	int max_current;
	int init_on_boot;
	int min_brightness;
	int max_brightness;
	int default_brightness;
	int factory_brightness;
};

struct lm3639_device {
	struct i2c_client *client;
	struct backlight_device *bl_dev;
	struct led_classdev cdev_flash;
	struct led_classdev cdev_torch;
	int gpio;
	int max_current;
	int min_brightness;
	int max_brightness;
	int default_brightness;
	int factory_brightness;
	struct mutex bl_mutex;
};

static const struct i2c_device_id lm3639_bl_id[] = {
	{ I2C_BL_NAME, 0 },
	{ },
};

static int cur_main_lcd_level = DEFAULT_BRIGHTNESS;
//                                                  
//static int saved_main_lcd_level = DEFAULT_BRIGHTNESS;
//                                                  
static struct lm3639_device *main_lm3639_dev;

static int lm3639_read_reg(struct i2c_client *client, unsigned char reg, unsigned char *data)
{
	int err;
	struct i2c_msg msg[] = {
		{
			client->addr, 0, 1, &reg
		},
		{
			client->addr, I2C_M_RD, 1, data
		},
	};

	err = i2c_transfer(client->adapter, msg, 2);
	if (err < 0) {
		dev_err(&client->dev, "i2c read error\n");
		return err;
	}
	return 0;
}

static int lm3639_write_reg(struct i2c_client *client, unsigned char reg, unsigned char data)
{
	int err;
	unsigned char buf[2];
	struct i2c_msg msg = {
		client->addr, 0, 2, buf
	};

	buf[0] = reg;
	buf[1] = data;

	err = i2c_transfer(client->adapter, &msg, 1);
	if (err < 0) {
		dev_err(&client->dev, "i2c write error\n");
	}
	return 0;
}

//                                                  
//static int exp_min_value = 150;
//                                                  
static unsigned char cal_value;

char level2bright_per(int level)
{
	char bright_per = 0;

	if (level < 20 || level > 0xff)
	{
		return (char)0;
	}

	if( (level -20) > 0) {
		bright_per = (char)((level - (unsigned int)20) *(unsigned int)100 / (unsigned int)235);
	} else {
		bright_per = (char)0;
	}

	//printk("%s:%d, bright_per=0x%x, level=0x%x\n", __func__, __LINE__, bright_per, level);

	return bright_per;
}

int bright_per2level(char bright_per)
{
	int level = 0;

	if (bright_per < 0 || bright_per > 100)
	{
		return 0;
	}
	else
	{
		level = (int)((bright_per * 235) / 100) + 20;
	}

	//printk("%s:%d, bright_per=0x%x, level=0x%x\n", __func__, __LINE__, bright_per, level);

	return level;
}

unsigned int get_map_from_level(int level)
{
	char bl_per = 0;
	unsigned int map = 0;
	bl_per = level2bright_per(level);

	if(bl_per < 0 || bl_per > 100) bl_per = 0;

	while( (map+2  < sizeof(lm3639_bright_per_2reg_map)/sizeof(struct lm3639_level2reg))  && \
		(bl_per > lm3639_bright_per_2reg_map[map + 1].level))
	{
		map++;
	}

	//printk("%s:%d, level=0x%x, map=0x%x\n", __func__, __LINE__, level, map);
	return map;
}

unsigned char cal_reg_from_level(int level)
{
	unsigned int map=0;
	char max_reg_value=0, min_reg_value=0, max_level=0, min_level=0;
	unsigned char calc_reg_value = 0;

	level = bright_per2level(level2bright_per(level));

	map = get_map_from_level(level);
	max_reg_value = lm3639_bright_per_2reg_map[map+1].reg_value;
	min_reg_value = lm3639_bright_per_2reg_map[map].reg_value;
	max_level = bright_per2level(lm3639_bright_per_2reg_map[map+1].level);
	min_level = bright_per2level(lm3639_bright_per_2reg_map[map].level);

	calc_reg_value = ((max_reg_value - min_reg_value) * (level - min_level)) / (max_level - min_level) + min_reg_value;

	printk("%s:%d, level=0x%x,brt_per:%d%%, max_reg_value=0x%x, min_reg_value=0x%x, max_level=0x%x, min_level=0x%x, calc_reg_value=0x%x\n",
		__func__, __LINE__, level, (int)level2bright_per(level), max_reg_value, min_reg_value, max_level, min_level, calc_reg_value);

	return calc_reg_value;

}

void lm3639_reg_write_(struct lm3639_reg_data* reg_data)
{
	printk("%s:%d, reg_addr=0x%x, reg_value=0x%x\n", __func__, __LINE__, reg_data->addr, reg_data->value);
	mutex_lock(&main_lm3639_dev->bl_mutex);
	lm3639_write_reg(main_lm3639_dev->client, reg_data->addr, reg_data->value);
	mutex_unlock(&main_lm3639_dev->bl_mutex);
}
EXPORT_SYMBOL(lm3639_reg_write_);

void lm3639_reg_read_(struct lm3639_reg_data* reg_data)
{
	mutex_lock(&main_lm3639_dev->bl_mutex);
	lm3639_read_reg(main_lm3639_dev->client, reg_data->addr, &(reg_data->value) );
	mutex_unlock(&main_lm3639_dev->bl_mutex);
	printk("%s:%d, reg_addr=0x%x, reg_value=0x%x\n", __func__, __LINE__, reg_data->addr, reg_data->value);
}
EXPORT_SYMBOL(lm3639_reg_read_);

static void lm3639_set_main_current_level(struct i2c_client *client, int level)
{
	struct lm3639_device *dev;
	enum lge_boot_mode_type bootmode = lge_get_boot_mode();
	//                                                    
	unsigned char bl_ctrl;
	//                                                    

	printk("%s:%d, level=%d\n", __func__, __LINE__, level);

	dev = (struct lm3639_device *)i2c_get_clientdata(client);

	switch(bootmode) {
		case LGE_BOOT_MODE_FACTORY:
		case LGE_BOOT_MODE_NORMAL:
		case LGE_BOOT_MODE_FACTORY2:
			break;
		case LGE_BOOT_MODE_PIFBOOT:
			level = dev->factory_brightness;
			break;
		default:
			break;
	}
//                                                                                             
	if (level == -1)
		level = cur_main_lcd_level; //dev->default_brightness;
	//                                                                                             

	cur_main_lcd_level = level;
	dev->bl_dev->props.brightness = cur_main_lcd_level;


	if (level != 0) {

		cal_value = cal_reg_from_level(level);

		if (!(lm3639_onoff_state & LM3639_BL_ON)) {
			//                                                          
			lm3639_write_reg(main_lm3639_dev->client, REG_BL_CONF_2, 0x54);
			//                                                          
			//                                                                                    
			lm3639_write_reg(main_lm3639_dev->client, REG_BL_CONF_3, 0x01);
			//                                                                                    
			lm3639_write_reg(main_lm3639_dev->client, REG_BL_CONF_5, cal_value);
			//                                                    
			bl_ctrl = 0;
			lm3639_read_reg(main_lm3639_dev->client, REG_ENABLE, &bl_ctrl);
			bl_ctrl |= 0x11;
			bl_ctrl &= 0xF7;
			lm3639_write_reg(main_lm3639_dev->client, REG_ENABLE, bl_ctrl);
			//                                                    
		}
		lm3639_write_reg(main_lm3639_dev->client, REG_BL_CONF_5, cal_value);

	} else {
		lm3639_write_reg(main_lm3639_dev->client, REG_BL_CONF_5, 0x00);
		//                                                    
		bl_ctrl = 0;
		lm3639_read_reg(main_lm3639_dev->client, REG_ENABLE, &bl_ctrl);
		bl_ctrl &= 0xE6;
		lm3639_write_reg(main_lm3639_dev->client, REG_ENABLE, bl_ctrl);
		//                                                    
	}
}

void lm3639_backlight_on(int level)
{
	mutex_lock(&main_lm3639_dev->bl_mutex);

	lm3639_config_gpio_on();

	lm3639_set_main_current_level(main_lm3639_dev->client, level);

	lm3639_onoff_state |= LM3639_BL_ON;

	mutex_unlock(&main_lm3639_dev->bl_mutex);

	return;
}

void lm3639_backlight_off(void)
{
	if (!(lm3639_onoff_state & LM3639_BL_ON))
		return;

	mutex_lock(&main_lm3639_dev->bl_mutex);

	lm3639_set_main_current_level(main_lm3639_dev->client, 0);

	lm3639_onoff_state &= ~LM3639_BL_ON;

	lm3639_config_gpio_off();

	mutex_unlock(&main_lm3639_dev->bl_mutex);

	return;
}

void lm3639_lcd_backlight_set_level(int level)
{
	if (level > MAX_BRIGHTNESS_LM3639)
		level = MAX_BRIGHTNESS_LM3639;

	if (lm3639_i2c_client != NULL) {
		if (level == 0) {
			lm3639_backlight_off();
		} else {
			lm3639_backlight_on(level);
		}
	} else {
		pr_err("%s(): No client\n", __func__);
	}
}
EXPORT_SYMBOL(lm3639_lcd_backlight_set_level);


static int bl_set_intensity(struct backlight_device *bd)
{

	struct i2c_client *client = to_i2c_client(bd->dev.parent);

	mutex_lock(&main_lm3639_dev->bl_mutex);
	lm3639_set_main_current_level(client, bd->props.brightness);
	mutex_unlock(&main_lm3639_dev->bl_mutex);

	cur_main_lcd_level = bd->props.brightness;

	return 0;
}

static int bl_get_intensity(struct backlight_device *bd)
{
	unsigned char val = 0;

	val &= 0x1f;
	return (int)val;
}


static struct backlight_ops lm3639_bl_ops = {
	.update_status = bl_set_intensity,
	.get_brightness = bl_get_intensity,
};


void lm3639_enable_torch_mode(enum led_status state)
{
	int err = 0;

	unsigned char data1;
	unsigned char torch_curr = 0;
	unsigned char strobe_curr = 0;

	unsigned char data2;
	unsigned char fled_sw_freq = 0x00; //default 2Mhz
	unsigned char fled_curr_limit = 0x00; //1.7A
	unsigned char strobe_timeout = 0x0F; //1024ms

	pr_err("%s: state = %d\n", __func__, state);

	if (state == LM3639_LED_LOW) {
		torch_curr = 0x30; // 112.5 mA
		strobe_curr = 0x01; // 93.75 mA
	} else {
		torch_curr = 0x00;
		strobe_curr = 0x00;
	}
	/* Configuration of frequency, current limit and timeout */
	data2 =  fled_sw_freq | fled_curr_limit | strobe_timeout;
	err = lm3639_write_reg(main_lm3639_dev->client, REG_FL_CONF_2, data2);

	/* Configuration of current */
	data1 = torch_curr | strobe_curr;
	err = lm3639_write_reg(main_lm3639_dev->client, REG_FL_CONF_1, data1);

	/* Control of I/O register */
	strobe_ctrl=0;
	err = lm3639_read_reg(main_lm3639_dev->client, REG_IO_CTRL, &strobe_ctrl);
	strobe_ctrl &= 0xAD; /* 1010 1101 */
	err = lm3639_write_reg(main_lm3639_dev->client, REG_IO_CTRL, strobe_ctrl);

	/* Enable */
	flash_ctrl=0;
	err = lm3639_read_reg(main_lm3639_dev->client, REG_ENABLE, &flash_ctrl);
	flash_ctrl |= 0x42; /* 0100 0010 */
	//                                                                                          
	// we must set the third bit 0 to set TORCH mode.
	flash_ctrl &= 0xFB; /* 1111 1011 */
	err = lm3639_write_reg(main_lm3639_dev->client, REG_ENABLE, flash_ctrl);

}



void lm3639_enable_flash_mode(enum led_status state)
{
	int err = 0;

	unsigned char data1;
	unsigned char torch_curr = 0;
	unsigned char strobe_curr = 0;

	unsigned char data2;
	unsigned char fled_sw_freq = 0x00; //default 2Mhz
	unsigned char fled_curr_limit = 0x10; //2.5A
	unsigned char strobe_timeout = 0x0F; //1024ms

	pr_err("%s: state = %d\n", __func__, state);

	if (state == LM3639_LED_LOW) {
		torch_curr = 0x10; // 56.25 mA
		strobe_curr = 0x01; // 93.75 mA
	} else {
		torch_curr = 0x70; // 225mA
		strobe_curr = 0x0F; // 750 mA
	}

	/* Configuration of frequency, current limit and timeout */
	data2 =  fled_sw_freq | fled_curr_limit | strobe_timeout;
	err = lm3639_write_reg(main_lm3639_dev->client, REG_FL_CONF_2, data2);

	/* Configuration of current */
	data1 = torch_curr | strobe_curr;
	err = lm3639_write_reg(main_lm3639_dev->client, REG_FL_CONF_1, data1);

	/* Control of I/O register */
	strobe_ctrl=0;
	err = lm3639_read_reg(main_lm3639_dev->client, REG_IO_CTRL, &strobe_ctrl);
	strobe_ctrl &= 0xAD; /* 1010 1101 */
	err = lm3639_write_reg(main_lm3639_dev->client, REG_IO_CTRL, strobe_ctrl);

	/* Enable */
	flash_ctrl=0;
	err = lm3639_read_reg(main_lm3639_dev->client, REG_ENABLE, &flash_ctrl);
	flash_ctrl |= 0x66; /* 0110 0110*/
	err = lm3639_write_reg(main_lm3639_dev->client, REG_ENABLE, flash_ctrl);

}

static void lm3639_config_gpio_on(void)
{
	int gpio = main_lm3639_dev->gpio;

	pr_err("%s: Start\n", __func__);

	if (gpio_get_value(gpio) != 1) {
		gpio_direction_output(gpio, 1);
		//                                                     
		mdelay(10);
		//                                                     
		pr_err("%s: ON!\n", __func__);
	}

	pr_err("[LM3639] %s:%d End. gpio=%d\n", __func__, __LINE__,  gpio_get_value(gpio));
}

static void lm3639_config_gpio_off(void)
{
	int gpio = main_lm3639_dev->gpio;

	pr_err("%s: Start\n", __func__);

	if(lm3639_onoff_state == LM3639_POWER_OFF)
	{
		gpio_direction_output(gpio, 0);
		pr_err("%s: OFF!\n", __func__);
	}

	pr_err("[LM3639] %s:%d End. gpio=%d\n", __func__, __LINE__,  gpio_get_value(gpio));
}

void lm3639_led_enable(void)
{
	pr_err("%s: Start\n", __func__);

	mutex_lock(&main_lm3639_dev->bl_mutex);

	lm3639_config_gpio_on();

	lm3639_onoff_state |= LM3639_FLASH_ON;

	mutex_unlock(&main_lm3639_dev->bl_mutex);
}

void lm3639_led_disable(void)
{
	int err = 0;

	pr_err("%s: Start\n", __func__);

	mutex_lock(&main_lm3639_dev->bl_mutex);

	flash_ctrl = 0;
	strobe_ctrl = 0;

	err = lm3639_read_reg(main_lm3639_dev->client, REG_IO_CTRL, &strobe_ctrl);
	strobe_ctrl &= 0x8F; /* 1000 1101 */
	err = lm3639_write_reg(main_lm3639_dev->client, REG_IO_CTRL, strobe_ctrl);


	err = lm3639_read_reg(main_lm3639_dev->client, REG_ENABLE, &flash_ctrl);
	flash_ctrl &= 0x99;
	err = lm3639_write_reg(main_lm3639_dev->client, REG_ENABLE, flash_ctrl);

	lm3639_onoff_state &= ~LM3639_FLASH_ON;

	lm3639_config_gpio_off();

	mutex_unlock(&main_lm3639_dev->bl_mutex);
}

int lm3639_flash_set_led_state(int led_state)
{
	int rc = 0;
	int batt_temp = 0;

	pr_err("%s: led_state = %d\n", __func__, led_state);

	switch (led_state) {
		case MSM_CAMERA_LED_OFF:
			lm3639_led_disable();
			break;
		case MSM_CAMERA_LED_LOW:
			lm3639_led_enable();
			lm3639_enable_torch_mode(LM3639_LED_LOW);
			break;
		case MSM_CAMERA_LED_HIGH:
			lm3639_led_enable();
			batt_temp = pm8921_batt_temperature();
			if(batt_temp > -100) {
				pr_err("%s: Working on LED_HIGH Flash mode (Battery temperature = %d)\n", __func__, batt_temp);
				lm3639_enable_flash_mode(LM3639_LED_HIGH);
			} else {
				pr_err("%s: Working on LED_LOW Flash mode (Battery temperature = %d)\n", __func__, batt_temp);
				lm3639_enable_flash_mode(LM3639_LED_LOW);
			}
			break;
		case MSM_CAMERA_LED_INIT:
			lm3639_config_gpio_on();
			break;
		case MSM_CAMERA_LED_RELEASE:
			lm3639_led_disable();
			break;
		default:
			rc = -EFAULT;
			break;
	}

	return rc;
}
EXPORT_SYMBOL(lm3639_flash_set_led_state);

/* torch */
static void lm3639_torch_led_set(struct led_classdev *led_cdev,
		enum led_brightness value)
{
	pr_err("%s: led_cdev->brightness[%d]\n", __func__, value);

	led_cdev->brightness = value;

	if(value)
		lm3639_enable_torch_mode(LM3639_LED_LOW);
	else
		lm3639_led_disable();
}

/* flash */
static void lm3639_flash_led_set(struct led_classdev *led_cdev,
		enum led_brightness value)
{
	pr_err("%s: led_cdev->brightness[%d]\n", __func__, value);

	led_cdev->brightness = value;

	if(value)
		lm3639_enable_flash_mode(LM3639_LED_LOW);
	else
		lm3639_led_disable();
}

static int lm3639_probe(struct i2c_client *i2c_dev, const struct i2c_device_id *id)
{
	struct backlight_platform_data *pdata;
	struct lm3639_device *dev;
	struct backlight_device *bl_dev;
	struct backlight_properties props;

	int err;

	pr_info("%s: i2c probe start\n", __func__);

	pdata = i2c_dev->dev.platform_data;
	lm3639_i2c_client = i2c_dev;

	dev = kzalloc(sizeof(struct lm3639_device), GFP_KERNEL);
	if (dev == NULL) {
		dev_err(&i2c_dev->dev, "fail alloc for lm3639_device\n");
		return 0;
	}

	pr_info("%s: gpio = %d\n", __func__,pdata->gpio);

	gpio_tlmm_config(GPIO_CFG(pdata->gpio, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	if (pdata->gpio && gpio_request(pdata->gpio, "lm3639_bl_en") != 0) {
		return -ENODEV;
	}

	main_lm3639_dev = dev;

	memset(&props, 0, sizeof(struct backlight_properties));

	props.type = BACKLIGHT_RAW;
	props.max_brightness = MAX_BRIGHTNESS_LM3639;
	bl_dev = backlight_device_register(I2C_BL_NAME, &i2c_dev->dev, NULL, &lm3639_bl_ops, &props);
	bl_dev->props.max_brightness = MAX_BRIGHTNESS_LM3639;
	bl_dev->props.brightness = DEFAULT_BRIGHTNESS;
	bl_dev->props.power = FB_BLANK_UNBLANK;

	dev->bl_dev = bl_dev;
	dev->client = i2c_dev;
	dev->gpio = pdata->gpio;
	dev->max_current = pdata->max_current;
	dev->min_brightness = pdata->min_brightness;
	dev->default_brightness = pdata->default_brightness;
	dev->max_brightness = pdata->max_brightness;
	i2c_set_clientdata(i2c_dev, dev);

	if(pdata->factory_brightness <= 0)
		dev->factory_brightness = DEFAULT_FTM_BRIGHTNESS;
	else
		dev->factory_brightness = pdata->factory_brightness;


	mutex_init(&dev->bl_mutex);

	//                                                  
#if 0
	err = device_create_file(&i2c_dev->dev, &dev_attr_lm3639_level);
	err = device_create_file(&i2c_dev->dev, &dev_attr_lm3639_backlight_on_off);
	err = device_create_file(&i2c_dev->dev, &dev_attr_lm3639_exp_min_value);
#endif
	//                                                  

	/* flash */
	dev->cdev_flash.name = I2C_FLASH_NAME;
	dev->cdev_flash.max_brightness = 16;
	dev->cdev_flash.brightness_set = lm3639_flash_led_set;
	err = led_classdev_register((struct device *)&i2c_dev->dev, &dev->cdev_flash);

	/* torch */
	dev->cdev_torch.name = I2C_TORCH_NAME;
	dev->cdev_torch.max_brightness = 8;
	dev->cdev_torch.brightness_set = lm3639_torch_led_set;
	err = led_classdev_register((struct device *)&i2c_dev->dev, &dev->cdev_torch);

	mutex_init(&dev->bl_mutex);

	//                                                  
#if 0 //def CONFIG_HAS_EARLYSUSPEND
	early_suspend.suspend = lm3639_early_suspend;
	early_suspend.resume = lm3639_late_resume;

	register_early_suspend(&early_suspend);
#endif
	//                                                  

	return 0;

}

static int lm3639_remove(struct i2c_client *i2c_dev)
{
	struct lm3639_device *dev = (struct lm3639_device *)i2c_get_clientdata(i2c_dev);
	int gpio = main_lm3639_dev->gpio;

	led_classdev_unregister(&dev->cdev_torch);
	led_classdev_unregister(&dev->cdev_flash);

	//                                                  
#if 0
	device_remove_file(&i2c_dev->dev, &dev_attr_lm3639_level);
	device_remove_file(&i2c_dev->dev, &dev_attr_lm3639_backlight_on_off);
	device_remove_file(&i2c_dev->dev, &dev_attr_lm3639_exp_min_value);
#endif
	//                                                  

	backlight_device_unregister(dev->bl_dev);

	i2c_set_clientdata(i2c_dev, NULL);

	if (gpio_is_valid(gpio))
		gpio_free(gpio);
	return 0;
}

static struct i2c_driver main_lm3639_driver = {
	.probe = lm3639_probe,
	.remove = lm3639_remove,
	.suspend = NULL,
	.resume = NULL,
	.id_table = lm3639_bl_id,
	.driver = {
		.name = I2C_BL_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init lcd_backlight_init(void)
{
	static int err;

	err = i2c_add_driver(&main_lm3639_driver);

	return err;
}

module_init(lcd_backlight_init);

MODULE_DESCRIPTION("LM3639 Backlight Control");
MODULE_AUTHOR("Jaeseong Gim <jaeseong.gim@lge.com>");
MODULE_LICENSE("GPL");
