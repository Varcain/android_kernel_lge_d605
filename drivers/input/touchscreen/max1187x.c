/* drivers/input/touchscreen/max1187x.c
 *
 * Copyright (c)2013 Maxim Integrated Products, Inc.
 *
 * Driver Version: 3.0.7.1
 * Release Date: Mar 19, 2013
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/delay.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#elif defined CONFIG_FB
#include <linux/notifier.h>
#include <linux/fb.h>
#endif

#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/firmware.h>
#include <linux/crc16.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/jiffies.h>
#include <asm/byteorder.h>
#include <linux/input/max1187x.h>
#include <linux/input/max1187x_config.h>

#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(fmt) MAX1187X_NAME "(%s:%d): " fmt, __func__, __LINE__
#endif

#define pr_info_if(a, b, ...) do { if (debug_mask & a) \
			pr_info(b, ##__VA_ARGS__);	\
			} while (0)
#define debugmask_if(a) (debug_mask & a)

#define NWORDS(a)    (sizeof(a) / sizeof(u16))
#define BYTE_SIZE(a) ((a) * sizeof(u16))
#define BYTEH(a)     ((a) >> 8)
#define BYTEL(a)     ((a) & 0xFF)

#define PDATA(a)      (ts->pdata->a)

static u16 debug_mask = DEBUG_GET_TOUCH_DATA | DEBUG_BASIC_INFO;

#ifdef MAX1187X_LOCAL_PDATA
struct max1187x_pdata local_pdata = { };
#endif
u16 tanlist[] = {0, 1144, 2289, 3435, 4583, 5734,
                        6888, 8047, 9210, 10380, 11556, 12739,
                        13930, 15130, 16340, 17560, 18792, 20036,
                        21294, 22566, 23853, 25157, 26478, 27818,
                        29178, 30559, 31964, 33392, 34846, 36327,
                        37837, 39377, 40951, 42559, 44204, 45888,
                        47614, 49384, 51202, 53069, 54990, 56969,
                        59008, 61112, 63286, 65535};

struct data {
	struct max1187x_pdata *pdata;
	struct i2c_client *client;
	struct input_dev *input_dev;
	char phys[32];
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#elif defined CONFIG_FB
	struct notifier_block fb_notif;
#endif

	u8 is_suspended;

	wait_queue_head_t waitqueue_all;
	u32 irq_receive_time;

	u16 chip_id;
	u16 config_id;
#ifdef CONFIG_MACH_LGE_L9II_COMMON
	u16 fw_version;
	u16 prev_touch_count;	/* Disable the continuous touch log */
	u8  prev_button_status;
	u16 prev_button_code;
	int pwr_state;
#endif

#ifndef CONFIG_MACH_LGE_L9II_COMMON
	atomic_t irq_processing;
#endif
	struct mutex i2c_mutex;

	struct semaphore sema_rbcmd;
	wait_queue_head_t waitqueue_rbcmd;
	u8 rbcmd_waiting;
	u8 rbcmd_received;
	u16 rbcmd_report_id;
	u16 *rbcmd_rx_report;
	u16 *rbcmd_rx_report_len;

	wait_queue_head_t waitqueue_report_sysfs;
	struct rw_semaphore rwsema_report_sysfs;
	u16 report_sysfs_rx_report[RPT_LEN_MAX];
	u16 report_sysfs_rx_report_len;

	u16 rx_report[RPT_LEN_MAX]; /* with header */
	u16 rx_report_len;
	u16 rx_packet[RPT_LEN_PACKET_MAX + 1]; /* with header */
	u32 irq_count;
	u16 framecounter;
	u16 list_finger_ids;
	u8 sysfs_created;
	u8 is_raw_mode;

	u16 button0:1;
	u16 button1:1;
	u16 button2:1;
	u16 button3:1;
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void early_suspend(struct early_suspend *h);
static void late_resume(struct early_suspend *h);
#elif defined CONFIG_FB
static int fb_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data);
#endif

static int device_init(struct i2c_client *client);
static int device_deinit(struct i2c_client *client);

static int bootloader_enter(struct data *ts);
static int bootloader_exit(struct data *ts);
static int bootloader_get_crc(struct data *ts, u16 *crc16,
		u16 addr, u16 len, u16 delay);
static int bootloader_set_byte_mode(struct data *ts);
static int bootloader_erase_flash(struct data *ts);
static int bootloader_write_flash(struct data *ts, const u8 *image, u16 length);

static int process_rbcmd(struct data *ts);
static int combine_multipacketreport(struct data *ts);
static int cmd_send(struct data *ts, u16 *buf, u16 len);
static int rbcmd_send_receive(struct data *ts, u16 *cmd_buf,
		u16 cmd_len, u16 rpt_id,
		u16 *rpt_buf, u16 *rpt_len, u16 timeout);

static int process_report_sysfs(struct data *ts);

static u16 max1187x_sqrt(u32 num);
static u16 binary_search(const u16 *array, u16 len, u16 val);
static s16 max1187x_orientation(s16 x, s16 y);

static u8 init_state;
#ifdef CONFIG_MACH_LGE_L9II_COMMON
static u8 button_state;
/* Disable the continuous touch log */
u16 x_position[10];
u16 y_position[10];
#endif
/* I2C communication */
/* debug_mask |= 0x1 for I2C RX communication */
static int i2c_rx_bytes(struct data *ts, u8 *buf, u16 len)
{
	int i, ret, written;
	char debug_string[DEBUG_STRING_LEN_MAX];
	do {
		ret = i2c_master_recv(ts->client, (char *) buf, (int) len);
	} while (ret == -EAGAIN);
	if (ret < 0) {
		pr_err("I2C RX fail (%d)", ret);
		return ret;
	}

	len = ret;

	if (debugmask_if(1)) {
		pr_info("I2C RX (%d):", len);
		written = 0;
		for (i = 0; i < len; i++) {
			written += snprintf(debug_string + written, 6,
					"0x%02X,", buf[i]);
			if (written + 6 >= DEBUG_STRING_LEN_MAX) {
				pr_info("%s", debug_string);
				written = 0;
			}
		}
		if (written > 0)
			pr_info("%s", debug_string);
	}

	return len;
}

static int i2c_rx_words(struct data *ts, u16 *buf, u16 len)
{
	int i, ret, written;
	char debug_string[DEBUG_STRING_LEN_MAX];

	do {
		ret = i2c_master_recv(ts->client,
			(char *) buf, (int) (len * 2));
	} while (ret == -EAGAIN);
	if (ret < 0) {
		pr_err("I2C RX fail (%d)", ret);
		return ret;
	}

	if ((ret % 2) != 0) {
		pr_err("I2C words RX fail: odd number of bytes (%d)", ret);
		return -EIO;
	}

	len = ret/2;

	for (i = 0; i < len; i++)
		buf[i] = cpu_to_le16(buf[i]);

	if (debugmask_if(1)) {
		pr_info("I2C RX (%d):", len);
		written = 0;
		for (i = 0; i < len; i++) {
			written += snprintf(debug_string + written,
					8, "0x%04X,", buf[i]);
			if (written + 8 >= DEBUG_STRING_LEN_MAX) {
				pr_info("%s", debug_string);
				written = 0;
			}
		}
		if (written > 0)
			pr_info("%s", debug_string);
	}

	return len;
}

/* debug_mask |= 0x2 for I2C TX communication */
static int i2c_tx_bytes(struct data *ts, u8 *buf, u16 len)
{
	int i, ret, written;
	char debug_string[DEBUG_STRING_LEN_MAX];

	do {
		ret = i2c_master_send(ts->client, (char *) buf, (int) len);
	} while (ret == -EAGAIN);
	if (ret < 0) {
		pr_err("I2C TX fail (%d)", ret);
		return ret;
	}

	len = ret;

	if (debugmask_if(2)) {
		pr_info("I2C TX (%d):", len);
		written = 0;
		for (i = 0; i < len; i++) {
			written += snprintf(debug_string + written, 6,
					"0x%02X,", buf[i]);
			if (written + 6 >= DEBUG_STRING_LEN_MAX) {
				pr_info("%s", debug_string);
				written = 0;
			}
		}
		if (written > 0)
			pr_info("%s", debug_string);
	}

	return len;
}

static int i2c_tx_words(struct data *ts, u16 *buf, u16 len)
{
	int i, ret, written;
	char debug_string[DEBUG_STRING_LEN_MAX];

	for (i = 0; i < len; i++)
		buf[i] = cpu_to_le16(buf[i]);

	do {
		ret = i2c_master_send(ts->client,
			(char *) buf, (int) (len * 2));
	} while (ret == -EAGAIN);
	if (ret < 0) {
		pr_err("I2C TX fail (%d)", ret);
		return ret;
	}
	if ((ret % 2) != 0) {
		pr_err("I2C words TX fail: odd number of bytes (%d)", ret);
		return -EIO;
	}

	len = ret/2;

	if (debugmask_if(2)) {
		pr_info("I2C TX (%d):", len);
		written = 0;
		for (i = 0; i < len; i++) {
			written += snprintf(debug_string + written, 8,
					"0x%04X,", buf[i]);
			if (written + 8 >= DEBUG_STRING_LEN_MAX) {
				pr_info("%s", debug_string);
				written = 0;
			}
		}
		if (written > 0)
			pr_info("%s", debug_string);
	}

	return len;
}

/* Read report */
static int read_mtp_report(struct data *ts, u16 *buf)
{
	int words = 1, words_tx, words_rx;
	int ret = 0, remainder = 0, offset = 0;
	u16 address = 0x000A;
#ifdef CONFIG_MACH_LGE_L9II_COMMON
	if(ts->pwr_state == PWR_SLEEP)
	{
		pr_err("Report fail: Power off status");
		return -EIO;
	}
#endif

	/* read header, get size, read entire report */
	{
		words_tx = i2c_tx_words(ts, &address, 1);
		if (words_tx != 1) {
			pr_err("Report RX fail: failed to set address");
			return -EIO;
		}

		if (ts->is_raw_mode == 0) {
			words_rx = i2c_rx_words(ts, buf, 2);
			if (words_rx != 2 ||
					BYTEL(buf[0]) > RPT_LEN_PACKET_MAX) {
				ret = -EIO;
				pr_err("Report RX fail: received (%d) " \
						"expected (%d) words, " \
						"header (%04X)",
						words_rx, words, buf[0]);
				return ret;
			}

			if ((((BYTEH(buf[0])) & 0xF) == 0x1)
				&& buf[1] == 0x0800)
				ts->is_raw_mode = 1;

			words = BYTEL(buf[0]) + 1;

			words_tx = i2c_tx_words(ts, &address, 1);
			if (words_tx != 1) {
				pr_err("Report RX fail:" \
					"failed to set address");
				return -EIO;
			}

			words_rx = i2c_rx_words(ts, &buf[offset], words);
			if (words_rx != words) {
				pr_err("Report RX fail 0x%X: received (%d) " \
					"expected (%d) words",
					address, words_rx, remainder);
				return -EIO;

			}

		} else {

			words_rx = i2c_rx_words(ts, buf,
					(u16) PDATA(i2c_words));
			if (words_rx != (u16) PDATA(i2c_words) || BYTEL(buf[0])
					> RPT_LEN_PACKET_MAX) {
				ret = -EIO;
				pr_err("Report RX fail: received (%d) " \
					"expected (%d) words, header (%04X)",
					words_rx, words, buf[0]);
				return ret;
			}

			if ((((BYTEH(buf[0])) & 0xF) == 0x1)
				&& buf[1] != 0x0800)
				ts->is_raw_mode = 0;

			words = BYTEL(buf[0]) + 1;
			remainder = words;

			if (remainder - (int) PDATA(i2c_words) > 0) {
				remainder -= (int) PDATA(i2c_words);
				offset += (u16) PDATA(i2c_words);
				address += (u16) PDATA(i2c_words);
			}

			words_tx = i2c_tx_words(ts, &address, 1);
			if (words_tx != 1) {
				pr_err("Report RX fail: failed to set " \
					"address 0x%X", address);
				return -EIO;
			}

			words_rx = i2c_rx_words(ts, &buf[offset], remainder);
			if (words_rx != remainder) {
				pr_err("Report RX fail 0x%X: received (%d) " \
						"expected (%d) words",
						address, words_rx, remainder);
				return -EIO;
			}
		}
	}
	return ret;
}

