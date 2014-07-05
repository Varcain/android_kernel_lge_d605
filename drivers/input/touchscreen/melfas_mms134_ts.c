/* drivers/input/touchscreen/melfas_ts.c
 *
 * Copyright (C) 2010 Melfas, Inc.
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

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/i2c/melfas_ts.h>
#include <linux/gpio.h>

#ifdef CONFIG_TS_INFO_CLASS
#include "ts_class.h"
#endif

#define TOUCH_NOISE_MODE
#if defined(TOUCH_NOISE_MODE) && defined(CONFIG_LGE_PM)
#include <linux/mfd/pm8xxx/pm8921-charger.h>
#endif

#include "MMS134S_ISC_Updater.h"

#define MODE_CONTROL                    	0x01
#define TS_READ_START_ADDR              	0x10

#define TS_READ_VERSION_ADDR			0xF0

#define TS_READ_CHIP_VENDOR_ID1_ADDR		0xC0
#define TS_READ_CHIP_VENDOR_ID2_ADDR		0xC1
#define TS_READ_HW_VER_ADDR				0xC2
#define TS_READ_FW_VER_ADDR				0xE2
#define TS_READ_CONFIG_VER_ADDR			0xE3

#define UNIVERSAL_CMD				0xA0
#define UNIVERSAL_CMD_RESULT_SIZE		0xAE
#define UNIVERSAL_CMD_RESULT			0xAF
#define UNIVCMD_ENTER_TEST_MODE			0x40
#define UNIVCMD_TEST_CM_DELTA			0x41
#define UNIVCMD_GET_PIXEL_CM_DELTA		0x42
#define UNIVERSAL_CMD_EXIT			0x4F

#define TS_READ_REGS_LEN			100
#define TS_READ_VERSION_INFO_LEN		6

#define MELFAS_MAX_TOUCH			7  /* ts->pdata->num_of_finger */
#define MELFAS_MAX_BTN				4
#define MELFAS_PACKET_SIZE			6


unsigned short MELFAS_binary_nLength;
unsigned char *MELFAS_binary;
int MELFAS_binary_fw_ver;
extern unsigned short MELFAS_binary_nLength_1;
extern unsigned char MELFAS_binary_1[];
extern int MELFAS_binary_fw_ver_1;
extern unsigned short MELFAS_binary_nLength_2;
extern unsigned char MELFAS_binary_2[];
extern int MELFAS_binary_fw_ver_2;

#define I2C_RETRY_CNT				10

#define PRESS_KEY				1
#define RELEASE_KEY				0

#define SET_DOWNLOAD_BY_GPIO			1

#define MIP_INPUT_EVENT_PACKET_SIZE		0x0F
#define MIP_INPUT_EVENT_INFORMATION		0x10

#define FW_VERSION_ADDR 	2
#define TS_MAKER_ID         68

#if defined(CONFIG_MACH_LGE_F6_TMUS)
#define ISIS_L2
#endif

#define get_time_interval(a, b) a >= b ? a-b : 1000000+a-b
struct timeval t_debug[2];

static volatile int init_values[20];
static volatile int irq_flag;
static volatile int tmp_flag[10];
static volatile int point_of_release;
static volatile int time_diff;
static volatile int pre_keycode;
static volatile int touch_prestate;
static volatile int btn_prestate;

static int num_tx_line = 20; //default value for d1lu,k,sk,v
static int num_rx_line = 12; //default value for d1lu,k,sk,v

#define DEBUG_TOUCH_IRQ
#define TOUCH_RETRY_I2C
#if defined(TOUCH_NOISE_MODE) && defined(CONFIG_LGE_PM)
static int ac_change;
static int is_noise_flag;
#endif

#define TOUCH_RESET_DELAY 100
static int touch_reset_cnt = 0;

/*                                                                               */
#define TOUCH_GHOST_DETECTION
#ifdef TOUCH_GHOST_DETECTION
static int ghost_touch_cnt = 0;
static int ghost_x = 1000;
static int ghost_y = 2000;
#endif
/*                                                                               */

/***************************************************************************
 * Debug Definitions
 ***************************************************************************/
enum {
	MELFAS_TS_DEBUG_PROBE = 1U << 0,
	MELFAS_TS_DEBUG_KEY_EVENT = 1U << 1,
	MELFAS_TS_DEBUG_TOUCH_EVENT = 1U << 2,
	MELFAS_TS_DEBUG_TOUCH_EVENT_ONETIME = 1U << 3,
	MELFAS_TS_DEBUG_EVENT_HANDLER = 1U << 4,
	MELFAS_TS_DEBUG_IRQ_HANDLER = 1U << 5,
	MELFAS_TS_DEBUG_TIME = 1U << 6,
};

static int melfas_ts_debug_mask = 9;

module_param_named(
	debug_mask, melfas_ts_debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP
);
#define MELFAS_TS_DPRINTK(mask, level, message, ...) \
	do { \
		if ((mask) & melfas_ts_debug_mask) \
			printk(level message, ## __VA_ARGS__); \
	} while (0)

#define MELFAS_TS_DEBUG_PRINT_TOUCH_EVENT(temp) \
	do { \
		MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_TOUCH_EVENT, KERN_INFO, \
			"[TOUCH] %s   %d : x=%d y=%d p=%d \n", \
			temp, i, g_Mtouch_info[i].posX, g_Mtouch_info[i].posY, g_Mtouch_info[i].pressure); \
		if (tmp_flag[i] == 1) { \
			MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_TOUCH_EVENT_ONETIME, KERN_INFO, \
			"[TOUCH] %s   %d : x=%d y=%d p=%d \n", \
			temp, i, g_Mtouch_info[i].posX, g_Mtouch_info[i].posY, g_Mtouch_info[i].pressure); \
			if (!strcmp (temp, "Press")) \
				tmp_flag[i] = 0;\
		} \
	} while (0)

#define MELFAS_TS_PRIVATE_PRINT_TOUCH_EVENT(temp) \
	do { \
		MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_TOUCH_EVENT, KERN_INFO, \
			"[TOUCH] %s   %d : x=### y=### p=%d \n", \
			temp, i, g_Mtouch_info[i].pressure); \
		if (tmp_flag[i] == 1) { \
			MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_TOUCH_EVENT_ONETIME, KERN_INFO, \
			"[TOUCH] %s   %d : x=### y=### p=%d \n", \
			temp, i, g_Mtouch_info[i].pressure); \
			if (!strcmp (temp, "Press")) \
				tmp_flag[i] = 0;\
		} \
	} while (0)

#define MELFAS_TS_DEBUG_PRINT_TIME() \
	do { \
		if (MELFAS_TS_DEBUG_TIME & melfas_ts_debug_mask) { \
			if (t_debug[0].tv_sec == 0	&& t_debug[0].tv_usec == 0) { \
				t_debug[0].tv_sec = t_debug[1].tv_sec; \
				t_debug[0].tv_usec = t_debug[1].tv_usec; \
			} else { \
				printk("Interrupt interval: %6luus\n", get_time_interval(t_debug[1].tv_usec, t_debug[0].tv_usec)); \
				t_debug[0].tv_sec = t_debug[1].tv_sec; \
				t_debug[0].tv_usec = t_debug[1].tv_usec; \
			} \
		} \
	} while (0)

/**************************************************************************/

enum {
	None = 0,
	TOUCH_SCREEN,
	TOUCH_KEY
};

struct muti_touch_info {
	int strength;
	int width;
	int posX;
	int posY;
	int pressure;
	int btn_touch;
};

struct btn_info {
	int key_code;
	int status;
};

struct melfas_ts_data {
#ifdef CONFIG_TS_INFO_CLASS
	struct ts_info_classdev cdev;
#endif
	uint16_t addr;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct melfas_tsi_platform_data *pdata;
	struct work_struct work;
	uint32_t flags;
	struct work_struct upgrade_work;
#ifdef DEBUG_TOUCH_IRQ
	struct delayed_work irq_restore_work;
#endif
#ifdef TOUCH_GHOST_DETECTION
	struct delayed_work ghost_monitor_work;
#endif

#if defined(CONFIG_USING_INNOTEK_PANEL_4_7)
	char group_version[10];
#endif
	int version;
	struct early_suspend early_suspend;
};

struct workqueue_struct*	touch_wq;

#ifdef CONFIG_HAS_EARLYSUSPEND
static void melfas_ts_early_suspend(struct early_suspend *h);
static void melfas_ts_late_resume(struct early_suspend *h);
#endif

static void release_all_fingers(struct melfas_ts_data *ts);
static void melfas_ts_sw_reset(struct melfas_ts_data *ts);

#if defined(TOUCH_NOISE_MODE) && defined(CONFIG_LGE_PM)
static void mms_set_noise_mode(struct melfas_ts_data *ts);
#endif

#if defined(TOUCH_NOISE_MODE)
static void monitor_ghost_finger(struct work_struct *work);
#endif

/*                                                
                                                         
  */
int touch_i2c_read(unsigned char *buf, unsigned char reg, int len);
int touch_i2c_write(unsigned char * buf, unsigned char reg, int len);
int touch_i2c_read_nlength(unsigned char *buf, int len);
int melfas_mms134_fw_upgrade(const char* fw_path);
/*                                                         */

#ifdef DEBUG_TOUCH_IRQ
static void melfas_ts_restore_irq(struct work_struct *work);
int touch_irq_cnt = 0;
int touch_before_irq_cnt = 100;
int is_request_irq = 0;
#endif

static struct melfas_ts_data mms134_ts_data;
static struct muti_touch_info g_Mtouch_info[MELFAS_MAX_TOUCH];
static struct btn_info g_btn_info[MELFAS_MAX_BTN];

static struct i2c_client *fw_client;

static int check_abs_time(void)
{
	time_diff = 0;

	if (!point_of_release)
		return 0;

	time_diff = jiffies_to_msecs(jiffies) - point_of_release;
	if (time_diff > 0)
		return time_diff;
	else
		return 0;
}
/*                                                
                                                                                         
  */
