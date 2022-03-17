#include "cfmu.h"
#include <opencv2/imgproc.hpp>

#define INCREMENT(index) (index = (index+1) & ~(1<<7))

using namespace cv;
using namespace std;

pthread_mutex_t mutex_carstatus = PTHREAD_MUTEX_INITIALIZER;

CFmu::CFmu()
{
    carstatus.carSpeed = 0;
    carstatus.engineSpeed = 0;
    carstatus.distance = 0;
}


CFmu::~CFmu(){}

void CFmu::setFrameTime(CCam *cam)
{
    currTime = getTime();

    time = to_string(currTime->tm_mday) + '-' + to_string(currTime->tm_mon+1) 
            + '-' + to_string(currTime->tm_year+1900) + ' ' + to_string(currTime->tm_hour) 
            + 'h' + to_string(currTime->tm_min) + "m" + to_string(currTime->tm_sec) + "s";
    putText(cam->lastFrame, time, Point(50,50), FONT_HERSHEY_DUPLEX, 1, Scalar(255,255,255),1);	
}

bool CFmu::getCarStatus(carstatus_t* sharedBlock)
{
    carstatus.carSpeed = sharedBlock->carSpeed;
    carstatus.engineSpeed = sharedBlock->engineSpeed;
    carstatus.distance = sharedBlock->distance;

    //returns true if the car engine's speed is greater than zero, meaning the car's engine is on
    if(carstatus.engineSpeed > 0)
        return true;
    else return false;
}
void CFmu::setCarStatus(CCam *cam)
{
    string carSpeedText, distanceTraveledText;

    pthread_mutex_lock(&mutex_carstatus);
    carSpeedText = "Car speed: " + to_string(carstatus.carSpeed) + " km/h";
    distanceTraveledText = "Distance: " + to_string(carstatus.distance) + " km";
    pthread_mutex_unlock(&mutex_carstatus);

    putText(cam->lastFrame, carSpeedText, Point(50,100), FONT_HERSHEY_DUPLEX, 1, Scalar(255,255,255),1);
    putText(cam->lastFrame, distanceTraveledText, Point(50,150), FONT_HERSHEY_DUPLEX, 1, Scalar(255,255,255),1);
}

void CFmu::setLaneLines(CCam *cam, CFpu *fpu)
{
    pthread_mutex_lock(&mutex_procframe);
    if(!fpu->processedFrame.empty())
    {
        cam->lastFrame += fpu->processedFrame;
        fpu->processedFrame.release();
    }
    pthread_mutex_unlock(&mutex_procframe);
    
    pthread_mutex_lock(&mutex_framebuff);
    frameBuff[writeIndex] = cam->lastFrame;
    pthread_mutex_unlock(&mutex_framebuff);
    INCREMENT(writeIndex);
}
