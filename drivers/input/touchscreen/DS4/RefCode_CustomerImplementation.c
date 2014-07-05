/*
   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2011 Synaptics, Inc.

   Permission is hereby granted, free of charge, to any person obtaining a copy of
   this software and associated documentation files (the "Software"), to deal in
   the Software without restriction, including without limitation the rights to use,
   copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
   Software, and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.


   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/
#include <linux/i2c.h>
#include <linux/delay.h>	//msleep
#include <linux/file.h>		//for file access
#include <linux/syscalls.h> //for file access
#include <linux/uaccess.h>  //for file access
#include <linux/input/lge_touch_core.h>
#include <linux/earlysuspend.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>

#include "RefCode.h"
#include "RefCode_PDTScan.h"

unsigned int   buf_cnt;
unsigned char* f54_buf;

static char currentPage;
extern struct i2c_client* ds4_i2c_client;
extern int touch_i2c_read(struct i2c_client *client, u8 reg, int len, u8 *buf);
extern int touch_i2c_write(struct i2c_client *client, u8 reg, int len, u8 * buf);

void device_I2C_read(unsigned char add, unsigned char *value, unsigned short len)
{
    if(touch_i2c_read(ds4_i2c_client, add, len, value) < 0) return;
}

void device_I2C_write(unsigned char add, unsigned char *value, unsigned short len)
{
    if(touch_i2c_write(ds4_i2c_client, add, len, value) < 0) return;
}

void InitPage(void)
{
    currentPage = 0;
}

void SetPage(unsigned char page)
{
    device_I2C_write(0xFF, &page, 1); //changing page
}

void readRMI(unsigned short add, unsigned char *value, unsigned short len)
{
    unsigned char temp;

    temp = add >> 8;
    if(temp != currentPage)
    {
        currentPage = temp;
        SetPage(currentPage);
    }
    device_I2C_read(add & 0xFF, value, len);
}

void writeRMI(unsigned short add, unsigned char *value, unsigned short len)
{
    unsigned char temp;

    temp = add >> 8;
    if(temp != currentPage)
    {
        currentPage = temp;
        SetPage(currentPage);
    }
    device_I2C_write(add & 0xFF, value, len);
}

void delayMS(int val)
{
	// Wait for val MS
	msleep(val);
}

void cleanExit(int code)
{
	//FIXME: add kernel exit function
	return;
}

int waitATTN(unsigned char set, int val)
{
    unsigned char command = 0;

	if(set == 1)
	{
		// Wait ATTN for val MS
		delayMS(val);

		// If get ATTN within val MS, return 1;
		// else, return 0;
		readRMI(F01_Cmd_Base, &command, 1);
        if (command != 0x00)
        {
            return 1;
        }
        else
        {
            return 0;
        }
	}
	else
	{
		// Wait no ATTN for val MS
		delayMS(val);

		// If get ATTN within val MS, return 1;
		// else, return 0;
		readRMI(F01_Cmd_Base, &command, set);
        if (command != 0x00)
        {
            return 1;
        }
        else
        {
            return 0;
        }
	}
}

/*
void TestSensor(void)
{
	// Run PDT Scan
	SYNA_PDTScan();
	SYNA_ConstructRMI_F54();
	SYNA_ConstructRMI_F1A();

	// Run test functions
	F54_FullRawCap(0, 0);

	// Check FW information including product ID
	FirmwareCheck();
}
*/

unsigned int TestFPCB(char *buf)
{
    f54_buf = buf;
    buf_cnt = 0;

    // Run PDT Scan
    SYNA_PDTScan();
    SYNA_ConstructRMI_F54();
    SYNA_ConstructRMI_F1A();

    // Check FW information including product ID
	FirmwareCheck();
	AttentionTest();

    // Run PDT Scan
	SYNA_PDTScan();
	SYNA_ConstructRMI_F54();
	SYNA_ConstructRMI_F1A();

    // Run test functions
    F54_FullRawCap(1, 0);
    F54_RxToRxReport();
    delayMS(300);
    F54_TxToGndReport();
    F54_TxToTxReport();

    return buf_cnt;
}

#if 0
void main(void)
/* Please be informed this main() function & related functions are an example for host implementation */
{
//	TestSensor();
	TestFPCB();
}
#endif
