/*
 * arch/arm/mach-msm/lge/lge_emmc_direct_access.c
 *
 * Copyright (C) 2010 LGE, Inc
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

#include <asm/div64.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <asm/uaccess.h>
#include <linux/kmod.h>
#include <linux/workqueue.h>
#include <lg_backup_items.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <lg_diag_cfg.h>
#include <mach/oem_rapi_client.h>
#include <linux/mutex.h>

/* Some useful define used to access the MBR/EBR table */
#define GPT_ENTRY_START           0x400
#define TABLE_ENTRY_0             0x1BE
#define TABLE_ENTRY_1             0x1CE
#define TABLE_ENTRY_2             0x1DE
#define TABLE_ENTRY_3             0x1EE
#define TABLE_SIGNATURE           0x1FE
#define TABLE_ENTRY_SIZE          0x010
#define GPT_ENTRY_SIZE          0x080


#define OFFSET_STATUS             0x00
#define OFFSET_TYPE               0x04
#define OFFSET_FIRST_SEC          0x08

#define GPT_OFF_FIRST_SEC			0x20
#define OFFSET_SIZE               0x0C
#define COPYBUFF_SIZE             (1024 * 16)
#define BINARY_IN_TABLE_SIZE      (16 * 512)
#define MAX_FILE_ENTRIES          20

#define MMC_BOOT_TYPE 0x48
#define MMC_SYSTEM_TYPE 0x78
#define MMC_USERDATA_TYPE 0x79

#define MMC_RCA 2

#define MAX_PARTITIONS 64

#define GET_LWORD_FROM_BYTE(x)    ((unsigned)*(x) | \
        ((unsigned)*((x)+1) << 8) | \
        ((unsigned)*((x)+2) << 16) | \
        ((unsigned)*((x)+3) << 24))

#define PUT_LWORD_TO_BYTE(x, y)   do{*(x) = (y) & 0xff;     \
    *((x)+1) = ((y) >> 8) & 0xff;     \
    *((x)+2) = ((y) >> 16) & 0xff;     \
    *((x)+3) = ((y) >> 24) & 0xff; }while(0)

#define GET_PAR_NUM_FROM_POS(x) ((((x) & 0x0000FF00) >> 8) + ((x) & 0x000000FF))

#define MMC_BOOT_TYPE 0x48
#define MMC_EXT3_TYPE 0x83
#define MMC_VFAT_TYPE 0xC

//                                              
// MOD 0010090: [FactoryReset] Enable Recovery mode FactoryReset
#define MMC_RECOVERY_TYPE		0x60
#define MMC_MISC_TYPE 0x77
#define MMC_XCALBACKUP_TYPE 0x6E
//                                            


typedef struct MmcPartition MmcPartition;

static unsigned ext3_count = 0;

// LG_FW : 2011.07.06 moon.yongho : saving webdload status variable to eMMC. ----------[[
#ifdef LG_FW_WEB_DOWNLOAD	
static char *ext3_partitions[] = {"persist", "bsp", "blb", "tombstones", "drm", "fota", "system", "cache", "userdata","NONE"};
#else	
static char *ext3_partitions[] = {"system", "userdata", "cache", "NONE"};
#endif /*LG_FW_WEB_DOWNLOAD*/	
// LG_FW : 2011.07.06 moon.yongho -----------------------------------------------------]]

static unsigned vfat_count = 0;
static char *vfat_partitions[] = {"modem", "NONE"};

struct MmcPartition {
    char *device_index;
    char *filesystem;
    char *name;
    unsigned dstatus;
    unsigned dtype ;
    unsigned dfirstsec;
    unsigned dsize;
};

typedef struct {
    MmcPartition *partitions;
    int partitions_allocd;
    int partition_count;
} MmcState;

static MmcState g_mmc_state = {
    NULL,   // partitions
    0,      // partitions_allocd
    -1      // partition_count
};

typedef struct {
	char ret[32];
} testmode_rsp_from_diag_type;

#define FACTORY_RESET_STR_SIZE 11
#define FACTORY_RESET_STR "FACT_RESET_"
#define MMC_DEVICENAME "/dev/block/mmcblk0"
#define MMC_DEVICENAME_MISC "/dev/block/platform/msm_sdcc.1/by-name/misc"


