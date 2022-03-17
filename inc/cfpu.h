#ifndef CFPU_H
#define CFPU_H

#include "cvideocam.h"
#include <string>

class CFpu
{
    Matx33f cameraMatrix;
    Vec<float, 4> invertedDistortionCoefficients;
    Mat mapX, mapY;
    Mat frameBeingProcessed, processedFrame;
    Mat undistortedFrame;
    Mat perspectiveMatrix, invertedPerspectiveMatrix, birdsEyeViewFrame;
    Mat filteredFrame;
    Mat outputFrame;
    Point leftLanePoint, rightLanePoint;

public:
    CFpu(CCam *cam);
    ~CFpu();
    void undistortFrame();
    void birdsEyeView();
    void sobelFilter();
    vector<Point2f> slidingWindow(Rect window);
    void drawLines();
    void dangerDetection();
    friend class CFmu;
};

#endif
