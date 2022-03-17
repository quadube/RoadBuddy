#ifndef EVENT_MONITOR_H
#define EVENT_MONITOR_H

#include <sys/epoll.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string>
#include <libudev.h>
#include <iostream>
#include <dirent.h>
#include <syslog.h>
#include <fcntl.h>   // open
#include <unistd.h>  // read, write, close
#include <cstdio>    // BUFSIZ
#include "shared_memory.h"
#include <semaphore.h>
#include <sys/statvfs.h> 
#include <time.h>
#include <cstring>

#define DEFAULT_MOUNT_PATH "/mnt/usb"
#define MANAGE_SPACE
#define FILENAME "/etc/roadbuddy/daemons/dPendriveManager"
#define SH_BLOCK_SIZE 64

//semaphores for shared memory
#define SEM_PRODUCER_FNAME "/daemon_sem"

using namespace std;

class eventMonitor {
    public:
        eventMonitor();
        ~eventMonitor();
        void daemonize();
        void mountOnStartup();
        void unmount();
        void givePID();
        struct udev_device* get_child(struct udev* udev, struct udev_device* parent, const char* subsystem);
        uint8_t autoMountDevices();
        void setupEvent();

        #ifdef MANAGE_SPACE
        void manageSpace();
        #endif

    private:
        struct udev *udev;
        struct udev_monitor *monitor;
        struct udev_device *dev;
        struct epoll_event epoll_udev;
        struct udev_enumerate *enumerate;
        struct udev_list_entry *device_list;

        DIR *dir;
        pid_t myPid;
        int fd_epoll, fd_udev, maxevents, status;
        string action, devicenode, devicesubsystem, partition, path;
        double used_space;
        
        char buf[BUFSIZ];
        size_t size;

};


#endif