int lge_erase_block(int secnum, size_t size);
int lge_write_block(unsigned int secnum, unsigned char *buf, size_t size);
int lge_read_block(unsigned int secnum, unsigned char *buf, size_t size);

int lge_erase_srd_block(int secnum, size_t size);
int lge_write_srd_block(unsigned int secnum, unsigned char *buf, size_t size);
int lge_read_srd_block(unsigned int secnum, unsigned char *buf, size_t size);


//2012.02.24 kabjoo.choi add it for testing  kernel panic  
static DEFINE_MUTEX(emmc_dir_lock);

static char *lge_strdup(const char *str)
{
	size_t len;
	char *copy;
	
	len = strlen(str) + 1;
	copy = kmalloc(len, GFP_KERNEL);
	if (copy == NULL)
		return NULL;
	memcpy(copy, str, len);
	return copy;
}

int lge_erase_block(int bytes_pos, size_t erase_size)
{
	unsigned char *erasebuf;
	unsigned written = 0;
	erasebuf = kmalloc(erase_size, GFP_KERNEL);
	// allocation exception handling
	if(!erasebuf)
	{
		printk("%s, allocation failed, expected size : %d\n", __func__, erase_size);
		return 0;
	}
	memset(erasebuf, 0xff, erase_size);
	written += lge_write_block(bytes_pos, erasebuf, erase_size);

	kfree(erasebuf);

	return written;
}
EXPORT_SYMBOL(lge_erase_block);

/*                                            */
/* MOD 0014570: [FACTORY RESET] change system call to filp function for handling the flag */
int lge_write_block(unsigned int bytes_pos, unsigned char *buf, size_t size)
{
	struct file *phMscd_Filp = NULL;
	mm_segment_t old_fs;
	unsigned int write_bytes = 0;

	// exception handling
	if((buf == NULL) || size <= 0)
	{
		printk(KERN_ERR "%s, NULL buffer or NULL size : %d\n", __func__, size);
		return 0;
	}
		
	old_fs=get_fs();
	set_fs(get_ds());

	// change from sys operation to flip operation, do not use system call since this routine is also system call service.
	// set O_SYNC for synchronous file io
	phMscd_Filp = filp_open(MMC_DEVICENAME, O_RDWR | O_SYNC, 0);
//	phMscd_Filp = filp_open(MMC_DEVICENAME_MISC, O_RDWR | O_SYNC, 0);
	if( !phMscd_Filp)
	{
		printk(KERN_ERR "%s, Can not access 0x%x bytes postition\n", __func__, bytes_pos );
		goto write_fail;
	}

	phMscd_Filp->f_pos = (loff_t)bytes_pos;
//		phMscd_Filp->f_pos = (loff_t)0x10000;
	write_bytes = phMscd_Filp->f_op->write(phMscd_Filp, buf, size, &phMscd_Filp->f_pos);

	if(write_bytes <= 0)
	{
		printk(KERN_ERR "%s, Can not write 0x%x bytes postition %d size \n", __func__, bytes_pos, size);
		goto write_fail;
	}

write_fail:
	if(phMscd_Filp != NULL)
		filp_close(phMscd_Filp,NULL);
	set_fs(old_fs); 
	return write_bytes;
	
}
/*                                         */

EXPORT_SYMBOL(lge_write_block);

