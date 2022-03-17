#include "event_monitor.h"

eventMonitor::eventMonitor() : fd_epoll{-1}, fd_udev{-1}, maxevents{5} {
    //Create a udev monitor object(returns a pointer to the allocated udev monitor)
    udev = udev_new();
    path = DEFAULT_MOUNT_PATH;
}

eventMonitor::~eventMonitor(){

}

#if 0
void eventMonitor::store(){
  
    int source = open("/etc/picture.png", O_RDONLY, 0); 
    int dest = open("/media/usb/folder/picture.png", O_WRONLY | O_CREAT /*| O_TRUNC/**/, 0644);

    while ((size = read(source, buf, BUFSIZ)) > 0) {
        write(dest, buf, size);
    }

    close(source);
    close(dest);

    return 0;
}
#endif

void eventMonitor::daemonize(){
    openlog("dPendriveManager", LOG_PID, LOG_DAEMON);
    /* Fork off the parent process */
    myPid = fork();
    // on error exit
    if (myPid < 0){ 
		syslog(LOG_ERR, "fork: %m\n");
		exit(EXIT_FAILURE);
	}
    /* Success: Let the parent terminate */
    if (myPid > 0){
        exit(EXIT_SUCCESS);
    }

    /* On success: The child process becomes session leader */
    if (setsid() < 0){ 
		syslog(LOG_ERR, "%s\n", "setsid");
		exit(EXIT_FAILURE);
	}

    //fork for the second time
    myPid = fork();
    if (myPid < 0){ 
		syslog(LOG_ERR, "fork: %m\n");
		exit(EXIT_FAILURE);
	}
    /* Success: Let the parent terminate */
    if (myPid > 0){
        exit(EXIT_SUCCESS);
    }

    /* On success: The child process becomes session leader */
    if (setsid() < 0){ 
		syslog(LOG_ERR, "%s\n", "setsid");
		exit(EXIT_FAILURE);
	}

    //set new file permissions
    umask(0);

    // make '/' the root directory
	if (chdir("/") < 0) { // on error exit
		syslog(LOG_ERR, "%s\n", "chdir");
		exit(EXIT_FAILURE);
	}
    
    close(STDIN_FILENO);  // close standard input file descriptor
	close(STDOUT_FILENO); // close standard output file descriptor
	close(STDERR_FILENO); // close standard error file descriptor
}

void eventMonitor::givePID(){
    myPid = getpid();

    //remove any leftover semaphore (precaution)
    sem_unlink(SEM_PRODUCER_FNAME);

    //create new semaphore
    sem_t * sem_prod = sem_open(SEM_PRODUCER_FNAME, O_CREAT, 0660, 0);
    if(sem_prod == SEM_FAILED)
        syslog(LOG_ERR, "Semaphore create: %m\n");
    
    //grab the shared memory block
    char* block = attach_memory_block(FILENAME, SH_BLOCK_SIZE);
    if(!block)
        syslog(LOG_ERR, "Attach block: %m\n");
    
    //put pid on shared memory
    sprintf(block, "%d", myPid);

    //change semaphore value to let know pid is available
    sem_post(sem_prod);

    //let go of shared memory block
    sem_close(sem_prod);
    detach_memory_block(block); 
}

struct udev_device* eventMonitor::get_child(struct udev* udev, struct udev_device* parent, const char* subsystem){
    struct udev_device* child = NULL;
    struct udev_enumerate *enumerate = udev_enumerate_new(udev);

    udev_enumerate_add_match_parent(enumerate, parent);
    udev_enumerate_add_match_subsystem(enumerate, subsystem);
    udev_enumerate_scan_devices(enumerate);

    struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);
    struct udev_list_entry *entry;

    udev_list_entry_foreach(entry, devices) {
        const char *path = udev_list_entry_get_name(entry);
        child = udev_device_new_from_syspath(udev, path);
        break;
    }

    udev_enumerate_unref(enumerate);
    return child;
}

void eventMonitor::unmount(){
    //if directory exists
    dir = opendir(path.c_str());
    if(dir){
        //close directory to be able to unmount
        closedir(dir);
        //unmount
        status = umount2(path.c_str(), MNT_DETACH);
        if(status != 0)
            syslog(LOG_ERR, "%m\n");
        //remove the directory
        status = rmdir(path.c_str());
        if(status != 0)
            syslog(LOG_ERR, "%m\n");
    }
}

void eventMonitor::mountOnStartup(){
    enumerate = udev_enumerate_new(udev);

    udev_enumerate_add_match_subsystem(enumerate, "scsi");
    udev_enumerate_add_match_property(enumerate, "DEVTYPE", "scsi_device");
    udev_enumerate_scan_devices(enumerate);

    device_list = udev_enumerate_get_list_entry(enumerate);
    struct udev_list_entry *entry;

    /// enumerate through any that are installed
    udev_list_entry_foreach(entry, device_list){

        const char *path1 = udev_list_entry_get_name(entry);

        struct udev_device *scsi = udev_device_new_from_syspath(udev, path1);
        struct udev_device *block = get_child(udev, scsi, "block");
        struct udev_device *scsi_disk = get_child(udev, scsi, "scsi_disk");
        struct udev_device *usb = udev_device_get_parent_with_subsystem_devtype(scsi, "usb", "usb_device");


        if(usb && block && scsi_disk){
            string node = udev_device_get_devnode(block);
            //device node  is /dev/sda but the filesystem to mount is /dev/sda1
            node.append("1");
            //if directory doesn't exist
            dir = opendir(path.c_str());
            if(!dir){
                //create the directory 
                status = mkdir(path.c_str(), 777); 
                if(status < 0)
                    syslog(LOG_ERR, "mkdir: %m\n");
                //mount 
                status = mount(node.c_str(), path.c_str(), "vfat", MS_NOATIME, NULL);
                if(status < 0)
                    syslog(LOG_ERR, "mount: %m\n");
            }
        }
        if(block)
            udev_device_unref(block);
        if(scsi_disk)
            udev_device_unref(scsi_disk);
        udev_device_unref(scsi);
    }
    udev_enumerate_unref(enumerate);
}