/* Send command */
static int send_mtp_command(struct data *ts, u16 *buf, u16 len)
{
	u16 tx_buf[CMD_LEN_PACKET_MAX + 2]; /* with address and header */
	u16 packets, words, words_tx;
	int i, ret = 0;

	/* check basics */
	if (len < 2 || len > CMD_LEN_MAX || (buf[1] + 2) != len) {
		pr_err("Command length is not valid");
		ret = -EINVAL;
		goto err_send_mtp_command;
	}

	/* packetize and send */
	packets = len / CMD_LEN_PACKET_MAX;
	if (len % CMD_LEN_PACKET_MAX)
		packets++;
	tx_buf[0] = 0x0000;

	for (i = 0; i < packets; i++) {
		words = (i == (packets - 1)) ? (len % CMD_LEN_PACKET_MAX) : CMD_LEN_PACKET_MAX;
		tx_buf[1] = (packets << 12) | ((i + 1) << 8) | words;
		memcpy(&tx_buf[2], &buf[i * CMD_LEN_PACKET_MAX],
			BYTE_SIZE(words));
		words_tx = i2c_tx_words(ts, tx_buf, words + 2);
		if (words_tx != (words + 2)) {
			pr_err("Command TX fail: transmitted (%d) " \
				"expected (%d) words, packet (%d)",
				words_tx, words + 2, i);
			ret = -EIO;
			goto err_send_mtp_command;
		}
		len -= CMD_LEN_PACKET_MAX;
	}

	return ret;

err_send_mtp_command:
	return ret;
}

/* Integer math operations */
u16 max1187x_sqrt(u32 num)
{
	u16 mask = 0x8000;
	u16 guess = 0;
	u32 prod = 0;

	if (num < 2)
		return num;

	while (mask) {
		guess = guess ^ mask;
		prod = guess*guess;
		if (num < prod)
			guess = guess ^ mask;
		mask = mask>>1;
	}
	if (guess != 0xFFFF) {
		prod = guess*guess;
		if ((num - prod) > (prod + 2*guess + 1 - num))
			guess++;
	}

	return guess;
}

/* Returns index of element in array closest to val */
static u16 binary_search(const u16 *array, u16 len, u16 val)
{
	s16 lt, rt, mid;
	if (len < 2)
		return 0;

	lt = 0;
	rt = len - 1;

	while (lt <= rt) {
		mid = (lt + rt)/2;
		if (val == array[mid])
			return mid;
		if (val < array[mid])
			rt = mid - 1;
		else
			lt = mid + 1;
	}

	if (lt >= len)
		return len - 1;
	if (rt < 0)
		return 0;
	if (array[lt] - val > val - array[lt-1])
		return lt-1;
	else
		return lt;
}

/* Given values of x and y, it calculates the orientation
 * with respect to y axis by calculating atan(x/y)
 */
static s16 max1187x_orientation(s16 x, s16 y)
{
	u16 sign = 0;
	s16 angle;
	u16 len = sizeof(tanlist)/sizeof(tanlist[0]);
	u32 quotient;

	if (x == y) {
		angle = 45;
		return angle;
	}
	if (x == 0) {
		angle = 0;
		return angle;
	}
	if (y == 0) {
		if (x > 0)
			angle = 90;
		else
			angle = -90;
		return angle;
	}

	if (x < 0) {
		sign = ~sign;
		x = -x;
	}
	if (y < 0) {
		sign = ~sign;
		y = -y;
	}

	if (x == y)
		angle = 45;
	else if (x < y) {
		quotient = ((u32)x << 16) - (u32)x;
		quotient = quotient / y;
		angle = binary_search(tanlist, len, quotient);
	} else {
		quotient = ((u32)y << 16) - (u32)y;
		quotient = quotient / x;
		angle = binary_search(tanlist, len, quotient);
		angle = 90 - angle;
	}
	if (sign == 0)
		return angle;
	else
		return -angle;
}

/* Returns time difference between time_later and time_earlier.
 * time is measures in units of jiffies32 */
u32 time_difference(u32 time_later, u32 time_earlier)
{
	u64	time_elapsed;
	if (time_later >= time_earlier)
		time_elapsed = time_later - time_earlier;
	else
		time_elapsed = time_later +
					0x100000000 - time_earlier;
	return (u32)time_elapsed;
}
/* Check button state change and seperate touch key report */
#ifdef CONFIG_MACH_LGE_L9II_COMMON
static void touch_key_report(struct data *ts, u16 button_code, u8 button_status, u16 count)
{
	button_state = 1;

	if(count != 0) {
		button_status = KEY_CANCLED;
	}

	ts->prev_button_status = button_status;
	ts->prev_button_code = button_code;

	switch(button_status) {
		case KEY_RELEASED:
			pr_info_if(4, "(TOUCH) KEY(%d) is released \n",button_code);
			break;
		case KEY_PRESSED:
			pr_info_if(4, "(TOUCH) KEY(%d) is pressed \n",button_code);
			break;
		case KEY_CANCLED:
			pr_info_if(4, "(TOUCH) KEY(%d) is cancled \n",button_code);
			break;
		default :
			break;
	}
	input_report_key(ts->input_dev, button_code, button_status);
}
#endif

