#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tps.h>
#include <sem.h>

//static char msg1[TPS_SIZE] = "Hello world!\n";
static char msg2[TPS_SIZE] = "hello world!\n";

//static sem_t sem1, sem2;

void *latest_mmap_addr; 
// global variable to make address returned by mmap accessible

void *__real_mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off);

void *__wrap_mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off)
{
	latest_mmap_addr = __real_mmap(addr, len, prot, flags, fildes, off);
	return latest_mmap_addr;
}

void test_create (void) {
	if (tps_create() == -1) {
		printf("Test TPS Creation Failure: Passed\n");
	}
}

void test_read_failure (char *buff) {
	if (tps_read(100, TPS_SIZE, buff) == -1) {
		printf("Test TPS Read Failure: Passed\n");
	}
}

void test_write_failure (char *buff) {
	if (tps_write(200, TPS_SIZE, buff) == -1) {
		printf("Test TPS Write Failure: Passed\n");
	}
}

void *thread1(void* arg) {
	char *buffer = malloc(TPS_SIZE);
	/* Create TPS */
	tps_create();
	test_create();
	test_write_failure(buffer);
	tps_write(0, TPS_SIZE, msg2);
	test_read_failure(buffer);

	memset(buffer, 0, TPS_SIZE);
	tps_read(0, TPS_SIZE, buffer);	

	return NULL;
}



void test_read(char *buff) {
	assert(memcmp(msg2, buff, TPS_SIZE));
	printf("thread2: read FAILED. Test Passed.\n");
}



void *thread2(void* arg) {

	char *buffer = malloc(TPS_SIZE);


	tps_read(0, TPS_SIZE, buffer);
	printf("%s\n", buffer);

	test_read(buffer);
	return NULL;
}

void *basic_thread_test(void* arg) { 
	/* Create TPS */
	tps_create();

	/* Get TPS page address as allocated via mmap() */
	char *tps_addr = latest_mmap_addr;
	
	/* Cause an intentional TPS protection error */
	tps_addr[0] = '\0';
	return NULL;
}

int main(int argc, char **argv)
{
	pthread_t tid;

	/* Init TPS API */
	tps_init(1);

	// pthread_create(&tid, NULL, basic_thread_test, NULL);

	/* Create thread 1 and wait */
	pthread_create(&tid, NULL, thread1, NULL);
	pthread_create(&tid, NULL, thread2, NULL);
	pthread_join(tid, NULL);

	

	return 0;
}
