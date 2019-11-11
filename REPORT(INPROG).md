# Phase 1
## Overview
### Struct
Our semaphore struct has a queue of blocked threads and a count. This queue is incremented every time a thread is blocked, and the count refers to the number of resources available in the semaphore.
### Functions
For all of these functions, the enter() and exit() critical section functions would be called before and after the critical sections. These sections are the ones that need to be performed atomically to avoid race conditions and ensure the accuracy of variable assignment and changes.

    sem_create()/sem_destroy()

Allocate/Deallocate a new semaphore of count 'count'.

    sem_up()/sem_down()
Block/Unblock any threads that need a resource to run. Also add/remove threads from the queue of blocked threads when they are allowed to run or told to wait.

    sem_getvalue()
If there are remaining resources, return that value. If not, return the number of threads currently blocked. This number is made negative to differentiate whether the number is resources available or threads blocked.
## Testing
To test our semaphore, we first ran simple threads that switched back and forth between which one has control of a critical section. After our semaphore worked for these, we moved on to the three given testing scripts.
# Phase 2
## Overview
### Structs
For phases 2.1 and 2.2 we just had a `tps` struct that included a `tid` and `adr` in memory. For 2.3 we have a `page` struct which includes an `adr` which is the address of each TPS in memory and a `refCount` which holds the number of threads pointing to that specific TPS. Our `tps` struct still has the `tid` of the thread using it, and a `currPage` corresponding to the page in memory it is pointing to.
### Globals
`queue_t tpsQueue`: A global queue of TPS structs used to keep track of different TPS's for different threads.
### Helper Functions
`findSegV()` and `findTid()`
These functions are both called by different functions using queue_iterate to find either the `adr` of a page in a TPS or the `tid` of a certain thread in the global queue.

`segv_handler()` 
A signal handler to check if a segmentation fault is due to attempting to access a TPS that the accessing thread does not have permission to modify.
### Functions
`tps_init()` simply creates the queue of TPS structs and initializes the signal handler.

`tps_create()` and `tps_destroy()` allocate or free the resources of a certain TPS, and check for errors in these creations or destructions.

`tps_read()` and `tps_write()` check if the input is valid (within the bounds) and then create new TPS structs, find the TPS in the tpsQueue if it exists, and then either read or write to/from them or the buffer. To do this, they temporarily disable the memory protections on the specific TPS and then return their protections after the reading or writing operation is complete. In phase 2.3, write now checks if multiple TPS structs are pointing to the same page, and if they are, it first allocates a new page and then writes the buffer to this new page, while decrementing the `refCount` on the original page. 

`tps_clone()` originally simply allocated a new TPS that was identical to that of the one to be copied, and then enqueued that in the global TPS queue. For phase 2.3, clone no longer allocates a new TPS, as that will only be done in write. Now, clone just makes a new TPS which points to the same page as the one to be copied, incremements the `refCount` of that page, and then enqueues this new TPS.

## Testing
2.1
Initial testing was done with the given testing scripts. Because this testing script includes testing of all of the functions that we needed to implement, we just kept running this script and getting errors at different points until. With each error, we would change the specific function where the crash occurred, until eventually we got the full execution without crashing. 
2.2
Testing here was done using the blueprint given in discussion. Our testing was implemented in the 'test/tps22test.c' file. We overloaded the mmap() function so we could keep track of the address in the TPS that was returned by this function. Then we would attempt to access this address from other functions or other threads that are not calling read() or write() on this specific TPS. Using the signal handler, we could determine whether these accesses were denied by memory protection. We also tested the creation, reading, and writing of the TPS by checking the -1 return values for all of the tps functions. These failed returns include ensuring the user can not create another TPS if the current thread has a TPS or if the read/write operations are out of bounds when they are called. Other testing required  creating and writing to a TPS in a thread. After that, try reading from that TPS in another thread. We ensured that this should not be successful. 
2.3
To do.
