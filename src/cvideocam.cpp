#include "cvideocam.h"
#include <opencv2/highgui.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>
#include <fstream>


#define INCREMENT(index) (index = (index+1) & ~(1<<7))

pthread_mutex_t mutex_framebuff = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_procframe = PTHREAD_MUTEX_INITIALIZER;
Mat frameBuff[128];
uint8_t writeIndex = 0;
uint8_t readIndex = 0;

struct tm * getTime()
{
    time_t t = time(0);
    struct tm* now = localtime(&t);
    return now;
}

CCam::CCam()
{
    frameSize = {1024, 576};
    ifstream myfile("CameraCalibrationValues.txt");
    if(myfile.is_open())
    {
        double fx, cx, fy, cy, k1, k2, p1, p2;
        myfile >> fx >> cx >> fy >> cy >> k1 >> k2 >> p1 >> p2;
        cameraMatrix << fx, 0, cx, 0, fy, cy, 0, 0, 1;
        distortionCoefficients << k1, k2, p1, p2;
    }
    else
    {
        cout << "Error: Can't open CameraCalibrationValues.txt for reading" << endl;
        cameraMatrix = (Matx33f::eye());
        distortionCoefficients = (0,0,0,0);
    }
}

CCam::~CCam()
{
    cam.release();
}

bool CCam::open()
{
    cam.open(0);
    if (!cam.isOpened())
        return false;
    cam.set(CV_CAP_PROP_FRAME_WIDTH, 1024);
	cam.set(CV_CAP_PROP_FRAME_HEIGHT, 576);
    cam.set(CV_CAP_PROP_FPS, 15);
    return true;
}

void CCam::captureFrame()
{
    cam >> lastFrame; //same as cam.read
}

int CCam::getParms(char p)
{
    if (p == 'W')
        return cam.get(CV_CAP_PROP_FRAME_WIDTH);
    else if (p == 'H')
        return cam.get(CV_CAP_PROP_FRAME_HEIGHT);
    else if (p == 'F')
        return cam.get(CAP_PROP_FPS);
    else return 0;
}

#ifdef CALIBRATION
void CCam::calibrate()
{
    vector<cv::String> fileNames;
    glob("/mnt/usb/Image*.png", fileNames, false);
    cout << "Number of images detected:" << fileNames.size() << endl;
    Size patternSize(6, 9); //chessboard size
    vector<vector<Point2f>> q(fileNames.size()); //image points vector
    vector<vector<Point3f>> Q; //object points vector
    vector<Point3f> objp;

    int checkerBoard[2] = {6, 9};
    int fieldSize = 26; //size of a square in the printed chessboard = 26mm
    //create known board position
    for(int i=0; i<checkerBoard[1]; i++){
        for(int j=0; j<checkerBoard[0]; j++){
            objp.push_back(Point3f(j*fieldSize, i*fieldSize, 0));
        }
    }

    size_t i = 0;
    cout << "Detecting corners on images..." << endl;
    for(auto const &f : fileNames){
        Mat img = imread(fileNames[i]);
        Mat gray;

        cvtColor(img, gray, COLOR_RGB2GRAY);

        bool patternFound = findChessboardCorners(gray, patternSize, q[i], CALIB_CB_ADAPTIVE_THRESH | CALIB_CB_NORMALIZE_IMAGE);

        if(patternFound){
            cornerSubPix(gray, q[i], Size(11,11), Size(-1,-1), TermCriteria(TermCriteria::EPS + TermCriteria::MAX_ITER, 30, 0.1));
            Q.push_back(objp);
        }

        drawChessboardCorners(img, patternSize, q[i], patternFound);
        imwrite("/mnt/usb/Points" + to_string(i) + ".png", img);
        i++;
    }

    vector<Mat> rvecs, tvecs;
    vector<double> stdIntrinsics, stdExtrinsics, perViewErrors;
    int flags = fisheye::CALIB_RECOMPUTE_EXTRINSIC + fisheye::CALIB_CHECK_COND + fisheye::CALIB_FIX_SKEW;
    cout << "Calibrating..." << endl;
    float error = fisheye::calibrate(Q, q, frameSize, cameraMatrix, distortionCoefficients, rvecs, tvecs, flags);
    cout << "Reprojection error = " << error << endl; 
    
    ofstream calVal;
    calVal.open("CameraCalibrationValues.txt");
    if(calVal.is_open())
        calVal << "K =\n" << cameraMatrix << "\nD=\n" << distortionCoefficients << endl;
    else 
        cout << "Error: Can't open CameraCalibrationValues.txt for writing" << endl;
    calVal.close();
}
#endif

void CCam::closeCam()
{
    cam.release();
}

CVideo::CVideo(){}

CVideo::~CVideo()
{
    video.release();
}

bool CVideo::open(CCam *cam)
{
    try
    {
        struct tm *currTime = getTime();
        const string videoPath = "/mnt/usb/" + to_string(currTime->tm_mday) + '-' + to_string(currTime->tm_mon+1) 
                             + '-' + to_string(currTime->tm_year+1900) + ' ' + to_string(currTime->tm_hour) 
                             + 'h' + to_string(currTime->tm_min) + "m.avi";
        int format = CV_FOURCC('H','2','6','4');
        Size S = Size(static_cast<int>(cam->getParms('W')), static_cast<int>(cam->getParms('H')));
        video.open(videoPath, format, cam->getParms('F'), S, true);
    }
    catch(cv::Exception& e)
    {
        const char* err_msg = e.what();
        std::cout << err_msg << std::endl;
    }
    if (!video.isOpened())
        return false;
    return true;
}

void CVideo::writeFrame()
{
    pthread_mutex_lock(&mutex_framebuff);
    //same as video.write
    video << frameBuff[readIndex];
    INCREMENT(readIndex);
    pthread_mutex_unlock(&mutex_framebuff);
}

void CVideo::closeVideo()
{
    video.release();
}
