/*
 * MMS100S ISC Updater customize source
 */

/*                                                
                                         
 */
#include "MMS134S_ISC_Updater_Customize.h"

const unsigned char mfs_i2c_slave_addr = 0x48;

uint8_t mfs_slave_addr;

mfs_bool_t MFS_I2C_set_slave_addr(unsigned char _slave_addr)
{
	mfs_slave_addr = _slave_addr << 1; /* Do not modify */

	/* TODO: set I2C slave address */
	return MFS_TRUE;
}

mfs_bool_t MFS_I2C_read_with_addr(unsigned char* _read_buf,
		unsigned char _addr, int _length)
{
	/* TODO: I2C로 1 byte address를 쓴 후 _length 갯수만큼 읽어 _read_buf에 채워 주세요. */
	mfs_bool_t ret;
	ret = ts_i2c_read(_read_buf, _addr, _length);
	if(ret < 0) return MFS_FALSE;	
	return MFS_TRUE;
}

mfs_bool_t MFS_I2C_write(unsigned char* _write_buf, int _length)
{
	/*
	 * TODO: I2C로 _write_buf의 내용을 _length 갯수만큼 써 주세요.
	 * address를 명시해야 하는 인터페이스의 경우, _write_buf[0]이 address가 되고
	 * _write_buf+1부터 _length-1개를 써 주시면 됩니다.
	 */
	mfs_bool_t ret;
#if 0  // for debugging
	printk(KERN_ERR "[Touch D] MFS_I2C_write call!\n");
	printk(KERN_ERR "[Touch D] *(_write_buf) : 0x%02x, *(_write_buf+1) : 0x%02x \n",*(_write_buf+4), *(_write_buf+5));
#endif
	ret = ts_i2c_write(_write_buf+1, *(_write_buf), _length-1);
	if(ret < 0) return MFS_FALSE;
	return MFS_TRUE;
}

mfs_bool_t MFS_I2C_read(unsigned char* _read_buf, int _length)
{
	/* TODO: I2C로 _length 갯수만큼 읽어 _read_buf에 채워 주세요. */
	mfs_bool_t ret;

	ret = ts_i2c_read_len(_read_buf, _length);
	if(ret < 0) return MFS_FALSE;
	return MFS_TRUE;
}

void MFS_debug_msg(const char* fmt, int a, int b, int c)
{
	printk(fmt,a,b,c);

}
void MFS_ms_delay(int msec)
{
	mdelay(msec);
}

void MFS_reboot(void)
{
	int touch_ldo_en = 1;
	MFS_debug_msg("<MELFAS> TOUCH IC REBOOT!!!\n", 0, 0, 0);
	// depend on model 
	gpio_set_value(touch_ldo_en, 0);
	mdelay(10);
	gpio_set_value(touch_ldo_en, 1);
	mdelay(100);

}

/*                                                
                                         
 */
