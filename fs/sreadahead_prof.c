
/*             
  
                                        
                                             
  
                                  
 */
#include <linux/mount.h>
#include <linux/debugfs.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include "mount.h"
#include "ext4/ext4.h"
#include "sreadahead_profi.h"
static struct sreadahead_prof prof;

//--------------------------------------------------------------
// functions - initialization
//--------------------------------------------------------------

static ssize_t sreadahead_dbgfs_read(
	struct file *file,
	char __user *buff,
	size_t buff_count,
	loff_t *ppos)
{
	int i;
	unsigned char linebuf[TOTALLEN];
	ssize_t readsize = 0;
	int strlen;

	for( i = 0; i < prof.data->cnt; ++i){
		strlen = sprintf( linebuf, "%s%s:%lld:%lld#%s\n", 
			prof.data->path[i], prof.data->name[i],
			prof.data->pos[i][0],
			prof.data->pos[i][1]-prof.data->pos[i][0],
            prof.data->procname[i]);

		if( copy_to_user( buff + readsize, linebuf, strlen))
			return readsize;
		readsize += (ssize_t)strlen;
	}
	(*ppos) = 0;
	return readsize;
}

static ssize_t sreadaheadflag_dbgfs_read(
	struct file *file,
	char __user *buff,
	size_t buff_count,
	loff_t *ppos)
{
	if( copy_to_user( buff, &prof.state, sizeof(int))) return 0;
	(*ppos) = 0;
	return sizeof(int);
}

static ssize_t sreadaheadflag_dbgfs_write(
	struct file *file,
	const char __user *buff,
	size_t count,
	loff_t *ppos)
{
	if( copy_from_user( &prof.state, buff, sizeof(int))) return 0;
	(*ppos) = 0;
	return sizeof(int);
}

static const struct file_operations sreadaheadflag_dbgfs_fops = {
	.read = sreadaheadflag_dbgfs_read,
	.write = sreadaheadflag_dbgfs_write,
};

static const struct file_operations sreadahead_dbgfs_fops = {
	.read = sreadahead_dbgfs_read,
};

static int sreadahead_struct_init( void)
{
	prof.data = (struct sreadahead_profdata*)vmalloc(sizeof(struct sreadahead_profdata));
	//                       
	// make codes robust
	if( prof.data == NULL) return -1;
	memset( prof.data, 0x00, sizeof(struct sreadahead_profdata));
	prof.state = PROF_RUN;
	return 0;
}

static int __init sreadahead_init( void)
{
	struct dentry *dbgfs_dir;

	// 1. semaphore init
	sema_init( &prof.lock, 1);

	// 2. state init
	prof.state = PROF_NOT;

	// 3. debugfs init for sreadahead
	dbgfs_dir = debugfs_create_dir( "sreadahead", NULL);
	if( !dbgfs_dir) return (-1);
	debugfs_create_file( "profilingdata",
		0444, dbgfs_dir, NULL,
		&sreadahead_dbgfs_fops);
	debugfs_create_file( "profilingflag",
		0644, dbgfs_dir, NULL,
		&sreadaheadflag_dbgfs_fops);
	return 0;
}

//in fact, never need to use this function
//because only user mode knows when to finish
//static int sreadahead_finish()
//{
//	vfree( prof.data);
//}

__initcall( sreadahead_init);

//--------------------------------------------------------------
// functions - sreadahead profiling
//--------------------------------------------------------------

