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

/* 
 * Page struct:
 * adr:		address in memory where TPS exists
 * refCount:	number of TPS structs pointing to page
*/
typedef struct page {
	char* adr;
	int refCount;
} *page_p;

/*
 * Tps struct:
 * tid:		TID of thread using the TPS
 * currPage:	page the TPS is pointing to
*/
typedef struct tps {
	pthread_t tid;
	page_p currPage;
} *tps_p;

/* Global queue of TPS structs */
queue_t tpsQueue;

/* Helper function to find address of seg faults */
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

/* Helper function to find TID of certain thread */
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

/* Error handler to determine whether seg fault is a TPS protection error */
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

/* Initializes the error handler and creates global queue */
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
}

/* Creates a TPS for current running thread */
int tps_create(void)
{
	queue_iterate(tpsQueue, findTid, (void*)tid, (void**)&currTps);
	
	if (currTps) {
		exit_critical_section();
		return -1;
	}

	tps_p newTps = malloc(sizeof(tps_p));
	newTps->currPage = malloc(sizeof(page_p));
	void* mMap;

	enter_critical_section();

	/* Maps a location in memory for new TPS */
	mMap = mmap(NULL, TPS_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);

	/* Initializes new TPS */
	newTps->tid = pthread_self();
	newTps->currPage->adr = mMap;
	newTps->currPage->refCount = 1;

	/* Puts new TPS in global queue */
	queue_enqueue(tpsQueue, (void*)newTps);

	exit_critical_section();

	return 0;
}

/* Destroys TPS of currently running thread */
int tps_destroy(void)
{
	pthread_t tid = pthread_self();
	tps_p currTps = NULL;

	/* Finds TPS of currently running thread */
	queue_iterate(tpsQueue, findTid, (void*)tid, (void**)&currTps);
	
	/* Checks if TID was found */
	if (currTps == NULL)
	{
		exit_critical_section();
		return -1;
	}
	
	/* Checks if TPS exists in global queue */
	if(queue_delete(tpsQueue, (void**)&currTps) == -1)
	{
		exit_critical_section();
		return -1;
	}

	/* Removes TPS */
	currTps->currPage->refCount--;
	free(currTps->currPage);
	free(currTps);

	exit_critical_section();

	return 0;
}

/* Reads to a buffer from TPS of current thread */
int tps_read(size_t offset, size_t length, char *buffer)
{
	/* Checks if input arguments are valid */
	if (offset + length > TPS_SIZE || buffer == NULL)
		return -1;

	tps_p currTps = NULL;
	pthread_t tid = pthread_self();

	enter_critical_section();

	/* Finds TPS of currently running thread */
	queue_iterate(tpsQueue, findTid, (void*)tid, (void**)&currTps);

	/* Checks if TID was found */
	if (currTps == NULL)
	{
		exit_critical_section();
		return -1;
	}

	/* Reads from TPS to buffer */
	mprotect(currTps->currPage->adr, TPS_SIZE, PROT_READ);
	memcpy(buffer, currTps->currPage->adr + offset, length);
	mprotect(currTps->currPage->adr, TPS_SIZE, PROT_NONE);

	exit_critical_section();
	
	return 0;
}

/* Writes to a buffer from TPS of current thread */
int tps_write(size_t offset, size_t length, char *buffer)
{
	/* Checks if input arguments are valid */
	if (offset + length > TPS_SIZE || buffer == NULL)
		return -1;
	
	tps_p currTps = NULL;
	tps_p toClone = malloc(sizeof(tps_p));
	toClone->currPage = malloc(sizeof(page_p));
	pthread_t tid = pthread_self();

	enter_critical_section();

	/* Finds TPS of currently running thread */
	queue_iterate(tpsQueue, findTid, (void*)tid, (void**)&currTps);

	/* Checks if TID was found */
	if (currTps == NULL)
	{
		exit_critical_section();
		return -1;
	}

	/* Writes to buffer from TPS if only one TPS is pointing to the page */
	if (currTps->currPage->refCount == 1)
	{
		mprotect(currTps->currPage->adr, TPS_SIZE, PROT_WRITE);
		memcpy(currTps->currPage->adr + offset, buffer, length);
		mprotect(currTps->currPage->adr, TPS_SIZE, PROT_NONE);
	}

	/* Allocates new page to be written to if multiple TPS pointing */
	else
	{
		void* mMap;

		page_p tempPage = currTps->currPage;
		tempPage->refCount--;

		mMap = mmap(NULL, TPS_SIZE, PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	
		/* Allocation of new page */
		currTps->currPage = malloc(sizeof(page_p));
		currTps->tid = pthread_self();
		currTps->currPage->adr = mMap;
		currTps->currPage->refCount = 1;

		/* Writing to new page */
		mprotect(tempPage->adr, TPS_SIZE, PROT_READ);
		memcpy(currTps->currPage->adr, tempPage->adr, TPS_SIZE);
		memcpy(currTps->currPage->adr + offset, buffer, length);

		/* Gives pages correct protections */
		mprotect(currTps->currPage->adr, TPS_SIZE, PROT_NONE);
		mprotect(tempPage->adr, TPS_SIZE, PROT_NONE);		
	}

	exit_critical_section();
	
	return 0;
}

/* Makes a new TPS point to the same page as an existing one */
int tps_clone(pthread_t tid)
{
	tps_p currTps = NULL;
	tps_p toClone = malloc(sizeof(tps_p));
	toClone->currPage = malloc(sizeof(page_p));
	
	enter_critical_section();
	
	toClone->tid = pthread_self();

	/* Finds TPS of currently running thread */
	queue_iterate(tpsQueue, findTid, (void*)tid, (void**)&currTps);

	/* Checks if TID was found */
	if (currTps == NULL)
	{
		exit_critical_section();
		return -1;
	}

	/* Points new TPS to same page of found TPS */
	toClone->currPage = currTps->currPage;
	toClone->currPage->refCount++;

	/* Phase 2.1 Code */
	/*
	mprotect(currTps->currPage->adr, TPS_SIZE, PROT_READ);
	memcpy(toClone->currPage->adr, currTps->currPage->adr, TPS_SIZE);
	mprotect(toClone->currPage->adr, TPS_SIZE, PROT_NONE);
	mprotect(currTps->currPage->adr, TPS_SIZE, PROT_NONE);	
	*/

	queue_enqueue(tpsQueue, (void*)toClone);

	exit_critical_section();
	
	return 0;
}

