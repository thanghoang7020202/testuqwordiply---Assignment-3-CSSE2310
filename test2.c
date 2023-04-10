#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <string.h>

#define READ_END 0
#define WRITE_END 1
#define INITIAL_BUFFER_SIZE 100

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

int main(int argc, char** argv){
    char* line;
    while((line = read_line(stdin))){
        // Create a pipe
        int fd[2];
        if(pipe(fd) == -1) {
            perror("Creating pipe!");
            exit(1);
        }
        // create a child process
        /* If this is the child */
        if(!fork()){
        // Redirect stdin to be from the pipe
            dup2(fd[READ_END],STDIN_FILENO);
        // Close the dangling ends of the pipe
            close(fd[READ_END]);
            close(fd[WRITE_END]);
            execlp("md5sum", "md5sum", NULL);
            _exit(99);
        }
        // Print the line to the pipe
        //write(fd[READ_END],fd[1],strlen(fd[1]));
        FILE* writeToMd5sum = fdopen(fd[WRITE_END],"w");
        fprintf(writeToMd5sum,"%s",line);
        fflush(writeToMd5sum);
        // Close the pipe
        close(fd[WRITE_END]);
        close(fd[READ_END]);
        // Wait for the child to terminate
        wait(NULL);
    }
    return 0;
}