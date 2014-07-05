/*
 *  apds9130.c - Linux kernel modules for proximity sensor
 *
 *  Copyright (C) 2012 Lee Kai Koon <kai-koon.lee@avagotech.com>
 *  Copyright (C) 2012 Avago Technologies
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/wakelock.h>

#include <asm/gpio.h>

#define APDS9130_DRV_NAME	"apds9130"
#define DRIVER_VERSION		"1.0.3"

#define APDS9130_INT		49

#if defined(CONFIG_MACH_LGE_FX3_TMUS) || defined(CONFIG_MACH_LGE_FX3_WCDMA_TRF_US)
#define APDS9130_PS_PULSE_NUMBER		9
#elif defined(CONFIG_MACH_LGE_F6_TMUS) || defined(CONFIG_MACH_LGE_F6_VDF)
#define APDS9130_PS_PULSE_NUMBER		7
#else
#define APDS9130_PS_PULSE_NUMBER		8
#endif
#define APDS9130_ALS_THRESHOLD_HSYTERESIS	20	/* 20 = 20% */

/* Change History 
 *
 * 1.0.0	Fundamental Functions of APDS-9130
 *
 * 1.0.1	Remove ioctl interface, remain using sysfs
 *
 */

#define APDS9130_IOCTL_PS_ENABLE		1
#define APDS9130_IOCTL_PS_GET_ENABLE	2
#define APDS9130_IOCTL_PS_GET_PDATA		3	// pdata
#define APDS9130_IOCTL_PS_GET_PSAT		4	// ps saturation - used to detect if ps is triggered by bright light
#define APDS9130_IOCTL_PS_POLL_DELAY	5

#define APDS9130_DISABLE_PS				0
#define APDS9130_ENABLE_PS_WITH_INT		1
#define APDS9130_ENABLE_PS_NO_INT		2

#define APDS9130_PS_POLL_SLOW			0	// 1 Hz (1s)
#define APDS9130_PS_POLL_MEDIUM			1	// 10 Hz (100ms)
#define APDS9130_PS_POLL_FAST			2	// 20 Hz (50ms)

/*
 * Defines
 */

#define APDS9130_ENABLE_REG	0x00
#define APDS9130_PTIME_REG	0x02
#define APDS9130_WTIME_REG	0x03
#define APDS9130_PILTL_REG	0x08
#define APDS9130_PILTH_REG	0x09
#define APDS9130_PIHTL_REG	0x0A
#define APDS9130_PIHTH_REG	0x0B
#define APDS9130_PERS_REG	0x0C
#define APDS9130_CONFIG_REG	0x0D
#define APDS9130_PPCOUNT_REG	0x0E
#define APDS9130_CONTROL_REG	0x0F
#define APDS9130_REV_REG	0x11
#define APDS9130_ID_REG		0x12
#define APDS9130_STATUS_REG	0x13
#define APDS9130_PDATAL_REG	0x18
#define APDS9130_PDATAH_REG	0x19

#define CMD_BYTE		0x80
#define CMD_WORD		0xA0
#define CMD_SPECIAL		0xE0

#define CMD_CLR_PS_INT		0xE5
#define CMD_CLR_ALS_INT		0xE6
#define CMD_CLR_PS_ALS_INT	0xE7

/* Register Value define : PERS */
#define APDS9130_PPERS_0	0x00  /* Every proximity ADC cycle */
#define APDS9130_PPERS_1	0x10  /* 1 consecutive proximity value out of range */
#define APDS9130_PPERS_2	0x20  /* 2 consecutive proximity value out of range */
#define APDS9130_PPERS_3	0x30  /* 3 consecutive proximity value out of range */
#define APDS9130_PPERS_4	0x40  /* 4 consecutive proximity value out of range */
#define APDS9130_PPERS_5	0x50  /* 5 consecutive proximity value out of range */
#define APDS9130_PPERS_6	0x60  /* 6 consecutive proximity value out of range */
#define APDS9130_PPERS_7	0x70  /* 7 consecutive proximity value out of range */
#define APDS9130_PPERS_8	0x80  /* 8 consecutive proximity value out of range */
#define APDS9130_PPERS_9	0x90  /* 9 consecutive proximity value out of range */
#define APDS9130_PPERS_10	0xA0  /* 10 consecutive proximity value out of range */
#define APDS9130_PPERS_11	0xB0  /* 11 consecutive proximity value out of range */
#define APDS9130_PPERS_12	0xC0  /* 12 consecutive proximity value out of range */
#define APDS9130_PPERS_13	0xD0  /* 13 consecutive proximity value out of range */
#define APDS9130_PPERS_14	0xE0  /* 14 consecutive proximity value out of range */
#define APDS9130_PPERS_15	0xF0  /* 15 consecutive proximity value out of range */

#define APDS9130_PRX_IR_DIOD	0x20  /* Proximity uses CH1 diode */

#define APDS9130_PGAIN_1X	0x00  /* PS GAIN 1X */
#define APDS9130_PGAIN_2X	0x04  /* PS GAIN 2X */
#define APDS9130_PGAIN_4X	0x08  /* PS GAIN 4X */
#define APDS9130_PGAIN_8X	0x0C  /* PS GAIN 8X */

#define APDS9130_PDRVIE_100MA	0x00  /* PS 100mA LED drive */
#define APDS9130_PDRVIE_50MA	0x40  /* PS 50mA LED drive */
#define APDS9130_PDRVIE_25MA	0x80  /* PS 25mA LED drive */
#define APDS9130_PDRVIE_12_5MA	0xC0  /* PS 12.5mA LED drive */

