#include "GetCarStatus.h"

CarStatus::CarStatus(){
    carStatus.carSpeed = 0;
    carStatus.engineSpeed = 0;
    carStatus.distance = 0;

     //remove any leftover semaphore (precaution)
    sem_unlink(SEM_CS_PRODUCER_FNAME);
    sem_unlink(SEM_CS_CONSUMER_FNAME);
    
    //create new producer semaphore
    sem_prod = sem_open(SEM_CS_PRODUCER_FNAME, O_CREAT, 0660, 0);
    if(sem_prod == SEM_FAILED)
        syslog(LOG_ERR, "Producer semaphore create: %m\n");

    //create new consumer semaphore
    sem_cons = sem_open(SEM_CS_CONSUMER_FNAME, O_CREAT, 0660, 1);
    if(sem_cons == SEM_FAILED)
        syslog(LOG_ERR, "Consumer semaphore create: %m\n");
}

CarStatus::~CarStatus(){
        //let go of shared memory block and semaphores
    sem_close(sem_prod);
    sem_close(sem_cons);
}

void CarStatus::daemonize(){
    openlog("EventMonitor", LOG_PID, LOG_DAEMON);
    /* Fork off the parent process */
    myPid = fork();
    // on error exit
    if (myPid < 0){ 
		syslog(LOG_ERR, "%s\n", "fork");
		exit(EXIT_FAILURE);
	}
    /* Success: Let the parent terminate */
    if (myPid > 0){
        exit(EXIT_SUCCESS);
    }

    /* On success: The child process becomes session leader */
    sid = setsid(); // create a new session
	// on error exit
    if (sid < 0){ 
		syslog(LOG_ERR, "%s\n", "setsid");
		exit(EXIT_FAILURE);
	}

    // make '/' the root directory
	if (chdir("/") < 0) { // on error exit
		syslog(LOG_ERR, "%s\n", "chdir");
		exit(EXIT_FAILURE);
	}
    
	umask(0);

    close(STDIN_FILENO);  // close standard input file descriptor
	close(STDOUT_FILENO); // close standard output file descriptor
	close(STDERR_FILENO); // close standard error file descriptor
}


void CarStatus::sendStatus(){

   
    
    //grab the shared memory block
    carstatus_t* block = (carstatus_t*) attach_memory_block(FILENAME, SH_BLOCK_SIZE);
    if(!block)
        syslog(LOG_ERR, "Attach block: %m\n");
    
    sem_wait(sem_cons);
    //put status on shared memory
        block->carSpeed = carStatus.carSpeed;
        block->engineSpeed = carStatus.engineSpeed;
        block->distance = carStatus.distance;

    //change semaphore value to let know status are available
    sem_post(sem_prod);
    detach_memory_block((char *)block); 
}

bool CarStatus::obdConfig(){
    int fd = open("/dev/rfcomm0", O_RDWR);
    if(fd < 0){
		syslog(LOG_ERR, "Setup: %m\n");
        //return false;
	}

    const char buf[5] = "ATE0";
    //write(fd, buf, 4);
    syslog(LOG_INFO, "%s\n", buf);
    sleep(0.5);

    const char buf1[5] = "ATL0";
    //write(fd, buf1, 4);
    syslog(LOG_INFO, "%s\n", buf1);
    sleep(0.5);

    const char buf2[6] = "ATST0";
    //write(fd, buf2, 5);
    syslog(LOG_INFO, "%s\n", buf2);
    sleep(0.5);

    const char buf3[7] = "ATSP00";
    //write(fd, buf3, 6);
    syslog(LOG_INFO, "%s\n", buf3);
    sleep(0.5);

    close(fd);
    return true;
}

bool CarStatus::getStatus(){
    int fd = open("/dev/rfcomm0", O_RDWR);
    if(fd < 0){
		syslog(LOG_ERR, "Get Status: %m\n");
        return false;
	}
   
    pid[0] = '0';
    pid[1] = '1';

    //get car speed
    pid[2] = 'O';
    pid[3] = 'D';

    write(fd, pid, 4);
    read(fd, status, 4);
    syslog(LOG_INFO, "%s\n", status);

    carStatus.carSpeed = status[3];
    sleep(0.5);

    //get engine speed
    pid[2] = 'O';
    pid[3] = 'C';

    write(fd, pid, 4);
    read(fd, status, 4);
    syslog(LOG_INFO, "%s\n", status);

    carStatus.engineSpeed = status[4] + (status[3] << 8);
    sleep(0.5);

    //get distance
    pid[2] = '3';
    pid[3] = '1';

    write(fd, pid, 4);
    read(fd, status, 4);
    syslog(LOG_INFO, "%s\n", status);

    carStatus.distance = status[4] + (status[3] << 8);
    sleep(0.5);

    close(fd);
    return true;
}
