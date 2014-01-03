/*
 * MMS100S ISC Updater의 public 헤더입니다.
 * !!주의!!
 * 내용을 수정하시면 안됩니다.
 */
#ifndef __MMS100S_ISC_Updater_H__
#define __MMS100S_ISC_Updater_H__

/*
 * Return values
 */
typedef enum
{
	MRET_NONE = -1,
	MRET_SUCCESS = 0,
	MRET_FILE_OPEN_ERROR,
	MRET_FILE_CLOSE_ERROR,
	MRET_FILE_FORMAT_ERROR,
	MRET_WRITE_BUFFER_ERROR,
	MRET_I2C_ERROR,
	MRET_MASS_ERASE_ERROR,
	MRET_FIRMWARE_WRITE_ERROR,
	MRET_FIRMWARE_VERIFY_ERROR,
	MRET_UPDATE_MODE_ENTER_ERROR,
	MRET_LIMIT
} eMFSRet_t;

/*
 * Boolean 관련 type 및 define.
 * 그대로 두셔도 되고, system에 맞게 고쳐 주셔도 됩니다.
 */
typedef int mfs_bool_t;
#define MFS_TRUE		(0==0)
#define MFS_FALSE		(0!=0)

typedef char                            INT8;
typedef unsigned char           UINT8;

typedef short                           INT16;
typedef unsigned short          UINT16;

typedef int                                     INT32;
typedef unsigned int            UINT32;

/*
 * Interfaces
 */
extern eMFSRet_t MFS_ISC_update(const char* fw_path);

/*                                                
                                                                                
  */
extern void assign_fp_read_func(int (*touch_i2c_read_func) (unsigned char *buf, unsigned char reg, int len));
extern void assign_fp_read_len_func(int (*touch_i2c_read_len_func) (unsigned char *buf, int len));
extern void assign_fp_write_func(int (*touch_i2c_write_func) (unsigned char *buf, unsigned char reg, int len));

extern int (*ts_i2c_read)(unsigned char *buf, unsigned char reg, int len);
extern int (*ts_i2c_write)(unsigned char *buf, unsigned char reg, int len);
extern int (*ts_i2c_read_len)(unsigned char *buf, int len);

/*                                                        */


#endif //__MMS100S_ISC_Updater_H__