//                                                                           
#define DEFAULT_CROSS_TALK		50

#if defined(CONFIG_MACH_LGE_FX3_TMUS) || defined(CONFIG_MACH_LGE_FX3_WCDMA_TRF_US)
#define ADD_TO_CROSS_TALK       300
#elif defined(CONFIG_MACH_LGE_L9II_COMMON)
#define ADD_TO_CROSS_TALK		250
#elif defined(CONFIG_MACH_LGE_F6_TMUS) || defined(CONFIG_MACH_LGE_F6_VDF)
#define ADD_TO_CROSS_TALK		320
#else
#define ADD_TO_CROSS_TALK       200
#endif

#if defined(CONFIG_MACH_LGE_L9II_COMMON)
#define SUB_FROM_PS_THRESHOLD	120
#else
#define SUB_FROM_PS_THRESHOLD	100
#endif

#if defined(CONFIG_MACH_LGE_L9II_COMMON)
#define MAX_CROSS_TALK			870
#else
#define MAX_CROSS_TALK			720
#endif

static int stored_cross_talk = 0;
//                                    

/*
 * Structs
 */

struct apds9130_data {
	struct i2c_client *client;
	struct mutex update_lock;
	struct delayed_work	dwork;		/* for PS interrupt */
	struct delayed_work	ps_dwork;	/* for PS polling */
	struct input_dev *input_dev_ps;

	unsigned int enable;
	unsigned int ptime;
	unsigned int wtime;
	unsigned int pilt;
	unsigned int piht;
	unsigned int pers;
	unsigned int config;
	unsigned int ppcount;
	unsigned int control;

	/* control flag from HAL */
	unsigned int enable_ps_sensor;

	/* PS parameters */
	unsigned int ps_threshold;
	unsigned int ps_hysteresis_threshold; 	/* always lower than ps_threshold */
	unsigned int ps_detection;				/* 0 = near-to-far; 1 = far-to-near */
	unsigned int ps_data;					/* to store PS data */
	unsigned int ps_sat;					/* to store PS saturation bit */
	unsigned int ps_poll_delay;				/* needed for PS polling */

//                                    
	int irq;
    unsigned char irq_wake;
	struct wake_lock wakelock;

	unsigned int cross_talk;			// add for calibration
    unsigned int avg_cross_talk;		// add for calibration

    unsigned int add_to_cross_talk;
    unsigned int sub_from_ps_threshold;
//                                    

};

/*
 * Global data
 */
static struct i2c_client *apds9130_i2c_client; /* global i2c_client to support ioctl */
static struct workqueue_struct *apds9130_workqueue;

/*
 * Management functions
 */

//                                                                           
static int __init prox_cal_data(char *str)
{
	s32 value = simple_strtol(str, NULL, 10);
    printk("[ProximitySensor] %s\n", __func__);
#if defined(CONFIG_MACH_LGE_L9II_COMMON)
	if(value == 0) {
		value += 200;
		printk("=====================================================\n"
			"ProximitySensor : Calibration has not been performed.\n"
			"   -> cross_talk is set to %d\n"
			"=====================================================\n", value);
	}
#endif
	stored_cross_talk = value;
	printk("ProximitySensor : cal_data = %d\n", stored_cross_talk);

	return 1;
}
__setup("prox_cal_data=", prox_cal_data);
//                                    


static int apds9130_set_command(struct i2c_client *client, int command)
{
	struct apds9130_data *data = i2c_get_clientdata(client);
	int ret;
	int clearInt;

	if (command == 0)
		clearInt = CMD_CLR_PS_INT;
	else if (command == 1)
		clearInt = CMD_CLR_ALS_INT;
	else
		clearInt = CMD_CLR_PS_ALS_INT;
		
	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte(client, clearInt);
	mutex_unlock(&data->update_lock);

	return ret;
}

static int apds9130_set_enable(struct i2c_client *client, int enable)
{
	struct apds9130_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS9130_ENABLE_REG, enable);
	mutex_unlock(&data->update_lock);

	data->enable = enable;

	return ret;
}

static int apds9130_set_ptime(struct i2c_client *client, int ptime)
{
	struct apds9130_data *data = i2c_get_clientdata(client);
	int ret;
	
	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS9130_PTIME_REG, ptime);
	mutex_unlock(&data->update_lock);

	data->ptime = ptime;

	return ret;
}

static int apds9130_set_wtime(struct i2c_client *client, int wtime)
{
	struct apds9130_data *data = i2c_get_clientdata(client);
	int ret;
	
	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS9130_WTIME_REG, wtime);
	mutex_unlock(&data->update_lock);

	data->wtime = wtime;

	return ret;
}

static int apds9130_set_pilt(struct i2c_client *client, int threshold)
{
	struct apds9130_data *data = i2c_get_clientdata(client);
	int ret;
	
	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_word_data(client, CMD_WORD|APDS9130_PILTL_REG, threshold);
	mutex_unlock(&data->update_lock);
	
	data->pilt = threshold;

	return ret;
}

static int apds9130_set_piht(struct i2c_client *client, int threshold)
{
	struct apds9130_data *data = i2c_get_clientdata(client);
	int ret;
	
	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_word_data(client, CMD_WORD|APDS9130_PIHTL_REG, threshold);
	mutex_unlock(&data->update_lock);
	
	data->piht = threshold;

	return ret;
}

static int apds9130_set_pers(struct i2c_client *client, int pers)
{
	struct apds9130_data *data = i2c_get_clientdata(client);
	int ret;
	
	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS9130_PERS_REG, pers);
	mutex_unlock(&data->update_lock);

	data->pers = pers;

	return ret;
}

