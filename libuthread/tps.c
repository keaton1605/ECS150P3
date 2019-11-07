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

typedef struct page {
	char* adr;
	int refCount;
} *page_p;

typedef struct tps {
	pthread_t tid;
	page_p currPage;
} *tps_p;

queue_t tpsQueue;

int findSegV(void* data, void* arg)
{
	tps_p currTps = (tps_p)data;
	char* adr = (char*)arg;

	if (currTps->currPage->adr == adr)
	{
		return 1;
	}

	return 0;
}

int findTid(void* data, void* arg)
{
	tps_p currTps = (tps_p)data;
	pthread_t tid = (pthread_t)arg;

	if (currTps->tid == tid)
	{
		return 1;
	}

	return 0;
}


static void segv_handler(int sig, siginfo_t *si, void *context)
{
	tps_p currTps = NULL;
	
	/*
	* Get the address corresponding to the beginning of the page where the
	* fault occurred
	*/
	void *p_fault = (void*)((uintptr_t)si->si_addr & ~(TPS_SIZE - 1));

	/*
	* Iterate through all the TPS areas and find if p_fault matches one of them
	*/
	queue_iterate(tpsQueue, findSegV, p_fault, (void**)&currTps);
	
	if (currTps != NULL)
	{
		/*if there is a match */
		/* Printf the following error message */
		fprintf(stderr, "TPS protection error!\n");
	}

	/* In any case, restore the default signal handlers */
	signal(SIGSEGV, SIG_DFL);
	signal(SIGBUS, SIG_DFL);
	/* And transmit the signal again in order to cause the program to crash */
	raise(sig);
}

int tps_init(int segv)
{
	if (tpsQueue != NULL)
		return -1;

	tpsQueue = queue_create();

	if (segv) {
		struct sigaction sa;

		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_SIGINFO;
		sa.sa_sigaction = segv_handler;
		sigaction(SIGBUS, &sa, NULL);
		sigaction(SIGSEGV, &sa, NULL);
	}

	return 0;
	/* TODO: Phase 2 */
}

int tps_create(void)
{
	tps_p newTps = malloc(sizeof(tps_p));
	newTps->currPage = malloc(sizeof(page_p));
	void* mMap;

	enter_critical_section();

	mMap = mmap(NULL, TPS_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);

	newTps->tid = pthread_self();
	newTps->currPage->adr = mMap;
	newTps->currPage->refCount = 1;

	queue_enqueue(tpsQueue, (void*)newTps);

	exit_critical_section();

	return 0;
	/* TODO: Phase 2 */
}

int tps_destroy(void)
{
	pthread_t tid = pthread_self();
	tps_p currTps = NULL;

	queue_iterate(tpsQueue, findTid, (void*)tid, (void**)&currTps);
	
	if (currTps == NULL)
	{
		exit_critical_section();
		return -1;
	}
	
	if(queue_delete(tpsQueue, (void**)&currTps) == -1)
	{
		exit_critical_section();
		return -1;
	}

	currTps->currPage->refCount--;
	free(currTps->currPage);
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

	mprotect(currTps->currPage->adr, TPS_SIZE, PROT_READ);
	memcpy(buffer, currTps->currPage->adr + offset, length);
	mprotect(currTps->currPage->adr, TPS_SIZE, PROT_NONE);

	exit_critical_section();
	
	return 0;
	/* TODO: Phase 2 */
}

int tps_write(size_t offset, size_t length, char *buffer)
{
	if (offset + length > TPS_SIZE || buffer == NULL)
		return -1;
	
	tps_p currTps = NULL;
	tps_p toClone = malloc(sizeof(tps_p));
	toClone->currPage = malloc(sizeof(page_p));
	pthread_t tid = pthread_self();

	enter_critical_section();

	queue_iterate(tpsQueue, findTid, (void*)tid, (void**)&currTps);

	if (currTps == NULL)
	{
		exit_critical_section();
		return -1;
	}

	if (currTps->currPage->refCount == 1)
	{
		mprotect(currTps->currPage->adr, TPS_SIZE, PROT_WRITE);
		memcpy(currTps->currPage->adr + offset, buffer, length);
		mprotect(currTps->currPage->adr, TPS_SIZE, PROT_NONE);
	}
	else
	{
		void* mMap;

		page_p tempPage = currTps->currPage;
		tempPage->refCount--;

		mMap = mmap(NULL, TPS_SIZE, PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	
		currTps->currPage = malloc(sizeof(page_p));
		currTps->tid = pthread_self();
		currTps->currPage->adr = mMap;
		currTps->currPage->refCount = 1;

		mprotect(tempPage->adr, TPS_SIZE, PROT_READ);
		memcpy(currTps->currPage->adr, tempPage->adr, TPS_SIZE);
		memcpy(currTps->currPage->adr + offset, buffer, length);
		mprotect(currTps->currPage->adr, TPS_SIZE, PROT_NONE);

		mprotect(tempPage->adr, TPS_SIZE, PROT_NONE);		
	}

	exit_critical_section();
	
	return 0;
	/* TODO: Phase 2 */
}

int tps_clone(pthread_t tid)
{
	tps_p currTps = NULL;
	tps_p toClone = malloc(sizeof(tps_p));
	toClone->currPage = malloc(sizeof(page_p));
	//void* mMap;
	
	enter_critical_section();

	//mMap = mmap(NULL, TPS_SIZE, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANON, -1, 0);
	
	toClone->tid = pthread_self();
	//toClone->currPage->adr = mMap;

	queue_iterate(tpsQueue, findTid, (void*)tid, (void**)&currTps);

	if (currTps == NULL)
	{
		exit_critical_section();
		return -1;
	}

	toClone->currPage = currTps->currPage;
	toClone->currPage->refCount++;

	/*
	mprotect(currTps->currPage->adr, TPS_SIZE, PROT_READ);
	memcpy(toClone->currPage->adr, currTps->currPage->adr, TPS_SIZE);
	mprotect(toClone->currPage->adr, TPS_SIZE, PROT_NONE);
	mprotect(currTps->currPage->adr, TPS_SIZE, PROT_NONE);	
	*/

	queue_enqueue(tpsQueue, (void*)toClone);

	exit_critical_section();
	
	return 0;
	/* TODO: Phase 2 */
}