/*                                            */
/* MOD 0014570: [FACTORY RESET] change system call to filp function for handling the flag */
int lge_read_block(unsigned int bytes_pos, unsigned char *buf, size_t size)
{
	struct file *phMscd_Filp = NULL;
	mm_segment_t old_fs;
	unsigned int read_bytes = 0;

	// exception handling
	if((buf == NULL) || size <= 0)
	{
		printk(KERN_ERR "%s, NULL buffer or NULL size : %d\n", __func__, size);
		return 0;
	}
		
	old_fs=get_fs();
	set_fs(get_ds());

	// change from sys operation to flip operation, do not use system call since this routine is also system call service.
	phMscd_Filp = filp_open(MMC_DEVICENAME, O_RDONLY, 0);
	if( !phMscd_Filp)
	{
		printk(KERN_ERR "%s, Can not access 0x%x bytes postition\n", __func__, bytes_pos );
		goto read_fail;
	}

	phMscd_Filp->f_pos = (loff_t)bytes_pos;
	read_bytes = phMscd_Filp->f_op->read(phMscd_Filp, buf, size, &phMscd_Filp->f_pos);

	if(read_bytes <= 0)
	{
		printk(KERN_ERR "%s, Can not read 0x%x bytes postition %d size \n", __func__, bytes_pos, size);
		goto read_fail;
	}

read_fail:
	if(phMscd_Filp != NULL)
		filp_close(phMscd_Filp,NULL);
	set_fs(old_fs); 
	return read_bytes;
}
/*                                         */
EXPORT_SYMBOL(lge_read_block);

const MmcPartition *lge_mmc_find_partition_by_name(const char *name)
{
    if (g_mmc_state.partitions != NULL) {
        int i;
        for (i = 0; i < g_mmc_state.partitions_allocd; i++) {
            MmcPartition *p = &g_mmc_state.partitions[i];
			printk(KERN_INFO "[find]p.name = %s   addr = %d", p->device_index, p->dfirstsec);
//            if (p->device_index !=NULL && p->name != NULL) {
//			if (p->device_index !=NULL && p->name != NULL) {
			if (p->device_index !=NULL) {
                //if (strcmp(p->name, name) == 0) {
                if (strcmp(p->device_index, "/dev/block/mmcblk0p23") == 0) {
                    return p;
                }
            }
        }
    }
    return NULL;
	
}
EXPORT_SYMBOL(lge_mmc_find_partition_by_name);

void lge_mmc_print_partition_status(void)
{
    if (g_mmc_state.partitions != NULL) 
    {
        int i;
        for (i = 0; i < g_mmc_state.partitions_allocd; i++) 
        {
            MmcPartition *p = &g_mmc_state.partitions[i];
            if (p->device_index !=NULL && p->name != NULL) {
                printk(KERN_INFO"Partition Name: %s\n",p->name);
                printk(KERN_INFO"Partition Name: %s\n",p->device_index);
            }
        }
    }
    return;
}
EXPORT_SYMBOL(lge_mmc_print_partition_status);

static void lge_mmc_partition_name (MmcPartition *mbr, unsigned int type) {
	char *name;
	name = kmalloc(64, GFP_KERNEL);
	printk(KERN_INFO "mbr->name = %s   mbr->dtype = %d",mbr->name, type);
    switch(type)
    {
		case MMC_MISC_TYPE:
            sprintf(name,"misc");
            mbr->name = lge_strdup(name);
				printk(KERN_INFO " misc find !!");
			break;
		case MMC_RECOVERY_TYPE:
            sprintf(name,"recovery");
            mbr->name = lge_strdup(name);
			break;
		case MMC_XCALBACKUP_TYPE:
            sprintf(name,"xcalbackup");
            mbr->name = lge_strdup(name);
			break;
        case MMC_BOOT_TYPE:
            sprintf(name,"boot");
            mbr->name = lge_strdup(name);
            break;
        case MMC_EXT3_TYPE:
            if (strcmp("NONE", ext3_partitions[ext3_count])) {
                strcpy((char *)name,(const char *)ext3_partitions[ext3_count]);
                mbr->name = lge_strdup(name);
                ext3_count++;
            }
            mbr->filesystem = lge_strdup("ext3");
            break;
        case MMC_VFAT_TYPE:
            if (strcmp("NONE", vfat_partitions[vfat_count])) {
                strcpy((char *)name,(const char *)vfat_partitions[vfat_count]);
                mbr->name = lge_strdup(name);
                vfat_count++;
            }
            mbr->filesystem = lge_strdup("vfat");
            break;
    };
	kfree(name);
}

