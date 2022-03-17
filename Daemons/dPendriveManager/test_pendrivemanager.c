#include <semaphore.h>
#include "shared_memory.h"
#include <syslog.h>
#include <iostream>
#include <signal.h>

#define SEM_PRODUCER_FNAME "/daemon_sem"
#define FILENAME "/etc/roadbuddy/daemons/dPendriveManager"
#define SH_BLOCK_SIZE 64

using namespace std;

int main(){
	//attach to existing semaphore
   	sem_t * sem_prod = sem_open(SEM_PRODUCER_FNAME, 0);
   	if(!sem_prod)
   	    syslog(LOG_ERR, "%d\n");
    
  	//grab the shared memory block
  	char* block = attach_memory_block(FILENAME, SH_BLOCK_SIZE);
  	if(!block)
  	    syslog(LOG_ERR, "%m\n");
    	
	//wait for pid
	sem_wait(sem_prod);
	//(...) get pid

    kill(atoi(block), SIGRTMIN+3);
	int pid = atoi(block);
	cout << pid <<  endl;

	
    //let go of shared memory block
   	detach_memory_block(block); 
    
    return 0;
}
