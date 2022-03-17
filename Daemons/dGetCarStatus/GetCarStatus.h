#ifndef EVENT_MONITOR_H
#define EVENT_MONITOR_H


#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string>
#include <iostream>
#include <syslog.h>
#include <fcntl.h>   // open
#include <unistd.h>  // read, write, close
#include <cstdio>    // BUFSIZ
#include "shared_memory.h"
#include <semaphore.h>
#include <string>

//semaphores for shared memory
#define SEM_CS_PRODUCER_FNAME "/sem_prodcs"
#define SEM_CS_CONSUMER_FNAME "/sem_conscs"
#define FILENAME "/etc/roadbuddy/daemons/dGetCarStatus"
#define SH_BLOCK_SIZE 64
#define O_RDWR 02

using namespace std;

struct carstatus_t{
    uint16_t carSpeed;
    uint16_t engineSpeed;
    uint16_t distance;
};

class CarStatus {
    public:
        CarStatus();
        ~CarStatus();
        void daemonize();
        void sendStatus();
        bool getStatus();
        bool obdConfig();
    private:
        pid_t myPid, sid;
        sem_t * sem_prod, *sem_cons;
        carstatus_t carStatus;
        uint8_t pid[3], status[4];
};


#endif