//static int lge_mmc_read_mbr (MmcPartition *mbr) {
/*                                            */
/* MOD 0014570: [FACTORY RESET] change system call to filp function for handling the flag */
int lge_mmc_read_mbr (MmcPartition *mbr) {
	//int fd;
	unsigned char *buffer = NULL;
	char *device_index = NULL;
	int idx, i;
	unsigned mmc_partition_count = 0;
	unsigned int dtype;
	unsigned int dfirstsec;
//	unsigned int EBR_first_sec;
	//unsigned int EBR_current_sec;
	int ret = -1;

	struct file *phMscd_Filp = NULL;
	mm_segment_t old_fs;

	old_fs=get_fs();
	set_fs(get_ds());

	buffer = kmalloc(512, GFP_KERNEL);
	device_index = kmalloc(128, GFP_KERNEL);
	if((buffer == NULL) || (device_index == NULL))
	{
		printk("%s, allocation failed\n", __func__);
		goto ERROR2;
	}

	// change from sys operation to flip operation, do not use system call since this routine is also system call service.
	phMscd_Filp = filp_open(MMC_DEVICENAME, O_RDONLY, 0);
	if( !phMscd_Filp)
	{
		printk(KERN_ERR "%s, Can't open device\n", __func__ );
		goto ERROR2;
	}

	phMscd_Filp->f_pos = (loff_t)0;
	if (phMscd_Filp->f_op->read(phMscd_Filp, buffer, 512, &phMscd_Filp->f_pos) != 512)
	{
		printk(KERN_ERR "%s, Can't read device: \"%s\"\n", __func__, MMC_DEVICENAME);
		goto ERROR1;
	}

	/* Check to see if signature exists */
	if ((buffer[TABLE_SIGNATURE] != 0x55) || \
		(buffer[TABLE_SIGNATURE + 1] != 0xAA))
	{
		printk(KERN_ERR "Incorrect mbr signatures!\n");
		goto ERROR1;
	}
	idx = TABLE_ENTRY_0;
//	for (i = 0; i < 4; i++)
	for (i = 0; i < MAX_PARTITIONS; i++)

	{
		//char device_index[128];

		mbr[mmc_partition_count].dstatus = \
		            buffer[idx + i * TABLE_ENTRY_SIZE + OFFSET_STATUS];
		mbr[mmc_partition_count].dtype   = \
		            buffer[idx + i * TABLE_ENTRY_SIZE + OFFSET_TYPE];
		mbr[mmc_partition_count].dfirstsec = \
		            GET_LWORD_FROM_BYTE(&buffer[idx + \
		                                i * TABLE_ENTRY_SIZE + \
		                                OFFSET_FIRST_SEC]);
		mbr[mmc_partition_count].dsize  = \
		            GET_LWORD_FROM_BYTE(&buffer[idx + \
		                                i * TABLE_ENTRY_SIZE + \
		                                OFFSET_SIZE]);
		dtype  = mbr[mmc_partition_count].dtype;
		dfirstsec = mbr[mmc_partition_count].dfirstsec;
		lge_mmc_partition_name(&mbr[mmc_partition_count], \
		                mbr[mmc_partition_count].dtype);

		sprintf(device_index, "%sp%d", MMC_DEVICENAME, (mmc_partition_count+1));
		mbr[mmc_partition_count].device_index = lge_strdup(device_index);

		mmc_partition_count++;
		if (mmc_partition_count == MAX_PARTITIONS)
			goto SUCCESS;
	}

	/* See if the last partition is EBR, if not, parsing is done */
	if (dtype != 0x05)
	{
		goto SUCCESS;
	}
#if 0 // because GPT
	EBR_first_sec = dfirstsec;
	EBR_current_sec = dfirstsec;

	phMscd_Filp->f_pos = (loff_t)(EBR_first_sec * 512);
	if (phMscd_Filp->f_op->read(phMscd_Filp, buffer, 512, &phMscd_Filp->f_pos) != 512)
	{
		printk(KERN_ERR "%s, Can't read device: \"%s\"\n", __func__, MMC_DEVICENAME);
		goto ERROR1;
	}

	/* Loop to parse the EBR */
	for (i = 0;; i++)
	{

		if ((buffer[TABLE_SIGNATURE] != 0x55) || (buffer[TABLE_SIGNATURE + 1] != 0xAA))
		{
		break;
		}
		mbr[mmc_partition_count].dstatus = \
                    buffer[TABLE_ENTRY_0 + OFFSET_STATUS];
		mbr[mmc_partition_count].dtype   = \
                    buffer[TABLE_ENTRY_0 + OFFSET_TYPE];
		mbr[mmc_partition_count].dfirstsec = \
                    GET_LWORD_FROM_BYTE(&buffer[TABLE_ENTRY_0 + \
                                        OFFSET_FIRST_SEC])    + \
                                        EBR_current_sec;
		mbr[mmc_partition_count].dsize = \
                    GET_LWORD_FROM_BYTE(&buffer[TABLE_ENTRY_0 + \
                                        OFFSET_SIZE]);
		lge_mmc_partition_name(&mbr[mmc_partition_count], \
                        mbr[mmc_partition_count].dtype);

		sprintf(device_index, "%sp%d", MMC_DEVICENAME, (mmc_partition_count+1));
		mbr[mmc_partition_count].device_index = lge_strdup(device_index);

		mmc_partition_count++;
		if (mmc_partition_count == MAX_PARTITIONS)
		goto SUCCESS;

		dfirstsec = GET_LWORD_FROM_BYTE(&buffer[TABLE_ENTRY_1 + OFFSET_FIRST_SEC]);
		if(dfirstsec == 0)
		{
			/* Getting to the end of the EBR tables */
			break;
		}
		
		 /* More EBR to follow - read in the next EBR sector */
		 phMscd_Filp->f_pos = (loff_t)((EBR_first_sec + dfirstsec) * 512);
		 if (phMscd_Filp->f_op->read(phMscd_Filp, buffer, 512, &phMscd_Filp->f_pos) != 512)
		 {
			 printk(KERN_ERR "%s, Can't read device: \"%s\"\n", __func__, MMC_DEVICENAME);
			 goto ERROR1;
		 }

		EBR_current_sec = EBR_first_sec + dfirstsec;
	}
#endif // GPT
SUCCESS:
    ret = mmc_partition_count;
	printk(KERN_INFO "mmc_partition succes!!");
ERROR1:
    if(phMscd_Filp != NULL)
		filp_close(phMscd_Filp,NULL);
ERROR2:
	set_fs(old_fs);
	if(buffer != NULL)
		kfree(buffer);
	if(device_index != NULL)
		kfree(device_index);
    return ret;
}
/*                                         */