static int apds9130_set_config(struct i2c_client *client, int config)
{
	struct apds9130_data *data = i2c_get_clientdata(client);
	int ret;
	
	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS9130_CONFIG_REG, config);
	mutex_unlock(&data->update_lock);

	data->config = config;

	return ret;
}

static int apds9130_set_ppcount(struct i2c_client *client, int ppcount)
{
	struct apds9130_data *data = i2c_get_clientdata(client);
	int ret;
	
	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS9130_PPCOUNT_REG, ppcount);
	mutex_unlock(&data->update_lock);

	data->ppcount = ppcount;

	return ret;
}

static int apds9130_set_control(struct i2c_client *client, int control)
{
	struct apds9130_data *data = i2c_get_clientdata(client);
	int ret;
	
	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS9130_CONTROL_REG, control);
	mutex_unlock(&data->update_lock);

	data->control = control;

	return ret;
}

static void apds9130_change_ps_threshold(struct i2c_client *client)
{
	struct apds9130_data *data = i2c_get_clientdata(client);

	data->ps_data =	i2c_smbus_read_word_data(client, CMD_WORD|APDS9130_PDATAL_REG);

	/*                                               
                                      
                                                                                        
                                           
  */
	apds9130_set_pers(client, APDS9130_PPERS_2);

	if(wake_lock_active(&data->wakelock))
		wake_unlock(&data->wakelock);

	wake_lock_timeout(&data->wakelock, 2*HZ);

	if ( (data->ps_data > data->pilt) && (data->ps_data >= data->piht) ) {
		/* far-to-near detected */
		data->ps_detection = 1;

		input_report_abs(data->input_dev_ps, ABS_DISTANCE, 0);/* FAR-to-NEAR detection */
		input_sync(data->input_dev_ps);

		i2c_smbus_write_word_data(client, CMD_WORD|APDS9130_PILTL_REG, data->ps_hysteresis_threshold);
		i2c_smbus_write_word_data(client, CMD_WORD|APDS9130_PIHTL_REG, 1023);

		data->pilt = data->ps_hysteresis_threshold;
		data->piht = 1023;

		printk("[ProximitySensor] far-to-NEAR\n");
	}
	else if ( (data->ps_data <= data->pilt) && (data->ps_data < data->piht) ) {
		/* near-to-far detected */
		data->ps_detection = 0;

		input_report_abs(data->input_dev_ps, ABS_DISTANCE, 1);/* NEAR-to-FAR detection */
		input_sync(data->input_dev_ps);

		i2c_smbus_write_word_data(client, CMD_WORD|APDS9130_PILTL_REG, 0);
		i2c_smbus_write_word_data(client, CMD_WORD|APDS9130_PIHTL_REG, data->ps_threshold);

		data->pilt = 0;
		data->piht = data->ps_threshold;

		printk("[ProximitySensor] near-to-FAR\n");
	}
	else if ( (data->pilt == 1023) && (data->piht == 0) ) {
		printk("[ProximitySensor] near-to-FAR with [pilt][piht] = [%d][%d]\n", data->pilt, data->piht);
		/* near-to-far detected */
		data->ps_detection = 0;

		input_report_abs(data->input_dev_ps, ABS_DISTANCE, 1);/* NEAR-to-FAR detection */
		input_sync(data->input_dev_ps);

		i2c_smbus_write_word_data(client, CMD_WORD|APDS9130_PILTL_REG, 0);
		i2c_smbus_write_word_data(client, CMD_WORD|APDS9130_PIHTL_REG, data->ps_threshold);

		data->pilt = 0;
		data->piht = data->ps_threshold;
	}

}

static void apds9130_reschedule_work(struct apds9130_data *data,
					  unsigned long delay)
{
	/*
	 * If work is already scheduled then subsequent schedules will not
	 * change the scheduled time that's why we have to cancel it first.
	 */
	__cancel_delayed_work(&data->dwork);
	queue_delayed_work(apds9130_workqueue, &data->dwork, delay);

}

/* PS polling routine */
static void apds9130_ps_polling_work_handler(struct work_struct *work)
{
	struct apds9130_data *data = container_of(work, struct apds9130_data, ps_dwork.work);
	struct i2c_client *client=data->client;
	int status;

	status = i2c_smbus_read_byte_data(client, CMD_BYTE|APDS9130_STATUS_REG);
	data->ps_data = i2c_smbus_read_word_data(client, CMD_WORD|APDS9130_PDATAL_REG);
		
	// check PS under bright light
	if ( (data->ps_data > data->ps_threshold) && (data->ps_detection == 0) && ((status&0x40)==0x00) ) {
		/* far-to-near detected */
		data->ps_detection = 1;

		input_report_abs(data->input_dev_ps, ABS_DISTANCE, 0);/* FAR-to-NEAR detection */
		input_sync(data->input_dev_ps);

		printk("[ProximitySensor] far-to-NEAR\n");
	} 
	else if ( (data->ps_data < data->ps_hysteresis_threshold) && (data->ps_detection == 1) && ((status&0x40)==0x00) ) {	
		// PS was previously in far-to-near condition
		/* near-to-far detected */
		data->ps_detection = 0;

		input_report_abs(data->input_dev_ps, ABS_DISTANCE, 1);/* NEAR-to-FAR detection */
		input_sync(data->input_dev_ps);

		printk("[ProximitySensor] near-to-FAR\n");
	}
	else {
		printk("* Triggered by background ambient noise\n");
	}

	if ( (status&0x40) == 0x40 ) {	// need to clear psat bit if it is set
		apds9130_set_command(client, 0);	/* 0 = CMD_CLR_PS_INT */
	}
	
	schedule_delayed_work(&data->ps_dwork, msecs_to_jiffies(data->ps_poll_delay));	// restart timer
}

