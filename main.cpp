#include <semaphore.h>
#include <opencv2/opencv.hpp>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#include "usertypes.h"

using namespace std;

int counter = 0;
uint8_t controlWord = 0; // MSB |X|X|X|WARN|END|ENG_STAT|PROC_F|BUZZ| LSB 

sem_t sem_frameprod;
sem_t * sem_prodcarstatus, * sem_conscarstatus;
pthread_mutex_t mutex_sighandler = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_tprocframe = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_twarnsys = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_tprocframe = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_twarnsys = PTHREAD_COND_INITIALIZER;

void tSignalHandler(int sig, siginfo_t * info, void * context)
{
    pthread_mutex_lock(&mutex_sighandler);
    if(sig == SIG_WARN_SYS)
    {
        //set WARN flag
        controlWord |= 0x10;
    }
    else if(sig == SIG_PROBUT)
    {
        //toggle PROC_F flag
        controlWord ^= 0x02;
    }
    else if(sig == SIG_BUZZBUT)
    {
        //toggle BUZZ flag
        controlWord ^= 0x01;
    }
    else if(sig == SIG_ENG_ON)
    {
        //set ENG_STAT flag
        controlWord |= 0x04;
    }
    else if(sig == SIG_ENG_OFF)
    {
        //set END flag
        controlWord |= 0x08;
    }
    pthread_mutex_unlock(&mutex_sighandler);
}

int getPenDriveDaemonPID()
{
    int pid;
    //attach to existing semaphore
    sem_t * sem_prod = sem_open(SEM_PD_PRODUCER_FNAME, 0);
    if(!sem_prod)
           handle_error_en(-1, "Pen Drive producer sempahore");
    
    //grab the shared memory block
    char* block = attach_memory_block(FILENAME_PD, BLOCK_SIZE);
    if(!block)
          handle_error_en(-2, "Shared memory block");
        
    //wait for pid
    sem_wait(sem_prod);

    //(...) get pid
    pid = atoi(block); 
    kill(pid, SIG_MOUNT);
    //let go of shared memory block and semaphore
    sem_close(sem_prod);
    detach_memory_block(block); 
    return pid;
}

void* tWarningSystem(void *threadid)
{
    char BuzzOn = '1';
    char BuzzOff = '0';
    int fd = -1;
    for(;;)
    {
        pthread_mutex_lock(&mutex_twarnsys);
        pthread_cond_wait(&cond_twarnsys, &mutex_twarnsys);
        pthread_mutex_unlock(&mutex_twarnsys);

        fd = open("/dev/buzzer0", O_WRONLY);
        if(fd < 0)
            handle_error_en(-3, "Buzzer: invalid file descriptor");
        write(fd, &BuzzOn, 1); //turn ON
        sleep(0.5);
        write(fd, &BuzzOff, 1); //turn off
        close(fd);
    }
    return NULL;
}

void* tGetCarStatus(void *ref)
{
    CFmu *fmu = static_cast<CFmu *>(ref);
    carstatus_t* sharedBlock = (carstatus_t*) attach_memory_block(FILENAME_CS, BLOCK_SIZE);
    if(!sharedBlock)
       handle_error_en(-4, "Car Status block");
    while(!(controlWord & 0x08))
    {
        sem_wait(sem_prodcarstatus);
        pthread_mutex_lock(&mutex_carstatus);
        if(fmu->getCarStatus(sharedBlock))
            //controlWord |= 0x04;
            cout << "eae, eu sou para apagar" << endl;
        pthread_mutex_unlock(&mutex_carstatus);
	    sem_post(sem_conscarstatus);
    }
    detach_memory_block((char*)sharedBlock); 
    destroy_memory_block(FILENAME_CS);
    return NULL;
}

void* tAcqnMergeFrame(void *ptr)
{
    struct obj_t *obj = (struct obj_t *) ptr;
    CCam *cam = obj->cam;
    CFmu *fmu = obj->fmu;
    CFpu *fpu = obj->fpu;
    int my_policy;
    struct sched_param my_param;
    pthread_getschedparam(pthread_self(), &my_policy, &my_param);
    while(!(controlWord & 0x04));
    while(!(controlWord & 0x08))
    {
        cam->captureFrame();
        fmu->setFrameTime(cam);
        fmu->setCarStatus(cam);
        fmu->setLaneLines(cam, fpu);
        sem_post(&sem_frameprod);
        if(controlWord & 0x02){ //PROC_F flag
            pthread_mutex_lock(&mutex_tprocframe);
            pthread_cond_signal(&cond_tprocframe);
            pthread_mutex_unlock(&mutex_tprocframe);
        }
    }
    return NULL;
}

void* tProcessFrame(void *ref)
{
    CFpu *fpu = static_cast<CFpu *>(ref);
    while(!(controlWord & 0x08))
    {
        pthread_mutex_lock(&mutex_tprocframe);
        pthread_cond_wait(&cond_tprocframe, &mutex_tprocframe);
        pthread_mutex_unlock(&mutex_tprocframe);
        if((controlWord  & 0x02))
        {
            fpu->undistortFrame();
            fpu->birdsEyeView();
            fpu->sobelFilter();
            fpu->drawLines();
            fpu->dangerDetection();
        }
        //BUZZ and WARN flags
        if((controlWord & 0x01) && (controlWord & 0x10))
        { 
            pthread_mutex_lock(&mutex_twarnsys);
            pthread_cond_signal(&cond_twarnsys);
            pthread_mutex_unlock(&mutex_twarnsys);
        }
    }
    return NULL;
}

