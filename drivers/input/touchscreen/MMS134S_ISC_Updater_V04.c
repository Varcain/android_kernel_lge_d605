/*
 * MMS100S ISC Updater V04
 * !! Warning !!
 * Do not modify algorithm code.
 */
#include "MMS134S_ISC_Updater_Customize.h"

/* firmware binary name */
#if defined(CONFIG_TOUCHSCREEN_MELFAS_MMS134) // for f6 tmus
// define model feature
//#include "MMS100S_FIRMWARE.c" // test firmware
//#include "mms134_tmus_R00_V01.c"
#include "mms134_tmus_R00_V03.c"
//#include "mms134_tmus_R01_V04.c"  // for Touch HW revision 1
//#include "mms134_tmus_R01_V05.c"
//#include "mms134_tmus_R01_V06.c"
//#include "mms134_tmus_R01_V08.c"
//#include "mms134_tmus_R01_V09.c"
//#include "mms134_tmus_R01_V10.c"
//#include "mms134_tmus_R01_V11.c"
#include "mms134_tmus_R01_V12.c"
#endif

#define MFS_HEADER_		5
#define MFS_DATA_		20480

/*                                                
                                                                                      
  */
//#define DATA_SIZE 1024
#define DATA_SIZE 256
/*                                                                                                  */
#define RET_READING_HEXFILE_FAILED                0x21
#define RET_FILE_ACCESS_FAILED                    0x22
#define RET_MELLOC_FAILED                                 0x23


#define CLENGTH 4
#define PACKET_			(MFS_HEADER_ + MFS_DATA_)

/*
 * State Registers
 */

/*
 * Config Update Commands
 */
#define ISC_CMD_ENTER_ISC						0x5F
#define ISC_CMD_ENTER_ISC_PARA1					0x01
#define ISC_CMD_ISC_ADDR						0xD5
#define ISC_CMD_ISC_STATUS_ADDR					0xD9

/*
 * ISC Status Value
 */
#define ISC_STATUS_RET_MASS_ERASE_DONE			0X0C
#define ISC_STATUS_RET_MASS_ERASE_MODE			0X08

#define MFS_CHAR_2_BCD(num)	\
	(((num/10)<<4) + (num%10))
#define MFS_MAX(x, y)		( ((x) > (y))? (x) : (y) )

#define MFS_DEFAULT_SLAVE_ADDR	0x48

static int firmware_write(const unsigned char *_pBinary_Data);
static int firmware_verify(const unsigned char *_pBinary_Data);
static int mass_erase(void);

/*                                                
                                                                                
  */
int (*ts_i2c_read)(unsigned char *buf, unsigned char reg, int len);
int (*ts_i2c_write)(unsigned char *buf, unsigned char reg, int len);
int (*ts_i2c_read_len)(unsigned char *buf, int len);

extern UINT16 MELFAS_binary_nLength;
extern UINT8 *MELFAS_binary;

void assign_fp_read_func(int (*touch_i2c_read_func) (unsigned char *buf, unsigned char reg, int len))
{
	ts_i2c_read = touch_i2c_read_func;
}

void assign_fp_read_len_func(int (*touch_i2c_read_len_func) (unsigned char *buf, int len))
{
	ts_i2c_read_len = touch_i2c_read_len_func;
}

void assign_fp_write_func(int (*touch_i2c_write_func) (unsigned char *buf, unsigned char reg, int len))
{
	ts_i2c_write = touch_i2c_write_func;
}

/*                                                                                             */


