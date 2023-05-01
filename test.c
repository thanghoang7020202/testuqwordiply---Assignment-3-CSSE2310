#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h> 
#include <stdbool.h>
#include <time.h>
//#include <csse2310a3.h>

struct timespec start = {2,0},remain;
int timeGetter = 0;
void handle_sigint(int sig) {
    printf("I'll keep running!!!\n");
    timeGetter = nanosleep(&remain, &remain);
}

int main(int argc, char** argv){ 
    signal(SIGINT, handle_sigint);    
    timeGetter = nanosleep(&start, &remain); 
        printf("Good sleep!\n");
        printf("Remaining time is: %lld.%.9ld", (long long)remain.tv_sec, remain.tv_nsec);  
    return 0; 
}
