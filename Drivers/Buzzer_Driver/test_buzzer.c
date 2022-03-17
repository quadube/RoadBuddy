#include <unistd.h>
#include <fcntl.h>

int main (void){
    int file_descriptor = open("/dev/buzzer0", O_WRONLY);
    char BuzzOn = '1';
    char BuzzOff = '0';

    while (1){
        write(file_descriptor, &BuzzOn, 1);
        sleep(5);
        write(file_descriptor, &BuzzOff, 1);
        sleep(5);
    }
    close(file_descriptor);
}