eMFSRet_t MFS_ISC_update(const char* fw_path)
{	
	eMFSRet_t ret;
	
	unsigned char *pBinary = {0,};
	int nBinary_length = 0;
	mm_segment_t old_fs = 0;
	struct stat fw_bin_stat;
	int fd = 0;
	int nRead = 0;

	if(unlikely(fw_path[0] != 0)) {
		
		old_fs = get_fs();
		set_fs(get_ds());

		fd = sys_open(fw_path, O_RDONLY, 0);
		if (fd < 0) {
			printk("read data fail\n");
			goto read_fail;
		}

		ret = sys_newstat(fw_path, (struct stat *)&fw_bin_stat);
		if (ret < 0) {
			printk("new stat fail\n");
			goto fw_mem_alloc_fail;
		}

		nBinary_length = fw_bin_stat.st_size;

		printk("length ==> %d\n", nBinary_length);

		pBinary = kzalloc(sizeof(unsigned char) * (nBinary_length + 1), GFP_KERNEL);
		if (pBinary == NULL) {
			printk("binary is NULL\n");
			goto fw_mem_alloc_fail;
		}

		nRead = sys_read(fd, (char __user *)pBinary, nBinary_length);			/* Read binary file */
		if (nRead != nBinary_length) {
			sys_close(fd);											   /* Close file */
			if (pBinary != NULL)								   /* free memory alloced.*/
				kfree(pBinary);
			goto fw_mem_alloc_fail;
		}
	}

	MFS_I2C_set_slave_addr(mfs_i2c_slave_addr);

	// need reboot
	MFS_reboot();

	if ((ret = mass_erase()) && ret != MRET_SUCCESS)
	{
		/* Slave Address != Default Address(0x48) case : Code start!!! */
		if (mfs_i2c_slave_addr != MFS_DEFAULT_SLAVE_ADDR)
		{
			MFS_debug_msg("<MELFAS> Slave Address is not matched!!!\n", 0, 0,
					0);
			MFS_debug_msg(
					"<MELFAS> Retry Mass Erase with the Default Address(0x%2X)!!!\n\n",
					MFS_DEFAULT_SLAVE_ADDR, 0, 0);

			MFS_I2C_set_slave_addr(MFS_DEFAULT_SLAVE_ADDR);
			if ((ret = mass_erase()) && ret != MRET_SUCCESS)
				goto MCSDL_DOWNLOAD_FINISH;
		}
		else
			goto MCSDL_DOWNLOAD_FINISH;
	}

	MFS_reboot();
	MFS_I2C_set_slave_addr(MFS_DEFAULT_SLAVE_ADDR);
	if(unlikely(fw_path[0] != 0)){
		/* external image */
		if ((ret = firmware_write(pBinary)) && ret != MRET_SUCCESS)
			goto MCSDL_DOWNLOAD_FINISH;
		
		if ((ret = firmware_verify(pBinary)) && ret != MRET_SUCCESS)
			goto MCSDL_DOWNLOAD_FINISH;
	}
	else{
		/* embedded image */
		if ((ret = firmware_write(MELFAS_binary)) && ret != MRET_SUCCESS)
			goto MCSDL_DOWNLOAD_FINISH;
		
		if ((ret = firmware_verify(MELFAS_binary)) && ret != MRET_SUCCESS)
			goto MCSDL_DOWNLOAD_FINISH;
	}
	MFS_debug_msg("<MELFAS> FIRMWARE_UPDATE_FINISHED!!!\n\n", 0, 0, 0);

	fw_mem_alloc_fail:
		sys_close(fd);
		ret = -1;
	read_fail:
		set_fs(old_fs);
		ret = -1;

	MCSDL_DOWNLOAD_FINISH:

	if(unlikely(fw_path[0] != 0)){
		kfree(pBinary);
	}

	MFS_I2C_set_slave_addr(mfs_i2c_slave_addr);

	// need reboot
	MFS_reboot();
	
	return ret;
}

int firmware_write(const unsigned char *_pBinary_Data)
{
	int i, k = 0, kbyte = 0;
	unsigned char fw_write_buffer[MFS_HEADER_ + DATA_SIZE];
	unsigned short int start_addr = 0;

	MFS_debug_msg("<MELFAS> FIRMARE WRITING...\n", 0, 0, 0);
	MFS_debug_msg("<MELFAS> ", 0, 0, 0);
	while (start_addr * CLENGTH < MFS_DATA_)
	{
		fw_write_buffer[0] = ISC_CMD_ISC_ADDR;
		fw_write_buffer[1] = (UINT8) ((start_addr) & 0X00FF);
		fw_write_buffer[2] = (UINT8) ((start_addr >> 8) & 0X00FF);
		fw_write_buffer[3] = 0X00;
		fw_write_buffer[4] = 0X00;

		for (i = 0; i < DATA_SIZE; i++){
			fw_write_buffer[MFS_HEADER_ + i] = _pBinary_Data[i
					+ start_addr * CLENGTH];
#if 0 // for debugging
			if(i % 1024 == 0)
				printk(KERN_ERR "[Touch D] 1th data : 0x%02x \n", fw_write_buffer[MFS_HEADER_ + i]);
			if(i % 1024 == 255)
				printk(KERN_ERR "[Touch D] 256th data : 0x%02x \n", fw_write_buffer[MFS_HEADER_ + i]);
#endif
		}		

		MFS_ms_delay(5);
		if (!MFS_I2C_write(fw_write_buffer, MFS_HEADER_ + DATA_SIZE))
			return MRET_I2C_ERROR;
		
		/*                                                
                                                                  
    */
		if(k % 4 == 3){ 
			kbyte++;
			MFS_debug_msg("%dKB ", kbyte, 0, 0);
		}
		/*                                                                             */

		MFS_ms_delay(5);
		k++;
		start_addr = DATA_SIZE * k / CLENGTH;
	}
	MFS_debug_msg("\n", 0, 0, 0);

	return MRET_SUCCESS;
}