int melfas_touch_i2c_read(unsigned char* buf, unsigned char reg, int len){
	int ret;
	if(fw_client != NULL){
		ret = touch_i2c_read(buf, reg, len);
		return ret;
	}else{
		printk(KERN_ERR "[Touch D] %s : fw_client is null!\n",__func__);
		return -1;
	}
	return 0;
}

int melfas_touch_i2c_write(unsigned char* buf, unsigned char reg, int len){
	int ret;
	if(fw_client != NULL){
		ret = touch_i2c_write(buf, reg, len);
		return ret;
	}else{
		printk(KERN_ERR "[Touch D] %s : fw_client is null!\n",__func__);
		return -1;
	}
	return 0;
}

int melfas_touch_i2c_read_len(unsigned char* buf, int len){
	int ret;
	if(fw_client != NULL){
		ret = touch_i2c_read_nlength(buf, len);
		return ret;
	}else{
		printk(KERN_ERR "[Touch D] %s : fw_client is null!\n",__func__);
		return -1;
	}
	return 0;
}

int touch_i2c_read(unsigned char *buf, unsigned char reg, int len)
{
	struct i2c_msg msgs[] = {
		{
			.addr = fw_client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg,
		},
		{
			.addr = fw_client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf,
		},
	};

	if (i2c_transfer(fw_client->adapter, msgs, 2) < 0) {
		if (printk_ratelimit())
			printk(KERN_ERR "[Touch E] I2C transfer error\n");
		return -EIO;
	} else
		return 0;
}

int touch_i2c_read_nlength(unsigned char *buf, int len)
{
	if(i2c_master_recv(fw_client, buf, len) < 0){
		if (printk_ratelimit())
			printk(KERN_ERR "[Touch E] I2C_master_recv error\n");
		return -EIO;
	} else
		return 0;
} 

int touch_i2c_write(unsigned char * buf, unsigned char reg, int len)
{
	unsigned char send_buf[len + 1];
	struct i2c_msg msgs[] = {
		{
			.addr = fw_client->addr,
			.flags = fw_client->flags,
			.len = len+1,
			.buf = send_buf,
		},
	};

	send_buf[0] = (unsigned char)reg;
	memcpy(&send_buf[1], buf, len);

	if (i2c_transfer(fw_client->adapter, msgs, 1) < 0) {
		if (printk_ratelimit())
			printk(KERN_ERR "[Touch E] I2C transfer error\n");
		return -EIO;
	} else
		return 0;
}

static void read_dummy_interrupt(void)
{
	uint8_t buf[TS_READ_REGS_LEN] = {0,};
	uint8_t read_num = 0;
	int ret = 0;
	struct melfas_ts_data *ts = &mms134_ts_data;

	if(ts == NULL)
		return;

	buf[0] = MIP_INPUT_EVENT_PACKET_SIZE;
	ret = i2c_master_send(ts->client, buf , 1);
	ret = i2c_master_recv(ts->client, &read_num, 1);
	
	printk("[Touch] dummy_interrupt_return: %d, %d \n", ret, buf[0]);
	if (ret)
		return;
	read_num = buf[0];

    if(read_num >= TS_READ_REGS_LEN) {
        read_num = TS_READ_REGS_LEN-1;
    }

	if(read_num)
	{
		buf[0] = MIP_INPUT_EVENT_INFORMATION;
		ret = i2c_master_send(ts->client, buf, 1);
		ret = i2c_master_recv(ts->client, &buf[0], read_num);
		
		printk("[Touch] dummy_interrupt_occured : %d, %d \n", ret, buf[0]);
		if(0x0F == buf[0])
			melfas_ts_sw_reset(ts);
	}
    return;
}


/*                                                                                                        */
#define to_delayed_work(_work)  container_of(_work, struct delayed_work, work)

//static void melfas_ts_event_handler(struct melfas_ts_data *ts)
static void melfas_ts_event_handler(struct work_struct *work)
{
	int ret = 0, i;
	uint8_t buf[TS_READ_REGS_LEN];
	int touchType = 0, touchState = 0, touchID = 0, pressed_type = 0;
	int posX = 0, posY = 0, width = 0, strength = 10;
	int keyID = 0, reportID = 0;
	uint8_t read_num = 0, pressed_count = 0;
	static int is_mix_event;
	struct melfas_ts_data *ts = container_of(work, struct melfas_ts_data, work);
	
	MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_EVENT_HANDLER, KERN_INFO,
			"melfas_ts_event_handler \n");

	if (ts == NULL){
		MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_EVENT_HANDLER, KERN_ERR,
				"melfas_ts_event_handler TS is NULL\n");
		printk(KERN_ERR "[Touch] ts is null \n");
		goto err_ts_null;
	}

	buf[0] = MIP_INPUT_EVENT_PACKET_SIZE;
	ret = i2c_master_send(ts->client, buf, 1);
#ifdef TOUCH_RETRY_I2C
		if(ret < 0){
			// i2c fail - retry, add 10 msec delay
			printk(KERN_ERR "[Touch] %s : first i2c fail, %d\n",__func__, ret);
			mdelay(10);
			ret = i2c_master_send(ts->client, buf, 1);
			if(ret < 0)
				printk(KERN_ERR "[Touch] %s, MIP_INPUT_EVENT_PACKET_SIZE, I2C master send fail : %d\n", __func__, ret);
		}
#endif

	ret = i2c_master_recv(ts->client, &read_num, 1);
	/* touch ic reset for ESD defense  */
	if (ret < 0) {
		melfas_ts_sw_reset(ts);
		goto err_free_irq;
	}

	if (read_num == 0) {
		MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_EVENT_HANDLER, KERN_ERR,
				"melfas_ts_event_handler: read number 0 \n");
		printk(KERN_ERR "[Touch] read number is 0 \n");
		goto err_free_irq;
	} else if (read_num > MELFAS_MAX_TOUCH*MELFAS_PACKET_SIZE) {
		MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_EVENT_HANDLER, KERN_ERR,
				"melfas_ts_event_handler: read number is out of range\n");
		printk(KERN_ERR "[Touch] read number is out of range : %d\n", read_num);
		goto err_free_irq;
	}

	buf[0] = MIP_INPUT_EVENT_INFORMATION;
	ret = i2c_master_send(ts->client, buf, 1);
#ifdef TOUCH_RETRY_I2C
	if(ret < 0){
		// i2c fail - retry, add 10 msec delay
		printk(KERN_ERR "[Touch] %s : first i2c fail, %d\n",__func__, ret);
		mdelay(10);
		ret = i2c_master_send(ts->client, buf, 1);
		if(ret < 0)
			printk(KERN_ERR "[Touch] %s, INPUT_INFORMATION, I2C master send fail : %d\n", __func__, ret);
	}
#endif
	ret = i2c_master_recv(ts->client, &buf[0], read_num);

	/* touch ic reset for ESD defense
	     if reportID is -0x0F, meflas touch IC need sw reset */
	reportID = (buf[0] & 0x0F);
	
#if	defined(TOUCH_NOISE_MODE) && defined(CONFIG_LGE_PM)
	// not support noise mode f/w version less than 3
	if(ts->version > 3){
		if(reportID == 0x0E){
			// check TA connection
			ac_change = pm8921_charger_is_ta_connected();
			if(ac_change){
				printk(KERN_ERR "[Touch] Noise Packet : 0x0E\n");
				is_noise_flag = 1;		
			}
			else{
				is_noise_flag = 0;
			}	
		}

		if(is_noise_flag){
			ac_change = pm8921_charger_is_ta_connected();	
			// if TA is disconnected
			if (ac_change == 0) {	
				uint8_t noise_buf[2];
				noise_buf[0] = 0x30;
				noise_buf[1] = 0x00;
				printk(KERN_INFO "[TOUCH] ta is disconnected!!, noise mode unset Send 0x30, 0x00\n");
				i2c_master_send(ts->client, noise_buf, 2);	

				is_noise_flag = 0;
			}
		}
	}
