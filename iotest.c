#include "iotest.h"


#define BLKSIZE_ERR_MSG		"block_size must be digit,eg. 1024,1k,1m,and so on\n"
#define MODE_ERR_MSG		"mode must be digit\n"
#define OPS_ERR_MSG			"operation must be r or w\n"
#define IOCNT_ERR_MSG		"io count must be digit\n"
#define THRNUM_ERR_MSG		"thread num must be digit\n"
#define TIME_ERR_MSG		"time must be digit\n"


#define OPS_READ	0x0
#define OPS_WRITE	0x1
/*
 * Usage:
 *
 * iotest 
 *
 * [ -s block_size]                  : IO block size
 * [ -m mode ]                       : IO mode
 * 					1 - seq,sync,non-direct
 * 					2 - seq,async,non-direct
 * 					3 - seq,sync,direct
 * 					4 - seq,async,direct
 * 					5 - ran,sync,non-direct
 * 					6 - ran,async,non-direct
 * 					7 - ran,sync,direct
 * 					8 - ran,async,direct
 * [ -o read/write ]                 : IO operation
 * [ -f file ]                       : target file or device
 * [ -c io_count]                    : io count
 * [ -n num ]                        : subsequent IO thread num
 * [ -t second ]                     : IO time, second
 * [ -l logfile]                     : log file
 * [ -g debug_level                  : enable debug mode and set debug level, default is 0
 */

struct iotest_parm {
	size_t block_size;
	int mode;
	int rand;
	int operation;
	char *tgtfile;
	int io_count;
	int thread_num;
	int time;
	char *logfile;
};

static struct iotest_parm ioparm = {
	.block_size = 4096,
	.mode = 0,
	.rand = 0,
	.operation = OPS_READ,
	.tgtfile= NULL,
	.io_count = 0,
	.thread_num = 1,
	.time = 0,
	.logfile = NULL
};


struct tgtinfo {
	int fd;
	unsigned int type;
	unsigned long long size;
	unsigned long blksize;
	unsigned long  blkcnt;
};

static struct tgtinfo tgtfile;


struct io_result {
	struct timeval start,stop;
	double total_io;
	double total_usec;
	double total_bytes;
	double iops;
	double throughout;
        double avr_response;
	long max_response;
	pthread_mutex_t mutex;
};

static struct io_result iores = {
	.total_io = 0.0,
	.total_usec = 0.0,
	.total_bytes = 0.0,
	.iops = 0.0,
	.throughout = 0.0,
	.avr_response = 0.0,
	.max_response = 0,
	.mutex = PTHREAD_MUTEX_INITIALIZER	
};

static int default_debug_level = 1;
static int log_fd;

static void usage(int version)
{
	if (version) {
		printf("iotest suite ver 1.5 2005\n");
	} else {
		printf("Usage:\n");
		printf("\t-b block_size\n");
	        printf("\t-m mode\n");
	        printf("\t\t 1 - seq,sync,non-direct\n");
	        printf("\t\t 2 - seq,async,non-direct\n");
	        printf("\t\t 3 - seq,sync,direct\n");
        	printf("\t\t 4 - seq,async,direct\n");
	        printf("\t\t 5 - ran,sync,non-direct\n");
        	printf("\t\t 6 - ran,async,non-direct\n");
	        printf("\t\t 7 - ran,sync,direct\n");
        	printf("\t\t 8 - ran,async,direct\n");
	        printf("\t-o r/w\n");
		printf("\t-f file\n");
		printf("\t-c io_count\n");
		printf("\t-n num\n");
		printf("\t-t time\n");
		printf("\t-l logfile\n");
		printf("\t-h\n");
		printf("\t-v\n");
		printf("\t-g debug_level\n");
	}

	return;
}

static void alarm_handler(int signo)
{
	struct io_result tiores;

	debug(2,"Enter alarm handler\n");

	gettimeofday(&iores.stop,0);
	
	pthread_mutex_lock(&iores.mutex);
	memcpy(&tiores,&iores,sizeof(iores));
	iores.total_io = 0;
	iores.total_bytes = 0;
	iores.total_usec = 0;
	pthread_mutex_unlock(&iores.mutex);

	/* compute */
	tiores.total_usec = (double)(tiores.stop.tv_sec * 1000000 + tiores.stop.tv_usec - 
			    tiores.start.tv_sec * 1000000 - tiores.start.tv_usec);
	
	debug(2,"tiores total_io %lf  total_usec %lf  total_bytes %lf\n"
			,tiores.total_io,tiores.total_usec,tiores.total_bytes);
	
	tiores.iops = (tiores.total_io * 1000000) / (tiores.total_usec);
	tiores.throughout = ( (tiores.total_bytes / 1024 / 1024 ) * 1000000 )/ tiores.total_usec;
	tiores.avr_response = tiores.total_usec / tiores.total_io;
	
	//call logwrite	
	
	printf("IOPS = %.2lf IO/s  Throughout = %.2lfMB  Avr_response = %.2lf usec  Max_response = %lu usec\n",
			tiores.iops,tiores.throughout,tiores.avr_response,tiores.max_response);
	
	alarm(2);
	
	gettimeofday(&iores.start,0);
	
	debug(2,"Leave alarm handler\n");
}