int firmware_verify(const unsigned char *_pBinary_Data)
{
	int i, k = 0, kbyte = 0;
	unsigned char write_buffer[MFS_HEADER_], read_buffer[DATA_SIZE];
	unsigned short int start_addr = 0;

	MFS_debug_msg("<MELFAS> FIRMARE VERIFY...\n", 0, 0, 0);
	MFS_debug_msg("<MELFAS> ", 0, 0, 0);
	while (start_addr * CLENGTH < MFS_DATA_)
	{
		write_buffer[0] = ISC_CMD_ISC_ADDR;
		write_buffer[1] = (UINT8) ((start_addr) & 0X00FF);
		write_buffer[2] = 0x40 + (UINT8) ((start_addr >> 8) & 0X00FF);
		write_buffer[3] = 0X00;
		write_buffer[4] = 0X00;

		if (!MFS_I2C_write(write_buffer, MFS_HEADER_))
			return MRET_I2C_ERROR;

		MFS_ms_delay(5);

		if (!MFS_I2C_read(read_buffer, DATA_SIZE))
			return MRET_I2C_ERROR;

		for (i = 0; i < DATA_SIZE; i++)
			if (read_buffer[i] != _pBinary_Data[i + start_addr * CLENGTH])
			{
				MFS_debug_msg("<MELFAS> VERIFY Failed\n", i, 0, 0);
				MFS_debug_msg(
						"<MELFAS> original : 0x%2x, buffer : 0x%2x, addr : %d \n",
						_pBinary_Data[i + start_addr * CLENGTH], read_buffer[i],
						i);
				
				return MRET_FIRMWARE_VERIFY_ERROR;
			}
#if 0 // for debugging
			else{
				MFS_debug_msg(
						"<MELFAS> original : 0x%2x, buffer : 0x%2x, addr : %d \n",
						_pBinary_Data[i + start_addr * CLENGTH], read_buffer[i],
						i);
			}
#endif
		/*                                                
                                                                 
    */
		if(k % 4 == 3){ 
			kbyte++;
			MFS_debug_msg("%dKB ", kbyte, 0, 0);
		}
		/*                                                                             */
		k++;
		start_addr = DATA_SIZE * k / CLENGTH;

	}
	MFS_debug_msg("\n", 0, 0, 0);
	return MRET_SUCCESS;
}

int mass_erase(void)
{
	int i = 0;
	unsigned char mass_erase_cmd[MFS_HEADER_] =
	{ ISC_CMD_ISC_ADDR, 0, 0xC1, 0, 0 };
	unsigned char read_buffer[4] =
	{ 0, };

	MFS_debug_msg("<MELFAS> mass erase start\n\n", 0, 0, 0);

	MFS_ms_delay(5);
	if (!MFS_I2C_write(mass_erase_cmd, MFS_HEADER_))
		return MRET_I2C_ERROR;
	MFS_ms_delay(5);

	while (read_buffer[2] != ISC_STATUS_RET_MASS_ERASE_DONE)
	{
		if (!MFS_I2C_read_with_addr(read_buffer, ISC_CMD_ISC_STATUS_ADDR, 4))
			return MRET_I2C_ERROR;

		if (read_buffer[2] == ISC_STATUS_RET_MASS_ERASE_DONE)
		{
			MFS_debug_msg("<MELFAS> Firmware Mass Erase done.\n", 0, 0, 0);
			return MRET_SUCCESS;
		}
		else if (read_buffer[2] == ISC_STATUS_RET_MASS_ERASE_MODE)
			MFS_debug_msg("<MELFAS> Firmware Mass Erase enter success!!!\n", 0,
					0, 0);

		MFS_ms_delay(1);
		if (i > 20)
			return MRET_MASS_ERASE_ERROR;
		i++;
	}
	return MRET_SUCCESS;
}

