#include <stddef.h>
#include <stdlib.h>

#include "queue.h"
#include "sem.h"
#include "thread.h"

/* 
 * Semaphore struct
 * blockedQueue: queue of threads waiting for resource
 * count: 	 number of available resources
*/
struct semaphore {
	queue_t blockedQueue;
	int count;
};

/*
 * During any critical sections (where operations need to
 * complete atomically) the enter() and exit() critical 
 * section commands are called. This ensures that these
 * sections will complete without interruption
*/


/* Initializes and allocates a semaphore of count 'count'*/
sem_t sem_create(size_t count)
{
	enter_critical_section();

	sem_t newSem = malloc(sizeof(sem_t));
	newSem->count = count;
	newSem->blockedQueue = queue_create();

	exit_critical_section();	

	return newSem;
}

/* Destroys semaphore if possible */
int sem_destroy(sem_t sem)
{
	/* check if sem exists */
	if (sem == NULL)
		return -1;
	/* Check if blocked queue is not empty */
	else if (queue_destroy(sem->blockedQueue) == -1)
		return -1;	

	free(sem);

	return 0;
}

/* Gives resource to calling thread if possible, if not, blocks it */
int sem_down(sem_t sem)
{
	pthread_t tid;

	/* Check if sem exists */
	if (sem == NULL)
		return -1;

	enter_critical_section();
	
	/* Checks if resource available */
	if (sem->count == 0)
	{
		tid = pthread_self();
		queue_enqueue(sem->blockedQueue, (void*)tid);
		thread_block();
	}

	/* Gives resource to waiting thread */
	sem->count--;

	exit_critical_section();
	return 0;
}

/* Gives up resource to next waiting thread if any */
int sem_up(sem_t sem)
{
	pthread_t tid;

	/* Check if sem exists */
	if (sem == NULL)
		return -1;

	enter_critical_section();

	/* Checks if there is a thread waiting for resource */
	if (queue_length(sem->blockedQueue) != 0)
	{
		queue_dequeue(sem->blockedQueue, (void**)&tid);
		thread_unblock(tid);
	}

	/* Releases resource from current thread */
	sem->count++;

	exit_critical_section();
	return 0;
}

/* Returns resource count of a semaphore */
int sem_getvalue(sem_t sem, int *sval)
{
	int numInQueue = 0;

	if (sem == NULL || sval == NULL)
		return -1;

	enter_critical_section();

	if (sem->count > 0)
		*sval = sem->count;

	/* If no available resources, sets sval to the number of blocked threads */
	else if (sem->count == 0)
	{
		numInQueue = queue_length(sem->blockedQueue);
		*sval = -numInQueue;
	}

	exit_critical_section();

	return 0;
}