/* PS interrupt routine */
static void apds9130_work_handler(struct work_struct *work)
{
	struct apds9130_data *data = container_of(work, struct apds9130_data, dwork.work);
	struct i2c_client *client=data->client;
	int status;
	int enable;
    int err = 0;

	status = i2c_smbus_read_byte_data(client, CMD_BYTE|APDS9130_STATUS_REG);
	enable = i2c_smbus_read_byte_data(client, CMD_BYTE|APDS9130_ENABLE_REG);

	err = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS9130_ENABLE_REG, 1);
    if(err) {
        printk("[ProximitySensor_E] %s : i2c_write fail(%d) at (%d)\n", __func__, err, __LINE__);
        return;
    }

	printk("[ProximitySensor] %s : [status:0x%02x][enable:0x%02x]\n", __func__, status, enable);
	if ((status & enable & 0x30) == 0x30) {
		/* both PS and ALS are interrupted - never happened*/

		if ( (status&0x40) != 0x40 ) // no PSAT bit set
			apds9130_change_ps_threshold(client);
		else {
			if (data->ps_detection == 1) {
				apds9130_change_ps_threshold(client);
			}
			else {
				printk("* Triggered by background ambient noise\n");
			}
		}

		err = apds9130_set_command(client, 2);	/* 2 = CMD_CLR_PS_ALS_INT */
	}
	else if ((status & enable & 0x20) == 0x20) {
		/* only PS is interrupted */
		
		if ( (status&0x40) != 0x40 ) // no PSAT bit set
			apds9130_change_ps_threshold(client);	// far-to-near
		else {
			if (data->ps_detection == 1) {
				apds9130_change_ps_threshold(client); // near-to-far
			}
			else {
				printk("* Triggered by background ambient noise\n");
			}
		}

		err = apds9130_set_command(client, 0);	/* 0 = CMD_CLR_PS_INT */
	}
	else if ((status & enable & 0x10) == 0x10) {
		/* only ALS is interrupted - will never happened*/	

		err = apds9130_set_command(client, 1);	/* 1 = CMD_CLR_ALS_INT */
	}

    if(err) {
        printk("[ProximitySensor_E] %s : i2c_write fail(%d) at (%d)\n", __func__, err, __LINE__);
        return;
    }

	err = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS9130_ENABLE_REG, data->enable);
    if(err) {
        printk("[ProximitySensor_E] %s : i2c_write fail(%d) at (%d)\n", __func__, err, __LINE__);
        return;
    }
}

/* assume this is ISR */
static irqreturn_t apds9130_interrupt(int vec, void *info)
{
	struct i2c_client *client=(struct i2c_client *)info;
	struct apds9130_data *data = i2c_get_clientdata(client);

	apds9130_reschedule_work(data, 0);	

	return IRQ_HANDLED;
}

static int apds9130_set_ps_poll_delay(struct i2c_client *client, unsigned int val)
{
	struct apds9130_data *data = i2c_get_clientdata(client);
	int ret;
	int wtime=0;
 	
	printk("[ProximitySensor] %s(%d)\n", __func__, val);

	if ((val != APDS9130_PS_POLL_SLOW) && (val != APDS9130_PS_POLL_MEDIUM) && (val != APDS9130_PS_POLL_FAST)) {
		printk("[ProximitySensor_E] %s : invalid value(%d)\n", __func__, val);
		return -1;
	}
	
	if (val == APDS9130_PS_POLL_FAST) {
		data->ps_poll_delay = 50;	// 50ms
		wtime = 0xEE;	// ~50ms
	}
	else if (val == APDS9130_PS_POLL_MEDIUM) {
		data->ps_poll_delay = 100;	// 100ms
		wtime = 0xDC;	// ~100ms
	}
	else {	// APDS9130_PS_POLL_SLOW
		data->ps_poll_delay = 1000;	// 1000ms
		wtime = 0x00;	// 696ms
	}

	ret = apds9130_set_wtime(client, wtime);
	if (ret < 0) 
		return ret;

	/*
	 * If work is already scheduled then subsequent schedules will not
	 * change the scheduled time that's why we have to cancel it first.
	 */
	__cancel_delayed_work(&data->ps_dwork);
	flush_delayed_work(&data->ps_dwork);
	queue_delayed_work(apds9130_workqueue, &data->ps_dwork, msecs_to_jiffies(data->ps_poll_delay));

	return 0;
}

