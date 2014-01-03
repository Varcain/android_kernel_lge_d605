
/*             
  
                                        
                                             
  
                                  
 */
#include <linux/semaphore.h>

#define FILETOSTOP "launcher.db"
#define MAXNUMFILES 2000
#define PATHLEN     100
#define NAMELEN     100
#define TOTALLEN    500 // PATHLEN + NAMELEN + "start offset" len + "length" len
#define ALIGNSIZELL 4096ll
#define ALIGNLL(x) (((x)/ALIGNSIZELL)*ALIGNSIZELL)
#define ENDALIGNLL(x) ((((x)+ALIGNSIZELL-1ll)/ALIGNSIZELL)*ALIGNSIZELL)

#define PROF_NOT   0
#define PROF_INIT  1
#define PROF_RUN   2
#define PROF_OPT   3
#define PROF_DONE  4

struct sreadahead_profdata {
    char procname[MAXNUMFILES][NAMELEN];
	unsigned char name[MAXNUMFILES][NAMELEN];
	unsigned char path[MAXNUMFILES][PATHLEN];
	size_t len[MAXNUMFILES]; // same as pos[][1] - pos[][0]
	long long pos[MAXNUMFILES][2]; // 0: start position 1: end position
	int cnt; // total number of profiled files
};

struct sreadahead_prof {
	struct sreadahead_profdata* data;
	int state;
	struct semaphore lock;
};

/*              */