int lge_mmc_read_gpt (MmcPartition *mbr) {
	//int fd;
	unsigned char *buffer = NULL;
	char *device_index = NULL;
	int idx, i;
	unsigned mmc_partition_count = 0;
	unsigned int dtype;
	unsigned int dfirstsec;
//	unsigned int EBR_first_sec;
	//unsigned int EBR_current_sec;
	int ret = -1;

	struct file *phMscd_Filp = NULL;
	mm_segment_t old_fs;

	old_fs=get_fs();
	set_fs(get_ds());

	buffer = kmalloc(1024, GFP_KERNEL);
	device_index = kmalloc(128, GFP_KERNEL);
	if((buffer == NULL) || (device_index == NULL))
	{
		printk("%s, allocation failed\n", __func__);
		goto ERROR2;
	}

	// change from sys operation to flip operation, do not use system call since this routine is also system call service.
	phMscd_Filp = filp_open(MMC_DEVICENAME, O_RDONLY, 0);
	if( !phMscd_Filp)
	{
		printk(KERN_ERR "%s, Can't open device\n", __func__ );
		goto ERROR2;
	}

	phMscd_Filp->f_pos = (loff_t)0;
	if (phMscd_Filp->f_op->read(phMscd_Filp, buffer, 1024, &phMscd_Filp->f_pos) != 1024)
	{
		printk(KERN_ERR "%s, Can't read device: \"%s\"\n", __func__, MMC_DEVICENAME);
		goto ERROR1;
	}

	/* Check to see if signature exists */
	if ((buffer[TABLE_SIGNATURE] != 0x55) || \
		(buffer[TABLE_SIGNATURE + 1] != 0xAA))
	{
		printk(KERN_ERR "Incorrect mbr signatures!\n");
		goto ERROR1;
	}
	idx = TABLE_ENTRY_0;
//	for (i = 0; i < 4; i++)
	for (i = 0; i < MAX_PARTITIONS; i++)

	{
		//char device_index[128];

		mbr[mmc_partition_count].dstatus = \
		            buffer[idx + i * GPT_ENTRY_SIZE+ OFFSET_STATUS];
		mbr[mmc_partition_count].dtype   = \
		            buffer[idx + i * GPT_ENTRY_SIZE + OFFSET_TYPE];
		mbr[mmc_partition_count].dfirstsec = \
		            GET_LWORD_FROM_BYTE(&buffer[idx + \
		                                i * GPT_ENTRY_SIZE + \
		                                GPT_OFF_FIRST_SEC]);
		mbr[mmc_partition_count].dsize  = \
		            GET_LWORD_FROM_BYTE(&buffer[idx + \
		                                i * GPT_ENTRY_SIZE + \
		                                OFFSET_SIZE]);
		dtype  = mbr[mmc_partition_count].dtype;
		dfirstsec = mbr[mmc_partition_count].dfirstsec;


		sprintf(device_index, "%sp%d", MMC_DEVICENAME, (mmc_partition_count+1));
		mbr[mmc_partition_count].device_index = lge_strdup(device_index);

		mmc_partition_count++;
		if (mmc_partition_count == MAX_PARTITIONS)
			goto SUCCESS;
	}

	/* See if the last partition is EBR, if not, parsing is done */
	if (dtype != 0x05)
	{
		goto SUCCESS;
	}

SUCCESS:
    ret = mmc_partition_count;
	printk(KERN_INFO "mmc_partition succes!!");
ERROR1:
    if(phMscd_Filp != NULL)
		filp_close(phMscd_Filp,NULL);
ERROR2:
	set_fs(old_fs);
	if(buffer != NULL)
		kfree(buffer);
	if(device_index != NULL)
		kfree(device_index);
    return ret;
}

