#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#define READ_END 0
#define WRITE_END 1
#define INITIAL_BUFFER_SIZE 80

char* read_line(FILE* stream) {

int bufferSize = INITIAL_BUFFER_SIZE;
    char* buffer = malloc(sizeof(char) * bufferSize);
    int numRead = 0;
int next;

if (feof(stream)) {
return NULL;
    }

while (1) {
next = fgetc(stream);
if (next == EOF && numRead == 0) {
free(buffer);
return NULL;
}
if (numRead == bufferSize - 1) {
bufferSize *= 2;
buffer = realloc(buffer, sizeof(char) * bufferSize);
}
if (next == '\n' || next == EOF) {
buffer[numRead] = '\0';
break;
}
buffer[numRead++] = next;
}
    return buffer;
}

int main(int argc, char* argv[]){
    // Create a pipe
    int fd[2];
    int stat;
    if(pipe(fd) == -1) {
        perror("Creating pipe error!");
        exit(1);
    }
    // create the child process
    /* If this is the child */
    if(!fork()){
        // Redirect stdout
        dup2(fd[WRITE_END],STDOUT_FILENO);
        // Close the dangling ends of the pipe
        close(fd[READ_END]);
        close(fd[WRITE_END]);
        // exec the program
        char* arr[argc+1];
        execvp(argv[1], &argv[1]);
        _exit(99);
    }
    // Close the dangling ends of the pipe
    close(fd[WRITE_END]);
    /* for each character/line read from the pipe */
    FILE* readFromFormat = fdopen(fd[READ_END], "r");
    for(char* line;(line = read_line(readFromFormat));){
        printf("%s", line);
    }
    // wait
    wait(&stat);
    // return with the same status as the program
    if(WIFEXITED(stat)) {
        return WEXITSTATUS(stat);
    }
    return 0;
}