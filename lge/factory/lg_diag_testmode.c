#include <linux/module.h>
#include <lg_diagcmd.h>
#include <linux/input.h>
#include <linux/syscalls.h>

#include <lg_fw_diag_communication.h>
#include <lg_diag_testmode.h>
#include <mach/qdsp5v2/audio_def.h>
#include <linux/delay.h>

#include <userDataBackUpDiag.h>
#include <userDataBackUpTypeDef.h> 
#include <../../kernel/arch/arm/mach-msm/smd_private.h>
#include <linux/slab.h>


#include <board_lge.h>
#include <lg_backup_items.h>

#include <linux/gpio.h>
#include <linux/mfd/pmic8058.h>
#include <mach/irqs.h>


extern void *smem_alloc(unsigned id, unsigned size);

extern int lge_mmc_scan_partitions(void);
extern int lge_erase_block(int secnum, size_t size);
extern int lge_write_block(int secnum, unsigned char *buf, size_t size);
extern int lge_read_block(int secnum, unsigned char *buf, size_t size);

extern int lge_erase_srd_block(int secnum, size_t size);
extern int lge_write_srd_block(int secnum, unsigned char *buf, size_t size);
extern int lge_read_srd_block(int secnum, unsigned char *buf, size_t size);


#ifdef CONFIG_LGE_DLOAD_SRD  //kabjoo.choi
#define SIZE_OF_SHARD_RAM  0x10000  //384K

//                                                                             
extern void diag_SRD_Init(udbp_req_type * req_pkt, udbp_rsp_type * rsp_pkt);
extern void diag_userDataBackUp_entrySet(udbp_req_type * req_pkt, udbp_rsp_type * rsp_pkt, script_process_type MODEM_MDM );
extern boolean writeBackUpNVdata( char * ram_start_address , unsigned int size);

extern unsigned int srd_bytes_pos_in_emmc ;
unsigned char * load_srd_shard_base;
unsigned char * load_srd_kernel_base;
#endif 


asmlinkage long sys_test (int* temp)
{
	printk(KERN_INFO "[SRD] SRD_test \n");
	if(temp)
		*temp = 10;
	return 3;
}


//====================================================================
// Self Recovery Download Support  diag command 249-XX
//====================================================================
#ifdef CONFIG_LGE_DLOAD_SRD  //kabjoo.choi
//                                                                     
asmlinkage long sys_LGE_Dload_SRD (void *req_pkt_ptr, void *rsp_pkt_ptr)
{
    udbp_req_type *req_ptr = (udbp_req_type *) req_pkt_ptr;
    udbp_rsp_type *rsp_ptr = (udbp_rsp_type *) rsp_pkt_ptr;
    //uint16 rsp_len = pkg_len;
    int write_size=0 , mtd_op_result=0;


    // DIAG_TEST_MODE_F_rsp_type union type is greater than the actual size, decrease it in case sensitive items
    switch(req_ptr->header.sub_cmd)
    {
        case  SRD_INIT_OPERATION:
printk(KERN_INFO "[SRD] SRD_INIT_OPERATION \n");
            diag_SRD_Init(req_ptr,rsp_ptr);
            break;

        case USERDATA_BACKUP_REQUEST:
printk(KERN_INFO "[SRD] USERDATA_BACKIP_REQUEST \n");
            //remote_rpc_srd_cmmand(req_ptr, rsp_ptr);  //userDataBackUpStart() ���⼭ ... shared ram ���� �ϵ���. .. 
            diag_userDataBackUp_entrySet(req_ptr,rsp_ptr,0);  //write info data ,  after rpc respons include write_sector_counter  

            //todo ..  rsp_prt->header.write_sector_counter,  how about checking  no active nv item  ; 
            // write ram data to emmc misc partition  as many as retruned setor counters 
            load_srd_shard_base=smem_alloc(SMEM_ERR_CRASH_LOG, SIZE_OF_SHARD_RAM);  //384K byte 

            if (load_srd_shard_base ==NULL)
            {
                ((udbp_rsp_type*)rsp_ptr)->header.err_code = UDBU_ERROR_CANNOT_COMPLETE;
				printk(KERN_INFO "[SRD] backup req smem alloc fail!! ");
                break;
                // return rsp_ptr;
            }

            write_size= req_ptr->nv_counter *256; //return nv backup counters  
            printk(KERN_INFO "[SRD] backup req// nv_counter = %d", req_ptr->nv_counter);
            write_size= (req_ptr->header.packet_version/0x10000) *256; //return nv backup counters 
            //write_size = 512;
		printk(KERN_INFO "[SRD] backup req// pkt_version = %d", (int) req_ptr->header.packet_version);

            if( write_size >SIZE_OF_SHARD_RAM)
            {
                ((udbp_rsp_type*)rsp_ptr)->header.err_code = UDBU_ERROR_CANNOT_COMPLETE;  //hue..
                break;
            }

            load_srd_kernel_base=kmalloc((size_t)write_size, GFP_KERNEL);

            memcpy(load_srd_kernel_base,load_srd_shard_base,write_size);
            //srd_bytes_pos_in_emmc+512 means that info data already writed at emmc first sector 
            mtd_op_result = lge_write_srd_block(srd_bytes_pos_in_emmc+512, load_srd_kernel_base, write_size);  //512 info data 

            if(mtd_op_result!= write_size)
            {
                ((udbp_rsp_type*)rsp_ptr)->header.err_code = UDBU_ERROR_CANNOT_COMPLETE;
                kfree(load_srd_kernel_base);
				printk(KERN_INFO "[SRD] backup req// mtd_op != wtite_size");
                break;
            }

            kfree(load_srd_kernel_base);
            #if 0
            if ( !writeBackUpNVdata( load_srd_base , write_size))
            {
                ((udbp_rsp_type*)rsp_ptr)->header.err_code = UDBU_ERROR_CANNOT_COMPLETE;
                return rsp_ptr;
            }
            #endif
            break;

 

        case GET_DOWNLOAD_INFO :
            break;

        case EXTRA_NV_OPERATION :
            #ifdef LG_FW_SRD_EXTRA_NV
            diag_extraNv_entrySet(req_ptr,rsp_ptr);
            #endif
            break;

        case PRL_OPERATION :
            #ifdef LG_FW_SRD_PRL
            diag_PRL_entrySet(req_ptr,rsp_ptr);
            #endif
            break;

        default :
            rsp_ptr =NULL; //(void *) diagpkt_err_rsp (DIAG_BAD_PARM_F, req_ptr, pkt_len);
            break;
    }

    /* Execption*/
    if (rsp_ptr == NULL){
        return FALSE;
    }
	printk(KERN_INFO "[SRD] syscall complete rsp->err_code = %d ", (int)rsp_ptr->header.err_code);

  return TRUE;
}
//                             
#endif 