static int apds9130_enable_ps_sensor(struct i2c_client *client, int val)
{
	struct apds9130_data *data = i2c_get_clientdata(client);

	printk("[ProximitySensor] %s(%d)\n", __func__, val);

	if ((val != APDS9130_DISABLE_PS) && (val != APDS9130_ENABLE_PS_WITH_INT) && (val != APDS9130_ENABLE_PS_NO_INT)) {
		printk("[ProximitySensor_E] %s : invalid value(%d)\n", __func__, val);
		return -1;
	}

	/* APDS9130_DISABLE_PS (0) = Disable PS */
	/* APDS9130_ENABLE_PS_WITH_INT (1) = Enable PS with interrupt enabled */
	/* APDS9130_ENABLE_PS_NO_INT (2) = Enable PS without interrupt enabled */

	if(val == APDS9130_ENABLE_PS_WITH_INT || val == APDS9130_ENABLE_PS_NO_INT) {
		//turn on p sensor
		data->enable_ps_sensor = val;

		apds9130_set_enable(client,0); /* Power Off */

		apds9130_set_pilt(client, 1023);		// to force first Near-to-Far interrupt
		apds9130_set_piht(client, 0);

		/*                                               
                                       
                                                                                         
                                         
   */
		apds9130_set_pers(client, APDS9130_PPERS_0);

		if (val == APDS9130_ENABLE_PS_WITH_INT) {
			apds9130_set_enable(client, 0x2D);	 /* enable PS interrupt */

			/*
			 * If work is already scheduled then subsequent schedules will not
			 * change the scheduled time that's why we have to cancel it first.
			 */
			__cancel_delayed_work(&data->ps_dwork);
			flush_delayed_work(&data->ps_dwork);
		}
		else {
			apds9130_set_enable(client, 0x0D);	 /* no PS interrupt */

			/*
			 * If work is already scheduled then subsequent schedules will not
			 * change the scheduled time that's why we have to cancel it first.
			 */
			__cancel_delayed_work(&data->ps_dwork);
			flush_delayed_work(&data->ps_dwork);
			schedule_delayed_work(&data->ps_dwork, msecs_to_jiffies(data->ps_poll_delay));
        }
	}
	else {
		apds9130_set_enable(client, 0);
		data->enable_ps_sensor = 0;

		/*
		 * If work is already scheduled then subsequent schedules will not
		 * change the scheduled time that's why we have to cancel it first.
		 */
		__cancel_delayed_work(&data->ps_dwork);
		flush_delayed_work(&data->ps_dwork);
	}

	return 0;
}

/*
 * SysFS support
 */
static ssize_t apds9130_show_pdata(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9130_data *data = i2c_get_clientdata(client);
	int pdata;

	mutex_lock(&data->update_lock);
	pdata = i2c_smbus_read_word_data(client, CMD_WORD|APDS9130_PDATAL_REG);
	mutex_unlock(&data->update_lock);

	return sprintf(buf, "%d\n", pdata);
}
static DEVICE_ATTR(pdata, S_IRUGO,
		   apds9130_show_pdata, NULL);

static ssize_t apds9130_show_enable_ps_sensor(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9130_data *data = i2c_get_clientdata(client);
	
	return sprintf(buf, "%d\n", data->enable_ps_sensor);
}

static ssize_t apds9130_store_enable_ps_sensor(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned long val = simple_strtoul(buf, NULL, 10);
	
	printk("[ProximitySensor] %s(%ld)\n", __func__, val);
	
	if ((val != 0) && (val != 1)) {
		printk("[ProximitySensor] %s : invalid value(%ld)\n", __func__, val);
		return count;
	}

	apds9130_enable_ps_sensor(client, val);	
	
	return count;
}

static DEVICE_ATTR(enable_ps_sensor, S_IWUSR | S_IWGRP | S_IRUGO,
		apds9130_show_enable_ps_sensor, apds9130_store_enable_ps_sensor);


static ssize_t apds9130_show_ps_poll_delay(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9130_data *data = i2c_get_clientdata(client);
	
	return sprintf(buf, "%d\n", data->ps_poll_delay);
}

static ssize_t apds9130_store_ps_poll_delay(struct device *dev,
					struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9130_data *data = i2c_get_clientdata(client);
	unsigned long val = simple_strtoul(buf, NULL, 10);

	if (data->enable_ps_sensor == APDS9130_ENABLE_PS_NO_INT) {	// only when ps is in polling mode
		apds9130_set_ps_poll_delay(client, val);
	}
	else {
		return 0;
	}

	return count;
}

static DEVICE_ATTR(ps_poll_delay, S_IWUSR | S_IRUGO,
				   apds9130_show_ps_poll_delay, apds9130_store_ps_poll_delay);


//                                                                           
static int apds9130_Run_Cross_talk_Calibration(struct i2c_client *client)
{
	struct apds9130_data *data = i2c_get_clientdata(client);
	int pdata[20], total_pdata = 0;
	int i, j, temp;
	bool isCal = false;

	printk("===================================================\n%s: START proximity sensor calibration\n", __func__);

	// proximity enable & wait
	apds9130_set_enable(client, 0x0D);

RE_CALIBRATION:

	// read 20 data to calculate the average pdata at no-object state
	for(i=0; i<20; i++) {
		pdata[i] = i2c_smbus_read_word_data(client, CMD_WORD|APDS9130_PDATAL_REG);
		mdelay(5);
	}

	// pdata sorting
	for(i=0; i<19; i++) {
		for(j=i+1; j<20; j++) {
			if(pdata[i] > pdata[j]) {
				temp = pdata[i];
				pdata[i] = pdata[j];
				pdata[j] = temp;
			}
		}
	}

	// calculate the cross-talk using central 10 data
	for(i=5; i<15; i++) {
        printk("pdata = %d\n", pdata[i]);
		total_pdata += pdata[i];
    }

	// store cross_talk value into the apds9190_data structure
	data->cross_talk = total_pdata / 10;
    data->avg_cross_talk = data->cross_talk;

	// check if the calibrated cross_talk data is valid or not
	if(data->cross_talk > MAX_CROSS_TALK) {
		printk("[ProximitySensor] %s: invalid calibrated data\n", __func__);

		if(!isCal) {
			printk("[ProximitySensor] %s: RE_CALIBRATION start\n", __func__);
			isCal = true;
			total_pdata = 0;
			goto RE_CALIBRATION;
		} else {
			printk("[ProximitySensor] %s: CALIBRATION FAIL -> cross_talk is set to DEFAULT\n", __func__);
			data->cross_talk = DEFAULT_CROSS_TALK;
		}
	}

	// proximity disable
	apds9130_set_enable(client, 0x00);

	printk("[ProximitySensor] %s: total_pdata = %d & cross_talk = %d\n%s: FINISH proximity sensor calibration\n===================================================\n",
            __func__, total_pdata, data->cross_talk, __func__);

	return data->cross_talk;
}