#ifdef MANAGE_SPACE
void eventMonitor::manageSpace(){
    struct statvfs vfs;
    //get status from mounted filesystem 
    int status = statvfs("/mnt/usb", &vfs);
    if(status < 0){
        syslog(LOG_ERR, "statvfs: %m\n");
		exit(EXIT_FAILURE);
    }
    //using information provided by status of mounted filesystem calculate occupied percentagem
    used_space = 100.0 * (double) (vfs.f_blocks - vfs.f_bfree) / (double) (vfs.f_blocks - vfs.f_bfree + vfs.f_bavail);

    if(used_space > 70.0f){
        struct dirent *entry, *oldestFile;
        DIR* dp;
        struct stat statbuf;
        time_t t_oldest;
        double sec;

        time(&t_oldest);
        if((dp = opendir(path.c_str())) != NULL) {
            chdir(path.c_str());
            while((entry = readdir(dp)) != NULL) {
                lstat(entry->d_name, &statbuf);       
                if(strcmp(".",entry->d_name) == 0 || strcmp("..",entry->d_name) == 0)
                    continue;
                syslog(LOG_INFO, "%s\t%s", entry->d_name, ctime(&statbuf.st_mtime));
                if(difftime(statbuf.st_mtime, t_oldest) < 0){
                    t_oldest = statbuf.st_mtime;
                    oldestFile = entry;
                }
            }
        } 
    
        syslog(LOG_ERR,"Removed: %s\n", oldestFile->d_name);
        remove(oldestFile->d_name);

        //syslog(LOG_ERR,"oldest time: %s\n", ctime(&t_oldest));
        int status = closedir(dp);
        if(status < 0) 
             syslog(LOG_ERR, "closedir: %m\n");
    }
}
#endif

void eventMonitor::setupEvent(){
    monitor = udev_monitor_new_from_netlink(udev, "udev");

    // adds a filter matching the device against a subsystem
    udev_monitor_filter_add_match_subsystem_devtype(monitor, "usb", "usb_device");  //usb device
    udev_monitor_filter_add_match_subsystem_devtype(monitor, "block", NULL);        //block device

    // Binds the udev_monitor socket to the event source
    udev_monitor_enable_receiving(monitor);

    // Creates epoll instance to verify if I/O possible
    fd_epoll = epoll_create1(0);
    if (fd_epoll  < 0) {
        syslog(LOG_ERR, "%m\n");
		exit(EXIT_FAILURE);
    }

    //gets udev fd
    fd_udev = udev_monitor_get_fd(monitor);

    //fill event struct
    epoll_udev.events = EPOLLOUT; //The associated file is available for write(2) operations.
    epoll_udev.data.fd = fd_udev; //sets event fd as udev fd

    //EPOLL_CTL_ADD Add an entry to the interest list of the epoll file descriptor
    if (epoll_ctl(fd_epoll, EPOLL_CTL_ADD, fd_udev, &epoll_udev) < 0) {
        syslog(LOG_ERR, "%m\n");
		exit(EXIT_FAILURE);
    }
}

uint8_t eventMonitor::autoMountDevices(){
         
    // Polling loop for devices
    struct epoll_event ev[maxevents];
    uint8_t ret = 1;
    //espera aqui por events e retorna o numero de events
    int fdcount = epoll_wait(fd_epoll, ev, maxevents, -1);
    if (fdcount < 0) {
        //If error is EINTR a signal occurred and not an actual error
        if (errno != EINTR){ 
            syslog(LOG_ERR, "%m\n");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < fdcount; i++) {
        //The associated file is available for write(2) operations[EPOLLOUT] and corresponds to the udev
        if (ev[i].data.fd == fd_udev && ev[i].events & EPOLLOUT){ 
            
            //returns a pointer to a newly referenced device
            dev = udev_monitor_receive_device(monitor); 
            //If no such entry can be found, or on failure, NULL is returned
            if (dev == NULL){ 
                continue;
            }

            action = udev_device_get_action(dev);
            devicenode = udev_device_get_devnode(dev);
            partition = udev_device_get_devtype(dev);
            
            #ifdef AUTO_UNMOUNT
            //pen removed
            if(!action.compare("remove") && !partition.compare("partition")){
                //if directory exists
                dir = opendir(path.c_str());
                if(dir){
                    //close directory to be able to unmount
                    closedir(dir);
                    //unmount
                    status = umount2(path.c_str(), MNT_DETACH);
                    if(status != 0)
                        syslog(LOG_ERR, "umount: %m\n");
                    //remove the directory
                    status = rmdir(path.c_str());
                    if(status != 0)
                        syslog(LOG_ERR, "rmdir: %m\n");
                }
            }
            #endif

            //pen inserted
            if(!action.compare("add") && !partition.compare("partition")){
                //if directory doesn't exist
                dir = opendir(path.c_str());
                if(!dir){
                    //create the directory 
                    status = mkdir(path.c_str(), 777); 
                    if(status != 0){
                        syslog(LOG_ERR, "mkdir: %m\n");
                    }
                }
                //mount 
                status = mount(devicenode.c_str(), path.c_str(), "vfat", MS_NOATIME, NULL);
                if(status != 0)
                    syslog(LOG_ERR, "mount: %m\n");
                else
                    ret = 0;
            }
            udev_unref(udev);
        }
    }
    return ret;
}