#endif

	if (reportID == 0x0F) {
		printk(KERN_ERR "[Touch] ESD 0x0F : ");
		melfas_ts_sw_reset(ts);
		goto err_free_irq;
	}

	touch_reset_cnt = 0;

	for (i = 0; i < read_num; i = i + 6) {
		if (ret < 0) {
			MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_EVENT_HANDLER, KERN_ERR,
				"melfas_ts_event_handler: i2c failed\n");
			goto err_free_irq;
		} else {
			touchType  =  ((buf[i] & 0x60) >> 5);				/* Touch Screen, Touch Key */
			touchState = ((buf[i] & 0x80) == 0x80);				/* touchAction = (buf[0]>>7)&&0x01;*/
			reportID = (buf[i] & 0x0F);					/* Touch Screen -> n.th finger input
											Touch Key -> n.th touch key area. */
			posX = (uint16_t) (buf[i + 1] & 0x0F) << 8 | buf[i + 2];	/* X position (0 ~ 4096, 12 bit) */
			posY = (uint16_t) (buf[i + 1] & 0xF0) << 4 | buf[i + 3];	/* Y position (0 ~ 4096, 12 bit) */
			width = buf[i + 4];
			strength = buf[i + 5];

			//printk(KERN_ERR "[Touch] posX : %d, posY : %d, width: %d, strength: %d, reportID : %d, touchType : %d, touchState: %d\n", posX, posY, width, strength, reportID, touchType, touchState);

			if(touchType == TOUCH_KEY && (reportID > 2 || reportID <= 0)){
				printk(KERN_ERR "[Touch] %d th Invalid Key ReportID  : %d from TOUCH IC!!!\n",i , reportID);
				goto INVALID_PACKET;
			}

			if(touchType== TOUCH_SCREEN && (reportID > 5 || reportID <= 0)){
				printk(KERN_ERR "[Touch] %d th Invalid Screen ReportID  : %d from TOUCH IC!!!\n",i , reportID);
				goto INVALID_PACKET;
			}

			if(touchType > TOUCH_KEY || touchType <= 0){
				printk(KERN_ERR "[Touch] %d th Invalid touchType : %d from TOUCH IC!!!\n",i , touchType);
				goto INVALID_PACKET;
			}

			if (touchType == TOUCH_KEY)
				keyID = reportID;
			else if (touchType == TOUCH_SCREEN) {
				touchID = reportID-1;
				pressed_type = TOUCH_SCREEN;
			}

			if (touchID > ts->pdata->num_of_finger-1)
				goto err_free_irq;

			if (touchType == TOUCH_SCREEN) {
				g_Mtouch_info[touchID].posX = posX;
				g_Mtouch_info[touchID].posY = posY;

				//printk(KERN_ERR "[Touch D] posX : %d, posY : %d\n",posX,posY);
				
				g_Mtouch_info[touchID].width = width;

				g_Mtouch_info[touchID].strength = strength;
				g_Mtouch_info[touchID].pressure = strength;
				g_Mtouch_info[touchID].btn_touch = touchState;
/*                                                                               */
#if defined(TOUCH_GHOST_DETECTION)
				if(touchState == 0){
					if(ghost_touch_cnt == 0)
					{
						ghost_x = g_Mtouch_info[touchID].posX;
						ghost_y = g_Mtouch_info[touchID].posY;
						ghost_touch_cnt++;
					}
					else
					{
						if(ghost_x + 40 >= g_Mtouch_info[touchID].posX && ghost_x - 40 <= g_Mtouch_info[touchID].posX)
							if(ghost_y + 40 >= g_Mtouch_info[touchID].posY && ghost_y - 40 <= g_Mtouch_info[touchID].posY)
								ghost_touch_cnt++;
					}
				}
#endif
/*                                                                               */

				if (btn_prestate && touch_prestate == 0) {
					input_report_key(ts->input_dev, pre_keycode, 0xff);

					if(pre_keycode == KEY_BACK) printk(KERN_ERR "[Touch] BACK key canceled \n");
					else if(pre_keycode == KEY_MENU) printk(KERN_ERR "[Touch] MENU key canceled \n");
					else printk(KERN_ERR "[Touch] %d key canceled \n", pre_keycode);
						
					btn_prestate = 0;
				}
			} else if (touchType == TOUCH_KEY) {
				g_btn_info[keyID].key_code = ts->pdata->button[keyID-1];
				g_btn_info[keyID].status = touchState;

				if (keyID > ts->pdata->num_of_button || keyID == 0) {
					MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_EVENT_HANDLER, KERN_ERR, "Touchkey ID error \n");
				} else if (is_mix_event == 1) {
					input_report_key(ts->input_dev, pre_keycode, 0xff);

					if(pre_keycode == KEY_BACK) printk(KERN_ERR "[Touch] BACK key canceled \n");
					else if(pre_keycode == KEY_MENU) printk(KERN_ERR "[Touch] MENU key canceled \n");
					else printk(KERN_ERR "[Touch] %d key canceled \n", pre_keycode);
					
					is_mix_event = 0;
					btn_prestate = touchState;
				} else{
					if (touch_prestate) {
						btn_prestate = touchState;
					} else if (check_abs_time() > 0 && check_abs_time() < 100) {
						btn_prestate = touchState;
						point_of_release = 0;
					} else if (btn_prestate != touchState) {
						if (touchState == PRESS_KEY) {
							pre_keycode = ts->pdata->button[keyID-1];
							
							input_report_key(ts->input_dev, ts->pdata->button[keyID-1], PRESS_KEY);
#ifdef ISIS_L2
							if(ts->pdata->button[keyID-1] == KEY_BACK) printk(KERN_ERR "[Touch] ## key pressed \n");
							else if(ts->pdata->button[keyID-1] == KEY_MENU) printk(KERN_ERR "[Touch] ## key pressed \n");
							else printk(KERN_ERR "[Touch] ## key pressed \n");
#else
							if(ts->pdata->button[keyID-1] == KEY_BACK) printk(KERN_ERR "[Touch] BACK key pressed \n");
							else if(ts->pdata->button[keyID-1] == KEY_MENU) printk(KERN_ERR "[Touch] MENU key pressed \n");
							else printk(KERN_ERR "[Touch] %d key pressed \n", pre_keycode);
#endif
						} else {
							
							input_report_key(ts->input_dev, ts->pdata->button[keyID-1], RELEASE_KEY);
#ifdef ISIS_L2
							if(ts->pdata->button[keyID-1] == KEY_BACK) printk(KERN_ERR "[Touch] ## key released \n");
							else if(ts->pdata->button[keyID-1] == KEY_MENU) printk(KERN_ERR "[Touch] ## key released \n");
							else printk(KERN_ERR "[Touch] ## key released \n");
#else
							if(ts->pdata->button[keyID-1] == KEY_BACK) printk(KERN_ERR "[Touch] BACK key released \n");
							else if(ts->pdata->button[keyID-1] == KEY_MENU) printk(KERN_ERR "[Touch] MENU key released \n");
							else printk(KERN_ERR "[Touch] %d key released \n", pre_keycode);
#endif
						}
						btn_prestate = touchState;
					}
				}

				if ((read_num > 6) && (pressed_type == TOUCH_SCREEN)) {
					if (touchState && (touch_prestate == 0))
						is_mix_event = 1;
					touchType = TOUCH_SCREEN;
				}

				MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_KEY_EVENT, KERN_INFO,
					"melfas_ts_event_handler: keyID : %d, touchState: %d\n",
					keyID, touchState);
				break;
			}

		}
	}

	if (touchType == TOUCH_SCREEN) {
		for (i = 0; i < ts->pdata->num_of_finger; i++) {
			if (g_Mtouch_info[i].btn_touch == -1)
				continue;

			if (g_Mtouch_info[i].btn_touch == 0) {
				g_Mtouch_info[i].btn_touch = -1;
				tmp_flag[i] = 1;
#ifdef ISIS_L2
				MELFAS_TS_PRIVATE_PRINT_TOUCH_EVENT("Release");
#else
				MELFAS_TS_DEBUG_PRINT_TOUCH_EVENT("Release");
#endif
				continue;
			}
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X, g_Mtouch_info[i].posX);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, g_Mtouch_info[i].posY);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, g_Mtouch_info[i].width);
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE, g_Mtouch_info[i].strength);
			input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, i);
			input_mt_sync(ts->input_dev);
#ifdef ISIS_L2
			MELFAS_TS_PRIVATE_PRINT_TOUCH_EVENT("Press");
#else
			MELFAS_TS_DEBUG_PRINT_TOUCH_EVENT("Press");
#endif
			touch_prestate = 1;
			pressed_count++;
		}
		if (pressed_count == 0) {
			input_mt_sync(ts->input_dev);
			touch_prestate = 0;
			point_of_release = jiffies_to_msecs(jiffies);
		}
	}
	input_sync(ts->input_dev);


#ifdef DEBUG_TOUCH_IRQ
	if(touch_irq_cnt <= 20000)
		touch_irq_cnt++;
	else 
		touch_irq_cnt = 0;
#endif

	MELFAS_TS_DEBUG_PRINT_TIME();
	enable_irq(ts->client->irq);
	return;
err_free_irq:
	enable_irq(ts->client->irq);
	return;
err_ts_null:	
	return;
INVALID_PACKET:
	release_all_fingers(ts);
	enable_irq(ts->client->irq);
	return;
}

static irqreturn_t melfas_ts_irq_handler(int irq, void *handle)
{
	struct melfas_ts_data *ts = (struct melfas_ts_data *)handle;

	irq_flag = 1;

	MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_IRQ_HANDLER, KERN_INFO, "melfas_ts_irq_handler\n");

	if (MELFAS_TS_DEBUG_TIME & melfas_ts_debug_mask)
	    do_gettimeofday(&t_debug[1]);

#ifdef DEBUG_TOUCH_IRQ
	if(is_request_irq){
#endif
		disable_irq_nosync(ts->client->irq);
		queue_work(touch_wq, &ts->work);
		//schedule_delayed_work(&ts->work, 0);
		//melfas_ts_event_handler(ts);
#ifdef DEBUG_TOUCH_IRQ
	}
#endif

	irq_flag = 0;
	return IRQ_HANDLED;
}


#ifdef DEBUG_TOUCH_IRQ
static void melfas_ts_restore_irq(struct work_struct *work){
	struct melfas_ts_data *ts = &mms134_ts_data;
	
	//printk(KERN_ERR "[Touch] Restore_irq entered!\n");
	if(ts == NULL){
		printk(KERN_ERR "[Touch] TS is null! : restore_irq \n");
		return;
	}

	if(gpio_get_value(11) == 0){
		// interrupt pin is low
		printk(KERN_DEBUG "[Touch] Touch inetrrupt pin is low. before = %d, current = %d, 3s\n", touch_before_irq_cnt, touch_irq_cnt);
		if(touch_irq_cnt == touch_before_irq_cnt){
			// touch irq handler is not operated - recovery irq
			int ret = 0;
			is_request_irq = 0;
			printk(KERN_ERR "[Touch] Touch inetrrupt pin is low during 3 seconds over. before = %d, current = %d\n", touch_before_irq_cnt, touch_irq_cnt);
			free_irq(ts->client->irq, ts);

			ts->pdata->power_enable(0, true);
			msleep(20);
			ts->pdata->power_enable(1, true);

			msleep(100);
			ret = request_threaded_irq(ts->client->irq, NULL, melfas_ts_irq_handler, IRQF_ONESHOT | IRQF_TRIGGER_LOW, ts->client->name, ts);
			if (ret > 0) {
				printk(KERN_ERR "[TOUCH] Restore_irq : Can't allocate irq %d, ret %d\n", ts->client->irq, ret);
				ret = -EBUSY;
			}
			// initialize variable
			touch_before_irq_cnt = touch_irq_cnt + 100;
			is_request_irq = 1;
		}
		else{
			// normal case
			if(touch_irq_cnt <= 20000)
				touch_irq_cnt++;
			else 
				touch_irq_cnt = 0;
			
			touch_before_irq_cnt = touch_irq_cnt;
		}
	}
	else{
		// interrupt pin is high - no action
	}
	//printk(KERN_ERR "[Touch] Restore_irq finished!\n");
	//schedule_delayed_work(&ts->irq_restore_work, msecs_to_jiffies(HZ * 30)); // 3 sec
	queue_delayed_work(touch_wq, &ts->irq_restore_work, msecs_to_jiffies(HZ * 30));
}