static int sreadahead_prof_RUN(
	struct file *filp,
	size_t len,
	loff_t pos)
{
	int i;
	unsigned char tmpstr[PATHLEN];

    // maxpark
    // this temp code must be replaced so that 
    // if len == 0, then assign the file whole size to 'len'
    if(len == 0) len = 4096 * 200;

	for( i = 0; i < prof.data->cnt; ++i){
		if( strcmp( prof.data->name[i], filp->f_path.dentry->d_name.name) == 0){
			// CASE1: not found entry yet 
			if( ENDALIGNLL(pos + (long long)len) < prof.data->pos[i][0] ||
			       prof.data->pos[i][1] < ALIGNLL( pos)) continue;

			// CASE2: found entry looking for
			if( prof.data->pos[i][0] > pos) 
				prof.data->pos[i][0] = ALIGNLL(pos);
			if( prof.data->pos[i][1] < pos + (long long)len)
				prof.data->pos[i][1] = ENDALIGNLL(pos + (long long)len);
			// byungchul.park
			// bugfix
			prof.data->len[i] = prof.data->pos[i][1] - prof.data->pos[i][0];
			break;
		}
	}
	// add a new entry
	if( i == prof.data->cnt){
		struct dentry* tmpdentry = 0;
		struct mount* tmpmnt;
		struct mount* tmpoldmnt;
		tmpmnt = real_mount(filp->f_vfsmnt);

		//                       
		// make codes robust
		strncpy( prof.data->name[i], filp->f_path.dentry->d_name.name, NAMELEN - 1);
		prof.data->name[i][NAMELEN - 1] = '\0';
		prof.data->len[i] = len;
		prof.data->pos[i][0] = prof.data->pos[i][1] = ALIGNLL( pos);
		prof.data->pos[i][1] += ENDALIGNLL( (long long)len);

		tmpdentry = filp->f_path.dentry->d_parent;
		do{
			tmpoldmnt = tmpmnt;
			while( !IS_ROOT(tmpdentry)){
				strcpy( tmpstr, prof.data->path[i]);
				//                       
				// make codes robust
				strncpy( prof.data->path[i], tmpdentry->d_name.name, PATHLEN - 1);
				prof.data->path[i][PATHLEN - 1] = '\0';
				if( strlen(prof.data->path[i]) + strlen("/") > PATHLEN -1) return -1;
				strcat( prof.data->path[i], "/");
				if( strlen(prof.data->path[i]) + strlen(tmpstr) > PATHLEN -1) return -1;
				strcat( prof.data->path[i], tmpstr);
				tmpdentry = tmpdentry->d_parent;
			}
			tmpdentry = tmpmnt->mnt_mountpoint;
			tmpmnt = tmpmnt->mnt_parent;
		}while( tmpmnt != tmpoldmnt);
		strcpy( tmpstr, prof.data->path[i]);
		strcpy( prof.data->path[i], "/");
		//                       
		// make codes robust
		if( strlen(prof.data->path[i]) + strlen(tmpstr) > PATHLEN -1) return -1;
		strcat( prof.data->path[i], tmpstr);
        // add process name using the file
        prof.data->procname[i][0] = '\0';
        get_task_comm( prof.data->procname[i], current);

		prof.data->cnt++;
		if( prof.data->cnt == MAXNUMFILES) 
			prof.state = PROF_OPT;
		if( !strcmp( filp->f_path.dentry->d_name.name, FILETOSTOP)) 
			prof.state = PROF_OPT;
	}
	return 0;
}
static int sreadahead_prof_OPT_systemonly( void)
{
	int rdcnt = 0;
	int wrcnt = 0;

	for( rdcnt = 0; rdcnt < prof.data->cnt; ++rdcnt){
		if( strncmp( prof.data->path[rdcnt], "/system/", 8) != 0){
			continue;
		}
		if( wrcnt != rdcnt){
			strcpy( prof.data->path[wrcnt], prof.data->path[rdcnt]);
			strcpy( prof.data->name[wrcnt], prof.data->name[rdcnt]);
			strcpy( prof.data->procname[wrcnt], prof.data->procname[rdcnt]);
			prof.data->len[wrcnt] = prof.data->len[rdcnt];
			prof.data->pos[wrcnt][0] = prof.data->pos[rdcnt][0];
			prof.data->pos[wrcnt][1] = prof.data->pos[rdcnt][1];
		}
		++wrcnt;
	}
	prof.data->cnt = wrcnt;
	return 0;
}
#define COMBINE_DONE 0
#define COMBINE_FAIL (-1)

static int combining( int rdcnt, int wrcnt)
{
	int i;
	int final_start, final_end;
	for( i = 0; i < wrcnt; ++i){
        if(	strcmp( prof.data->name[rdcnt], prof.data->name[i]) != 0 ||
                strcmp( prof.data->path[rdcnt], prof.data->path[i]) != 0 ||
                strcmp( prof.data->procname[rdcnt], prof.data->procname[i]) != 0
          ) continue;
        if(	prof.data->pos[rdcnt][1] < prof.data->pos[i][0] ||
                prof.data->pos[i][1] < prof.data->pos[rdcnt][0]) continue;
        final_start = min( prof.data->pos[rdcnt][0], prof.data->pos[i][0]);
		final_end   = max( prof.data->pos[rdcnt][1], prof.data->pos[i][1]);
		prof.data->pos[i][0] = final_start;
		prof.data->pos[i][1] = final_end;
		prof.data->len[i] = final_end - final_start;
		return COMBINE_DONE;
	}
	return COMBINE_FAIL;
}

static bool sreadahead_prof_OPT_combining( void)
{
	int rdcnt = 0;
	int wrcnt = 0;

	for( rdcnt = 0; rdcnt < prof.data->cnt; ++rdcnt){
		if( combining( rdcnt, wrcnt) == COMBINE_DONE){
			continue;
		}
		if( wrcnt != rdcnt){
			strcpy( prof.data->path[wrcnt], prof.data->path[rdcnt]);
			strcpy( prof.data->name[wrcnt], prof.data->name[rdcnt]);
			strcpy( prof.data->procname[wrcnt], prof.data->procname[rdcnt]);
			prof.data->len[wrcnt] = prof.data->len[rdcnt];
			prof.data->pos[wrcnt][0] = prof.data->pos[rdcnt][0];
			prof.data->pos[wrcnt][1] = prof.data->pos[rdcnt][1];
		}
		++wrcnt;
	}
	prof.data->cnt = wrcnt;
	return (wrcnt == rdcnt);
}

static int sreadahead_prof_OPT( void)
{
	sreadahead_prof_OPT_systemonly();
	while( !sreadahead_prof_OPT_combining());
	prof.state = PROF_DONE;
	return 0;
}

int sreadahead_prof(
	struct file *filp,
	size_t len,
	loff_t pos)
{
	if( prof.state == PROF_NOT) return 0; // minimize overhead in case of not profiling
	if( filp->f_op == &ext4_file_operations){
		down( &prof.lock);
		if( prof.state == PROF_INIT) sreadahead_struct_init();
		if( prof.state == PROF_RUN ) sreadahead_prof_RUN( filp, len, pos);
		if( prof.state == PROF_OPT ) sreadahead_prof_OPT();
		up( &prof.lock);
	}
	return 0;
}

/*              */
