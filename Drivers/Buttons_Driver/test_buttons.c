#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

#define SIG_BUZZBUT (SIGRTMIN+5)
#define SIG_PROBUT (SIGRTMIN+4)
#define O_RDWR 02

extern int errno;
volatile sig_atomic_t flag =0;

void signal_catcher(int signo, siginfo_t *info, void *context){
	if(info->si_signo == SIG_BUZZBUT) flag = 1;
	if(info->si_signo ==  SIG_PROBUT) flag = 2;
}

int main(){
	int file_descriptor; //file descriptor of the device
	
	//handle signal
	struct sigaction act;		
	sigemptyset(&act.sa_mask);			// ??
	act.sa_flags = (SA_SIGINFO | SA_RESTART); 	//sigaction is to be used instead of signal and ??
	act.sa_sigaction = signal_catcher;		//handler function
	
	sigaction(SIG_PROBUT, &act, NULL);		
	sigaction(SIG_BUZZBUT, &act, NULL);

	//open device
	file_descriptor = open("/dev/button0", O_RDWR);	//open device and get file descriptor, file descriptor negative means error
	if(file_descriptor < 0){
		printf("\nFailed to open /dev/button: %s\n\n", strerror(errno));	//error handling
		return -EINVAL; 
	}
	else printf("\n/dev/button0 Opened!\nFile descriptor: %d\nWaiting for signal: %d or %d\n", file_descriptor, SIG_PROBUT, SIG_BUZZBUT);//success


	//hold program here	
	while(1) {
		if(flag == 1){
			printf("Disable/Enable Buzzer alarm!\n");
			flag = 0;
		}
		if(flag == 2){
			printf("Disable/Enable Image Processing signo!\n");
			flag = 0;
		}
	}

	close(file_descriptor);
}
