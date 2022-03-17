#include "GetCarStatus.h"


int main(){
    CarStatus data;

    data.daemonize();
    data.obdConfig();
    while(1){
        //get data
        if(data.getStatus())
            data.sendStatus();
        sleep(2);
    }

    return 0;
}