static int lge_mmc_partition_initialied = 0;
int lge_mmc_scan_partitions(void) {
    int i;
    //ssize_t nbytes;

mutex_lock(&emmc_dir_lock);  //kabjoo.choi
	
	if ( lge_mmc_partition_initialied )
	{
		mutex_unlock(&emmc_dir_lock);	//kabjoo.choi
		return g_mmc_state.partition_count;
	}
	
    if (g_mmc_state.partitions == NULL) {

        const int nump = MAX_PARTITIONS;
        MmcPartition *partitions = kmalloc(nump * sizeof(*partitions), GFP_KERNEL);
        if (partitions == NULL) {
	      mutex_unlock(&emmc_dir_lock);	//kabjoo.choi
            return -1;
        }
        g_mmc_state.partitions = partitions;
        g_mmc_state.partitions_allocd = nump;
        memset(partitions, 0, nump * sizeof(*partitions));
    }
    g_mmc_state.partition_count = 0;
    ext3_count = 0;
    vfat_count = 0;

    /* Initialize all of the entries to make things easier later.
     * (Lets us handle sparsely-numbered partitions, which
     * may not even be possible.)
     */
    for (i = 0; i < g_mmc_state.partitions_allocd; i++) {

        MmcPartition *p = &g_mmc_state.partitions[i];

        if (p->device_index != NULL) {
            kfree(p->device_index);
            p->device_index = NULL;
        }
        if (p->name != NULL) {
            kfree(p->name);
            p->name = NULL;
        }
        if (p->filesystem != NULL) {
            kfree(p->filesystem);
            p->filesystem = NULL;
        }
    }

//    g_mmc_state.partition_count = lge_mmc_read_mbr(g_mmc_state.partitions);
	g_mmc_state.partition_count = lge_mmc_read_gpt(g_mmc_state.partitions);
    if(g_mmc_state.partition_count == -1)
    {
        printk(KERN_ERR"Error in reading mbr!\n");
        // keep "partitions" around so we can free the names on a rescan.
        g_mmc_state.partition_count = -1;
    }
	if ( g_mmc_state.partition_count != -1 )
		lge_mmc_partition_initialied = 1;

    mutex_unlock(&emmc_dir_lock);	//kabjoo.choi
    	//printk(KERN_INFO "mmcpartrion name = %s", g_mmc_state.partitions[i].name);
    return g_mmc_state.partition_count;
}

