#include "cfpu.h"
#include <unistd.h>
#include <signal.h>
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

#define SIG_WARN_SYS (SIGRTMIN+3)

using namespace cv;
using namespace std;


CFpu::CFpu(CCam *cam){
    Point2f regionOfInterestVertices[4], outputVertices[4];
    cameraMatrix = cam->cameraMatrix;
    //vertices of the region of interest
    regionOfInterestVertices[0] = Point(343, 230);
    regionOfInterestVertices[1] = Point(568, 230);
    regionOfInterestVertices[2] = Point(740, 360);
    regionOfInterestVertices[3] = Point(50, 360);
    //vertices of a 1024x576 frame
    outputVertices[0] = Point(0, 0);
    outputVertices[1] = Point(1024, 0);
    outputVertices[2] = Point(1024, 576);
    outputVertices[3] = Point(0, 576);
    perspectiveMatrix = getPerspectiveTransform(regionOfInterestVertices, outputVertices); //sets the region of interest in a full frame 
    invert(perspectiveMatrix, invertedPerspectiveMatrix);
    invertedDistortionCoefficients[0] = -cam->distortionCoefficients[0];
    invertedDistortionCoefficients[1] = -cam->distortionCoefficients[1];
    invertedDistortionCoefficients[2] = -cam->distortionCoefficients[2];
    invertedDistortionCoefficients[3] = -cam->distortionCoefficients[3];
    fisheye::initUndistortRectifyMap(cameraMatrix, cam->distortionCoefficients, Matx33f::eye(), cameraMatrix, cam->frameSize, CV_32FC1, mapX, mapY);
}

CFpu::~CFpu(){}

void CFpu::undistortFrame()
{
    processedFrame.release();
    int8_t index;
    if(frameBeingProcessed.empty())
        sleep(1);
    index = writeIndex-1;
    if(index < 0)
        index = 127;
    pthread_mutex_lock(&mutex_framebuff);
    frameBeingProcessed = frameBuff[index];
    pthread_mutex_unlock(&mutex_framebuff);l;
    remap(frameBeingProcessed, undistortedFrame, mapX, mapY, INTER_LINEAR);

    //used for testing
    //imwrite("/mnt/usb/undistortedframe.png", undistortedFrame); 
}

void CFpu::birdsEyeView()
{
    warpPerspective(undistortedFrame, birdsEyeViewFrame, perspectiveMatrix, birdsEyeViewFrame.size(), INTER_LINEAR, BORDER_CONSTANT);

    //used for testing
    //imwrite("/mnt/usb/birdseyeview.png", birdsEyeViewFrame);
}

void CFpu::sobelFilter()
{
    Mat blackAndWhiteFrame, mask;
    Mat maskWhite, maskYellow, kernel;
    const Size kernelSize = Size(9,9);
    const int thresholdVal = 185;
    cvtColor(birdsEyeViewFrame, blackAndWhiteFrame, COLOR_RGB2GRAY);
    //imwrite("/mnt/usb/blackandwhite.png", blackAndWhiteFrame);
    inRange(blackAndWhiteFrame, Scalar(10,0,90), Scalar(50,255,255), maskYellow);
    inRange(blackAndWhiteFrame, Scalar(0,190,0), Scalar(255,255,255), maskWhite);

    bitwise_or(maskYellow, maskWhite, mask);
    bitwise_and(blackAndWhiteFrame, mask, filteredFrame);

    GaussianBlur(filteredFrame, filteredFrame, kernelSize, 0);
    kernel = Mat::ones(15,15,CV_8U);
    dilate(filteredFrame, filteredFrame, kernel);
    erode(filteredFrame, filteredFrame, kernel);
    morphologyEx(filteredFrame, filteredFrame, MORPH_CLOSE, kernel);
    //imwrite("/mnt/usb/blurred.png", filteredFrame);

    threshold(filteredFrame, filteredFrame, thresholdVal, 255, THRESH_BINARY);

    //used for testing
    //imwrite("/mnt/usb/filtered.png", filteredFrame);
}

