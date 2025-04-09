#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "tool.h"
void * wgos_malloc(size_t size)
{
	char *ptr=NULL;
	ptr = malloc(size);
	if (ptr == NULL)
		return NULL;
	return ptr;
}

void * wgos_zalloc(size_t size)
{
	return calloc(1, size);
}

int get_process_state(const char *process_nae,int length){
	int bytes;
	char buf[10];
	char cmd[60];
    FILE   *strea;
	if(length > 20){
		wmg_printf(MSG_ERROR,"process nae is too long!\n");
		return -1;
	}
	sprintf(cmd,"ps | grep %s | grep -v grep",process_nae);
    strea = popen(cmd, "r" ); //CHECK_WIFI_SHELL-->> FILE* strea
    if(!strea) return -1;
    bytes = fread( buf, sizeof(char), sizeof(buf), strea);
    pclose(strea);
    if(bytes > 0){
        wmg_printf(MSG_DEBUG,"%s :process exist\n",process_nae);
		return 1;
    }else {
        wmg_printf(MSG_DEBUG,"%s :process not exist\n",process_nae);
        return -1;
    }
}
int sys_get_time(struct sys_time *t)
{
	int res;
	struct timeval tv;
	res = gettimeofday(&tv, NULL);
	t->sec = tv.tv_sec;
	t->usec = tv.tv_usec;
	return res;
}

void ms_sleep(unsigned long ms)
{
	struct timeval tv;
	tv.tv_sec = ms/1000;
	tv.tv_usec = (ms%1000)*1000;
	int err;
	do {
		err = select(0,NULL,NULL,NULL,&tv);
	}while(err < 0 && errno == EINTR);
}