/* debug_mask |= 0x4 for touch reports */
static int process_report(struct data *ts, u16 *buf)
{
	u32 i, j;
	u16 x, y, z, swap_u16, curr_finger_ids, tool_type;

	u32 area;
	s16 swap_s16;
	u32 major_axis, minor_axis;
	s16 xsize, ysize, orientation;

#ifdef CONFIG_MACH_LGE_L9II_COMMON
	bool is_ghost_touch = false;
#endif

	struct max1187x_touch_report_header *header;
	struct max1187x_touch_report_basic *reportb;
	struct max1187x_touch_report_extended *reporte;

	header = (struct max1187x_touch_report_header *) buf;
#ifdef CONFIG_MACH_LGE_L9II_COMMON
	button_state = 0;
#endif

	if (BYTEH(header->header) != 0x11)
		goto process_report_complete;

	if (PDATA(enable_touch_wakeup) == 1) {
		if (device_may_wakeup(&ts->client->dev)
				&& ts->is_suspended == 1) {
			pr_info_if(4, "Received gesture: (0x%04X)\n", buf[3]);
			if (header->report_id == MAX1187X_REPORT_POWER_MODE
					&& buf[3] == 0x0102) {
				pr_info_if(4, "Received touch wakeup report\n");
				input_report_key(ts->input_dev,	KEY_POWER, 1);
				input_sync(ts->input_dev);
				input_report_key(ts->input_dev,	KEY_POWER, 0);
				input_sync(ts->input_dev);
			}
			goto process_report_complete;
		}
	}

	if (ts->is_suspended == 1)
		goto process_report_complete;

	if (header->report_id != MAX1187X_REPORT_TOUCH_BASIC &&
			header->report_id != MAX1187X_REPORT_TOUCH_EXTENDED)
		goto process_report_complete;

	if (ts->framecounter == header->framecounter) {
		pr_err("Same framecounter (%u) encountered at irq (%u)!\n",
				ts->framecounter, ts->irq_count);
		goto err_process_report_framecounter;
	}
	ts->framecounter = header->framecounter;

	if (header->button0 != ts->button0) {
//                              
#ifdef CONFIG_MACH_LGE_L9II_COMMON
		touch_key_report(ts, PDATA(button_code0), header->button0, header->touch_count);
#else
		input_report_key(ts->input_dev, PDATA(button_code0),
				header->button0);
#endif
//             
		input_sync(ts->input_dev);
		ts->button0 = header->button0;
	}

	if (header->button1 != ts->button1) {
//                              
#ifdef CONFIG_MACH_LGE_L9II_COMMON
		touch_key_report(ts, PDATA(button_code1), header->button1, header->touch_count);
#else
		input_report_key(ts->input_dev, PDATA(button_code1),
				header->button1);
#endif
//             
		input_sync(ts->input_dev);
		ts->button1 = header->button1;
	}
	if (header->button2 != ts->button2) {
//                              
#ifdef CONFIG_MACH_LGE_L9II_COMMON
		touch_key_report(ts, PDATA(button_code2), header->button2, header->touch_count);
#else
		input_report_key(ts->input_dev, PDATA(button_code2),
				header->button2);
#endif
//             
		input_sync(ts->input_dev);
		ts->button2 = header->button2;
	}
	if (header->button3 != ts->button3) {
//                              
#ifdef CONFIG_MACH_LGE_L9II_COMMON
		touch_key_report(ts, PDATA(button_code3), header->button3, header->touch_count);
#else
		input_report_key(ts->input_dev, PDATA(button_code3),
				header->button3);
#endif
//             
		input_sync(ts->input_dev);
		ts->button3 = header->button3;
	}

	if (header->touch_count > 10) {
		pr_err("Touch count (%u) out of bounds [0,10]!",
				header->touch_count);
		goto err_process_report_touchcount;
	}

	if (header->touch_count == 0) {
#ifdef CONFIG_MACH_LGE_L9II_COMMON
/* Disable the continuous touch log */
		j = 1;
#else
		pr_info_if(4, "(TOUCH): Finger up (all)\n");
#endif
		if (PDATA(linux_touch_protocol) == 0)
			input_mt_sync(ts->input_dev);
		else {
			for (i = 0; i < MAX1187X_TOUCH_COUNT_MAX; i++) {
				input_mt_slot(ts->input_dev, i);
				input_mt_report_slot_state(ts->input_dev,
						MT_TOOL_FINGER, 0);
#ifdef CONFIG_MACH_LGE_L9II_COMMON
/* Disable the continuous touch log */
				if (((ts->list_finger_ids & j) != 0) && !button_state)
					pr_info_if(4, "(TOUCH): Finger is released (%d) X(%d) Y(%d)\n",i,x_position[i],y_position[i]);
				j <<= 1;
#endif
			}
		}
		input_sync(ts->input_dev);
		ts->list_finger_ids = 0;
	} else {
		curr_finger_ids = 0;
		reportb = (struct max1187x_touch_report_basic *)
				((u8 *)buf + sizeof(*header));
		reporte = (struct max1187x_touch_report_extended *)
				((u8 *)buf + sizeof(*header));
		for (i = 0; i < header->touch_count; i++) {
#ifdef CONFIG_MACH_LGE_L9II_COMMON
			if (header->touch_count > 1 && reportb->z < 300) {
				reportb++;
				if (reportb->z < 300 || reportb->z > 10000) {
					pr_info_if(4, "(TOUCH) Ghost finger is happend. So ignore the touch event\n");
					is_ghost_touch = true;
					break;
				}
				else
					 header->touch_count--;
			}
#endif
			x = reportb->x;
			y = reportb->y;
			z = reportb->z;
			if (PDATA(coordinate_settings) & MAX1187X_SWAP_XY) {
				swap_u16 = x;
				x = y;
				y = swap_u16;
			}
			if (PDATA(coordinate_settings) & MAX1187X_REVERSE_X) {
				x = PDATA(panel_margin_xl) + PDATA(lcd_x)
					+ PDATA(panel_margin_xh) - 1 - x;
			}
			if (PDATA(coordinate_settings) & MAX1187X_REVERSE_Y) {
				y = PDATA(panel_margin_yl) + PDATA(lcd_y)
					+ PDATA(panel_margin_yh) - 1 - y;
			}
#ifdef CONFIG_MACH_LGE_L9II_COMMON
/* Disable the continuous touch log */
                        x_position[reportb->finger_id] = x;
                        y_position[reportb->finger_id] = y;
#endif

			tool_type = reportb->tool_type;
			if (tool_type == 1)
				tool_type = MT_TOOL_PEN;
			else
				tool_type = MT_TOOL_FINGER;

			curr_finger_ids |= (1<<reportb->finger_id);
			if (PDATA(linux_touch_protocol) == 0) {
				input_report_abs(ts->input_dev,
				ABS_MT_TRACKING_ID,	reportb->finger_id);
				input_report_abs(ts->input_dev,
				ABS_MT_TOOL_TYPE, tool_type);
			} else {
				input_mt_slot(ts->input_dev,
					reportb->finger_id);
				input_mt_report_slot_state(ts->input_dev,
						tool_type, 1);
			}
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
			input_report_abs(ts->input_dev,	ABS_MT_POSITION_Y, y);
			if (PDATA(enable_pressure_shaping) == 1) {
#ifdef CONFIG_MACH_LGE_L9II_COMMON
				z = max1187x_sqrt(z);
#else
				z = (PRESSURE_MAX_SQRT >> 2) + max1187x_sqrt(z);
#endif
				if (z > PRESSURE_MAX_SQRT)
					z = PRESSURE_MAX_SQRT;
			}
			input_report_abs(ts->input_dev,
					ABS_MT_PRESSURE, z);

#ifdef CONFIG_MACH_LGE_L9II_COMMON
/* Disable the continuous touch log */
			if (ts->prev_touch_count < header->touch_count)
#endif
				pr_info_if(4, "(TOUCH): (%u) Finger %u: "\
				"X(%d) Y(%d) Z(%d) TT(%d)",
				header->framecounter, reportb->finger_id,
				x, y, z, tool_type);

			if (header->report_id
				== MAX1187X_REPORT_TOUCH_EXTENDED) {
				if (PDATA(max1187x_report_mode) == 2) {
					if (PDATA(coordinate_settings)
						& MAX1187X_SWAP_XY) {
						swap_s16 =
							reporte->xpixel;
						reporte->xpixel =
							reporte->ypixel;
						reporte->ypixel =
							swap_s16;
					}
					if (PDATA(coordinate_settings)
							& MAX1187X_REVERSE_X)
						reporte->xpixel =
							-reporte->xpixel;
					if (PDATA(coordinate_settings)
							& MAX1187X_REVERSE_Y)
						reporte->ypixel =
							-reporte->ypixel;
					area = reporte->area
						* (PDATA(lcd_x)/
							PDATA(num_sensor_x))
						* (PDATA(lcd_y)/
							PDATA(num_sensor_y));
					xsize = reporte->xpixel
					* (s16)(PDATA(lcd_x)/
						PDATA(num_sensor_x));
					ysize = reporte->ypixel
					* (s16)(PDATA(lcd_y)/
						PDATA(num_sensor_y));
					pr_info_if(4, "(TOUCH): pixelarea " \
						"(%u) xpixel (%d) ypixel " \
						"(%d) xsize (%d) ysize (%d)\n",
						reporte->area,
						reporte->xpixel,
						reporte->ypixel,
						xsize, ysize);

				if (PDATA(enable_fast_calculation) == 0) {
					/* Calculate orientation as
					 * arctan of xsize/ysize) */
					orientation =
						max1187x_orientation(
							xsize, ysize);
					/* Major axis of ellipse if hypotenuse
					 * formed by xsize and ysize */
					major_axis = xsize*xsize + ysize*ysize;
					major_axis = max1187x_sqrt(major_axis);
					/* Minor axis can be reverse calculated
					 * using the area of ellipse:
					 * Area of ellipse =
					 *	pi / 4 * Major axis * Minor axis
					 * Minor axis =
					 *	4 * Area / (pi * Major axis)
					 */
					minor_axis = (2 * area) / major_axis;
					minor_axis = (minor_axis<<17)
							/ MAX1187X_PI;
				} else {
					if (xsize < 0)
						xsize = -xsize;
					if (ysize < 0)
						ysize = -ysize;
					orientation = (xsize > ysize) ? 0 : 90;
					major_axis = (xsize > ysize)
							? xsize : ysize;
					minor_axis = (xsize > ysize)
							? ysize : xsize;
				}
					pr_info_if(4, "(TOUCH): Finger %u: " \
						"Orientation(%d) Area(%u) Major_axis(%u) Minor_axis(%u)",
						reportb->finger_id,
						orientation, area,
						major_axis, minor_axis);
					input_report_abs(ts->input_dev,
						ABS_MT_ORIENTATION,
						orientation);
					input_report_abs(ts->input_dev,
							ABS_MT_TOUCH_MAJOR,
							major_axis);
					input_report_abs(ts->input_dev,
							ABS_MT_TOUCH_MINOR,
							minor_axis);

				}
				reporte++;
				reportb = (struct max1187x_touch_report_basic *)
						((u8 *) reporte);
			} else {
				reportb++;
			}
			if (PDATA(linux_touch_protocol) == 0)
				input_mt_sync(ts->input_dev);
		}

		i = 0;
		j = 1;
		while (i < 10) {
			if ((ts->list_finger_ids & j) != 0 &&
					(curr_finger_ids & j) == 0
#ifdef CONFIG_MACH_LGE_L9II_COMMON
					&& !is_ghost_touch
#endif
			) {
#ifdef CONFIG_MACH_LGE_L9II_COMMON
/* Disable the continuous touch log */
				pr_info_if(4, "(TOUCH): Finger up (%d) X(%d) Y(%d)\n", i,x_position[i],y_position[i]);
#else
				pr_info_if(4, "(TOUCH): Finger up (%d)\n", i);
#endif
				if (PDATA(linux_touch_protocol) != 0) {
					input_mt_slot(ts->input_dev, i);
					input_mt_report_slot_state(
							ts->input_dev,
							MT_TOOL_FINGER, 0);
				}
			}
			i++;
			j <<= 1;
		}

		input_sync(ts->input_dev);
		ts->list_finger_ids = curr_finger_ids;
	}
#ifdef CONFIG_MACH_LGE_L9II_COMMON
/* Disable the continuous touch log */
	ts->prev_touch_count = header->touch_count;
#endif
process_report_complete:
	return 0;

err_process_report_framecounter:
err_process_report_touchcount:
	return -EIO;
}

/* debug_mask |= 0x20 for irq_handler */
static irqreturn_t irq_handler_soft(int irq, void *context)
{
	struct data *ts = (struct data *) context;
	int ret;
	u32	time_elapsed;

	pr_info_if(0x20, "Enter\n");

	mutex_lock(&ts->i2c_mutex);

	if (gpio_get_value(ts->pdata->gpio_tirq) != 0)
		goto irq_handler_soft_complete;

	ret = read_mtp_report(ts, ts->rx_packet);

	time_elapsed = time_difference(jiffies, ts->irq_receive_time);

	/* Verify time_elapsed < 1s */
	if (ret == 0 && time_elapsed < HZ) {
		ret = combine_multipacketreport(ts);
		if (ret == 0)
			ret = process_report(ts, ts->rx_packet);
		if (ret == 0)
			ret = process_rbcmd(ts);
		if (ret == 0)
			ret = process_report_sysfs(ts);
	}

irq_handler_soft_complete:
	mutex_unlock(&ts->i2c_mutex);
#ifndef CONFIG_MACH_LGE_L9II_COMMON
	atomic_set(&ts->irq_processing, 0);
#endif
	pr_info_if(0x20, "Exit\n");
	return IRQ_HANDLED;
}

static irqreturn_t irq_handler_hard(int irq, void *context)
{
	struct data *ts = (struct data *) context;

	pr_info_if(0x20, "Enter\n");

#ifndef CONFIG_MACH_LGE_L9II_COMMON
	if (atomic_read(&ts->irq_processing) == 1)
		goto irq_handler_hard_complete;
#endif

	if (gpio_get_value(ts->pdata->gpio_tirq) != 0)
		goto irq_handler_hard_complete;

#ifndef CONFIG_MACH_LGE_L9II_COMMON
	atomic_set(&ts->irq_processing, 1);
#endif

	ts->irq_receive_time = jiffies;
	ts->irq_count++;

	pr_info_if(0x20, "Exit\n");
	return IRQ_WAKE_THREAD;

irq_handler_hard_complete:
	pr_info_if(0x20, "Exit\n");
	return IRQ_HANDLED;
}

static ssize_t init_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", init_state);
}

static ssize_t init_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	int value, ret;

	if (sscanf(buf, "%d", &value) != 1) {
		pr_err("bad parameter");
		return -EINVAL;
	}
	switch (value) {
	case 0:
		if (init_state == 0)
			break;
		ret = device_deinit(to_i2c_client(dev));
		if (ret != 0) {
			pr_err("deinit error (%d)", ret);
			return ret;
		}
		break;
	case 1:
		if (init_state == 1)
			break;
		ret = device_init(to_i2c_client(dev));
		if (ret != 0) {
			pr_err("init error (%d)", ret);
			return ret;
		}
		break;
	case 2:
		if (init_state == 1) {
			ret = device_deinit(to_i2c_client(dev));
			if (ret != 0) {
				pr_err("deinit error (%d)", ret);
				return ret;
			}
		}
		ret = device_init(to_i2c_client(dev));
		if (ret != 0) {
			pr_err("init error (%d)", ret);
			return ret;
		}
		break;
	default:
		pr_err("bad value");
		return -EINVAL;
	}

	return count;
}

static ssize_t sreset_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct data *ts = i2c_get_clientdata(client);
	u16 cmd_buf[] = {0x00E9, 0x0000};
	/* Report should be of length + 1 < 10 */
	u16 rpt_buf[10], rpt_len;
	int ret;

	ret = rbcmd_send_receive(ts, cmd_buf, 2, 0x01A0,
			rpt_buf, &rpt_len, 3 * HZ);
	if (ret)
		pr_err("Failed to do soft reset.");
	return count;
}