int iotest(int id)
{
	int fd;
	char buf[ioparm.block_size+PAGE_SIZE];
	char *p;
	unsigned long iobyte = 0;
	long iotime = 0;
	unsigned long start = 0;
	unsigned long range = 0;
	struct timeval tv0,tv1;
	unsigned long i = 0;

	debug(2,"Enter iotest, thread id %d\n",id);
	
	fd = open(ioparm.tgtfile, ioparm.mode);
	if (fd < 0) {
		perror("");
		exit(1);
	}
	
	p = (char *)ROUND_UP((unsigned int)buf);
	
	range = (unsigned long)(tgtfile.size / ioparm.block_size);
	debug(2,"range(): %lu (size %lu blksize %lu\n",range,tgtfile.size,ioparm.block_size);

	lseek(fd, 0, SEEK_SET);

	//if (iores.iops == 0)
	gettimeofday(&iores.start,0);
	
	for (;;) {
		
		if (ioparm.io_count != 0 && i >= ioparm.io_count) {
			alarm_handler(SIGALRM);
			break;
		}
		
		if (ioparm.rand) {
			start = (unsigned long)(rand() % range);
			lseek(fd, start * ioparm.block_size, SEEK_SET);
		} else {
			start++;
			if (start >= range) {
				start = 0;
				lseek(fd,0,SEEK_SET);
			} 
		}
		debug(2,"id: %d start %lu\n",id,start);
		
		if (ioparm.operation == OPS_WRITE) {	
			gettimeofday(&tv0,0);
			iobyte = write(fd,p,ioparm.block_size);
			gettimeofday(&tv1,0);
		} else {
			gettimeofday(&tv0,0);
			iobyte = read(fd,p,ioparm.block_size);
			gettimeofday(&tv1,0);
		}

		debug(2,"iobyte %d\n",iobyte);
		
		if (iobyte < 0) {
			perror("io error:");
			abort();
		}

		iotime = tv1.tv_sec * 1000000 + tv1.tv_usec - tv0.tv_sec * 1000000 - tv0.tv_usec;
	
		pthread_mutex_lock(&iores.mutex);
	
		iores.total_io += 1;
		iores.total_bytes += (double)iobyte;
		iores.total_usec += (double)iotime;
		
		if ( iores.max_response < iotime )
			iores.max_response = iotime;
		
		debug(2,"iotime %lu  total_io %lf  total_usec %lf max_response %lu\n",
				iotime,iores.total_io,iores.total_usec,iores.max_response);

		pthread_mutex_unlock(&iores.mutex);
		i++;
	}	

	close(fd);

	debug(2,"Leave iotest, thread id %d\n",id);
	
	return 0;
}

