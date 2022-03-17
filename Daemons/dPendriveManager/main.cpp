#include "event_monitor.h"
#include <signal.h>

#define SIG_UNMOUNT (SIGRTMIN+2)
#define SIG_MOUNT (SIGRTMIN+7)

eventMonitor event;
volatile sig_atomic_t status;

void signal_catcher(int sig){
    if(sig == SIG_MOUNT) {
        event.mountOnStartup();
        #ifdef MANAGE_SPACE
        event.manageSpace();
        #endif
    }
    else if(sig == SIG_UNMOUNT) event.unmount();
}

int main(){
    uint8_t ret = 1;
    signal(SIG_MOUNT, signal_catcher); //mouny
    signal(SIG_UNMOUNT, signal_catcher); //mount
    signal(SIGTERM, signal_catcher); //terminate gracefully
	
    event.daemonize();
    event.givePID();
    event.setupEvent();

    while(1){
        ret = event.autoMountDevices();
        #ifdef MANAGE_SPACE1
        if(ret == 0){
            ret = 1;
            syslog(LOG_DEBUG, "space manager\n");
            event.manageSpace();  
        }
        #endif
        sleep(1);
    }

    syslog(LOG_INFO, "Daemon terminated!\n");
    closelog();
    return 0;
}