EXPORT_SYMBOL(lge_mmc_scan_partitions);

#ifdef CONFIG_MACH_LGE_120_BOARD //                                                                    
static int write_status_power(const char *val)
{
	int h_file = 0;
	int ret = 0;

	int offset = (int)PTN_POWER_POSITION_IN_MISC_PARTITION;

	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);
	h_file = sys_open(MMC_DEVICENAME_MISC, O_RDWR | O_SYNC,0);

	if(h_file >= 0)
	{
		sys_lseek( h_file, offset, 0 );

		ret = sys_write( h_file, val, 1);

		if( ret != 1 )
		{
			printk("Can't write in MISC partition.\n");
			return ret;
		}

		sys_close(h_file);
	}
	else
	{
		printk("Can't open MISC partition handle = %d.\n",h_file);
		return 0;
	}
	set_fs(old_fs);

	return 1;

	
}

int read_status_power(void)
{
	int h_file = 0;
	int ret = 0;
	char buf;

	int offset = (int)PTN_POWER_POSITION_IN_MISC_PARTITION;

	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);
	h_file = sys_open(MMC_DEVICENAME_MISC, O_RDWR | O_SYNC,0);

	if(h_file >= 0)
	{
		sys_lseek( h_file, offset, 0 );

		ret = sys_read( h_file, &buf, 1);

		if( ret != 1 )
		{
			printk("Can't read MISC partition.\n");
			return ret;
		}

		sys_close(h_file);
	}
	else
	{
		printk("Can't open MISC partition handle = %d.\n",h_file);
		return 0;
	}
	set_fs(old_fs);

	return (int)buf;
}

int set_status_power(char val)
{	
	int ret;
	char *stats;

	stats = kmalloc(1, GFP_KERNEL);
	*stats = val;

	printk("set_status_power :%d\n",*stats);

	ret = write_status_power(stats);
	if (ret != 1) {
		printk("%s : write val has failed!!\n", __func__);
		}
	kfree(stats);
	return 0;
		
}
EXPORT_SYMBOL(set_status_power);

int get_status_power(void)
{	
	char ret;
	char *stats;

	stats = kmalloc(1, GFP_KERNEL);

	ret = read_status_power();
	printk("get_status_power : %d \n",ret);

	kfree(stats);

	return ret;
		
}
EXPORT_SYMBOL(get_status_power);
//                                                                  
#endif
//daheui.kim for kcal_S

//for SRD
int lge_erase_srd_block(int bytes_pos, size_t erase_size)
{
	unsigned char *erasebuf;
	unsigned written = 0;
	erasebuf = kmalloc(erase_size, GFP_KERNEL);
	// allocation exception handling
	if(!erasebuf)
	{
		printk("%s, allocation failed, expected size : %d\n", __func__, erase_size);
		return 0;
	}
	memset(erasebuf, 0xff, erase_size);
	written += lge_write_srd_block(bytes_pos, erasebuf, erase_size);

	kfree(erasebuf);

	return written;
}
EXPORT_SYMBOL(lge_erase_srd_block);

