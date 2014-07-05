#ifndef __REFCODE_H__
#define __REFCODE_H__

//#include "Validation.h"
#include <linux/kernel.h>

//#define _F34_TEST_
#define _F54_TEST_
//#define _DS4_3_0_	// TM2000, TM2145, TM2195
#define _DS4_3_2_	// TM2371, TM2370, PLG137, PLG122
//#define _FW_TESTING_
//#define _DELTA_IMAGE_TEST_

//(important) should be defined the value(=register address) according to register map
//'Multi Metric Noise Mitigation Control'
//#define NoiseMitigation 0x1A1	// TM2000 (~E025), TM2195
//#define NoiseMitigation 0x1B1	// TM2000 (E027~)
//#define NoiseMitigation 0x0196	// TM2145
//#define NoiseMitigation 0x15E	// TM2370, TM2371, PLG137, PLG122
//#define NoiseMitigation 0x157	// PLG129, PLG141
#define NoiseMitigation 0x155	// PLG124, PLG129(E010~)
#define F54_CBCSettings 0x117	// PLG129(E010~)
//#define F54_CBCSettings_0D 0x1B6	// TM2000 (E027~)
//#define F54_CBCSettings_0D 0x163	// TM2370, TM2371, PLG137, PLG122
//#define F54_CBCSettings_0D 0x170	// PLG129, PLG141
#define F54_CBCSettings_0D 0x16E	// PLG124, PLG129(E010~)
#ifdef _DS4_3_2_
#define F55_PhysicalRx_Addr 0x301	// TM2371, TM2370, PLG137, PLG122, PLG129, PLG141
#endif

#ifdef _F54_TEST_
unsigned char F54_FullRawCap(int, int);
unsigned char F54_RxToRxReport(void);
unsigned char F54_TxToGndReport(void);
unsigned char F54_TxToTxReport(void);
unsigned char F54_TxOpenReport(void);
unsigned char F54_RxOpenReport(void);
unsigned char F54_HighResistance(void);
#endif

#ifdef _F34_TEST_
void CompleteReflash_OmitLockdown(void);
void CompleteReflash(void);
void CompleteReflash_Lockdown(void);
void ConfigBlockReflash(void);
#endif

void FirmwareCheck( void );
void AttentionTest( void );
void FirmwareCheck_temp( void );

int waitATTN(unsigned char set, int val); //HSLEE

#ifdef _FW_TESTING_
void HostImplementationTesting( void );
#endif

#ifdef _DELTA_IMAGE_TEST_
unsigned char F54_ButtonDeltaImage(void);
void CalculateSNR(void);
#endif

#endif

void writeRMI(unsigned short add, unsigned char *value, unsigned short len);
void readRMI(unsigned short add, unsigned char *value, unsigned short len);
void delayMS(int val);
void cleanExit(int code);
extern unsigned char* f54_buf;
extern unsigned int   buf_cnt;
