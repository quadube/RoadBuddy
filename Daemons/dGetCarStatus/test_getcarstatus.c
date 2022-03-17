#include <semaphore.h>
#include "shared_memory.h"
#include <syslog.h>
#include <iostream>
#include <signal.h>

#define SEM_PRODUCER_FNAME "/give_car_sem"
#define SEM_CONSUMER_FNAME "/take_car_sem"
#define FILENAME "/etc/roadbuddy/daemons/dGetCarStatus"
#define SH_BLOCK_SIZE 128

struct carstatus_t{
    uint8_t carSpeed;
    uint8_t trottlePos;
    uint16_t engineSpeed;
    uint16_t distance;
    uint16_t time;
};

using namespace std;

int main(){
	//attach to existing semaphore
   	sem_t * sem_prod = sem_open(SEM_PRODUCER_FNAME, 0);
   	if(!sem_prod)
   	    syslog(LOG_ERR, "%d\n");
	
	sem_t * sem_cons = sem_open(SEM_PRODUCER_FNAME, 0);
   	if(!sem_cons)
   	    syslog(LOG_ERR, "%d\n");
    
  	//grab the shared memory block
  	carstatus_t* block = (carstatus_t*) attach_memory_block(FILENAME, SH_BLOCK_SIZE);
  	if(!block)
  	    syslog(LOG_ERR, "%m\n");
    	
	//wait for pid
	sem_wait(sem_prod);
	cout << "Car speed: "<< block->carSpeed << endl
		 << "Trottle Position: " << block->trottlePos << endl
		 << "Engine Speed: " << block->engineSpeed << endl
		 << "Distance: " << block->distance << endl
		 << "Time: " << block->time << endl;

	sem_post(sem_cons);
	
	sem_close(sem_prod);
	sem_close(sem_cons);
    //let go of shared memory block
   	detach_memory_block((char*)block); 
    
    return 0;
}
