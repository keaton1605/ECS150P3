#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "queue.h"
#include "thread.h"
#include "tps.h"

/* TODO: Phase 2 */

typedef struct tps {
	pthread_t tid;
	char* adr;
} *tps_p;

queue_t tpsQueue;

int findTid(void* data, void* arg)
{
	tps_p currTps = (tps_p)data;
	pthread_t tid = (pthread_t)arg;
	
	//printf("About to seg fault\n");
	
	if (currTps->tid == tid)
	{
		return 1;
	}

	return 0;
}

int tps_init(int segv)
{
	if (tpsQueue != NULL)
		return -1;

	tpsQueue = queue_create();

	return 0;
	/* TODO: Phase 2 */
}

int tps_create(void)
{
	tps_p newTps = malloc(sizeof(tps_p));
	void* mMap;

	enter_critical_section();

	mMap = mmap(NULL, TPS_SIZE, PROT_NONE, MAP_PRIVATE, -1, 0);

	newTps->tid = pthread_self();
	newTps->adr = mMap;

	queue_enqueue(tpsQueue, (void*)newTps);

	exit_critical_section();

	return 0;
	/* TODO: Phase 2 */
}

int tps_destroy(void)
{
	pthread_t tid = pthread_self();
	tps_p currTps = NULL;

	queue_iterate(tpsQueue, findTid, (void*)tid, (void**)currTps);
	
	/*if (currTps == NULL)
	{
		exit_critical_section();
		return -1;
	}*/
	
	if(queue_delete(tpsQueue, (void**)&currTps) == -1)
	{
		exit_critical_section();
		return -1;
	}

	free(currTps);

	exit_critical_section();

	return 0;

	/* TODO: Phase 2 */
}

int tps_read(size_t offset, size_t length, char *buffer)
{
	if (offset + length > TPS_SIZE || buffer == NULL)
		return -1;

	tps_p currTps = NULL;
	pthread_t tid = pthread_self();

	enter_critical_section();

	queue_iterate(tpsQueue, findTid, (void*)tid, (void**)&currTps);

	if (currTps == NULL)
	{
		exit_critical_section();
		return -1;
	}

	mprotect(currTps->adr, TPS_SIZE, PROT_READ);
	memcpy(buffer, currTps->adr + offset, length);
	mprotect(currTps->adr, TPS_SIZE, PROT_NONE);
	//printf("Hi: %s\n", buffer);
	exit_critical_section();
	
	return 0;
	/* TODO: Phase 2 */
}

int tps_write(size_t offset, size_t length, char *buffer)
{
	if (offset + length > TPS_SIZE || buffer == NULL)
		return -1;
	
	tps_p currTps = NULL;
	pthread_t tid = pthread_self();

	enter_critical_section();

	queue_iterate(tpsQueue, findTid, (void*)tid, (void**)currTps);

	if (currTps == NULL)
	{
		exit_critical_section();
		return -1;
	}

	mprotect(currTps->adr, TPS_SIZE, PROT_WRITE);
	memcpy(currTps->adr + offset, buffer, length);
	mprotect(currTps->adr, TPS_SIZE, PROT_NONE);

	exit_critical_section();
	
	return 0;
	/* TODO: Phase 2 */
}

int tps_clone(pthread_t tid)
{
	tps_p currTps = NULL;
	tps_p toClone = malloc(sizeof(tps_p));
	//char* adr;

	enter_critical_section();

	queue_iterate(tpsQueue, findTid, (void*)tid, (void**)currTps);

	if (currTps == NULL)
	{
		exit_critical_section();
		return -1;
	}

	memcpy(toClone->adr, currTps->adr, TPS_SIZE);
	queue_enqueue(tpsQueue, (void*)toClone);

	exit_critical_section();
	
	return 0;
	/* TODO: Phase 2 */
}

