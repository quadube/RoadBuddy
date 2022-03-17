#include "cusertimer.h"
#include <time.h>
#include <cstring>

CUserTimer::CUserTimer(){}

CUserTimer::CUserTimer(int sig)
{
    this->sig = sig;
    memset(&my_signal, 0, sizeof(struct sigevent));        
    my_signal.sigev_notify = SIGEV_SIGNAL;    
    my_signal.sigev_signo = sig;    
}

CUserTimer::~CUserTimer(){};

void CUserTimer::startTimer(unsigned long int time)
{
    struct itimerspec value; //timer struct
  
    value.it_value.tv_nsec = time; //tempo antes de começar a contar
    value.it_interval.tv_nsec = value.it_value.tv_nsec; //intervalo entre sinais (tempo que vai efetivamente contar)

    timer_create(CLOCK_REALTIME, &my_signal, &gTimerid); //cria o timer, CLOCK_REALTIME - timer normal, NULL - (SIGALRM - flag que recebe o signal do timer), timer id 
    timer_settime(gTimerid, 0, &value, NULL); //da set do tempo no timer com o id
}

void CUserTimer::stopTimer()
{
    struct itimerspec value; // struct do timer
  
    value.it_value.tv_sec = 0; 
    value.it_interval.tv_sec = value.it_value.tv_sec; 

    timer_settime(gTimerid, 0, &value, NULL); //da set do tempo no timer com 0
}

void CUserTimer::setCallback(void (*funPtr)(uint))
{
    signal(sig, funPtr);
}
