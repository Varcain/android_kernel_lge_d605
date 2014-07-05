//FEATURE_SDCARD_MEDIAEXN_SYSTEMCALL_ENCRYPTION

#include <linux/kernel.h>
#include <linux/linkage.h>
#include <linux/sched.h>
#include <linux/unistd.h>
#include "LGSDEncManager.h"

int propertyMediaCheck;				// Media Ecryption
char savedfileExtList[MAX_MEDIA_EXT_LENGTH];
char asecExtension[MAX_MEDIA_EXT_LENGTH] = ".ASEC";

/*
*     System property For Media Encryption.
*/
asmlinkage long sys_set_media_property(int value)
{
//	printk("%s :: SystemCall value: %d , propertymediacheck : %d\n", __func__,value,propertyMediaCheck);
	propertyMediaCheck = value;
	return 1;
}

int getMediaProperty(void){
	return propertyMediaCheck;
}

/*
* Media File System call.
*/
asmlinkage long sys_set_media_ext(char *mediaExtList)
{
	memset(savedfileExtList, 0, sizeof(savedfileExtList));

	if(mediaExtList != NULL){
		strncpy(savedfileExtList, mediaExtList, strlen(mediaExtList));
	}

//	printk("%s :: savedfileExtList: %s\n", __func__,savedfileExtList);

	return 1;
}
//#endif //FEATURE_SDCARD_MEDIAEXN_SYSTEMCALL_ENCRYPTION

char *ecryptfs_Extfilename(const unsigned char *filename){
	char *pos = NULL;
	int len , i;

	if(filename == NULL) {
		return pos;
	}

	pos = strrchr(filename,'.');
	if(pos == NULL){
		return pos;
	}

	len = strlen(pos);
	for(i = 0 ; i < len ; i++){
		if(*(pos+i) >= 'a' && *(pos+i) <= 'z'){
			*(pos+i) = *(pos+i) -32;
		}
	}
	return pos+1;
}
int ecryptfs_asecFileSearch(const unsigned char *filename){
    char *extP = NULL;

    // extract extension in filename
    extP = ecryptfs_Extfilename(filename);
	if(extP == NULL || strlen(extP) < 2){
		printk(KERN_DEBUG "Extfilename is NULL\n");
		return 0;
	}

    //check if the extension is asec
    if(strstr(asecExtension,extP) == NULL){
        return 0;
    }
	return 1;
}
int ecryptfs_mediaFileSearch(const unsigned char *filename){
	char *extP = NULL;

	// extension from filename.
	extP = ecryptfs_Extfilename(filename);
	if(extP == NULL || strlen(extP) < 2){
		printk(KERN_DEBUG "%s :: Extfilename is NULL\n", __func__);
		return 0;
	}

	printk("%s :: savedfileExtList: %s\n", __func__,savedfileExtList);

	//check extention list
	if(sizeof(savedfileExtList) != 0)
	{
		if(strstr(savedfileExtList,extP) == NULL){
			printk(KERN_DEBUG "%s :: NOT ECRYPTFS_MEDIA_EXCEPTION\n", __func__);
			return 0;
		}
	}
	else
	{
		printk(KERN_DEBUG "%s :: getMediaExtList() = NULL\n", __func__);
		return 0;
	}

	return 1;
}