#endif

static ssize_t
mms136_fw_read_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	uint8_t verbuf[TS_READ_VERSION_INFO_LEN];
	int len,ret;

	verbuf[0] = TS_READ_VERSION_ADDR;
	i2c_master_send(ts->client, &verbuf[0], 1);
	i2c_master_recv(ts->client, &verbuf[0], TS_READ_VERSION_INFO_LEN);
	ts->version = verbuf[FW_VERSION_ADDR];

	if (ts -> version != ts->pdata->fw_ver) {
//		ts->pdata->power_enable(0, true);
		mdelay(50);

		free_irq(ts->client->irq, ts);

		//mms100_download(isp_sys_type, embedded_img);
		ts->pdata->power_enable(0, true);
		msleep(20);
		ts->pdata->power_enable(1, true);

		msleep(100);
		ret = request_threaded_irq(ts->client->irq, NULL, melfas_ts_irq_handler, IRQF_ONESHOT | IRQF_TRIGGER_LOW, ts->client->name, ts);
		if (ret > 0) {
			MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_ERR,
				"[TOUCH] Can't allocate irq %d, ret %d\n", ts->client->irq, ret);
			ret = -EBUSY;
		}
	}

	verbuf[0] = TS_READ_VERSION_ADDR;
	ret = i2c_master_send(ts->client, &verbuf[0], 1);
	ret = i2c_master_recv(ts->client, &verbuf[0], TS_READ_VERSION_INFO_LEN);
	ts->version = verbuf[FW_VERSION_ADDR];

	len = snprintf(buf, PAGE_SIZE, "%d\n", ts->version);

	return len;
}

static ssize_t
mms136_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	uint8_t verbuf[TS_READ_VERSION_INFO_LEN];
	int len;

	verbuf[0] = TS_READ_CONFIG_VER_ADDR;
	i2c_master_send(ts->client, &verbuf[0], 1);
	i2c_master_recv(ts->client, &verbuf[0], 1);

	ts->version = verbuf[0];

	len = snprintf(buf, PAGE_SIZE, "%x\n", ts->version);
	return len;
}

static ssize_t
mms136_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	int len;
	len = snprintf(buf, PAGE_SIZE, "\nMMS-134S Device Status\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "=============================\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "irq num       is %d\n", ts->client->irq);
	len += snprintf(buf + len, PAGE_SIZE - len, "gpio_irq num  is %d(level=%d)\n", ts->pdata->i2c_int_gpio, gpio_get_value(ts->pdata->i2c_int_gpio));
	len += snprintf(buf + len, PAGE_SIZE - len, "gpio_scl num  is %d\n", ts->pdata->gpio_scl);
	len += snprintf(buf + len, PAGE_SIZE - len, "gpio_sda num  is %d\n", ts->pdata->gpio_sda);
	len += snprintf(buf + len, PAGE_SIZE - len, "gpio_en  num  is %d\n", ts->pdata->gpio_ldo);
	len += snprintf(buf + len, PAGE_SIZE - len, "Power status  is %s\n", gpio_get_value(ts->pdata->gpio_ldo) ? "on" : "off");
	return len;
}

static ssize_t
mms134_store_fw_upgrade(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	int cmd, i = 1, ret;
	uint8_t verbuf[TS_READ_VERSION_INFO_LEN];
	uint8_t read_num = 0;
	uint8_t databuf[TS_READ_REGS_LEN];
	char path[256] = {0};
	
	sscanf(buf, "%d %s", &cmd, path);		
	
	printk(KERN_INFO "\n");
	printk(KERN_INFO "[Touch] Firmware image path: %s\n", path[0] != 0 ? path : "Internal");

	switch (cmd) {
		/* embedded firmware image */
	case 1:
		ts->pdata->power_enable(1, true);
		mdelay(100);
		if (irq_flag == 0) {
			printk("disable_irq_nosync\n");
			disable_irq_nosync(ts->client->irq);
			irq_flag = 1;
		}
		do {
			mdelay(100);
			if (i % 10 == 0) {
				databuf[0] = MIP_INPUT_EVENT_PACKET_SIZE;
				ret = i2c_master_send(ts->client, databuf, 1);
				ret = i2c_master_recv(ts->client, &read_num, 1);
				/* touch ic reset for ESD defense  */
				if (ret < 0) {
					melfas_ts_sw_reset(ts);
					continue;
				}

				if (read_num == 0) {
					printk("melfas_ts_event_handler: read number 0 \n");
					continue;
				}
				databuf[0] = MIP_INPUT_EVENT_INFORMATION;
				ret = i2c_master_send(ts->client, databuf, 1);
				ret = i2c_master_recv(ts->client, &databuf[0], read_num);
				mdelay(100);
			}
			i++;
		} while (!gpio_get_value(ts->pdata->i2c_int_gpio));

		melfas_mms134_fw_upgrade(path);
		
		ts->pdata->power_enable(0, true);
		ts->pdata->power_enable(1, true);
		msleep(100);

		verbuf[0] = TS_READ_VERSION_ADDR;
		ret = i2c_master_send(ts->client, &verbuf[0], 1);
		ret = i2c_master_recv(ts->client, &verbuf[0], TS_READ_VERSION_INFO_LEN);

		ts->version = verbuf[FW_VERSION_ADDR];

		if (irq_flag == 1) {
			printk("enable_irq\n");
			enable_irq(ts->client->irq);
			irq_flag = 0;
		}
		break;
		/* external firmware image */
	case 2:
		ts->pdata->power_enable(1, true);
		mdelay(100);
		if (irq_flag == 0) {
			printk("disable_irq_nosync\n");
			disable_irq_nosync(ts->client->irq);
			irq_flag = 1;
		}
		do {
			mdelay(100);
			if (i % 10 == 0) {
				databuf[0] = MIP_INPUT_EVENT_PACKET_SIZE;
				ret = i2c_master_send(ts->client, databuf, 1);
				ret = i2c_master_recv(ts->client, &read_num, 1);
				/* touch ic reset for ESD defense  */
				if (ret < 0) {
					melfas_ts_sw_reset(ts);
					continue;
				}

				if (read_num == 0) {
					printk("melfas_ts_event_handler: read number 0 \n");
					continue;
				}
				databuf[0] = MIP_INPUT_EVENT_INFORMATION;
				ret = i2c_master_send(ts->client, databuf, 1);
				ret = i2c_master_recv(ts->client, &databuf[0], read_num);
				mdelay(100);
			}
			i++;
		} while (!gpio_get_value(ts->pdata->i2c_int_gpio));

		melfas_mms134_fw_upgrade(path);
		
		ts->pdata->power_enable(0, true);
		msleep(20);
		ts->pdata->power_enable(1, true);
		msleep(100);

		verbuf[0] = TS_READ_VERSION_ADDR;
		ret = i2c_master_send(ts->client, &verbuf[0], 1);
		ret = i2c_master_recv(ts->client, &verbuf[0], TS_READ_VERSION_INFO_LEN);

		ts->version = verbuf[FW_VERSION_ADDR];

		if (irq_flag == 1) {
			printk("enable_irq\n");
			enable_irq(ts->client->irq);
			irq_flag = 0;
		}
	break;
	default:
		/* external f/w upgrade usage : echo 1 /sdcard/master.bin > fw_upgrade */
		printk(KERN_INFO "usage: echo [1|2] [optional:external_fw_path] > fw_upgrade\n");
		printk(KERN_INFO "  - 1: firmware upgrade with embedded firmware image\n");
		printk(KERN_INFO "  - 2: firmware upgrade with external firmware image in user area\n");
		break;
	}
	return count;
}

static ssize_t
mms136_power_control_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	int cmd;

	if (sscanf(buf, "%d", &cmd) != 1)
		return -EINVAL;

	switch (cmd) {
	case 1: /* touch power on */
		ts->pdata->power_enable(1, true);
		break;
	case 2: /*touch power off */
		ts->pdata->power_enable(0, true);
		break;
	case 3:
		ts->pdata->power_enable(0, true);
		msleep(20);
		ts->pdata->power_enable(1, true);
		break;
	default:
		printk(KERN_INFO "usage: echo [1|2|3] > control\n");
		printk(KERN_INFO "  - 1: power on\n");
		printk(KERN_INFO "  - 2: power off\n");
		printk(KERN_INFO "  - 3: power reset\n");
		break;
	}
	return count;
}

static ssize_t
mms136_irq_control_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	int cmd, ret;

	if (sscanf(buf, "%d", &cmd) != 1)
		return -EINVAL;

	switch (cmd) {
	case 1: /* interrupt pin high */
		ret = gpio_direction_input(ts->pdata->i2c_int_gpio);
		if (ret < 0) {
			printk(KERN_ERR "%s: gpio input direction fail\n", __FUNCTION__);
			break;
		}
		gpio_set_value(ts->pdata->i2c_int_gpio, 1);
		printk(KERN_INFO "MMS-136 INTR GPIO pin high\n");
		break;
	case 2: /* interrupt pin LOW */
		ret = gpio_direction_input(ts->pdata->i2c_int_gpio);
		if (ret < 0) {
			printk(KERN_ERR "%s: gpio input direction fail\n", __FUNCTION__);
			break;
		}
		gpio_set_value(ts->pdata->i2c_int_gpio, 0);
		printk(KERN_INFO "MMS-136 INTR GPIO pin low\n");
		break;
	case 3: /* irq disable */
		disable_irq(ts->client->irq);
		printk(KERN_INFO "MMS-134 disable irq\n");
		break;
	case 4: /* irq enable */
		enable_irq(ts->client->irq);
		printk(KERN_INFO "MMS-134 enable irq\n");
		break;
	default:
		printk(KERN_INFO "usage: echo [1|2|3|4] > control\n");
		printk(KERN_INFO "  - 1: interrupt pin high\n");
		printk(KERN_INFO "  - 2: interrupt pin low\n");
		printk(KERN_INFO "  - 3: irq disable\n");
		printk(KERN_INFO "  - 4: irq enable\n");
		break;
	}
	return count;
}