vector<Point2f> CFpu::slidingWindow(Rect window)
{
    vector<Point2f> points;
    const Size frameSize = filteredFrame.size();
    bool end = false;
    
    for(;;)
    {
        float currentX = window.x + window.width * 0.5f;
        Mat regionOfInterest = filteredFrame(window);
        
        vector<Point> locations;
        float avgX = 0.0f;
        
        findNonZero(regionOfInterest, locations);
        
        for(int i = 0; i < locations.size(); i++)
        {
            float xcoord = locations[i].x;
            avgX += window.x + xcoord;
        }
        avgX = locations.empty() ? currentX : avgX/locations.size();
        
        Point point (avgX, window.y + window.height * 0.5f);
        points.push_back(point);
        
        window.y -= window.height;

        if(window.y < 0)
        {
            window.y = 0;
            end = true;
        }

        window.x += (point.x - currentX);

        if(window.x < 0)
            window.x = 0;
        
        if(window.x + window.width >= frameSize.width)
            window.x = frameSize.width - window.width-1;
        
        if(end)
            break;
    }
    return points;
}

void CFpu::drawLines()
{
    Mat distortedBackLinesFrame;
    Mat linesFrame(576, 1024, CV_8UC3);
    vector<Point2f> leftLinePoints, rightLinePoints, leftLinePointsBEV, rightLinePointsBEV;
    leftLinePointsBEV = slidingWindow(Rect(0,504,300,72));
    rightLinePointsBEV = slidingWindow(Rect(724,504,300,72));

    cvtColor(filteredFrame, outputFrame, COLOR_GRAY2BGR);
    perspectiveTransform(leftLinePointsBEV, leftLinePoints, invertedPerspectiveMatrix);

    //draw left lane's line in an empty frame to further merge with the original frame
    for(int i = 0; i < leftLinePoints.size()-1; i++)
        line(linesFrame, leftLinePoints[i], leftLinePoints[i+1], Scalar(255,0,0), 3);
    
    //draw left lane's line in the birdseyeview+filtered frame
    for (int i = 0; i < leftLinePointsBEV.size()-1; i++)
        line(outputFrame, leftLinePointsBEV[i], leftLinePointsBEV[i+1], Scalar(255,0,0));
    
    leftLanePoint = leftLinePointsBEV[0]; //assign the left lane first point for further departure detection

    perspectiveTransform(rightLinePointsBEV, rightLinePoints, invertedPerspectiveMatrix);

    //draw right lane's line in the frame with the left line already drawed to further merge with the original frame
    for(int i = 0; i < rightLinePoints.size()-1; i++)
        line(linesFrame, rightLinePoints[i], rightLinePoints[i+1], Scalar(0,0,255), 3);

    //draw right lane's line in the birdseyeview+filtered frame
    for (int i = 0; i < rightLinePointsBEV.size()-1; i++)
        line(outputFrame, rightLinePointsBEV[i], rightLinePointsBEV[i+1], Scalar(0,0,255));

    rightLanePoint = rightLinePointsBEV[0]; //assign the right lane first point for further departure detection

    //undistort the frame containing the lane's lines with inverted distortion coefficients, to match the distortion of the original frame
    undistort(linesFrame, distortedBackLinesFrame, cameraMatrix, invertedDistortionCoefficients, noArray());

    //refresh the processedFrame, used by the cfmu to merge the lane's lines with the frames being captured by the camera
    pthread_mutex_lock(&mutex_procframe);
    processedFrame = distortedBackLinesFrame;
    pthread_mutex_unlock(&mutex_procframe);

    //used for testing
    /*frameBeingProcessed += distortedBackLinesFrame;
    imwrite("/mnt/usb/frameWithLines.png", frameBeingProcessed); 
    imwrite("/mnt/usb/undistortedFrameWithLines.png", undistortedFrame+linesFrame);*/
}

void CFpu::dangerDetection()
{
    Point centerLanePoint;
    double leftCenterDistance, rightCenterDistance, laneWidth, offset;
    centerLanePoint = {(leftLanePoint.x + rightLanePoint.x)/2, (leftLanePoint.y + rightLanePoint.y)/2};
    circle(outputFrame, centerLanePoint, 5, Scalar(0,255,0), CV_FILLED, 8, 0);
    //imwrite("/mnt/usb/birdsEyeViewOutput.png", outputFrame);
    leftCenterDistance = norm(centerLanePoint - leftLanePoint);
    rightCenterDistance = norm(rightLanePoint - centerLanePoint);
    laneWidth = norm(rightLanePoint - leftLanePoint);

    offset = ((leftCenterDistance-rightCenterDistance)/laneWidth)*100;

    if(abs(offset) >= 10.0f)
        kill(getpid(), SIG_WARN_SYS);
}
