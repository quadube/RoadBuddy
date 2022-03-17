#ifndef CFMU_H
#define CFMU_H

#include "cfpu.h"
#include <string>

extern pthread_mutex_t mutex_carstatus;

struct carstatus_t
{
    uint16_t carSpeed;
    uint16_t engineSpeed;
    uint16_t distance;
};


class CFmu
{
    struct tm *currTime;
    string time;
    struct carstatus_t carstatus;

public:
    CFmu();
    ~CFmu();
    void setFrameTime(CCam *cam);
    bool getCarStatus(carstatus_t* sharedBlock);
    void setCarStatus(CCam *cam);
    void setLaneLines(CCam *cam, CFpu *fpu);
    friend class CCam;
};

#endif