int main(int argc,char **argv)
{
	signed char c;
	struct option long_options[] = {
		{"bs", 1, 0, 'b'},
		{"mode", 1, 0, 'm'},
		{"ops", 1, 0, 'o'},
		{"file", 1, 0, 'f'},
		{"count", 1, 0, 'c'},
		{"num", 1, 0, 'n'},
		{"time", 1, 0, 't'},
		{"log", 1, 0, 'l'},
		{"help", 0, 0, 'h'},
		{"version", 0, 0, 'v'},
		{0, 0, 0, 0}
	};
	int option_index = 0; 
	struct stat64 stat_info;
	int ret = 0;
	pthread_t pt[32];
	int i;

	while ( (c = getopt_long(argc,argv,"b:m:o:f:c:s:d:n:t:l:g:hv",long_options,&option_index)) 
		> 0) {
		char *p;
		int scale;
		
		debug(2,"-%d %s\n",c,optarg);
		
		switch (c) {
			case 'b':
				p = optarg;
					
				while (isdigit(*p) != 0 && p <= (optarg + strlen(optarg)))
					p++;
				
				if (p != optarg) {
					if ( *p == 0) {
						scale = 1;
					} else if ( *p == 'k' || *p == 'K' ) {
						scale = 1024;
					} else if ( *p == 'm' || *p == 'M' ) {
					       scale = 1024*1024;
					}  else {
						printf(BLKSIZE_ERR_MSG);
						exit(1);
					}
					*p = 0;
					ioparm.block_size = atoi(optarg) * scale;
				} else {
					printf(BLKSIZE_ERR_MSG);
					exit(1);
				}

				debug(2,"block_size %d\n",ioparm.block_size);	
				break;
			case 'm':
				switch (atoi(optarg)) {
					
					case 1:
						ioparm.mode |= O_SYNC | O_LARGEFILE;
						ioparm.rand = 0;
 						break;
					case 2:
						ioparm.mode |= O_LARGEFILE;
						ioparm.rand = 0;
						break;	       
					case 3:
						ioparm.mode |= O_SYNC | O_LARGEFILE | O_DIRECT;
						ioparm.rand = 0;
 						break;
					case 4:
						ioparm.mode |= O_LARGEFILE | O_DIRECT;
						ioparm.rand = 0;
						break;	       
					case 5:
						ioparm.mode |= O_SYNC | O_LARGEFILE;
						ioparm.rand = 1;
 						break;
					case 6:
						ioparm.mode |= O_LARGEFILE;
						ioparm.rand = 1;
						break;	       
					case 7:
						ioparm.mode |= O_SYNC | O_LARGEFILE | O_DIRECT;
						ioparm.rand = 1;
 						break;
					case 8:
						ioparm.mode |= O_LARGEFILE | O_DIRECT;
						ioparm.rand = 1;
						break;	       
						
					default:
						printf(MODE_ERR_MSG);
						exit(1);
				}
				
				debug(2,"mode %x , rand %d\n",ioparm.mode,ioparm.rand);
				break;
			case 'o':
				p = optarg;
				if ( *p == 'r') {
					ioparm.operation = OPS_READ;
					ioparm.mode |= O_RDONLY;
				} else if ( *p == 'w' ) {
					ioparm.operation = OPS_WRITE;	
					ioparm.mode |= O_RDWR;
				} else {
					printf(OPS_ERR_MSG);
					exit(1);
				}
				debug(2,"operation %d/n",ioparm.operation);
				break;
			case 'f':
				ioparm.tgtfile = optarg;
				debug(2,"tgtfile %s\n",ioparm.tgtfile);
				break;
			case 'c':
				if ( (ioparm.io_count = atoi(optarg)) == 0) {
					printf(IOCNT_ERR_MSG);
					exit(1);
				}
				debug(2,"io_count %d\n",ioparm.io_count);
				break;
			case 'n':
				if ( (ioparm.thread_num = atoi(optarg)) == 0) {
					printf(THRNUM_ERR_MSG);
					exit(1);
				}
				debug(2,"thread_num %d\n",ioparm.thread_num);
				break;
			case 't':
				if ( (ioparm.time = atoi(optarg)) == 0) {
					printf(TIME_ERR_MSG);
					exit(1);
				}
				debug(2,"time %d\n",ioparm.time);
				break;
			case 'l':
				ioparm.logfile = optarg;
				debug(2,"logfile %s\n",ioparm.logfile);
				break;
			case 'g':
				default_debug_level = atoi(optarg);
				
				debug(2," default_debug_level %d\n",default_debug_level);
				break;
			case 'v':
				usage(1);
				exit(0);
			case 'h':
			case ':':
			case '?':
			default:
				usage(0);
				exit(0);
		}
	}

	if ( ioparm.tgtfile == NULL) {
		printf("Which is the target file? Please specify with -f or --file option\n");
		exit(1);
	}

	//test tgtfile logfile
	if ( (tgtfile.fd = open(ioparm.tgtfile,ioparm.mode)) < 0) {
		perror("open");
		exit(1);
	}
	
	/* Get target file stat info */
	if ( fstat64(tgtfile.fd,&stat_info) < 0) {
		perror("fstat64:");
		exit(1);
	}

	tgtfile.type = stat_info.st_mode;

	if ( S_ISREG(tgtfile.type) ) {
		
		tgtfile.size = stat_info.st_size;
		tgtfile.blksize = stat_info.st_blksize;
		tgtfile.blkcnt = stat_info.st_blocks;
		
	} else if ( S_ISBLK(tgtfile.type) ) {

		if ( ioctl(tgtfile.fd,BLKSSZGET,&tgtfile.blksize) < 0) {
			perror("get blksize:");
			goto err;
		}
		printf("%s: block size %d ",ioparm.tgtfile,tgtfile.blksize);
		
		if ( ioctl(tgtfile.fd,BLKGETSIZE,&tgtfile.blkcnt) < 0) {
			perror("get blkcount:");
			goto err;
		}
		printf("block count %d ",tgtfile.blkcnt);
		
		tgtfile.size = (unsigned long long)tgtfile.blksize * (unsigned long long)tgtfile.blkcnt;
		printf("file size %llu MBytes\n",tgtfile.size >> 20);
		
	} else {
		printf("target file must be regular file or block dev\n");
		goto err;
	}

	debug(2,"tgtfile: type %d  size %llu  blksize %lu blkcnt %lu\n",
			tgtfile.type,tgtfile.size,tgtfile.blksize,tgtfile.blkcnt);
	
	
	/* setup signal handler */
	if (signal(SIGALRM,alarm_handler) == SIG_ERR) {
		perror("signal setup:");
		goto err;
	}
	
	pthread_mutex_init(&iores.mutex,NULL);
	
	//init logfile
	//log_fd = open(ioparm.logfile,O_CREAT | O_TRUNC | O_WRONLY,0666);
	//if (log_fd < 0) {
	//	perror("log fd:");
	//}
	
	alarm(2);
			
	//create thread
	for (i=0; i < ioparm.thread_num; i++) {
		if ( pthread_create(&pt[i],NULL,(void *)iotest,(void *)i) != 0) {
			printf("iotest fail\n");
			goto err;
		}
	}

#if 1
	while (1)
		pause();
#else	
	for (i=0; i < ioparm.thread_num; i++)
		pthread_join(pt[i],NULL);
#endif	
err:	
	close(tgtfile.fd);
	if (log_fd)
		close(log_fd);

	return ret;
}