static ssize_t irq_count_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct data *ts = i2c_get_clientdata(client);

	return snprintf(buf, PAGE_SIZE, "%u\n", ts->irq_count);
}

static ssize_t irq_count_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct data *ts = i2c_get_clientdata(client);

	ts->irq_count = 0;
	return count;
}

static ssize_t dflt_cfg_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct data *ts = i2c_get_clientdata(client);

	return snprintf(buf, PAGE_SIZE, "%u 0x%x 0x%x\n", PDATA(defaults_allow),
			PDATA(default_config_id), PDATA(default_chip_id));
}

static ssize_t dflt_cfg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct data *ts = i2c_get_clientdata(client);

	(void) sscanf(buf, "%u 0x%x 0x%x", &PDATA(defaults_allow),
			&PDATA(default_config_id), &PDATA(default_chip_id));
	return count;
}

static ssize_t panel_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct data *ts = i2c_get_clientdata(client);

	return snprintf(buf, PAGE_SIZE, "%u %u %u %u %u %u\n",
			PDATA(panel_margin_xl), PDATA(panel_margin_xh),
			PDATA(panel_margin_yl), PDATA(panel_margin_yh),
			PDATA(lcd_x), PDATA(lcd_y));
}

static ssize_t panel_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct data *ts = i2c_get_clientdata(client);

	(void) sscanf(buf, "%u %u %u %u %u %u", &PDATA(panel_margin_xl),
			&PDATA(panel_margin_xh), &PDATA(panel_margin_yl),
			&PDATA(panel_margin_yh), &PDATA(lcd_x),
			&PDATA(lcd_y));
	return count;
}

static ssize_t fw_ver_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct data *ts = i2c_get_clientdata(client);

	int ret, count = 0;
	u16 cmd_buf[2];
	/* Both reports should be of length + 1 < 100 */
	u16 rpt_buf[100], rpt_len;
	u16 cst_info_addr = 0;

	/* Read firmware version */
	cmd_buf[0] = 0x0040;
	cmd_buf[1] = 0x0000;

	ret = rbcmd_send_receive(ts, cmd_buf, 2, 0x0140,
			rpt_buf, &rpt_len, HZ/4);

	if (ret)
		goto err_fw_ver_show;

	ts->chip_id = BYTEH(rpt_buf[4]);
	count += snprintf(buf, PAGE_SIZE, "fw_ver (%u.%u.%u) " \
					"chip_id (0x%02X)\n",
					BYTEH(rpt_buf[3]),
					BYTEL(rpt_buf[3]),
					rpt_buf[5],
					ts->chip_id);

	/* Read touch configuration */
	cmd_buf[0] = 0x0002;
	cmd_buf[1] = 0x0000;

	ret = rbcmd_send_receive(ts, cmd_buf, 2, 0x0102,
			rpt_buf, &rpt_len, HZ/4);

	if (ret) {
		pr_err("Failed to receive chip config\n");
		goto err_fw_ver_show;
	}

	ts->config_id = rpt_buf[3];

	count += snprintf(buf + count, PAGE_SIZE, "config_id (0x%04X) ",
					ts->config_id);
	switch (ts->chip_id) {
	case 0x55:
	case 0x57:
		cst_info_addr = 42;
		break;
	case 0x72:
	case 0x73:
	case 0x74:
	case 0x75:
		cst_info_addr = 58;
		break;
	default:
		break;
	}
	if (cst_info_addr != 0)
		count += snprintf(buf + count, PAGE_SIZE,
				"customer_info[1:0] (0x%04X, 0x%04X)\n",
				rpt_buf[cst_info_addr + 1],
				rpt_buf[cst_info_addr]);
	return count;

err_fw_ver_show:
	return count;
}
#ifdef CONFIG_MACH_LGE_L9II_COMMON
/* Sysfs - for touch firmware version */
static ssize_t device_fw_ver_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
        struct i2c_client *client = to_i2c_client(dev);
        struct data *ts = i2c_get_clientdata(client);

	return snprintf(buf, PAGE_SIZE, "%u\n", ts->fw_version);
}
#endif

static ssize_t driver_ver_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "3.2.3: May 29, 2013\n");
}

static ssize_t debug_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%04X\n", debug_mask);
}

static ssize_t debug_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	if (sscanf(buf, "%hx", &debug_mask) != 1) {
		pr_err("bad parameter");
		return -EINVAL;
	}

	return count;
}

static ssize_t command_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct data *ts = i2c_get_clientdata(client);
	u16 buffer[CMD_LEN_MAX];
	char scan_buf[5];
	int i, ret;

	count--; /* ignore carriage return */
	if ((count % 4) != 0) {
		pr_err("words not properly defined");
		return -EINVAL;
	}
	scan_buf[4] = '\0';
	for (i = 0; i < count; i += 4) {
		memcpy(scan_buf, &buf[i], 4);
		if (sscanf(scan_buf, "%hx", &buffer[i / 4]) != 1) {
			pr_err("bad word (%s)", scan_buf);
			return -EINVAL;
		}
	}
	ret = cmd_send(ts, buffer, count / 4);
	if (ret)
		pr_err("MTP command failed");
	return ++count;
}

static int process_report_sysfs(struct data *ts)
{
	down_write(&ts->rwsema_report_sysfs);
	ts->report_sysfs_rx_report_len = ts->rx_report_len;
	memcpy(ts->report_sysfs_rx_report, ts->rx_report,
			(ts->rx_report_len + 1)<<1);
	up_write(&ts->rwsema_report_sysfs);
	wake_up_interruptible(&ts->waitqueue_report_sysfs);
	return 0;
}

static ssize_t report_read(struct file *file, struct kobject *kobj,
	struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
	struct i2c_client *client = kobj_to_i2c_client(kobj);
	struct data *ts = i2c_get_clientdata(client);
	int printed;
	u32 i, offset = 0;
	u32 payload, num_term_char, return_len;

	DEFINE_WAIT(wait);
	prepare_to_wait(&ts->waitqueue_report_sysfs, &wait, TASK_INTERRUPTIBLE);
	schedule();
	finish_wait(&ts->waitqueue_report_sysfs, &wait);
	if (signal_pending(current))
		goto err_report_read_signal;

	down_read(&ts->rwsema_report_sysfs);

	payload = ts->report_sysfs_rx_report_len;
	num_term_char = 2; /* number of term char */
	return_len = 4 * payload + num_term_char;

	if (count < return_len)
		goto err_report_read_count;

	if (count > return_len)
		count = return_len;

	for (i = 1; i <= payload; i++) {
		printed = snprintf(&buf[offset], PAGE_SIZE, "%04X\n",
			ts->report_sysfs_rx_report[i]);
		if (printed <= 0)
			goto err_report_read_snprintf;
		offset += printed - 1;
	}
	snprintf(&buf[offset], PAGE_SIZE, ",\n");

	up_read(&ts->rwsema_report_sysfs);
	return count;

err_report_read_count:
err_report_read_snprintf:
	up_read(&ts->rwsema_report_sysfs);
	return -EIO;
err_report_read_signal:
	return -ERESTARTSYS;
}

static DEVICE_ATTR(init, S_IRUGO | S_IWUSR, init_show, init_store);
static DEVICE_ATTR(sreset, S_IWUSR, NULL, sreset_store);
static DEVICE_ATTR(irq_count, S_IRUGO | S_IWUSR, irq_count_show,
		irq_count_store);
static DEVICE_ATTR(dflt_cfg, S_IRUGO | S_IWUSR, dflt_cfg_show, dflt_cfg_store);
static DEVICE_ATTR(panel, S_IRUGO | S_IWUSR, panel_show, panel_store);
static DEVICE_ATTR(fw_ver, S_IRUGO, fw_ver_show, NULL);
static DEVICE_ATTR(driver_ver, S_IRUGO, driver_ver_show, NULL);
static DEVICE_ATTR(debug, S_IRUGO | S_IWUSR, debug_show, debug_store);
static DEVICE_ATTR(command, S_IWUSR, NULL, command_store);
#ifdef CONFIG_MACH_LGE_L9II_COMMON
/* Sysfs - for touch firmware version */
static DEVICE_ATTR(version, 0444, device_fw_ver_show, NULL);
#endif
static struct bin_attribute dev_attr_report = {
		.attr = {.name = "report", .mode = S_IRUGO},
		.read = report_read };

static struct device_attribute *dev_attrs[] = {
		&dev_attr_sreset,
		&dev_attr_irq_count,
		&dev_attr_dflt_cfg,
		&dev_attr_panel,
		&dev_attr_fw_ver,
		&dev_attr_driver_ver,
		&dev_attr_debug,
		&dev_attr_command,
#ifdef CONFIG_MACH_LGE_L9II_COMMON
/* Sysfs - for touch firmware version */
		&dev_attr_version,
#endif
		NULL };

/* Send command to chip.
 */
static int cmd_send(struct data *ts, u16 *buf, u16 len)
{
	int ret;

	mutex_lock(&ts->i2c_mutex);
	ret = send_mtp_command(ts, buf, len);
	mutex_unlock(&ts->i2c_mutex);

	return ret;
}

/* Send command to chip and expect a report with
 * id == rpt_id within timeout time.
 * timeout is measured in jiffies. 1s = HZ jiffies
 */
/* debug_mask |= 0x40 for all rbcmd */
static int rbcmd_send_receive(struct data *ts, u16 *cmd_buf,
		u16 cmd_len, u16 rpt_id,
		u16 *rpt_buf, u16 *rpt_len, u16 timeout)
{
	int ret;
	pr_info_if(0x40, "Enter\n");
	ret = down_interruptible(&ts->sema_rbcmd);
	if (ret != 0)
		goto err_rbcmd_send_receive_sema_rbcmd;

	ts->rbcmd_report_id = rpt_id;
	ts->rbcmd_rx_report = rpt_buf;
	ts->rbcmd_rx_report_len = rpt_len;
	ts->rbcmd_received = 0;
	ts->rbcmd_waiting = 1;

	ret = cmd_send(ts, cmd_buf, cmd_len);
	if (ret)
		goto err_rbcmd_send_receive_cmd_send;

	ret = wait_event_interruptible_timeout(ts->waitqueue_rbcmd,
			ts->rbcmd_received != 0, timeout);
	if (ret < 0 || ts->rbcmd_received == 0)
		goto err_rbcmd_send_receive_timeout;

	pr_info_if(0x40, "Received report_ID (0x%04X) " \
					"report_len (%d)\n",
					ts->rbcmd_report_id,
					*ts->rbcmd_rx_report_len);

	ts->rbcmd_waiting = 0;
	up(&ts->sema_rbcmd);
	pr_info_if(0x40, "Exit");
	return 0;

err_rbcmd_send_receive_timeout:
	pr_info_if(0x40, "Timed out waiting for report_ID (0x%04X)\n",
				ts->rbcmd_report_id);
err_rbcmd_send_receive_cmd_send:
	ts->rbcmd_waiting = 0;
	up(&ts->sema_rbcmd);
err_rbcmd_send_receive_sema_rbcmd:
	pr_info_if(0x40, "Exit\n");
	return -ERESTARTSYS;
}

