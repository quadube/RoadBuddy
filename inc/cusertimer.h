#ifndef USERTIMER_H
#define USERTIMER_H

#include <signal.h>

using namespace std;

class CUserTimer
{
    timer_t gTimerid;
    struct sigevent my_signal;
    struct sigaction sa;
    int sig;

public:
    CUserTimer();
    CUserTimer(int sig);
    ~CUserTimer();
    void startTimer(unsigned long int time);
    void stopTimer();
};

#endif