static ssize_t
mms136_reg_control_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	int cmd, ret, reg_addr, length, i;
	uint8_t reg_buf[TS_READ_REGS_LEN];
	if (sscanf(buf, "%d, 0x%x, %d", &cmd, &reg_addr, &length) != 3)
		return -EINVAL;
	switch (cmd) {
	case 1:
		reg_buf[0] = reg_addr;
		ret = i2c_master_send(ts->client, reg_buf, 1);
		if (ret < 0) {
			printk(KERN_ERR "i2c master send fail\n");
			break;
		}
		ret = i2c_master_recv(ts->client, reg_buf, length);
		if (ret < 0) {
			printk(KERN_ERR "i2c master recv fail\n");
			break;
		}
		for (i = 0; i < length; i++) {
			printk(KERN_INFO "0x%x", reg_buf[i]);
		}
		printk(KERN_INFO "\n 0x%x register read done\n", reg_addr);
		break;
	case 2:
		reg_buf[0] = reg_addr;
		reg_buf[1] = length;
		ret = i2c_master_send(ts->client, reg_buf, 2);
		if (ret < 0) {
			printk(KERN_ERR "i2c master send fail\n");
			break;
		}
		printk(KERN_INFO "\n 0x%x register write done\n", reg_addr);
		break;
	default:
		printk(KERN_INFO "usage: echo [1(read)|2(write)], [reg address], [length|value] > reg_control\n");
		printk(KERN_INFO "  - Register Set or Read\n");
		break;
	}
	return count;
}

// cmdelta, intensity supported  firmware version 6 over
static ssize_t
mms136_cmdelta_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	char c = '-';
	int len, i, j, t;
	uint8_t write_buf[5];
	uint8_t read_buf[40];
	uint8_t read_size = 0;
	uint16_t cmdata = 0;
	int flag = 0;

    if (irq_flag == 0) {
            printk("disable_irq_nosync\n");
            disable_irq_nosync(ts->client->irq);
            irq_flag = 1;
    }

	write_buf[0] = UNIVERSAL_CMD;
	write_buf[1] = UNIVCMD_ENTER_TEST_MODE;
	i2c_master_send(ts->client, write_buf, 2);

	while (gpio_get_value(ts->pdata->i2c_int_gpio)) {
		flag++;
		if (flag == 30) {
			flag = 0;
			break;
		}
		msleep(100);
	}
	flag = 0;

	write_buf[0] = UNIVERSAL_CMD_RESULT_SIZE;
	i2c_master_send(ts->client, write_buf, 1);
	i2c_master_recv(ts->client, read_buf, 1);
	printk("TEST MODE ENTER =%d \n", read_buf[0]);

	write_buf[0] = UNIVERSAL_CMD;
	write_buf[1] = UNIVCMD_TEST_CM_DELTA;
	i2c_master_send(ts->client, write_buf, 2);

	while (gpio_get_value(ts->pdata->i2c_int_gpio)) {
		flag++;
		if (flag == 30) {
			flag = 0;
			break;
		}
		msleep(100);
	}
	flag = 0;

	write_buf[0] = UNIVERSAL_CMD_RESULT_SIZE;
	i2c_master_send(ts->client, write_buf, 1);
	i2c_master_recv(ts->client, read_buf, 1);

	write_buf[0] = UNIVERSAL_CMD_RESULT;
	i2c_master_send(ts->client, write_buf, 1);
	i2c_master_recv(ts->client, read_buf, read_size);

	printk("CM DELTA TEST =%d \n", read_buf[0]);
	len = snprintf(buf, PAGE_SIZE, "Result = %s\n", read_buf[0]==1 ? "PASS" : "FAIL"); 

	len += snprintf(buf + len, PAGE_SIZE, "Touch Firmware Version is %d\n", ts->version);
	len += snprintf(buf + len, PAGE_SIZE - len, "===============================================");
	len += snprintf(buf + len, PAGE_SIZE - len, "===============================================");
	len += snprintf(buf + len, PAGE_SIZE - len, "========================\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "%5c", c);
	for (j = 0; j < num_tx_line; j++)
		len += snprintf(buf + len, PAGE_SIZE - len, "%5d\t", j);
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "-----------------------------------------------");
	len += snprintf(buf + len, PAGE_SIZE - len, "-----------------------------------------------");
	len += snprintf(buf + len, PAGE_SIZE - len, "------------------------\n");

	/* read touch screen cmdelta */
	for (i = 0; i < num_rx_line ; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len, "%2d : ", i);
			write_buf[0] = UNIVERSAL_CMD;
			write_buf[1] = UNIVCMD_GET_PIXEL_CM_DELTA;
		write_buf[2] = 0xFF;
			write_buf[3] = i;
			i2c_master_send(ts->client, write_buf, 4);

			while (gpio_get_value(ts->pdata->i2c_int_gpio)) {
				flag++;
				if (flag == 100) {
					flag = 0;
					break;
				}
				udelay(100);
			}

			write_buf[0] = UNIVERSAL_CMD_RESULT_SIZE;
			i2c_master_send(ts->client, write_buf, 1);
			i2c_master_recv(ts->client, &read_size, 1);

			write_buf[0] = UNIVERSAL_CMD_RESULT;
			i2c_master_send(ts->client, write_buf, 1);
			i2c_master_recv(ts->client, read_buf, read_size);

		for (j = 0; j < num_tx_line ; j++) {
			cmdata = read_buf[2 * j] | (read_buf[2 * j + 1] << 8);
			len += snprintf(buf + len, PAGE_SIZE - len, "%5d\t", cmdata);
		}
		len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	}

	/* read touch key cmdelta */
	len += snprintf(buf + len, PAGE_SIZE - len, "key: ");
	if(2){
		write_buf[0] = UNIVERSAL_CMD;
		write_buf[1] = 0x4A;
		write_buf[2] = 0xff; //KEY CH.
		write_buf[3] = 0; //Dummy Info

		
		i2c_master_send(ts->client, write_buf, 4);

		while (gpio_get_value(ts->pdata->i2c_int_gpio)) {
			flag++;
			if (flag == 100) {
				flag = 0;
				break;
			}
			udelay(100);
		}

		write_buf[0] = UNIVERSAL_CMD_RESULT_SIZE;
		i2c_master_send(ts->client, write_buf, 1);
		i2c_master_recv(ts->client, &read_size, 1);

		write_buf[0] = UNIVERSAL_CMD_RESULT;
		i2c_master_send(ts->client, write_buf, 1);
		i2c_master_recv(ts->client, read_buf, read_size);

		for (t = 0; t < 2; t++) //Model Dependent
		{			
			cmdata = read_buf[2 * t] | (read_buf[2 * t + 1] << 8);
			len += snprintf(buf + len, PAGE_SIZE - len, "%5d\t", cmdata);
		}
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "\n===============================================");
	len += snprintf(buf + len, PAGE_SIZE - len, "===============================================");
	len += snprintf(buf + len, PAGE_SIZE - len, "========================\n");

	write_buf[0] = UNIVERSAL_CMD;
	write_buf[1] = UNIVERSAL_CMD_EXIT;

	i2c_master_send(ts->client, write_buf, 2);

        if (irq_flag == 1) {
                printk("enable_irq\n");
                enable_irq(ts->client->irq);
                irq_flag = 0;
        }
	ts->pdata->power_enable(0, true);
	msleep(20);
	ts->pdata->power_enable(1, true); //enable touch by setting power on-off
	return len;
}

static ssize_t
mms136_key_intensity_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	char c = '-';
	int len, j;
	uint8_t write_buf[10];
	uint8_t read_buf[25];
	uint8_t read_size = 0;
	int8_t cmdata = 0;
	int flag = 0;

	if (irq_flag == 0) {
		printk("disable_irq_nosync\n");
		disable_irq_nosync(ts->client->irq);
		irq_flag = 1;
	}

	write_buf[0] = UNIVERSAL_CMD;
	write_buf[1] = UNIVCMD_ENTER_TEST_MODE;
	i2c_master_send(ts->client, write_buf, 2);

	while (gpio_get_value(ts->pdata->i2c_int_gpio)) {
		flag++;
		if (flag == 30) {
			flag = 0;
			break;
		}
		msleep(100);
	}
	flag = 0;

	write_buf[0] = UNIVERSAL_CMD_RESULT_SIZE;
	i2c_master_send(ts->client, write_buf, 1);
	i2c_master_recv(ts->client, read_buf, 1);
	printk("TEST MODE ENTER =%d \n", read_buf[0]);

	len = snprintf(buf, PAGE_SIZE, "Touch Firmware Version is %d\n", ts->version);
	len += snprintf(buf + len, PAGE_SIZE - len, "===============================================");
	len += snprintf(buf + len, PAGE_SIZE - len, "===============================================");
	len += snprintf(buf + len, PAGE_SIZE - len, "========================\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "%5c", c);
	for (j = 0; j < 2; j++)
		len += snprintf(buf + len, PAGE_SIZE - len, "%5d", j);
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "-----------------------------------------------");
	len += snprintf(buf + len, PAGE_SIZE - len, "-----------------------------------------------");
	len += snprintf(buf + len, PAGE_SIZE - len, "------------------------\n");

	if(2){
		j = 0;
		write_buf[0] = UNIVERSAL_CMD;
		write_buf[1] = 0x71;
		write_buf[2] = 0xFF;  /*Exciting CH.*/
		write_buf[3] = 0;  /*Sensing CH.*/

		i2c_master_send(ts->client, write_buf, 4);

		while (gpio_get_value(ts->pdata->i2c_int_gpio)) {
			flag++;
			if (flag == 100) {
				flag = 0;
				break;
			}
			udelay(100);
		}

		write_buf[0] = UNIVERSAL_CMD_RESULT_SIZE;
		i2c_master_send(ts->client, write_buf, 1);
		i2c_master_recv(ts->client, &read_size, 1);

		write_buf[0] = UNIVERSAL_CMD_RESULT;
		i2c_master_send(ts->client, write_buf, 1);
		i2c_master_recv(ts->client, read_buf, read_size);

		for (j = 0; j < 2 ; j++) {
			cmdata = read_buf[2 * j] | (read_buf[2 * j + 1] << 8);
			len += snprintf(buf + len, PAGE_SIZE - len, "%5d\t", cmdata);
		}
		len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "===============================================");
	len += snprintf(buf + len, PAGE_SIZE - len, "===============================================");
	len += snprintf(buf + len, PAGE_SIZE - len, "========================\n");

	write_buf[0] = UNIVERSAL_CMD;
	write_buf[1] = UNIVERSAL_CMD_EXIT;

	i2c_master_send(ts->client, write_buf, 2);

	if (irq_flag == 1) {
		printk("enable_irq\n");
		enable_irq(ts->client->irq);
		irq_flag = 0;
	}
	ts->pdata->power_enable(0, true);
	msleep(20);
	ts->pdata->power_enable(1, true); //enable touch by setting power on-off
	return len;
}