static ssize_t apds9130_show_run_calibration(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9130_data *data = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", data->avg_cross_talk);
}

static ssize_t apds9130_store_run_calibration(struct device *dev,
					struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9130_data *data = i2c_get_clientdata(client);
    int err;

	// start calibration
	apds9130_Run_Cross_talk_Calibration(client);

	// set threshold for near/far status
	data->ps_threshold = data->cross_talk + data->add_to_cross_talk;
	data->ps_hysteresis_threshold = data->ps_threshold - data->sub_from_ps_threshold;

	err = apds9130_set_piht(client, data->ps_threshold);
    if(err < 0) return err;
	err = apds9130_set_pilt(client, data->ps_hysteresis_threshold);
	if(err < 0) return err;

	printk("[ProximitySensor] %s: [piht][pilt][c_t] = [%d][%d][%d]\n",
				__func__, data->ps_threshold, data->ps_hysteresis_threshold, data->cross_talk);

	return count;
}

static DEVICE_ATTR(run_calibration, S_IRUGO | S_IWUSR| S_IWGRP,
	apds9130_show_run_calibration, apds9130_store_run_calibration);

static ssize_t apds9130_show_default_crosstalk(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", DEFAULT_CROSS_TALK);
}

static ssize_t apds9130_store_default_crosstalk(struct device *dev,
					struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9130_data *data = i2c_get_clientdata(client);

    data->ps_threshold = DEFAULT_CROSS_TALK + data->add_to_cross_talk;
	data->ps_hysteresis_threshold = data->ps_threshold - data->sub_from_ps_threshold;

	printk("[ProximitySensor] %s: [piht][pilt][c_t] = [%d][%d][%d]\n",
				__func__, data->ps_threshold, data->ps_hysteresis_threshold, data->cross_talk);

    return count;
}

static DEVICE_ATTR(default_crosstalk, S_IRUGO | S_IWUSR | S_IWGRP,
         apds9130_show_default_crosstalk, apds9130_store_default_crosstalk);

static ssize_t apds9130_show_ps_threshold(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    struct i2c_client *client = to_i2c_client(dev);
    int value;

	value = i2c_smbus_read_word_data(client, CMD_WORD|APDS9130_PIHTL_REG);
    return sprintf(buf, "%d\n", value);
}

static ssize_t apds9130_store_ps_threshold(struct device *dev,
					struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9130_data *data = i2c_get_clientdata(client);
	unsigned long val = simple_strtoul(buf, NULL, 10);

    data->ps_threshold = val;

    return count;
}

static DEVICE_ATTR(piht, S_IRUGO | S_IWUSR | S_IWGRP,
         apds9130_show_ps_threshold, apds9130_store_ps_threshold);

static ssize_t apds9130_show_ps_hysteresis_threshold(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    struct i2c_client *client = to_i2c_client(dev);
    int value;

	value = i2c_smbus_read_word_data(client, CMD_WORD|APDS9130_PILTL_REG);

    return sprintf(buf, "%d\n", value);
}

static ssize_t apds9130_store_ps_hysteresis_threshold(struct device *dev,
					struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9130_data *data = i2c_get_clientdata(client);
	unsigned long val = simple_strtoul(buf, NULL, 10);

    data->ps_hysteresis_threshold = val;

    return count;
}

static DEVICE_ATTR(pilt, S_IRUGO | S_IWUSR | S_IWGRP,
         apds9130_show_ps_hysteresis_threshold, apds9130_store_ps_hysteresis_threshold);

static ssize_t apds9130_show_ppcount(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct apds9130_data *data = i2c_get_clientdata(client);

    return sprintf(buf, "%d\n", data->ppcount);
}

static ssize_t apds9130_store_ppcount(struct device *dev,
					struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned long val = simple_strtoul(buf, NULL, 10);

	apds9130_set_ppcount(client, val);

    return count;
}

static DEVICE_ATTR(ppcount, S_IRUGO | S_IWUSR | S_IWGRP,
         apds9130_show_ppcount, apds9130_store_ppcount);

static ssize_t apds9130_show_add_to_cross_talk(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct apds9130_data *data = i2c_get_clientdata(client);

    return sprintf(buf, "%d\n", data->add_to_cross_talk);
}

static ssize_t apds9130_store_add_to_cross_talk(struct device *dev,
					struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
    struct apds9130_data *data = i2c_get_clientdata(client);
	unsigned long val = simple_strtoul(buf, NULL, 10);

	data->add_to_cross_talk = val;

    return count;
}

static DEVICE_ATTR(add_to_cross_talk, S_IRUGO | S_IWUSR | S_IWGRP,
         apds9130_show_add_to_cross_talk, apds9130_store_add_to_cross_talk);

static ssize_t apds9130_show_sub_from_ps_threshold(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct apds9130_data *data = i2c_get_clientdata(client);

    return sprintf(buf, "%d\n", data->sub_from_ps_threshold);
}

