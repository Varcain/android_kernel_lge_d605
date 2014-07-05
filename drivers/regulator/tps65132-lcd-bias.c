#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <mach/board.h>
#include <mach/board_lge.h>

#define TPS65132_DEV_NAME "tps65132"

#define DSV_EN_GPIO    56

static struct i2c_client *tps65132_i2c_client = NULL;

static const struct i2c_device_id tps65132_i2c_id[] = {
	{ TPS65132_DEV_NAME, 0 },
	{ },
};

static int tps65132_write_reg(struct i2c_client *client, unsigned char reg, unsigned char data)
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

/*
static int tps65132_read_reg(struct i2c_client *client, unsigned char reg, unsigned char* data)
{
        int err;
        struct i2c_msg msg[2] = {
			{
                client->addr, 0, 1, &reg
			},
			{
				client->addr, I2C_M_RD, 1, data
			}
        };

        err = i2c_transfer(client->adapter, msg, 2);
        if (err < 0) {
                dev_err(&client->dev, "i2c write error\n");
        }
        return 0;
}
*/

void tps65132_lcd_bias_enable(int onoff)
{
	if (onoff == 0) {
		gpio_direction_output(DSV_EN_GPIO, 0);
	} else {
		if(tps65132_i2c_client != NULL) {
			tps65132_write_reg(tps65132_i2c_client, 0x00, 0x0A); //+5.0V
			tps65132_write_reg(tps65132_i2c_client, 0x01, 0x0A); //-5.0V
			tps65132_write_reg(tps65132_i2c_client, 0x03, 0x00); //smartphone

			gpio_direction_output(DSV_EN_GPIO, 1);
		} else {
			printk("%s client is NULL\n", __func__);
		}
	}
}
EXPORT_SYMBOL(tps65132_lcd_bias_enable);

static int tps65132_probe(struct i2c_client *i2c_dev, const struct i2c_device_id *id)
{
	int rc;

	tps65132_i2c_client = i2c_dev;
	pr_info("%s: i2c probe start\n", __func__);

	if (!i2c_check_functionality(i2c_dev->adapter, I2C_FUNC_I2C)) {
		pr_info("%s: i2c check function error \n", __func__);
		return -1;
	}

	if (i2c_dev == NULL) {
		printk("%s client is NULL\n", __func__);
		return -1;
	} else {
		printk("%s %s %x %x\n", __func__, i2c_dev->adapter->name, i2c_dev->addr, i2c_dev->flags);
	}

	gpio_tlmm_config(GPIO_CFG(DSV_EN_GPIO, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

	rc = gpio_request(DSV_EN_GPIO, "dsv_en");
	if (rc) {
		pr_err("request gpio 56 failed, rc=%d\n", rc);
		return -ENODEV;
	}
	gpio_direction_output(DSV_EN_GPIO, 1);

	tps65132_write_reg(tps65132_i2c_client, 0x00, 0x0A); //+5.0V
	tps65132_write_reg(tps65132_i2c_client, 0x01, 0x0A); //-5.0V
	tps65132_write_reg(tps65132_i2c_client, 0x03, 0x01); //smartphone

	return 0;
}


static int tps65132_remove(struct i2c_client *i2c_dev)
{
	tps65132_i2c_client = NULL;
	return 0;
}

static struct i2c_driver tps65132_driver = {
	.driver = {
		.name	= TPS65132_DEV_NAME,
		.owner = THIS_MODULE,
	},
	.probe		= tps65132_probe,
	.remove		= tps65132_remove,
	.id_table	= tps65132_i2c_id,
};

static int __init tps65132_init(void)
{
	static int err;

	err = i2c_add_driver(&tps65132_driver);

	return err;
}

subsys_initcall(tps65132_init);

MODULE_DESCRIPTION("TPS65132 DSV Control");
MODULE_LICENSE("GPL");