static ssize_t
mms136_intensity_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	char c = '-';
	int len, i, j;
	uint8_t write_buf[10];
	uint8_t read_buf[40];
	int8_t cmdata = 0;
	uint8_t read_size = 0;
	int flag = 0;
	int ret = 0;

	disable_irq(ts->client->irq);

	len = snprintf(buf, PAGE_SIZE, "Touch Firmware Version is %d\n", ts->version);
	len += snprintf(buf + len, PAGE_SIZE - len, "===============================================");
	len += snprintf(buf + len, PAGE_SIZE - len, "===============================================");
	len += snprintf(buf + len, PAGE_SIZE - len, "========================\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "%5c", c);
	for (j = 0; j < num_tx_line; j++)
		len += snprintf(buf + len, PAGE_SIZE - len, "%5d\t", j);
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "-----------------------------------------------");
	len += snprintf(buf + len, PAGE_SIZE - len, "-----------------------------------------------");
	len += snprintf(buf + len, PAGE_SIZE - len, "------------------------\n");

	for (i = 0; i < num_rx_line; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len, "%2d : ", i);
		j = 0;
		write_buf[0] = UNIVERSAL_CMD;
		write_buf[1] = 0x70;
		write_buf[2] = 0xFF;  /*Exciting CH.*/
		write_buf[3] = i;  /*Sensing CH.*/

		ret = i2c_master_send(ts->client, write_buf, 4);
		
		printk(KERN_ERR "I2c1 =%d \n", ret);

		while (gpio_get_value(ts->pdata->i2c_int_gpio)) {
			flag++;
			if (flag == 100) {
				flag = 0;
				break;
			}
			udelay(100);
		}

		write_buf[0] = UNIVERSAL_CMD_RESULT_SIZE;
		i2c_master_send(ts->client, write_buf, 1);
		printk(KERN_ERR "I2c2 =%d \n", ret);
		i2c_master_recv(ts->client, &read_size, 1);
		printk(KERN_ERR "I2c3 =%d \n", ret);
		//printk(KERN_ERR "[Touch] Intensity read size : %d\n", read_size);

		write_buf[0] = UNIVERSAL_CMD_RESULT;
		i2c_master_send(ts->client, write_buf, 1);
		printk(KERN_ERR "I2c4 =%d \n", ret);

		if(read_size > 40) read_size = 40;
		
		i2c_master_recv(ts->client, read_buf, read_size);
		printk(KERN_ERR "I2c5 =%d \n", ret);

		read_size >>= 1;

		for (j = 0; j < read_size ; j++) {
			printk(KERN_ERR "[Touch] 1:%d, 2:%d \n", read_buf[2*j], (read_buf[2*j+1]<<8));
			cmdata = read_buf[2 * j] | (read_buf[2 * j + 1] << 8);
			len += snprintf(buf + len, PAGE_SIZE - len, "%5d\t", cmdata);
		}
		len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "===============================================");
	len += snprintf(buf + len, PAGE_SIZE - len, "===============================================");
	len += snprintf(buf + len, PAGE_SIZE - len, "========================\n");

	enable_irq(ts->client->irq);
	return len;
}

static ssize_t
mms136_all_version_show(struct device *dev, struct device_attribute *attr,
char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	int ret, len;
	uint8_t version_buf[6];

	version_buf[0] = TS_READ_VERSION_ADDR;
	ret = i2c_master_send(ts->client, &version_buf[0], 1);
	ret = i2c_master_recv(ts->client, &version_buf[0], TS_READ_VERSION_INFO_LEN);

	len = snprintf(buf, PAGE_SIZE, "Melfas Version Info\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "============================\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "Firmware Version : %d\n", version_buf[0]);
	len += snprintf(buf + len, PAGE_SIZE - len, "Hardware Version : %d\n", version_buf[1]);
	len += snprintf(buf + len, PAGE_SIZE - len, "Compatibility Group : %c\n", version_buf[2]);
	len += snprintf(buf + len, PAGE_SIZE - len, "Core Firmware Version : %d\n", version_buf[3]);
	len += snprintf(buf + len, PAGE_SIZE - len, "Private Custom Version : %d\n", version_buf[4]);
	len += snprintf(buf + len, PAGE_SIZE - len, "Public Custom Version : %d\n", version_buf[5]);
	len += snprintf(buf + len, PAGE_SIZE - len, "============================\n");

	return len;
}

static ssize_t
mms136_new_version_show(struct device *dev, struct device_attribute *attr,
char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	int len;

	len = snprintf(buf, PAGE_SIZE, "%d\n", ts->pdata->fw_ver);

	return len;
}


///////// attribute path : /sys/devices/i2c-3/3-0048/
///////// attribute path : /sys/devices/i2c-[GSBINUMBER]/[GSBINUMBER-SLAVEADDR]
static struct device_attribute mms136_device_attrs[] = {
	__ATTR(status,  S_IRUGO | S_IWUSR, mms136_status_show, NULL),
	__ATTR(version, S_IRUGO | S_IWUSR, mms136_version_show, NULL),
	__ATTR(fw_upgrade, S_IRUGO | S_IWUSR, NULL, mms134_store_fw_upgrade),
	__ATTR(power_control, S_IRUGO | S_IWUSR, NULL, mms136_power_control_store),
	__ATTR(irq_control, S_IRUGO | S_IWUSR, NULL, mms136_irq_control_store),
	__ATTR(reg_control, S_IRUGO | S_IWUSR, NULL, mms136_reg_control_store),
	__ATTR(cmdelta, S_IRUGO | S_IWUSR, mms136_cmdelta_show, NULL),
	__ATTR(intensity, S_IRUGO | S_IWUSR, mms136_intensity_show, NULL),
	__ATTR(key_intensity, S_IRUGO | S_IWUSR, mms136_key_intensity_show, NULL),
	__ATTR(all_version, S_IRUGO | S_IWUSR, mms136_all_version_show, NULL),
	__ATTR(fw_read, S_IRUGO | S_IWUSR, mms136_fw_read_show, NULL),
	__ATTR(new_version, S_IRUGO | S_IWUSR, mms136_new_version_show, NULL),
};

int melfas_mms134_fw_upgrade(const char* fw_path)
{
	int ret = 0;
	// assign function pointer for f/w upgrade i2c
	assign_fp_read_func(touch_i2c_read);
	assign_fp_write_func(touch_i2c_write);
	assign_fp_read_len_func(touch_i2c_read_nlength);

	ret = MFS_ISC_update(fw_path);	
	return ret;
}

static int melfas_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct melfas_ts_data *ts;
	int ret = 0, i;
	int m = 1;                       /* l1a,l2s use double resolution */
	char fw_path[2] = {0};
	uint8_t buf[TS_READ_VERSION_INFO_LEN];
	irq_flag = 0;

	MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_INFO, "[TOUCH] melfas_ts_probe Start!!!\n");

#if defined(TOUCH_NOISE_MODE) && defined(CONFIG_LGE_PM)
	ac_change = pm8921_charger_is_ta_connected();
	is_noise_flag = 0;
#endif

	for(i=0; i<20; i++)
		init_values[i] = 1;
	printk(KERN_ERR "[Touch] Enter i2c_check_functionality\n");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_ERR, "[TOUCH] need I2C_FUNC_I2C\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	ts = &mms134_ts_data;
#if 0
	ts = kmalloc(sizeof(struct melfas_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_ERR, "[TOUCH] failed to create a state of melfas-ts\n");
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}
#endif

	ts->pdata = client->dev.platform_data;

	ts->client = client;

	INIT_WORK(&ts->work, melfas_ts_event_handler);

#ifdef DEBUG_TOUCH_IRQ
	INIT_DELAYED_WORK(&ts->irq_restore_work,melfas_ts_restore_irq);
#endif
#ifdef TOUCH_GHOST_DETECTION
	INIT_DELAYED_WORK(&ts->ghost_monitor_work, monitor_ghost_finger);
#endif

	i2c_set_clientdata(client, ts);

	/* firmwre binary checking */

	ts->pdata->fw_ver = 0x01;

	fw_client = client;
	// initialize firmware
	MELFAS_binary_nLength = MELFAS_binary_nLength_2;
	MELFAS_binary = MELFAS_binary_2;	
	MELFAS_binary_fw_ver = MELFAS_binary_fw_ver_2;

	// check no touch panel status, enter recovery download mode
	for (i = 0; i < I2C_RETRY_CNT; i++) {
		ret = i2c_master_send(ts->client, &buf[0], 1);
		if (ret >= 0) {
			MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_INFO, "[TOUCH] i2c_master_send() ok [%d]\n", ret);
			break;
		} else {
			MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_ERR, "[TOUCH] i2c_master_send() failed[%d]\n", ret);
			if (i == I2C_RETRY_CNT-1) {
				// recovery touch firmware
				ret = melfas_mms134_fw_upgrade(fw_path);
				if (ret == 0) {
					MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_INFO, "[touch] touch fw update success \n");
				}
				else {
					MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_ERR, "[TOUCH] no touch panel \n");
					// if no touch panel case -> need power off
					ts->pdata->power_enable(0, true);
					return ret ;
				}
			}
		}
	} 
	/*                                                
                                                                                      
   */
	
	buf[0] = TS_READ_HW_VER_ADDR;
	ret = i2c_master_send(ts->client, &buf[0], 1);
	ret = i2c_master_recv(ts->client, &buf[0], 1);

	buf[1] = TS_READ_FW_VER_ADDR;
	ret = i2c_master_send(ts->client, &buf[1], 1);
	ret = i2c_master_recv(ts->client, &buf[1], 1);

	buf[2] = TS_READ_CONFIG_VER_ADDR;
	ret = i2c_master_send(ts->client, &buf[2], 1);
	ret = i2c_master_recv(ts->client, &buf[2], 1);

	printk(KERN_INFO "\n= Melfas Version Info =\n");
	printk(KERN_INFO "Release HW Version :: %d, Core version :: 0x%02x, Firmware Version :: 0x%02x\n", buf[0], buf[1], buf[2]);

