#ifndef CVIDEOCAM_H
#define CVIDEOCAM_H

#include "cvideocam.h"
#include <opencv2/videoio.hpp>
#include <iostream>

using namespace cv;
using namespace std;

extern Mat frameBuff[128];
extern uint8_t readIndex, writeIndex;
extern pthread_mutex_t mutex_framebuff, mutex_procframe;

struct tm * getTime();

class CCam
{
    Mat lastFrame;
    VideoCapture cam;
    Size frameSize;
    Matx33f cameraMatrix;
    Vec<float, 4> distortionCoefficients;

public:
    CCam();
    ~CCam();

    bool open();
    void captureFrame();
    int getParms(char p);
    void calibrate();
    void closeCam();
    friend class CFmu;
    friend class CFpu;
};

class CVideo
{
    VideoWriter video;

public:
    CVideo();
    ~CVideo();

    bool open(CCam *cam);
    void writeFrame();
    void closeVideo();
};

#endif