/* debug_mask |= 0x8 for all driver INIT */
static int read_chip_data(struct data *ts)
{
	int ret;
	u16 loopcounter;
	u16 cmd_buf[2];
	/* Both reports should be of length + 1 < 100 */
	u16 rpt_buf[100], rpt_len;

	/* Read firmware version */
	cmd_buf[0] = 0x0040;
	cmd_buf[1] = 0x0000;

	loopcounter = 0;
	ret = -1;
	while (loopcounter < MAX_FW_RETRIES && ret != 0) {
		ret = rbcmd_send_receive(ts, cmd_buf, 2, 0x0140,
				rpt_buf, &rpt_len, HZ/4);
		loopcounter++;
	}

	if (ret) {
		pr_err("Failed to receive fw version\n");
		goto err_read_chip_data;
	}

	ts->chip_id = BYTEH(rpt_buf[4]);
	pr_info_if(8, "(INIT): fw_ver (%u.%u) " \
					"chip_id (0x%02X)\n",
					BYTEH(rpt_buf[3]),
					BYTEL(rpt_buf[3]),
					ts->chip_id);

	/* Read touch configuration */
	cmd_buf[0] = 0x0002;
	cmd_buf[1] = 0x0000;

#ifdef CONFIG_MACH_LGE_L9II_COMMON
	ts->fw_version = rpt_buf[5];
#endif

	loopcounter = 0;
	ret = -1;
	while (loopcounter < MAX_FW_RETRIES && ret != 0) {
		ret = rbcmd_send_receive(ts, cmd_buf, 2, 0x0102,
				rpt_buf, &rpt_len, HZ/4);
		loopcounter++;
	}

	if (ret) {
		pr_err("Failed to receive chip config\n");
		goto err_read_chip_data;
	}

	ts->config_id = rpt_buf[3];
#ifdef CONFIG_MACH_LGE_L9II_COMMON
/* use the config)id section to check the touch fw version */
	if (ts->config_id < 200)
		ts->fw_version = ts->config_id;
#endif

	pr_info_if(8, "(INIT): config_id (0x%04X)\n",
					ts->config_id);
	return 0;

err_read_chip_data:
	return ret;
}

static int device_fw_load(struct data *ts, const struct firmware *fw,
	u16 fw_index)
{
	u16 filesize, file_codesize, loopcounter;
	u16 file_crc16_1, file_crc16_2, local_crc16;
	int chip_crc16_1 = -1, chip_crc16_2 = -1, ret;

	filesize = PDATA(fw_mapping[fw_index]).filesize;
	file_codesize = PDATA(fw_mapping[fw_index]).file_codesize;

	if (fw->size != filesize) {
		pr_err("filesize (%d) is not equal to expected size (%d)",
				fw->size, filesize);
		return -EIO;
	}

	file_crc16_1 = crc16(0, fw->data, file_codesize);

	loopcounter = 0;
	do {
		ret = bootloader_enter(ts);
		if (ret == 0)
			ret = bootloader_get_crc(ts, &local_crc16,
				0, file_codesize, 200);
		if (ret == 0)
			chip_crc16_1 = local_crc16;
		ret = bootloader_exit(ts);
		loopcounter++;
	} while (loopcounter < MAX_FW_RETRIES && chip_crc16_1 == -1);

	pr_info_if(8, "(INIT): file_crc16_1 = 0x%04x, chip_crc16_1 = 0x%04x\n",
			file_crc16_1, chip_crc16_1);

	if (file_crc16_1 != chip_crc16_1) {
		loopcounter = 0;
		file_crc16_2 = crc16(0, fw->data, filesize);

		while (loopcounter < MAX_FW_RETRIES && file_crc16_2
				!= chip_crc16_2) {
			pr_info_if(8, "(INIT): Reprogramming chip. Attempt %d",
					loopcounter+1);
			ret = bootloader_enter(ts);
			if (ret == 0)
				ret = bootloader_erase_flash(ts);
			if (ret == 0)
				ret = bootloader_set_byte_mode(ts);
			if (ret == 0)
				ret = bootloader_write_flash(ts, fw->data,
					filesize);
			if (ret == 0)
				ret = bootloader_get_crc(ts, &local_crc16,
					0, filesize, 200);
			if (ret == 0)
				chip_crc16_2 = local_crc16;
			pr_info_if(8, "(INIT): file_crc16_2 = 0x%04x, "\
					"chip_crc16_2 = 0x%04x\n",
					file_crc16_2, chip_crc16_2);
			ret = bootloader_exit(ts);
			loopcounter++;
		}

		if (file_crc16_2 != chip_crc16_2)
			return -EAGAIN;
	}

	loopcounter = 0;
	do {
		ret = bootloader_exit(ts);
		loopcounter++;
	} while (loopcounter < MAX_FW_RETRIES && ret != 0);

	if (ret != 0)
		return -EIO;

	return 0;
}

static void validate_fw(struct data *ts)
{
	const struct firmware *fw;
	u16 config_id, chip_id;
	int i, ret;
	u16 cmd_buf[3];

	ret = read_chip_data(ts);
	if (ret && PDATA(defaults_allow) == 0) {
		pr_err("Firmware is not responsive "\
				"and default update is disabled\n");
		return;
	}

	if (ts->chip_id != 0)
		chip_id = ts->chip_id;
	else
		chip_id = PDATA(default_chip_id);

	if (ts->config_id != 0)
		config_id = ts->config_id;
	else
		config_id = PDATA(default_config_id);

	if (PDATA(enable_fw_download) == 1) {
		for (i = 0; i < PDATA(num_fw_mappings); i++) {
//                                                                   
#ifdef CONFIG_MACH_LGE_L9II_COMMON
		if (PDATA(fw_mapping[i]).chip_id == chip_id)
#else
			if (PDATA(fw_mapping[i]).config_id == config_id &&
				PDATA(fw_mapping[i]).chip_id == chip_id)
#endif 			
//                                                  
				break;
		}

		if (i == PDATA(num_fw_mappings)) {
			pr_err("FW not found for configID(0x%04X) and chipID(0x%04X)",
			config_id, chip_id);
		return;
	}

	pr_info_if(8, "(INIT): Firmware file (%s)",
		PDATA(fw_mapping[i]).filename);

	ret = request_firmware(&fw, PDATA(fw_mapping[i]).filename,
					&ts->client->dev);

		if (ret || fw == NULL) {
			pr_err("firmware request failed (ret = %d, fwptr = %p)",
				ret, fw);
			return;
		}

		mutex_lock(&ts->i2c_mutex);
		disable_irq(ts->client->irq);
		if (device_fw_load(ts, fw, i)) {
			release_firmware(fw);
			pr_err("firmware download failed");
			enable_irq(ts->client->irq);
			mutex_unlock(&ts->i2c_mutex);
			return;
		}

		release_firmware(fw);
		pr_info_if(8, "(INIT): firmware okay\n");
		enable_irq(ts->client->irq);
		mutex_unlock(&ts->i2c_mutex);
		ret = read_chip_data(ts);
	}

	cmd_buf[0] = 0x0018;
	cmd_buf[1] = 0x0001;
	cmd_buf[2] = PDATA(max1187x_report_mode);
	ret = cmd_send(ts, cmd_buf, 3);
	if (ret) {
		pr_err("Failed to set up touch report mode");
		return;
	}
}

/* #ifdef CONFIG_OF */
static struct max1187x_pdata *max1187x_get_platdata_dt(struct device *dev)
{
	struct max1187x_pdata *pdata = NULL;
	struct device_node *devnode = dev->of_node;
	u32 i;
	u32 datalist[MAX1187X_NUM_FW_MAPPINGS_MAX];