#if 1 // check touch HW revision
	if(buf[0] == 1){
		MELFAS_binary_nLength = MELFAS_binary_nLength_1;
		MELFAS_binary = MELFAS_binary_1;	
		MELFAS_binary_fw_ver = MELFAS_binary_fw_ver_1;
	}
#endif
	
	if(MELFAS_binary != NULL)
		//printk(KERN_INFO "[Touch D] Image's Core Version :: 0x%02x, Image's Firmware Revision :: 0x%02x\n\n", MELFAS_binary[17574], MELFAS_binary[19464]);
		printk(KERN_INFO "[Touch D] Image's Firmware Revision :: 0x%02x\n\n", MELFAS_binary_fw_ver);
	else
		printk(KERN_INFO "[Touch D] Melfas binary is null!! \n");
	/*                                                                                                   */

	ts->version = buf[FW_VERSION_ADDR];
	
	MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_INFO,
		"[TOUCH] i2c_master_send() [%d], Add[%d]\n", ret, ts->client->addr);

	/*                                                
                                                                         
   */
	if(MELFAS_binary != NULL){
		printk(KERN_INFO "[Touch D] Board f/w version : 0x%02x, image f/w version 0x%02x \n", ts->version, MELFAS_binary_fw_ver);
	//	printk(KERN_INFO "[Touch D] Board f/w version : 0x%02x, image f/w version 0x%02x \n", ts->version, MELFAS_binary[19464]);
	//	if(MELFAS_binary[19464] > ts->version){
		if(MELFAS_binary_fw_ver > ts->version){
			printk(KERN_ERR "[Touch D] Firmware Upgrade Start!\n");
			melfas_mms134_fw_upgrade(fw_path);

			buf[0] = TS_READ_HW_VER_ADDR;
			ret = i2c_master_send(ts->client, &buf[0], 1);
			ret = i2c_master_recv(ts->client, &buf[0], 1);

			buf[1] = TS_READ_FW_VER_ADDR;
			ret = i2c_master_send(ts->client, &buf[1], 1);
			ret = i2c_master_recv(ts->client, &buf[1], 1);

			buf[2] = TS_READ_CONFIG_VER_ADDR;
			ret = i2c_master_send(ts->client, &buf[2], 1);
			ret = i2c_master_recv(ts->client, &buf[2], 1);

			printk(KERN_INFO "[Touch D]============= AFTER F/W Upgrade =============\n");
			printk(KERN_INFO "\n= Melfas Version Info =\n");
			printk(KERN_INFO "Release HW Version :: %d, Core version :: 0x%02x, Firmware Version :: 0x%02x\n", buf[0], buf[1], buf[2]);
		}
		else{
			// check abnormal firmware case(f/w crash) : 0xff, test f/w 0x80 ~ 0x99
			if(ts->version > 153){ // 0x99 = 153
				printk(KERN_ERR "[Touch D] Internal Firmware Crash!, Version : 0x%02x \n", ts->version);
				printk(KERN_ERR "[Touch D] Firmware Recovery Start!\n");
				melfas_mms134_fw_upgrade(fw_path);

				buf[0] = TS_READ_HW_VER_ADDR;
				ret = i2c_master_send(ts->client, &buf[0], 1);
				ret = i2c_master_recv(ts->client, &buf[0], 1);

				buf[1] = TS_READ_FW_VER_ADDR;
				ret = i2c_master_send(ts->client, &buf[1], 1);
				ret = i2c_master_recv(ts->client, &buf[1], 1);

				buf[2] = TS_READ_CONFIG_VER_ADDR;
				ret = i2c_master_send(ts->client, &buf[2], 1);
				ret = i2c_master_recv(ts->client, &buf[2], 1);

				printk(KERN_INFO "[Touch D]============= AFTER F/W Recovery =============\n");
				printk(KERN_INFO "\n= Melfas Version Info =\n");
				printk(KERN_INFO "Release HW Version :: %d, Core version :: 0x%02x, Firmware Version :: 0x%02x\n", buf[0], buf[1], buf[2]);
			}
			else
				printk(KERN_INFO "[Touch D] Don`t need f/w upgrade\n");
		}
	}
	else{
		printk(KERN_INFO "[Touch D] Melfas binary is null!! \n");
	}
	/*                                                                                                   */

	ts->version = buf[FW_VERSION_ADDR];

	ts->input_dev = input_allocate_device();
	if (!ts->input_dev) {
		MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_INFO, "[TOUCH] Not enough memory\n");
		ret = -ENOMEM;
		goto err_input_dev_alloc_failed;
	}

	ts->input_dev->name = "melfas-ts" ;

	ts->input_dev->evbit[0] = BIT_MASK(EV_ABS) | BIT_MASK(EV_KEY);

	for (i = 0; i < ts->pdata->num_of_button; i++)
		ts->input_dev->keybit[BIT_WORD(ts->pdata->button[i])] |= BIT_MASK(ts->pdata->button[i]);

	/*                                                
                                                  
   */
	if(ts != NULL){
		//if(ts->version < 80 && ts->version >= 6)
		if(ts->version >= 6)
			// modify touch max resolution			
			m = 2;
		else
			m = 1;

		printk(KERN_DEBUG "[Touch] Set Touch resolution %d X %d , f/w version : %d \n",ts->pdata->x_max * m,ts->pdata->y_max * m,ts->version);
	}
	/*                                                               */

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0,  ts->pdata->x_max * m, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0,  ts->pdata->y_max * m, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 40, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, MELFAS_MAX_TOUCH - 1, 0, 0);

	ret = input_register_device(ts->input_dev);
	if (ret) {
		MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_ERR,
			"[TOUCH] Failed to register device\n");
		ret = -ENOMEM;
		goto err_input_register_device_failed;
	}


	if (ts->client->irq) {
		MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_ERR,
			"[TOUCH] trying to request irq: %s-%d\n", ts->client->name, ts->client->irq);
		ret = request_threaded_irq(client->irq, NULL, melfas_ts_irq_handler, IRQF_ONESHOT | IRQF_TRIGGER_LOW, ts->client->name, ts);
#ifdef DEBUG_TOUCH_IRQ
		is_request_irq = 1;
#endif

		if (ret > 0) {
			MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_ERR,
				"[TOUCH] Can't allocate irq %d, ret %d\n", ts->client->irq, ret);
			ret = -EBUSY;
			goto err_request_irq;
		}
	}

	MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_INFO, "[TOUCH] succeed to register input device\n");

	for (i = 0; i < ARRAY_SIZE(mms136_device_attrs); i++) {
		ret = device_create_file(&client->dev, &mms136_device_attrs[i]);
		if (ret) {
			goto err_request_irq;
		}
	}

	for (i = 0; i < MELFAS_MAX_TOUCH; i++) {
		g_Mtouch_info[i].btn_touch = -1;
		tmp_flag[i] = 1;
	}
	
#ifdef TOUCH_GHOST_DETECTION
	queue_delayed_work(touch_wq, &ts->ghost_monitor_work, msecs_to_jiffies(HZ * 10));
//	schedule_delayed_work(&ts->ghost_monitor_work, msecs_to_jiffies(HZ * 10));
#endif

#ifdef CONFIG_TS_INFO_CLASS
	ts->cdev.name = "touchscreen";
#if defined(CONFIG_USING_INNOTEK_PANEL_4_7)
	ts->cdev.version = ts->group_version;
#else
	ts->cdev.version = ts->version;
#endif
	ts->cdev.flags = ts->flags;

	ts_info_classdev_register(&client->dev, &ts->cdev);
#endif

#if CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = melfas_ts_early_suspend;
	ts->early_suspend.resume = melfas_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

#ifdef DEBUG_TOUCH_IRQ
	//schedule_delayed_work(&ts->irq_restore_work, msecs_to_jiffies(HZ * 12)); // 1.2 sec
	queue_delayed_work(touch_wq, &ts->irq_restore_work, msecs_to_jiffies(HZ * 30));
#endif

	MELFAS_TS_DPRINTK(MELFAS_TS_DEBUG_PROBE, KERN_INFO,
			"[TOUCH] Start touchscreen. name: %s, irq: %d\n",
			ts->client->name, ts->client->irq);
	return 0;

err_request_irq:
	printk(KERN_ERR "melfas-ts: err_request_irq failed\n");
	free_irq(client->irq, ts);
err_input_register_device_failed:
	printk(KERN_ERR "melfas-ts: err_input_register_device failed\n");
	input_free_device(ts->input_dev);
err_input_dev_alloc_failed:
	printk(KERN_ERR "melfas-ts: err_input_dev_alloc failed\n");
#if 0
err_alloc_data_failed:
	printk(KERN_ERR "melfas-ts: err_alloc_data failed_\n");
#endif
err_check_functionality_failed:
	printk(KERN_ERR "melfas-ts: err_check_functionality failed_\n");

	return ret;
}