static ssize_t apds9130_store_sub_from_ps_threshold(struct device *dev,
					struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
    struct apds9130_data *data = i2c_get_clientdata(client);
	unsigned long val = simple_strtoul(buf, NULL, 10);

	data->sub_from_ps_threshold = val;

    return count;
}

static DEVICE_ATTR(sub_from_ps_threshold, S_IRUGO | S_IWUSR | S_IWGRP,
         apds9130_show_sub_from_ps_threshold, apds9130_store_sub_from_ps_threshold);

static ssize_t apds9130_show_pdrive(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    struct i2c_client *client = to_i2c_client(dev);
    char pdrive = (APDS9130_PDRVIE_12_5MA & i2c_smbus_read_byte_data(client, CMD_WORD|APDS9130_CONTROL_REG)) >> 6;

    return sprintf(buf, "%x\n", pdrive);
}

static ssize_t apds9130_store_pdrive(struct device *dev,
					struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned char val = simple_strtoul(buf, NULL, 10);
    unsigned char cntl_reg = 0x3F & i2c_smbus_read_byte_data(client, CMD_WORD|APDS9130_CONTROL_REG);

    if(val > 3) {
        printk("[ProximitySensor_E] %s: invalid argument\n", __func__);
        return count;
    }
    cntl_reg |= (val << 6);
    printk("[ProximitySensor] %s: pdrive=%x, cntl_reg=%x\n", __func__, val, cntl_reg);
	apds9130_set_control(client, cntl_reg);

    return count;
}
static DEVICE_ATTR(pdrive, S_IRUGO | S_IWUSR | S_IWGRP,
         apds9130_show_pdrive, apds9130_store_pdrive);
//                                    
static struct attribute *apds9130_attributes[] = {
	&dev_attr_pdata.attr,
	&dev_attr_enable_ps_sensor.attr,
	&dev_attr_ps_poll_delay.attr,
//                                                                           
	&dev_attr_run_calibration.attr,
	&dev_attr_default_crosstalk.attr,
    &dev_attr_piht.attr,
    &dev_attr_pilt.attr,
    &dev_attr_ppcount.attr,
    &dev_attr_add_to_cross_talk.attr,
    &dev_attr_sub_from_ps_threshold.attr,
    &dev_attr_pdrive.attr,
//                                    
	NULL
};

static const struct attribute_group apds9130_attr_group = {
	.attrs = apds9130_attributes,
};

/*
 * Initialization function
 */
static int apds9130_init_client(struct i2c_client *client)
{
	int err;
	int id;

	err = apds9130_set_enable(client, 0);
	if (err < 0)
		return err;
	
	id = i2c_smbus_read_byte_data(client, CMD_BYTE|APDS9130_ID_REG);
	if (id == 0x39) {
		printk("[ProximitySensor] APDS-9130\n");
	} else {
		printk("[ProximitySensor_E] Not APDS-9130 %x\n", id);
		return -EIO;
	}

	err = apds9130_set_ptime(client, 0xFF);	// 2.72ms Prox integration time
	if (err < 0) return err;

	err = apds9130_set_wtime(client, 0xDC);	// 100ms Wait time for POLL_MEDIUM
	if (err < 0) return err;

	err = apds9130_set_ppcount(client, APDS9130_PS_PULSE_NUMBER);
	if (err < 0) return err;

	err = apds9130_set_config(client, 0);   // no long wait
	if (err < 0) return err;

#if defined(CONFIG_MACH_LGE_L9II_COMMON)
	err = apds9130_set_control(client, APDS9130_PDRVIE_100MA|APDS9130_PRX_IR_DIOD|APDS9130_PGAIN_2X);
#else
	err = apds9130_set_control(client, APDS9130_PDRVIE_100MA|APDS9130_PRX_IR_DIOD|APDS9130_PGAIN_4X);
#endif
	if (err < 0) return err;

	err = apds9130_set_pilt(client, 1023);		// to force first Near-to-Far interrupt
	if (err < 0) return err;

	err = apds9130_set_piht(client, 0);
	if (err < 0) return err;

	err = apds9130_set_pers(client, APDS9130_PPERS_2);	// 2 consecutive Interrupt persistence
	if (err < 0) return err;

	// sensor is in disabled mode but all the configurations are preset

	return 0;
}

/*
 * I2C init/probing/exit functions
 */