void* tStoreFrame(void *ref)
{
    CVideo *video = static_cast<CVideo *>(ref);
    while(!(controlWord & 0x08))
    {
        sem_wait(&sem_frameprod);
        video->writeFrame();
        counter++;
        cout << "counter: " << to_string(counter) << endl;
    }
    //kill(getpid(),SIG_ENG_OFF);
    if(!(controlWord & 0x02))
    {
        pthread_mutex_lock(&mutex_tprocframe);
        pthread_cond_signal(&cond_tprocframe);
        pthread_mutex_unlock(&mutex_tprocframe);
    }
    return NULL;
}

int main(int argc, char *argv[])
{   
    int status, penDriveDaemonPID;
    struct sigaction act;   

    controlWord |= 0x02;

    //controlWord |= 0x04;
    cout << "ControlWord: " << to_string(controlWord) << endl;

    status = open("/dev/button0", O_RDWR);
    if(status<0)
        handle_error_en(-5, "Buttons driver: ");

    sigemptyset(&act.sa_mask);            
    act.sa_flags = (SA_SIGINFO | SA_RESTART);     
    act.sa_sigaction = tSignalHandler;    
    sigaction(SIG_PROBUT, &act, NULL);        
    sigaction(SIG_BUZZBUT, &act, NULL);
    sigaction(SIG_ENG_OFF, &act, NULL);
    sigaction(SIG_ENG_ON, &act, NULL);
    sigaction(SIG_WARN_SYS, &act, NULL);

    penDriveDaemonPID = getPenDriveDaemonPID();

    sem_init(&sem_frameprod, 0, 0);
    sem_prodcarstatus = sem_open(SEM_CS_PRODUCER_FNAME, 0);
    sem_conscarstatus = sem_open(SEM_CS_CONSUMER_FNAME, 0);

   	if(!sem_prodcarstatus)
   	    handle_error_en(-6, "Sempahore sem_prodcarstatus");
   	if(!sem_conscarstatus)
   	    handle_error_en(-7, "Sempahore sem_conscarstatus");

    CCam cam;
    CCam* camRef = &cam;
    CVideo video;
    CVideo* vidRef = &video;
    CFmu fmu;
    CFmu* fmuRef = &fmu;
    CFpu fpu(camRef);
    CFpu* fpuRef = &fpu;

    struct obj_t obj;
    obj.cam = camRef;
    obj.vid = vidRef;
    obj.fmu = fmuRef;
    obj.fpu = fpuRef;

    if(!camRef->open())
       handle_error_en(-8, "Unable to open camera\n");
    if(!vidRef->open(camRef))
       handle_error_en(-9, "Could not open the output video for write\n");
    
    //camRef->calibrate();

    cpu_set_t cpuset;

    pthread_t WarningSystemID, StoreFrameID, ProcessFrameID, AcqnMergeFrameID, GetCarStatusID;
    pthread_attr_t threadAttr;
    struct sched_param threadParam;

    pthread_attr_init(&threadAttr);
    pthread_attr_getschedparam(&threadAttr, &threadParam);
    pthread_attr_setschedpolicy(&threadAttr, SCHED_OTHER);
    pthread_attr_setinheritsched (&threadAttr, PTHREAD_EXPLICIT_SCHED);

    threadParam.sched_priority = sched_get_priority_max(SCHED_FIFO)-1;
    pthread_attr_setschedparam(&threadAttr, &threadParam);
    status = pthread_create(&WarningSystemID, &threadAttr, tWarningSystem, NULL);
    if(status != 0)
        handle_error_en(status, "tWarningSystem");

    threadParam.sched_priority = sched_get_priority_max(SCHED_FIFO)-1;
    pthread_attr_setschedparam(&threadAttr, &threadParam);
    status = pthread_create(&StoreFrameID, &threadAttr, tStoreFrame, vidRef);
    if(status != 0)
        handle_error_en(status, "tStoreFrame");
    
    threadParam.sched_priority = sched_get_priority_max(SCHED_FIFO)-3;
    pthread_attr_setschedparam(&threadAttr, &threadParam);
    status = pthread_create(&ProcessFrameID, &threadAttr, tProcessFrame, fpuRef);
    if(status != 0)
        handle_error_en(status, "tProcessFrame");

    threadParam.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_attr_setschedparam(&threadAttr, &threadParam);
    status = pthread_create(&AcqnMergeFrameID, &threadAttr, tAcqnMergeFrame, &obj);
    if(status != 0)
        handle_error_en(status, "tAcqnMergeFrame");

    threadParam.sched_priority = sched_get_priority_max(SCHED_FIFO)-2;
    pthread_attr_setschedparam(&threadAttr, &threadParam);
    status = pthread_create(&GetCarStatusID, &threadAttr, tGetCarStatus, fmuRef);
    if(status != 0)
        handle_error_en(status, "tGetCarStatus");

    CPU_ZERO(&cpuset);
    CPU_SET(1,&cpuset);
    pthread_setaffinity_np(ProcessFrameID, sizeof(cpu_set_t), &cpuset);

    CPU_ZERO(&cpuset);
    CPU_SET(2,&cpuset);
    pthread_setaffinity_np(AcqnMergeFrameID, sizeof(cpu_set_t), &cpuset);

    CPU_ZERO(&cpuset);
    CPU_SET(3,&cpuset);
    pthread_setaffinity_np(StoreFrameID, sizeof(cpu_set_t), &cpuset);
    
    pthread_join(ProcessFrameID, nullptr);
    pthread_join(AcqnMergeFrameID, nullptr);
    pthread_join(StoreFrameID, nullptr);

    sem_destroy(&sem_frameprod);
    sem_close(sem_prodcarstatus);
	sem_close(sem_conscarstatus);

    cout << counter << endl;

    kill(penDriveDaemonPID, SIG_UNMOUNT);

    return 0;
}