/*                                            */
/* MOD 0014570: [FACTORY RESET] change system call to filp function for handling the flag */
int lge_write_srd_block(unsigned int bytes_pos, unsigned char *buf, size_t size)
{
	struct file *phMscd_Filp = NULL;
	mm_segment_t old_fs;
	unsigned int write_bytes = 0;

	// exception handling
	if((buf == NULL) || size <= 0)
	{
		printk(KERN_ERR "%s, NULL buffer or NULL size : %d\n", __func__, size);
		return 0;
	}
		
	old_fs=get_fs();
	set_fs(get_ds());

	// change from sys operation to flip operation, do not use system call since this routine is also system call service.
	// set O_SYNC for synchronous file io
	phMscd_Filp = filp_open(MMC_DEVICENAME_MISC, O_RDWR | O_SYNC, 0);
//	phMscd_Filp = filp_open(MMC_DEVICENAME_MISC, O_RDWR | O_SYNC, 0);
	if( !phMscd_Filp)
	{
		printk(KERN_ERR "%s, Can not access 0x%x bytes postition\n", __func__, bytes_pos );
		goto write_fail;
	}

	phMscd_Filp->f_pos = (loff_t)bytes_pos;
//		phMscd_Filp->f_pos = (loff_t)0x10000;
	write_bytes = phMscd_Filp->f_op->write(phMscd_Filp, buf, size, &phMscd_Filp->f_pos);

	if(write_bytes <= 0)
	{
		printk(KERN_ERR "%s, Can not write 0x%x bytes postition %d size \n", __func__, bytes_pos, size);
		goto write_fail;
	}

write_fail:
	if(phMscd_Filp != NULL)
		filp_close(phMscd_Filp,NULL);
	set_fs(old_fs); 
	return write_bytes;
	
}
/*                                         */

EXPORT_SYMBOL(lge_write_srd_block);

/*                                            */
/* MOD 0014570: [FACTORY RESET] change system call to filp function for handling the flag */
int lge_read_srd_block(unsigned int bytes_pos, unsigned char *buf, size_t size)
{
	struct file *phMscd_Filp = NULL;
	mm_segment_t old_fs;
	unsigned int read_bytes = 0;

	// exception handling
	if((buf == NULL) || size <= 0)
	{
		printk(KERN_ERR "%s, NULL buffer or NULL size : %d\n", __func__, size);
		return 0;
	}
		
	old_fs=get_fs();
	set_fs(get_ds());

	// change from sys operation to flip operation, do not use system call since this routine is also system call service.
	phMscd_Filp = filp_open(MMC_DEVICENAME_MISC, O_RDONLY, 0);
	if( !phMscd_Filp)
	{
		printk(KERN_ERR "%s, Can not access 0x%x bytes postition\n", __func__, bytes_pos );
		goto read_fail;
	}

	phMscd_Filp->f_pos = (loff_t)bytes_pos;
	read_bytes = phMscd_Filp->f_op->read(phMscd_Filp, buf, size, &phMscd_Filp->f_pos);

	if(read_bytes <= 0)
	{
		printk(KERN_ERR "%s, Can not read 0x%x bytes postition %d size \n", __func__, bytes_pos, size);
		goto read_fail;
	}

read_fail:
	if(phMscd_Filp != NULL)
		filp_close(phMscd_Filp,NULL);
	set_fs(old_fs); 
	return read_bytes;
}
/*                                         */
EXPORT_SYMBOL(lge_read_srd_block);


static int __init lge_emmc_direct_access_init(void)
{
	printk(KERN_INFO"%s: finished\n", __func__);

/*                                            */
/* ADD 0013860: [FACTORY RESET] ERI file save */
#ifdef CONFIG_LGE_ERI_DOWNLOAD
	eri_dload_wq = create_singlethread_workqueue("eri_dload_wq");
	INIT_WORK(&eri_dload_data.work, eri_dload_func);
#endif
/*                                          */

#ifdef CONFIG_LGE_DID_BACKUP
	did_dload_wq = create_singlethread_workqueue("did_dload_wq");
	INIT_WORK(&did_dload_data.work, did_dload_func);
#endif

#ifdef CONFIG_LGE_VOLD_SUPPORT_CRYPT
	cryptfs_cmd_wq = create_singlethread_workqueue("cryptfs_cmd_wq");
	INIT_WORK(&cryptfs_cmd_data.work, cryptfs_cmd_func);
#endif

	return 0;
}

static void __exit lge_emmc_direct_access_exit(void)
{
	return;
}

module_init(lge_emmc_direct_access_init);
module_exit(lge_emmc_direct_access_exit);

MODULE_DESCRIPTION("LGE emmc direct access apis");
MODULE_AUTHOR("SeHyun Kim <sehyuny.kim@lge.com>");
MODULE_LICENSE("GPL");