	if (!devnode)
		return NULL;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		pr_err("Failed to allocate memory for pdata\n");
		return NULL;
	}

	/* Parse gpio_tirq */
	if (of_property_read_u32(devnode, "gpio_tirq", &pdata->gpio_tirq)) {
		pr_err("Failed to get property: gpio_tirq\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse num_fw_mappings */
	if (of_property_read_u32(devnode, "num_fw_mappings",
		&pdata->num_fw_mappings)) {
		pr_err("Failed to get property: num_fw_mappings\n");
		goto err_max1187x_get_platdata_dt;
	}

	if (pdata->num_fw_mappings > MAX1187X_NUM_FW_MAPPINGS_MAX)
		pdata->num_fw_mappings = MAX1187X_NUM_FW_MAPPINGS_MAX;

	/* Parse config_id */
	if (of_property_read_u32_array(devnode, "config_id", datalist,
			pdata->num_fw_mappings)) {
		pr_err("Failed to get property: config_id\n");
		goto err_max1187x_get_platdata_dt;
	}

	for (i = 0; i < pdata->num_fw_mappings; i++)
		pdata->fw_mapping[i].config_id = datalist[i];

	/* Parse chip_id */
	if (of_property_read_u32_array(devnode, "chip_id", datalist,
			pdata->num_fw_mappings)) {
		pr_err("Failed to get property: chip_id\n");
		goto err_max1187x_get_platdata_dt;
	}

	for (i = 0; i < pdata->num_fw_mappings; i++)
		pdata->fw_mapping[i].chip_id = datalist[i];

	/* Parse filename */
	for (i = 0; i < pdata->num_fw_mappings; i++) {
		if (of_property_read_string_index(devnode, "filename", i,
			(const char **) &pdata->fw_mapping[i].filename)) {
				pr_err("Failed to get property: "\
					"filename[%d]\n", i);
				goto err_max1187x_get_platdata_dt;
			}
	}

	/* Parse filesize */
	if (of_property_read_u32_array(devnode, "filesize", datalist,
		pdata->num_fw_mappings)) {
		pr_err("Failed to get property: filesize\n");
		goto err_max1187x_get_platdata_dt;
	}

	for (i = 0; i < pdata->num_fw_mappings; i++)
		pdata->fw_mapping[i].filesize = datalist[i];

	/* Parse file_codesize */
	if (of_property_read_u32_array(devnode, "file_codesize", datalist,
		pdata->num_fw_mappings)) {
		pr_err("Failed to get property: file_codesize\n");
		goto err_max1187x_get_platdata_dt;
	}

	for (i = 0; i < pdata->num_fw_mappings; i++)
		pdata->fw_mapping[i].file_codesize = datalist[i];

	/* Parse defaults_allow */
	if (of_property_read_u32(devnode, "defaults_allow",
		&pdata->defaults_allow)) {
		pr_err("Failed to get property: defaults_allow\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse default_config_id */
	if (of_property_read_u32(devnode, "default_config_id",
		&pdata->default_config_id)) {
		pr_err("Failed to get property: default_config_id\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse default_chip_id */
	if (of_property_read_u32(devnode, "default_chip_id",
		&pdata->default_chip_id)) {
		pr_err("Failed to get property: default_chip_id\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse i2c_words */
	if (of_property_read_u32(devnode, "i2c_words", &pdata->i2c_words)) {
		pr_err("Failed to get property: i2c_words\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse coordinate_settings */
	if (of_property_read_u32(devnode, "coordinate_settings",
		&pdata->coordinate_settings)) {
		pr_err("Failed to get property: coordinate_settings\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse panel_margin_xl */
	if (of_property_read_u32(devnode, "panel_margin_xl",
		&pdata->panel_margin_xl)) {
		pr_err("Failed to get property: panel_margin_xl\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse lcd_x */
	if (of_property_read_u32(devnode, "lcd_x", &pdata->lcd_x)) {
		pr_err("Failed to get property: lcd_x\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse panel_margin_xh */
	if (of_property_read_u32(devnode, "panel_margin_xh",
		&pdata->panel_margin_xh)) {
		pr_err("Failed to get property: panel_margin_xh\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse panel_margin_yl */
	if (of_property_read_u32(devnode, "panel_margin_yl",
		&pdata->panel_margin_yl)) {
		pr_err("Failed to get property: panel_margin_yl\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse lcd_y */
	if (of_property_read_u32(devnode, "lcd_y", &pdata->lcd_y)) {
		pr_err("Failed to get property: lcd_y\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse panel_margin_yh */
	if (of_property_read_u32(devnode, "panel_margin_yh",
		&pdata->panel_margin_yh)) {
		pr_err("Failed to get property: panel_margin_yh\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse row_count */
	if (of_property_read_u32(devnode, "num_sensor_x",
		&pdata->num_sensor_x)) {
		pr_err("Failed to get property: num_sensor_x\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse num_sensor_y */
	if (of_property_read_u32(devnode, "num_sensor_y",
		&pdata->num_sensor_y)) {
		pr_err("Failed to get property: num_sensor_y\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse button_code0 */
	if (of_property_read_u32(devnode, "button_code0",
		&pdata->button_code0)) {
		pr_err("Failed to get property: button_code0\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse button_code1 */
	if (of_property_read_u32(devnode, "button_code1",
		&pdata->button_code1)) {
		pr_err("Failed to get property: button_code1\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse button_code2 */
	if (of_property_read_u32(devnode, "button_code2",
		&pdata->button_code2)) {
		pr_err("Failed to get property: button_code2\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse button_code3 */
	if (of_property_read_u32(devnode, "button_code3",
		&pdata->button_code3)) {
		pr_err("Failed to get property: button_code3\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse linux_touch_protocol */
	if (of_property_read_u32(devnode, "linux_touch_protocol",
		&pdata->linux_touch_protocol)) {
		pr_err("Failed to get property: linux_touch_protocol\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse max1187x_report_mode */
	if (of_property_read_u32(devnode, "max1187x_report_mode",
		&pdata->max1187x_report_mode)) {
		pr_err("Failed to get property: max1187x_report_mode\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse enable_touch_wakeup */
	if (of_property_read_u32(devnode, "enable_touch_wakeup",
		&pdata->enable_touch_wakeup)) {
		pr_err("Failed to get property: enable_touch_wakeup\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse enable_pressure_shaping */
	if (of_property_read_u32(devnode, "enable_pressure_shaping",
		&pdata->enable_pressure_shaping)) {
		pr_err("Failed to get property: enable_pressure_shaping\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse enable_fast_calculation */
	if (of_property_read_u32(devnode, "enable_fast_calculation",
		&pdata->enable_fast_calculation)) {
		pr_err("Failed to get property: enable_fast_calculation\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse enable_fw_download */
	if (of_property_read_u32(devnode, "enable_fw_download",
		&pdata->enable_fw_download)) {
		pr_err("Failed to get property: enable_fw_download\n");
		goto err_max1187x_get_platdata_dt;
	}

	return pdata;

err_max1187x_get_platdata_dt:
	devm_kfree(dev, pdata);
	return NULL;
}
/*
#else
static inline struct max1187x_pdata *
	max1187x_get_platdata_dt(struct device *dev)
{
	return NULL;
}
#endif
*/

static int validate_pdata(struct max1187x_pdata *pdata)
{
	if (pdata == NULL) {
		pr_err("Platform data not found!\n");
		goto err_validate_pdata;
	}

	if (pdata->gpio_tirq == 0) {
		pr_err("gpio_tirq (%u) not defined!\n", pdata->gpio_tirq);
		goto err_validate_pdata;
	}

	if (pdata->lcd_x < 480 || pdata->lcd_x > 0x7FFF) {
		pr_err("lcd_x (%u) out of range!\n", pdata->lcd_x);
		goto err_validate_pdata;
	}

	if (pdata->lcd_y < 240 || pdata->lcd_y > 0x7FFF) {
		pr_err("lcd_y (%u) out of range!\n", pdata->lcd_y);
		goto err_validate_pdata;
	}

	if (pdata->num_sensor_x == 0 || pdata->num_sensor_x > 40) {
		pr_err("num_sensor_x (%u) out of range!\n",
				pdata->num_sensor_x);
		goto err_validate_pdata;
	}

	if (pdata->num_sensor_y == 0 || pdata->num_sensor_y > 40) {
		pr_err("num_sensor_y (%u) out of range!\n",
				pdata->num_sensor_y);
		goto err_validate_pdata;
	}

	if (pdata->linux_touch_protocol > 1) {
		pr_err("linux_touch_protocol (%u) out of range!\n",
				pdata->linux_touch_protocol);
		goto err_validate_pdata;
	}

	if (pdata->max1187x_report_mode == 0 ||
			pdata->max1187x_report_mode > 2) {
		pr_err("max1187x_report_mode (%u) out of range!\n",
				pdata->max1187x_report_mode);
		goto err_validate_pdata;
	}

	return 0;

err_validate_pdata:
	return -ENXIO;
}

static int max1187x_chip_init(struct max1187x_pdata *pdata, int value)
{
	int  ret;

	if (value) {
#ifdef CONFIG_MACH_LGE_L9II_COMMON
		ret = gpio_request(pdata->gpio_tirq, "touch_int"); // Hanz_touch 2013-04-15 touch irq  "max1187x_tirq");
#else
		ret = gpio_request(pdata->gpio_tirq, "max1187x_tirq");
#endif
		if (ret) {
			pr_err("GPIO request failed for max1187x_tirq (%d)\n",
				pdata->gpio_tirq);
			return -EIO;
		}
		ret = gpio_direction_input(pdata->gpio_tirq);
		if (ret) {
			pr_err("GPIO set input direction failed for "\
				"max1187x_tirq (%d)\n", pdata->gpio_tirq);
			gpio_free(pdata->gpio_tirq);
			return -EIO;
		}
	} else {
		gpio_free(pdata->gpio_tirq);
	}

	return 0;
}

static int device_init_thread(void *arg)
{
	return device_init((struct i2c_client *) arg);
}

static int device_init(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct data *ts = NULL;
	struct max1187x_pdata *pdata = NULL;
	struct device_attribute **dev_attr = dev_attrs;
	int ret = 0;

	init_state = 1;
	dev_info(dev, "(INIT): Start");

	/* if I2C functionality is not present we are done */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("I2C core driver does not support I2C functionality");
		ret = -ENXIO;
		goto err_device_init;
	}
	pr_info_if(8, "(INIT): I2C functionality OK");

	/* allocate control block; nothing more to do if we can't */
	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (!ts) {
		pr_err("Failed to allocate control block memory");
		ret = -ENOMEM;
		goto err_device_init;
	}

	/* Get platform data */
#ifdef MAX1187X_LOCAL_PDATA
	pdata = &local_pdata;
	if (!pdata) {
		pr_err("Platform data is missing");
		ret = -ENXIO;
		goto err_device_init_pdata;
	}
#else
	pdata = dev_get_platdata(dev);
	/* If pdata is missing, try to get pdata from device tree (dts) */
	if (!pdata)
		pdata = max1187x_get_platdata_dt(dev);

	/* Validate if pdata values are okay */
	ret = validate_pdata(pdata);
	if (ret < 0)
		goto err_device_init_pdata;
	pr_info_if(8, "(INIT): Platform data OK");
#endif

	ts->pdata = pdata;
	ts->client = client;
	i2c_set_clientdata(client, ts);
#ifndef CONFIG_MACH_LGE_L9II_COMMON
	atomic_set(&ts->irq_processing, 0);
#endif
	mutex_init(&ts->i2c_mutex);
	sema_init(&ts->sema_rbcmd, 1);
	init_rwsem(&ts->rwsema_report_sysfs);
	ts->button0 = 0;
	ts->button1 = 0;
	ts->button2 = 0;
	ts->button3 = 0;

#ifdef CONFIG_MACH_LGE_L9II_COMMON
	if (ts->pdata->power_func) {
		ret = ts->pdata->power_func(1);
		if (ret) {
			pr_err("power on failed");
			goto err_device_init_pdata;
		}
	}
	ts->pwr_state = PWR_ON;
#endif

	init_waitqueue_head(&ts->waitqueue_rbcmd);
	init_waitqueue_head(&ts->waitqueue_report_sysfs);

	pr_info_if(8, "(INIT): Memory allocation OK");

	/* Initialize GPIO pins */
	if (max1187x_chip_init(ts->pdata, 1) < 0) {
		ret = -EIO;
		goto err_device_init_gpio;
	}
	pr_info_if(8, "(INIT): chip init OK");

	/* allocate and register touch device */
	ts->input_dev = input_allocate_device();
	if (!ts->input_dev) {
		pr_err("Failed to allocate touch input device");
		ret = -ENOMEM;
		goto err_device_init_alloc_inputdev;
	}
	snprintf(ts->phys, sizeof(ts->phys), "%s/input0",
			dev_name(dev));
	ts->input_dev->name = MAX1187X_TOUCH;
	ts->input_dev->phys = ts->phys;
	ts->input_dev->id.bustype = BUS_I2C;
	__set_bit(EV_SYN, ts->input_dev->evbit);
	__set_bit(EV_ABS, ts->input_dev->evbit);
	__set_bit(EV_KEY, ts->input_dev->evbit);

	if (PDATA(linux_touch_protocol) == 0)
		input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID,
				0, MAX1187X_TOUCH_COUNT_MAX, 0, 0);
	else
		input_mt_init_slots(ts->input_dev, MAX1187X_TOUCH_COUNT_MAX);

	ts->list_finger_ids = 0;
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X,
			PDATA(panel_margin_xl),
			PDATA(panel_margin_xl) + PDATA(lcd_x), 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y,
			PDATA(panel_margin_yl),
			PDATA(panel_margin_yl) + PDATA(lcd_y), 0, 0);

	if (PDATA(enable_pressure_shaping) == 0)
		input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE,
				0, 0xFFFF, 0, 0);
	else
		input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE,
			0, PRESSURE_MAX_SQRT, 0, 0);

	if (PDATA(linux_touch_protocol) == 0)
		input_set_abs_params(ts->input_dev, ABS_MT_TOOL_TYPE,
			0, MT_TOOL_MAX, 0, 0);

	if (PDATA(max1187x_report_mode) == 2) {
		input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR,
			0, PDATA(lcd_x) + PDATA(lcd_y), 0, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MINOR,
			0, PDATA(lcd_x) + PDATA(lcd_y), 0, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_ORIENTATION,
				-90, 90, 0, 0);
	}

	if (PDATA(button_code0) != KEY_RESERVED)
		set_bit(pdata->button_code0, ts->input_dev->keybit);
	if (PDATA(button_code1) != KEY_RESERVED)
		set_bit(pdata->button_code1, ts->input_dev->keybit);
	if (PDATA(button_code2) != KEY_RESERVED)
		set_bit(pdata->button_code2, ts->input_dev->keybit);
	if (PDATA(button_code3) != KEY_RESERVED)
		set_bit(pdata->button_code3, ts->input_dev->keybit);

	if (PDATA(enable_touch_wakeup) == 1)
		set_bit(KEY_POWER, ts->input_dev->keybit);

	ret = input_register_device(ts->input_dev);
	if (ret) {
		pr_err("Failed to register touch input device");
		ret = -EPERM;
		goto err_device_init_register_inputdev;
	}
	pr_info_if(8, "(INIT): Input touch device OK");

	/* Setup IRQ and handler */
	ret = request_threaded_irq(client->irq,
			irq_handler_hard, irq_handler_soft,
			IRQF_TRIGGER_FALLING, client->name, ts);
	if (ret != 0) {
			pr_err("Failed to setup IRQ handler");
			ret = -EIO;
			goto err_device_init_irq;
	}
	pr_info_if(8, "(INIT): IRQ handler OK");

	/* collect controller ID and configuration ID data from firmware   */
	/* and perform firmware comparison/download if we have valid image */
	validate_fw(ts);

	/* configure suspend/resume */
#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 1;
	ts->early_suspend.suspend = early_suspend;
	ts->early_suspend.resume = late_resume;
	register_early_suspend(&ts->early_suspend);
#elif defined CONFIG_FB
	ts->fb_notif.notifier_call = fb_notifier_callback;
	ret = fb_register_client(&ts->fb_notif);
	if (ret)
		pr_err("Unable to register fb_notifier");
#endif

	ts->is_suspended = 0;
	pr_info_if(8, "(INIT): suspend/resume registration OK");

	/* set up debug interface */
	while (*dev_attr) {
		if (device_create_file(&client->dev, *dev_attr) < 0) {
			pr_err("failed to create sysfs file");
			return 0;
		}
		ts->sysfs_created++;
		dev_attr++;
	}

	if (device_create_bin_file(&client->dev, &dev_attr_report) < 0) {
		pr_err("failed to create sysfs file [report]");
		return 0;
	}
	ts->sysfs_created++;

	if (PDATA(enable_touch_wakeup) == 1) {
		pr_info("Touch Wakeup Feature setup complete\n");
		device_init_wakeup(&client->dev, 1);
		device_wakeup_disable(&client->dev);
	}

	pr_info("(INIT): Done\n");
	return 0;

err_device_init_irq:
err_device_init_register_inputdev:
	input_free_device(ts->input_dev);
	ts->input_dev = NULL;
err_device_init_alloc_inputdev:
err_device_init_gpio:
err_device_init_pdata:
	kfree(ts);
err_device_init:
	return ret;
}

static int device_deinit(struct i2c_client *client)
{
	struct data *ts = i2c_get_clientdata(client);
	struct max1187x_pdata *pdata;
	struct device_attribute **dev_attr = dev_attrs;

	if (ts == NULL)
		return 0;

	pdata = ts->pdata;

	init_state = 0;

	if (PDATA(enable_touch_wakeup) == 1)
		device_init_wakeup(&client->dev, 0);

	while (*dev_attr) {
		if (ts->sysfs_created && ts->sysfs_created--)
			device_remove_file(&client->dev, *dev_attr);
		dev_attr++;
	}
	if (ts->sysfs_created && ts->sysfs_created--)
		device_remove_bin_file(&client->dev, &dev_attr_report);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ts->early_suspend);
#elif defined CONFIG_FB
	if (fb_unregister_client(&ts->fb_notif))
		pr_err("Error occurred while unregistering fb_notifier.");
#endif

	if (client->irq)
			free_irq(client->irq, ts);

	input_unregister_device(ts->input_dev);

	(void) max1187x_chip_init(pdata, 0);
	kfree(ts);

	pr_info("(INIT): Deinitialized\n");
	return 0;
}

static int probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	if (device_create_file(&client->dev, &dev_attr_init) < 0) {
		pr_err("failed to create sysfs file [init]");
		return 0;
	}

	if (IS_ERR(kthread_run(device_init_thread, (void *) client,
			MAX1187X_NAME))) {
		pr_err("failed to start kernel thread");
		return -EAGAIN;
	}
	return 0;
}

static int remove(struct i2c_client *client)
{
	int ret = device_deinit(client);

	device_remove_file(&client->dev, &dev_attr_init);
	return ret;
}

/*
 Commands
 */
/* debug_mask |= 0x40 for all rbcmd */
static int process_rbcmd(struct data *ts)
{
	pr_info_if(0x40, "Enter\n");
	if (ts->rbcmd_waiting == 0)
		goto process_rbcmd_complete;
	if (ts->rbcmd_report_id != ts->rx_report[1])
		goto process_rbcmd_complete;

	ts->rbcmd_received = 1;
	memcpy(ts->rbcmd_rx_report, ts->rx_report, (ts->rx_report_len + 1)<<1);
	*ts->rbcmd_rx_report_len = ts->rx_report_len;
	wake_up_interruptible(&ts->waitqueue_rbcmd);

process_rbcmd_complete:
	pr_info_if(0x40, "Exit\n");
	return 0;
}

static int combine_multipacketreport(struct data *ts)
{
	u16 packet_header = ts->rx_packet[0];
	u8 packet_seq_num = BYTEH(packet_header);
	u8 packet_size = BYTEL(packet_header);
	u16 total_packets, this_packet_num, offset;
	static u16 packet_seq_combined;

	if (packet_seq_num == 0x11) {
		memcpy(ts->rx_report, ts->rx_packet, (packet_size + 1) << 1);
		ts->rx_report_len = packet_size;
		packet_seq_combined = 1;
		goto combine_multipacketreport_full_received;
	}

	total_packets = (packet_seq_num & 0xF0) >> 4;
	this_packet_num = packet_seq_num & 0x0F;

	if (this_packet_num == 1) {
		/* At this time, only raw reports are
		 * expected to be multi-packet */
		if (ts->rx_packet[1] == 0x0800) {
			ts->rx_report_len = ts->rx_packet[2] + 2;
			packet_seq_combined = 1;
			memcpy(ts->rx_report, ts->rx_packet,
					(packet_size + 1) << 1);
			goto err_combine_multipacketreport_partial;
		} else {
			goto err_combine_multipacketreport_bad_id;
		}
	} else if (this_packet_num == packet_seq_combined + 1) {
		packet_seq_combined++;
		offset = (this_packet_num - 1) * 0xF4 + 1;
		memcpy(ts->rx_report + offset, ts->rx_packet + 1,
				packet_size << 1);
		if (total_packets == this_packet_num)
			goto combine_multipacketreport_full_received;
		else
			goto err_combine_multipacketreport_partial;
	} else
		goto err_combine_multipacketreport_outofsync;

combine_multipacketreport_full_received:
	return 0;

err_combine_multipacketreport_partial:
err_combine_multipacketreport_bad_id:
err_combine_multipacketreport_outofsync:
	return -EIO;
}

/* debug_mask |= 0x10 for pm functions */

static void set_suspend_mode(struct data *ts)
{
	u16 cmd_buf[] = {0x0020, 0x0001, 0x0000};
	int ret;

	pr_info_if(0x10, "Enter\n");

	disable_irq(ts->client->irq);
	ts->is_suspended = 1;

	if (PDATA(enable_touch_wakeup) == 1)
		if (device_may_wakeup(&ts->client->dev))
			cmd_buf[2] = 0x6;

	ret = cmd_send(ts, cmd_buf, 3);
	if (ret)
		pr_err("Failed to set sleep mode");

	if (PDATA(enable_touch_wakeup) == 1)
		if (device_may_wakeup(&ts->client->dev))
			enable_irq(ts->client->irq);

	pr_info_if(0x10, "Exit\n");
	return;
}

static void set_resume_mode(struct data *ts)
{
	u16 cmd_buf[] = {0x0020, 0x0001, 0x0002};
	int ret;

	pr_info_if(0x10, "Enter\n");

	if (PDATA(enable_touch_wakeup) == 1)
		if (device_may_wakeup(&ts->client->dev))
			disable_irq(ts->client->irq);

	ret = cmd_send(ts, cmd_buf, 3);
	if (ret)
		pr_err("Failed to set active mode");

	cmd_buf[0] = 0x0018;
	cmd_buf[1] = 0x0001;
	cmd_buf[2] = PDATA(max1187x_report_mode);
	ret = cmd_send(ts, cmd_buf, 3);
	if (ret)
		pr_err("Failed to set up touch report mode");

	ts->is_suspended = 0;

	enable_irq(ts->client->irq);

	pr_info_if(0x10, "Exit\n");

	return;
}

#ifdef CONFIG_MACH_LGE_L9II_COMMON
/* When entering the suspend mode,
 * if user press touch-panel, release them automatically.  */
static void release_all_ts_event(struct data *ts)
{
#ifndef MAX1187X_PROTOCOL_A
        int i;
#endif

        if (ts->list_finger_ids) { //Finger Released
#ifdef MAX1187X_PROTOCOL_A
                input_mt_sync(ts->input_dev);
#else
                for (i = 0; i < MAX1187X_TOUCH_COUNT_MAX; i++) {
                        input_mt_slot(ts->input_dev, i);
                        input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
                }
#endif
                ts->list_finger_ids = 0;
                pr_info_if(4, "(TOUCH) touch finger position released \n");
        }
        else if (ts->prev_button_status == KEY_PRESSED) {  //Button Release
                input_report_key(ts->input_dev, ts->prev_button_code,KEY_RELEASED);
                pr_info_if(4, "(TOUCH) touch button(%d) is released \n",ts->prev_button_code);
        }
        input_sync(ts->input_dev);
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void early_suspend(struct early_suspend *h)
{
	struct data *ts = container_of(h, struct data, early_suspend);

	pr_info_if(0x10, "Enter\n");

	set_suspend_mode(ts);

#ifdef CONFIG_MACH_LGE_L9II_COMMON
// release all touch at suspend
	release_all_ts_event(ts);

	ts->pwr_state = PWR_SLEEP;
	if (ts->pdata->power_func) {
		int ret = ts->pdata->power_func(0);
		if (ret)
			pr_err("power off failed");
	}
#endif

	pr_info_if(0x10, "Exit\n");
	return;
}

static void late_resume(struct early_suspend *h)
{
	struct data *ts = container_of(h, struct data, early_suspend);

	pr_info_if(0x10, "Enter\n");

#ifdef CONFIG_MACH_LGE_L9II_COMMON
        if (ts->pdata->power_func) {
                int ret = ts->pdata->power_func(1);
                if (ret)
                        pr_err("power on failed");
        }
        ts->pwr_state = PWR_WAKE;
	msleep(20);
#endif

	set_resume_mode(ts);

	pr_info_if(0x10, "Exit\n");
}
#elif defined CONFIG_FB
static int fb_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct data *ts = container_of(self, struct data, fb_notif);

	if (evdata && evdata->data && event == FB_EVENT_BLANK && ts &&
			ts->client) {
		blank = evdata->data;
		if (ts->is_suspended == 0 && *blank != FB_BLANK_UNBLANK) {
			pr_info_if(0x10, "FB_BLANK_BLANKED\n");
			set_suspend_mode(ts);
		} else if (ts->is_suspended == 1
				&& *blank == FB_BLANK_UNBLANK) {
			pr_info_if(0x10, "FB_BLANK_UNBLANK\n");
			set_resume_mode(ts);
		}
	}
	return 0;
}
#endif

static int suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct data *ts = i2c_get_clientdata(to_i2c_client(dev));

	pr_info_if(0x10, "Enter\n");

#if (!defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND))
	set_suspend_mode(ts);
#endif

	if (PDATA(enable_touch_wakeup) == 1)
		if (device_may_wakeup(&client->dev))
			enable_irq_wake(client->irq);

	pr_info_if(0x10, "Exit\n");

	return 0;
}

static int resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct data *ts = i2c_get_clientdata(to_i2c_client(dev));

	pr_info_if(0x10, "Enter\n");

	if (PDATA(enable_touch_wakeup) == 1)
		if (device_may_wakeup(&client->dev))
			disable_irq_wake(client->irq);

#if (!defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND))
	set_resume_mode(ts);
#endif

	pr_info_if(0x10, "Exit\n");

	return 0;
}

static const struct dev_pm_ops max1187x_pm_ops = {
	.resume = resume,
	.suspend = suspend,
};

#define STATUS_ADDR_H 0x00
#define STATUS_ADDR_L 0xFF
#define DATA_ADDR_H   0x00
#define DATA_ADDR_L   0xFE
#define STATUS_READY_H 0xAB
#define STATUS_READY_L 0xCC
#define RXTX_COMPLETE_H 0x54
#define RXTX_COMPLETE_L 0x32
static int bootloader_read_status_reg(struct data *ts, const u8 byteL,
	const u8 byteH)
{
	u8 buffer[] = { STATUS_ADDR_L, STATUS_ADDR_H }, i;

	for (i = 0; i < 3; i++) {
		if (i2c_tx_bytes(ts, buffer, 2) != 2) {
			pr_err("TX fail");
			return -EIO;
		}
		if (i2c_rx_bytes(ts, buffer, 2) != 2) {
			pr_err("RX fail");
			return -EIO;
		}
		if (buffer[0] == byteL && buffer[1] == byteH)
			break;
	}
	if (i == 3) {
		pr_err("Unexpected status => %02X%02X vs %02X%02X",
				buffer[0], buffer[1], byteL, byteH);
		return -EIO;
	}

	return 0;
}

static int bootloader_write_status_reg(struct data *ts, const u8 byteL,
	const u8 byteH)
{
	u8 buffer[] = { STATUS_ADDR_L, STATUS_ADDR_H, byteL, byteH };

	if (i2c_tx_bytes(ts, buffer, 4) != 4) {
		pr_err("TX fail");
		return -EIO;
	}
	return 0;
}

static int bootloader_rxtx_complete(struct data *ts)
{
	return bootloader_write_status_reg(ts, RXTX_COMPLETE_L,
				RXTX_COMPLETE_H);
}

static int bootloader_read_data_reg(struct data *ts, u8 *byteL, u8 *byteH)
{
	u8 buffer[] = { DATA_ADDR_L, DATA_ADDR_H, 0x00, 0x00 };

	if (i2c_tx_bytes(ts, buffer, 2) != 2) {
		pr_err("TX fail");
		return -EIO;
	}
	if (i2c_rx_bytes(ts, buffer, 4) != 4) {
		pr_err("RX fail");
		return -EIO;
	}
	if (buffer[2] != 0xCC && buffer[3] != 0xAB) {
		pr_err("Status is not ready");
		return -EIO;
	}

	*byteL = buffer[0];
	*byteH = buffer[1];
	return bootloader_rxtx_complete(ts);
}

static int bootloader_write_data_reg(struct data *ts, const u8 byteL,
	const u8 byteH)
{
	u8 buffer[6] = { DATA_ADDR_L, DATA_ADDR_H, byteL, byteH,
			RXTX_COMPLETE_L, RXTX_COMPLETE_H };

	if (bootloader_read_status_reg(ts, STATUS_READY_L,
		STATUS_READY_H) < 0) {
		pr_err("read status register fail");
		return -EIO;
	}
	if (i2c_tx_bytes(ts, buffer, 6) != 6) {
		pr_err("TX fail");
		return -EIO;
	}
	return 0;
}

static int bootloader_rxtx(struct data *ts, u8 *byteL, u8 *byteH,
	const int tx)
{
	if (tx > 0) {
		if (bootloader_write_data_reg(ts, *byteL, *byteH) < 0) {
			pr_err("write data register fail");
			return -EIO;
		}
		return 0;
	}

	if (bootloader_read_data_reg(ts, byteL, byteH) < 0) {
		pr_err("read data register fail");
		return -EIO;
	}
	return 0;
}

static int bootloader_get_cmd_conf(struct data *ts, int retries)
{
	u8 byteL, byteH;

	do {
		if (bootloader_read_data_reg(ts, &byteL, &byteH) >= 0) {
			if (byteH == 0x00 && byteL == 0x3E)
				return 0;
		}
		retries--;
	} while (retries > 0);

	return -EIO;
}

static int bootloader_write_buffer(struct data *ts, u8 *buffer, int size)
{
	u8 byteH = 0x00;
	int k;

	for (k = 0; k < size; k++) {
		if (bootloader_rxtx(ts, &buffer[k], &byteH, 1) < 0) {
			pr_err("bootloader RX-TX fail");
			return -EIO;
		}
	}
	return 0;
}

static int bootloader_enter(struct data *ts)
{
	int i;
	u16 enter[3][2] = { { 0x7F00, 0x0047 }, { 0x7F00, 0x00C7 }, { 0x7F00,
			0x0007 } };

	for (i = 0; i < 3; i++) {
		if (i2c_tx_words(ts, enter[i], 2) != 2) {
			pr_err("Failed to enter bootloader");
			return -EIO;
		}
	}

	if (bootloader_get_cmd_conf(ts, 5) < 0) {
		pr_err("Failed to enter bootloader mode");
		return -EIO;
	}
	return 0;
}

static int bootloader_exit(struct data *ts)
{
	int i;
	u16 exit[3][2] = { { 0x7F00, 0x0040 }, { 0x7F00, 0x00C0 }, { 0x7F00,
			0x0000 } };

	for (i = 0; i < 3; i++) {
		if (i2c_tx_words(ts, exit[i], 2) != 2) {
			pr_err("Failed to exit bootloader");
			return -EIO;
		}
	}

	return 0;
}

static int bootloader_get_crc(struct data *ts, u16 *crc16,
		u16 addr, u16 len, u16 delay)
{
	u8 crc_command[] = {0x30, 0x02, BYTEL(addr),
			BYTEH(addr), BYTEL(len), BYTEH(len)};
	u8 byteL = 0, byteH = 0;
	u16 rx_crc16 = 0;

	if (bootloader_write_buffer(ts, crc_command, 6) < 0) {
		pr_err("write buffer fail");
		return -EIO;
	}
	msleep(delay);

	/* reads low 8bits (crcL) */
	if (bootloader_rxtx(ts, &byteL, &byteH, 0) < 0) {
		pr_err("Failed to read low byte of crc response!");
		return -EIO;
	}
	rx_crc16 = (u16) byteL;

	/* reads high 8bits (crcH) */
	if (bootloader_rxtx(ts, &byteL, &byteH, 0) < 0) {
		pr_err("Failed to read high byte of crc response!");
		return -EIO;
	}
	rx_crc16 = (u16)(byteL << 8) | rx_crc16;

	if (bootloader_get_cmd_conf(ts, 5) < 0) {
		pr_err("CRC get failed!");
		return -EIO;
	}
	*crc16 = rx_crc16;

	return 0;
}

static int bootloader_set_byte_mode(struct data *ts)
{
	u8 buffer[2] = { 0x0A, 0x00 };

	if (bootloader_write_buffer(ts, buffer, 2) < 0) {
		pr_err("write buffer fail");
		return -EIO;
	}
	if (bootloader_get_cmd_conf(ts, 10) < 0) {
		pr_err("command confirm fail");
		return -EIO;
	}
	return 0;
}

static int bootloader_erase_flash(struct data *ts)
{
	u8 byteL = 0x02, byteH = 0x00;
	int i, verify = 0;

	if (bootloader_rxtx(ts, &byteL, &byteH, 1) < 0) {
		pr_err("bootloader RX-TX fail");
		return -EIO;
	}

	for (i = 0; i < 10; i++) {
		msleep(60); /* wait 60ms */

		if (bootloader_get_cmd_conf(ts, 0) < 0)
			continue;

		verify = 1;
		break;
	}

	if (verify != 1) {
		pr_err("Flash Erase failed");
		return -EIO;
	}

	return 0;
}

static int bootloader_write_flash(struct data *ts, const u8 *image, u16 length)
{
	u8 buffer[130];
	u8 length_L = length & 0xFF;
	u8 length_H = (length >> 8) & 0xFF;
	u8 command[] = { 0xF0, 0x00, length_H, length_L, 0x00 };
	u16 blocks_of_128bytes;
	int i, j;

	if (bootloader_write_buffer(ts, command, 5) < 0) {
		pr_err("write buffer fail");
		return -EIO;
	}

	blocks_of_128bytes = length >> 7;

	for (i = 0; i < blocks_of_128bytes; i++) {
		for (j = 0; j < 100; j++) {
			usleep_range(1500, 2000);
			if (bootloader_read_status_reg(ts, STATUS_READY_L,
			STATUS_READY_H)	== 0)
				break;
		}
		if (j == 100) {
			pr_err("Failed to read Status register!");
			return -EIO;
		}

		buffer[0] = ((i % 2) == 0) ? 0x00 : 0x40;
		buffer[1] = 0x00;
		memcpy(buffer + 2, image + i * 128, 128);

		if (i2c_tx_bytes(ts, buffer, 130) != 130) {
			pr_err("Failed to write data (%d)", i);
			return -EIO;
		}
		if (bootloader_rxtx_complete(ts) < 0) {
			pr_err("Transfer failure (%d)", i);
			return -EIO;
		}
	}

	usleep_range(10000, 11000);
	if (bootloader_get_cmd_conf(ts, 5) < 0) {
		pr_err("Flash programming failed");
		return -EIO;
	}
	return 0;
}

/****************************************
 *
 * Standard Driver Structures/Functions
 *
 ****************************************/
static const struct i2c_device_id id[] = { { MAX1187X_NAME, 0 }, { } };

MODULE_DEVICE_TABLE(i2c, id);

static struct of_device_id max1187x_dt_match[] = {
	{ .compatible = "maxim,max1187x_tsc" },	{ } };

static struct i2c_driver driver = {
		.probe = probe,
		.remove = remove,
		.id_table = id,
		.driver = {
			.name = MAX1187X_NAME,
			.owner	= THIS_MODULE,
			.of_match_table = max1187x_dt_match,
			.pm = &max1187x_pm_ops,
		},
};

static int __devinit max1187x_init(void)
{
	return i2c_add_driver(&driver);
}

static void __exit max1187x_exit(void)
{
	i2c_del_driver(&driver);
}

module_init(max1187x_init);
module_exit(max1187x_exit);

MODULE_AUTHOR("Maxim Integrated Products, Inc.");
MODULE_DESCRIPTION("MAX1187X Touchscreen Driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("3.2.3");
