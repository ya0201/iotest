#ifndef __IOTEST_H__
#define __IOTEST_H__


#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>

#define __USE_LARGEFILE64
#include <sys/stat.h>

//#include <fcntl.h>
#include <time.h>
#include <getopt.h>
#include <ctype.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <asm/ioctl.h>
#include <asm/fcntl.h>

//#define _IO(x,y)       (((x)<<8)|y)
#define BLKGETSIZE _IO(0x12,96) /* return device size /512 (long *arg) */
#define BLKSSZGET  _IO(0x12,104)/* get block device sector size */
#define BLKBSZGET  _IOR(0x12,112,size_t)

#ifdef DEBUG
#define debug(level,fmt,args...) ({                          		\
	if ( level <= default_debug_level)                   		\
		printf("%s(%d):"fmt,__FUNCTION__,__LINE__,##args);      \
})                                                           		\

#else
#define debug(level,fmt...)
#endif

#define PAGE_SIZE 	0x1000
#define PAGE_MASK 	(~(PAGE_SIZE-1))
#define ROUND_UP(x)     (((x)+PAGE_SIZE-1)&PAGE_MASK)
#define ROUND_DOWN(x)   ((x)&PAGE_MASK)

#endif