static int melfas_ts_remove(struct i2c_client *client)
{
	struct melfas_ts_data *ts = i2c_get_clientdata(client);

	unregister_early_suspend(&ts->early_suspend);
	free_irq(client->irq, ts);
	input_unregister_device(ts->input_dev);
#ifdef CONFIG_TS_INFO_CLASS
	ts_info_classdev_unregister(&ts->cdev);
#endif
#ifndef DEBUG_TOUCH_IRQ
	kfree(ts);
#endif
	return 0;
}

static void release_all_fingers(struct melfas_ts_data *ts)
{
	int i;
	for (i = 0; i < ts->pdata->num_of_finger; i++) {
		if (g_Mtouch_info[i].btn_touch < 0)
			continue;

		input_report_abs(ts->input_dev, ABS_MT_POSITION_X, g_Mtouch_info[i].posX);
		input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, g_Mtouch_info[i].posY);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, g_Mtouch_info[i].width);
		input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);
		input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, i);
		input_mt_sync(ts->input_dev);

		g_Mtouch_info[i].posX = 0;
		g_Mtouch_info[i].posY = 0;
		g_Mtouch_info[i].strength = 0;
		g_Mtouch_info[i].btn_touch = -1;
		tmp_flag[i] = 1;
	}
	input_sync(ts->input_dev);

	input_mt_sync(ts->input_dev);
	input_sync(ts->input_dev);
	
	touch_prestate = 0;
}

static void release_all_keys(struct melfas_ts_data *ts)
{
	int i;

	for (i = 0; i < MELFAS_MAX_BTN; i++) {
		//printk("g_btn_info status : %d, key_code : %d\n",g_btn_info[i].status , g_btn_info[i].key_code);
		if (g_btn_info[i].status <= 0)
			continue;
		input_report_key(ts->input_dev, g_btn_info[i].key_code, RELEASE_KEY);
	}
	btn_prestate = RELEASE_KEY;
	input_sync(ts->input_dev);
}

/*                                                                               */
#ifdef TOUCH_GHOST_DETECTION

static void monitor_ghost_finger(struct work_struct *work)
{
	struct melfas_ts_data *ts = &mms134_ts_data;
	//unsigned long flags;

	if (ghost_touch_cnt >= 8){
		printk("[Touch] ghost finger pattern DETECTED! : %d \n", ghost_touch_cnt);

		//local_irq_save(flags);

		release_all_fingers(ts);
		release_all_keys(ts);		

		msleep(50); 
		ts->pdata->power_enable(0, true);
		msleep(100); 
		ts->pdata->power_enable(1, true);
		msleep(100); 

		release_all_fingers(ts);
		
		//local_irq_restore(flags);
	}
	

	ghost_touch_cnt = 0;
	ghost_x = 1000;
	ghost_y = 2000;

	queue_delayed_work(touch_wq, &ts->ghost_monitor_work, msecs_to_jiffies(HZ * 10));
	
	return;

}
#endif
/*                                                                               */


static void melfas_ts_sw_reset(struct melfas_ts_data *ts)
{
	unsigned long flags;
	printk(KERN_ERR "[TOUCH] sw reset!!!! \n");

	release_all_fingers(ts);
	release_all_keys(ts);
	local_irq_save(flags);
	
	ts->pdata->power_enable(0, true);
	msleep(TOUCH_RESET_DELAY + 20 * touch_reset_cnt);
	ts->pdata->power_enable(1, true);
	msleep(100); 

	if(touch_reset_cnt <= 10)
		touch_reset_cnt++;
	else
		touch_reset_cnt = 0;

	local_irq_restore(flags);
}

#if defined(TOUCH_NOISE_MODE) && defined(CONFIG_LGE_PM)
static void mms_set_noise_mode(struct melfas_ts_data *ts){
	if(ts == NULL)
		return;
	
	// not support noise mode f/w version less than 3
	if(ts->version <= 3){
		printk(KERN_INFO "[Touch] Noise mode is not supported old firmware : %d\n",ts->version);
		return;
	}	

	ac_change = pm8921_charger_is_ta_connected();
		
	printk(KERN_INFO "[Touch] Noise Mode entered! is_noise_flag = %d, pm8921_charger_connected : %d \n", is_noise_flag, ac_change);
	if(!ac_change){
		is_noise_flag = 0;
		return;
	}

	// noise mode enter
	if(!is_noise_flag) 
		return;  
	
	// if TA is connected
	if (ac_change){
		uint8_t noise_buf[2];
		printk(KERN_INFO "[TOUCH] ta is connected!!, Send 0x30, 0x01\n");
		noise_buf[0] = 0x30;
		noise_buf[1] = 0x01;
		msleep(20);
	
		i2c_master_send(ts->client, noise_buf, 2);	
	}	
	else{
		uint8_t noise_buf2[2];
		printk(KERN_INFO "[TOUCH] ta is disconnected!!, Send 0x30, 0x00\n");
		noise_buf2[0] = 0x30;
		noise_buf2[1] = 0x00;
		msleep(20);

		i2c_master_send(ts->client, noise_buf2, 2);	
		is_noise_flag = 0;
	}
}
#endif


static void melfas_ts_suspend_func(struct melfas_ts_data *ts)
{

	//TODO: temp patch add.
	//touch power always on patch for sensor function.

	int ret;
#if defined(TOUCH_NOISE_MODE) && defined(CONFIG_LGE_PM)
	uint8_t noise_buf[2];
#endif

	printk(KERN_INFO "[TOUCH] suspend start \n");
	printk(KERN_INFO "[Touch] DISABLE_IRQ \n");
	disable_irq(ts->client->irq);	
	
#if defined(TOUCH_NOISE_MODE) && defined(CONFIG_LGE_PM)		
	noise_buf[0] = 0xB0;
	noise_buf[1] = 0x01;
	
	ret = i2c_master_send(ts->client, noise_buf, 2);
#ifdef TOUCH_RETRY_I2C
	if(ret < 0){
		printk(KERN_ERR "[Touch] %s : first i2c fail, %d\n",__func__, ret);
		mdelay(10);
		ret = i2c_master_send(ts->client, noise_buf, 2);
		if(ret < 0)
			printk(KERN_ERR "[Touch] %s, I2C master send fail : %d\n", __func__, ret);			
	}
#endif
	printk(KERN_INFO "[TOUCH] Touch suspend : 0xB0 write! \n");
#endif
	ret = cancel_work_sync(&ts->work);
/*                                                                               */
#ifdef TOUCH_GHOST_DETECTION
	cancel_delayed_work_sync(&ts->ghost_monitor_work);
#endif
/*                                                                                */

#ifdef DEBUG_TOUCH_IRQ
	cancel_delayed_work_sync(&ts->irq_restore_work); 
#endif
 
	ret = ts->pdata->power_enable(0, true);

	/* move release timing */
	release_all_fingers(ts);
	release_all_keys(ts);

	printk(KERN_INFO "[TOUCH] suspend end \n");

}

static void melfas_ts_resume_func(struct melfas_ts_data *ts)
{
	int ret = 0;

	printk(KERN_INFO "[TOUCH] resume start \n");


	ret = ts->pdata->power_enable(1, true);
	msleep(100);
#if defined(TOUCH_NOISE_MODE) && defined(CONFIG_LGE_PM)
	mms_set_noise_mode(ts);
#endif
	read_dummy_interrupt();

	enable_irq(ts->client->irq);

/*                                                                               */
#ifdef TOUCH_GHOST_DETECTION	
	ghost_touch_cnt = 0;
	//schedule_delayed_work(&ts->ghost_monitor_work, msecs_to_jiffies(HZ * 10));
	queue_delayed_work(touch_wq, &ts->ghost_monitor_work, msecs_to_jiffies(HZ * 10));
#endif
/*                                                                                */

#ifdef DEBUG_TOUCH_IRQ
	//schedule_delayed_work(&ts->irq_restore_work, msecs_to_jiffies(HZ * 12)); // 1.2 sec
	queue_delayed_work(touch_wq, &ts->irq_restore_work, msecs_to_jiffies(HZ * 30));
#endif

	printk(KERN_INFO "[TOUCH] resume end \n");

}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void melfas_ts_early_suspend(struct early_suspend *h)
{
	struct melfas_ts_data *ts;
	ts = container_of(h, struct melfas_ts_data, early_suspend);
	melfas_ts_suspend_func(ts);
}

static void melfas_ts_late_resume(struct early_suspend *h)
{
	struct melfas_ts_data *ts;
	ts = container_of(h, struct melfas_ts_data, early_suspend);
	melfas_ts_resume_func(ts);
}
#endif

static const struct i2c_device_id melfas_ts_id[] = {
	{ MELFAS_TS_NAME, 0 },
	{ }
};

static struct i2c_driver melfas_ts_driver = {
	.driver		= {
		.name	= MELFAS_TS_NAME,
	},
	.id_table		= melfas_ts_id,
	.probe		= melfas_ts_probe,
	.remove		= __devexit_p (melfas_ts_remove),
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend		= melfas_ts_suspend,
	.resume		= melfas_ts_resume,
#endif
};

static int __devinit melfas_ts_init(void)
{
	int ret = 0;
	printk(KERN_ERR "[Touch] start melfas_ts_init\n");

	touch_wq = create_singlethread_workqueue("touch_wq");
	if (!touch_wq) {
		printk(KERN_ERR "[Touch] CANNOT create new workqueue\n");
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}
	
	ret = i2c_add_driver(&melfas_ts_driver);
	if (ret < 0) {
		printk(KERN_ERR "[TOUCH]failed to i2c_add_driver\n");
		goto err_i2c_driver_register;
	}
	return ret;
err_i2c_driver_register:
	destroy_workqueue(touch_wq);
	return ret;
err_alloc_data_failed:	
	return ret;
}

static void __exit melfas_ts_exit(void)
{
	i2c_del_driver(&melfas_ts_driver);

	if (touch_wq)
		destroy_workqueue(touch_wq);
}

MODULE_DESCRIPTION("Driver for Melfas MTSI Touchscreen Controller");
MODULE_AUTHOR("MinSang, Kim <kimms@melfas.com>");
MODULE_LICENSE("GPL");

module_init(melfas_ts_init);
module_exit(melfas_ts_exit);