static struct i2c_driver apds9130_driver;
static int __devinit apds9130_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct apds9130_data *data;
	int err = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE)) {
		err = -EIO;
		goto exit;
	}

	data = kzalloc(sizeof(struct apds9130_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}

	data->client = client;
	apds9130_i2c_client = client;

	i2c_set_clientdata(client, data);

	data->enable = 0;	/* default mode is standard */
	data->ps_detection = 0;	/* default to no detection */
	data->enable_ps_sensor = 0;	// default to 0
	data->ps_poll_delay = 100;	// 100ms
    data->add_to_cross_talk = ADD_TO_CROSS_TALK;
    data->sub_from_ps_threshold = SUB_FROM_PS_THRESHOLD;

    if(stored_cross_talk >=0 && stored_cross_talk <= 720)
        data->cross_talk = stored_cross_talk;
    else
        data->cross_talk = DEFAULT_CROSS_TALK;

	data->avg_cross_talk = stored_cross_talk;
	data->ps_threshold = data->cross_talk + data->add_to_cross_talk;
	data->ps_hysteresis_threshold = data->ps_threshold - data->sub_from_ps_threshold;
	
    printk("[ProximitySensor] %s : stored_cross_talk = %d, data->cross_talk = %d\n[ProximitySensor] %s : %d < ps_data < %d\n",
                                            __func__, stored_cross_talk, data->cross_talk,
                                            __func__, data->ps_hysteresis_threshold, data->ps_threshold);

    data->irq_wake = 0;

	mutex_init(&data->update_lock);

	err = gpio_request(APDS9130_INT, "apds9130_irq");
	if(err) {
        printk("[ProximitySensor_E] %s : gpio request fail\n", __func__);
		goto exit_kfree;
	}
	gpio_direction_input(APDS9130_INT);
	data->irq = gpio_to_irq(APDS9130_INT);

	if (request_irq(data->irq, apds9130_interrupt, IRQF_DISABLED|IRQ_TYPE_EDGE_FALLING,
		APDS9130_DRV_NAME, (void *)client)) {
		printk("[ProximitySensor_E] %s : Could not allocate APDS9130_IRQ !\n", __func__);
	
		goto exit_kfree;
	}

//	irq_set_irq_wake(data->irq, 1);

	INIT_DELAYED_WORK(&data->dwork, apds9130_work_handler);
	INIT_DELAYED_WORK(&data->ps_dwork, apds9130_ps_polling_work_handler);

	/* Initialize the APDS-9130 chip */
	err = apds9130_init_client(client);
	if (err)
		goto exit_kfree;

	data->input_dev_ps = input_allocate_device();
	if (!data->input_dev_ps) {
		err = -ENOMEM;
		printk("[ProximitySensor_E] %s : Failed to allocate input device ps !\n", __func__);
		goto exit_free_irq;
	}
	
	set_bit(EV_ABS, data->input_dev_ps->evbit);

	input_set_abs_params(data->input_dev_ps, ABS_DISTANCE, 0, 1, 0, 0);

	data->input_dev_ps->name = "Avago proximity sensor";

	err = input_register_device(data->input_dev_ps);
	if (err) {
		err = -ENOMEM;
		printk("[ProximitySensor_E] %s : Unable to register input device ps(%s)\n",
                __func__, data->input_dev_ps->name);
		goto exit_free_dev_ps;
	}

	/* Register sysfs hooks */
	err = sysfs_create_group(&client->dev.kobj, &apds9130_attr_group);
	if (err)
		goto exit_unregister_dev_ps;

	printk("[ProximitySensor] %s : support ver. %s enabled\n", __func__, DRIVER_VERSION);

	wake_lock_init(&data->wakelock, WAKE_LOCK_SUSPEND, data->input_dev_ps->name);

	return 0;

exit_unregister_dev_ps:
	input_unregister_device(data->input_dev_ps);	
exit_free_dev_ps:
exit_free_irq:
	free_irq(data->irq, client);
exit_kfree:
	kfree(data);
exit:
	return err;
}

static int __devexit apds9130_remove(struct i2c_client *client)
{
	struct apds9130_data *data = i2c_get_clientdata(client);

	/* Power down the device */
	apds9130_set_enable(client, 0);

	sysfs_remove_group(&client->dev.kobj, &apds9130_attr_group);

	input_unregister_device(data->input_dev_ps);
	
	free_irq(data->irq, client);

	wake_lock_destroy(&data->wakelock);

	kfree(data);

	return 0;
}

#ifdef CONFIG_PM

static int apds9130_suspend(struct i2c_client *client, pm_message_t mesg)
{
    struct apds9130_data *data = i2c_get_clientdata(client);

    printk("[ProximitySensor] %s [%x][%d]\n", __func__, data->enable, data->irq_wake);

    disable_irq(data->irq);
    if( !enable_irq_wake(data->irq) )
        data->irq_wake = 1;

    flush_delayed_work_sync(&data->dwork);

    if( data->enable & 0x20 ) {
        apds9130_set_enable(client, 0x2D);
    } else {
        apds9130_set_enable(client, 0);
    }

	return 0;
}

static int apds9130_resume(struct i2c_client *client)
{
    struct apds9130_data *data = i2c_get_clientdata(client);

    printk("[ProximitySensor] %s [%x][%d]\n", __func__, data->enable, data->irq_wake);

    if(data->irq_wake) {
        disable_irq_wake(data->irq);
        data->irq_wake = 0;
    }
    enable_irq(data->irq);

	return 0;
}

#else

#define apds9130_suspend	NULL
#define apds9130_resume		NULL

#endif /* CONFIG_PM */

static const struct i2c_device_id apds9130_id[] = {
	{ "apds9130", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, apds9130_id);

static struct i2c_driver apds9130_driver = {
	.driver = {
		.name	= APDS9130_DRV_NAME,
		.owner	= THIS_MODULE,
	},
	.suspend = apds9130_suspend,
	.resume	= apds9130_resume,
	.probe	= apds9130_probe,
	.remove	= __devexit_p(apds9130_remove),
	.id_table = apds9130_id,
};

static int __init apds9130_init(void)
{
	apds9130_workqueue = create_workqueue("proximity");
	
	if (!apds9130_workqueue)
		return -ENOMEM;

	return i2c_add_driver(&apds9130_driver);
}

static void __exit apds9130_exit(void)
{
	if (apds9130_workqueue)
		destroy_workqueue(apds9130_workqueue);

	apds9130_workqueue = NULL;

	i2c_del_driver(&apds9130_driver);
}

MODULE_AUTHOR("Lee Kai Koon <kai-koon.lee@avagotech.com>");
MODULE_DESCRIPTION("APDS9130 proximity sensor driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);

module_init(apds9130_init);
module_exit(apds9130_exit);

