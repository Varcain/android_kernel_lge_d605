#ifndef __PP2106_KEYPAD_H__
#define __PP2106_KEYPAD_H__

#define PP2106_KEYPAD_DEV_NAME "pp2106-keypad"

#define PP2106_KEYPAD_ROW 8
#define PP2106_KEYPAD_COL 8

struct pp2106_keypad_platform_data {
	unsigned int reset_pin;
	unsigned int irq_pin;
	unsigned int irq_num;
	unsigned int sda_pin;
	unsigned int scl_pin;
	unsigned int keypad_row;
	unsigned int keypad_col;
	unsigned char *keycode;
};

#endif /* __PP2106_KEYPAD_H__ */
