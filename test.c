#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> //exec
#include <sys/stat.h> // open
#include <fcntl.h> 
 
 
int main(int argc, char** argv){ 
    if (argc < 3){ 
        return 1; 
    } 
 
    // Open files 
        int rf = open(argv[1],O_RDONLY); 
        int wf = open(argv[2],O_WRONLY | O_CREAT); 
        if (rf == -1 || wf == -1) exit(2); 
    // Redirect stdin and stdout 
        /*dup2(rf,STDIN_FILENO); 
        dup2(wf,STDOUT_FILENO); */
        close(rf); 
        close(wf); 
    // Execute md5sum
        execlp("ls","ls",(char*)NULL);
    //execlp("./md5sum","md5sum","STDIN_FILENO","STDOUT_FILENO",(char*)0); 
    return 0; 
}