#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h> 
#include <csse2310a3.h>

#define QUIET 0
#define PARALLEL 1
#define MAX_JOBS 5000
#define READ_END 0
#define WRITE_END 1
#define ERROR_END 2
#define UQCMP1_END 3
#define UQCMP2_END 4
#define PID_UQCMP_STDOUT 0
#define PID_UQCMP__STDERR 1
typedef struct cmdArg{
	char flag[4];
	char* testprogram;
	char* jobfile; 
} cmdArg;

typedef struct job{
	char* inputFileName;
	FILE* inputFile;
	char** args;
	int numArgs;
	int lineNum;
} job;

typedef struct job_List{
	job job[MAX_JOBS];
	int size;
} job_List;

void exit_code_two() {
	fprintf(stderr,"Usage: testuqwordiply [--quiet] [--parallel] testprogram jobfile\n");
	exit(2);
}

void exit_code_three(char* filename) {
	fprintf(stderr,"testuqwordiply: Unable to open job file \"%s\"\n", filename);
	exit(3);
}

void exit_code_four(char* filename, int lineNum) {
	fprintf(stderr,"testuqwordiply: syntax error on line %d of \"%s\"\n", lineNum, filename);
	exit(4);
}

void exit_code_five(char* inputFileName, char* filename, int lineNum) {
	fprintf(stderr,"testuqwordiply: unable to open file \"%s\" specified on line %d of \"%s\"\n", inputFileName, lineNum, filename);
	exit(5);
}

void exit_code_six(char* filename) {
	fprintf(stderr,"testuqwordiply: no jobs found in \"%s\"\n", filename);
	exit(6);
}

cmdArg pre_run_checking(int argc, char* argv[]) {
	//printf("argc = %d\n", argc);
	cmdArg para_exist;
	para_exist.flag[QUIET] = 0;
	para_exist.flag[PARALLEL] = 0;
	para_exist.testprogram = NULL;
	para_exist.jobfile = NULL;
	if(argc < 3) {
		exit_code_two();
	}
	for (int i = 1 ; i < argc ; i++) {
		//printf("argv[%d] = %s\n", i, argv[i]);
		if (strstr(argv[i], "--") != NULL) {
			if(para_exist.testprogram != NULL || para_exist.jobfile != NULL) {
				exit_code_two();
			}
			if (!strcmp(argv[i], "--quiet") && !para_exist.flag[QUIET]) {
				para_exist.flag[QUIET] = 1; // mark that quiet is set
				continue;
			}
			if (!strcmp(argv[i], "--parallel") && !para_exist.flag[PARALLEL]) {
				para_exist.flag[PARALLEL] = 1; // mark that parallel is set
				continue;
			}
			exit_code_two();
		} else {
			if(i == argc - 2 && para_exist.testprogram == NULL) { // second last argument
				para_exist.testprogram = argv[i];
				continue;
			} else if (i == argc - 1 && para_exist.jobfile == NULL) { // last argument
				para_exist.jobfile = argv[i];
				continue;
			}
			exit_code_two();
		}
	}
	if (para_exist.jobfile == NULL || para_exist.testprogram == NULL) {
		exit_code_two();
	}
	return para_exist;
}

job_List line_splitter(FILE* jobFile, cmdArg para_exist) {
	job_List jobList;
	jobList.size = 0;
	char* line = NULL;
	for(int i = 0, lineNum = 1;(line = read_line(jobFile)) != NULL;lineNum++){
		if(line[0] == '#' || !strcmp(line,"")){
			continue; // skip it if it is a comment line or empty line
		}
		char** splitedResult = split_line(line,',');
        if (splitedResult[0] == NULL || splitedResult[1] == NULL) {
            // edit later ?
			free(splitedResult);
            exit_code_four(para_exist.jobfile, lineNum);
        }
		jobList.job[i].inputFileName = splitedResult[0];
		//6 Lines below help to relocate all element to its position + 1
		//because first arg will be 'name of program' when using exec
		char** jobArgs = split_space_not_quote(splitedResult[1], &jobList.job[i].numArgs);
		jobList.job[i].args = (char**)malloc(sizeof(char*)*(++jobList.job[i].numArgs));
		jobList.job[i].args[0] = jobList.job[i].inputFileName;
		for( int argLoc = 0; argLoc < jobList.job[i].numArgs -1; argLoc++) {
			jobList.job[i].args[argLoc +1] = jobArgs[argLoc];
		}		
		jobList.job[i].lineNum = lineNum;
		if((jobList.job[i].inputFile = fopen(jobList.job[i].inputFileName, "r")) == NULL) {
			exit_code_five(jobList.job[i].inputFileName, para_exist.jobfile, lineNum);
		}
		//close(jobList.job[i].inputFile);
		jobList.size++;
        i++;

		//noBrackets can not be free internally...
		free(splitedResult);
	}
	free(line);
	if (jobList.size == 0) {
		exit_code_six(para_exist.jobfile);
	}
	return jobList;
}

pid_t* job_runner(cmdArg para_exist, job job) {
    int devNull = open("/dev/null", O_WRONLY);
	int stdout_TestProgToUqcmpFd[2];
	int stdout_DemoProgToUqcmpFd[2];
	int stderr_TestProgToUqcmpFd[2];
	int stderr_DemoProgToUqcmpFd[2];
	if(pipe(stdout_TestProgToUqcmpFd) == -1) {
        perror("Creating pipe stdout from test program to uqcmp error!");
        exit(1);
    }
	if(pipe(stderr_TestProgToUqcmpFd) == -1) {
		perror("Creating pipe stderr from test program to uqcmp error!");
        exit(1);
	}
	pid_t pidTestProg = fork();
	if (pidTestProg == 0) {
		// testprogram child process
		int input_test = open(job.inputFileName, O_RDONLY);
		dup2(input_test, STDIN_FILENO);

		dup2(stdout_TestProgToUqcmpFd[WRITE_END],STDOUT_FILENO);
		close(stdout_TestProgToUqcmpFd[READ_END]);
		close(stdout_TestProgToUqcmpFd[WRITE_END]);
		
		dup2(stderr_TestProgToUqcmpFd[WRITE_END],STDERR_FILENO);
		close(stderr_TestProgToUqcmpFd[READ_END]);
		close(stderr_TestProgToUqcmpFd[WRITE_END]);
		execvp(para_exist.testprogram, job.args);
		_exit(99);
	}
	if(pipe(stdout_DemoProgToUqcmpFd) == -1) {
        perror("Creating pipe stdout from Demo to uqcmp error!");
        exit(1);
    }
	if(pipe(stderr_DemoProgToUqcmpFd) == -1) {
		perror("Creating pipe stderr from Demo to uqcmp error!");
        exit(1);
	}
	pid_t pidDemo = fork();
	if (pidDemo == 0) {
		// demo child process
		int input_test = open(job.inputFileName, O_RDONLY);
		dup2(input_test, STDIN_FILENO);
		dup2(stdout_DemoProgToUqcmpFd[WRITE_END],STDOUT_FILENO);
		close(stdout_DemoProgToUqcmpFd[READ_END]);
		close(stdout_DemoProgToUqcmpFd[WRITE_END]);
		
		dup2(stderr_DemoProgToUqcmpFd[WRITE_END],STDERR_FILENO);
		close(stderr_DemoProgToUqcmpFd[READ_END]);
		close(stderr_DemoProgToUqcmpFd[WRITE_END]);
		execvp("demo-uqwordiply", job.args);
		_exit(99);
	}
	pid_t pidUqcmpStdout = fork();
	if (pidUqcmpStdout == 0) {
		// uqcmp stdout child process
		dup2(devNull, STDIN_FILENO); // ignore stdin
		if (para_exist.flag[QUIET] == 1) {
			dup2(devNull, STDOUT_FILENO); // ignore stdout
		}
		dup2(stdout_TestProgToUqcmpFd[READ_END], UQCMP1_END);
		close(stdout_TestProgToUqcmpFd[READ_END]);
		close(stdout_TestProgToUqcmpFd[WRITE_END]);

		dup2(stdout_DemoProgToUqcmpFd[READ_END], UQCMP2_END);
		close(stdout_DemoProgToUqcmpFd[READ_END]);
		close(stdout_DemoProgToUqcmpFd[WRITE_END]);
		execlp("uqcmp", "uqcmp", NULL);
		_exit(99);
	}
	// parent process
	close(stdout_TestProgToUqcmpFd[READ_END]);
	close(stdout_TestProgToUqcmpFd[WRITE_END]);
	
	close(stdout_DemoProgToUqcmpFd[READ_END]);
	close(stdout_DemoProgToUqcmpFd[WRITE_END]);

	pid_t pidUqcmpStderr = fork();
	if (pidUqcmpStderr == 0) {
		// uqcmp stderr child process
		dup2(devNull, STDIN_FILENO); // ignore stdin
		if (para_exist.flag[QUIET] == 1) {
			dup2(devNull, STDOUT_FILENO); // ignore stdout
		}
		dup2(stderr_TestProgToUqcmpFd[READ_END], UQCMP1_END);
		close(stderr_TestProgToUqcmpFd[READ_END]);
		close(stderr_TestProgToUqcmpFd[WRITE_END]);

		dup2(stderr_DemoProgToUqcmpFd[READ_END], UQCMP2_END);
		close(stderr_DemoProgToUqcmpFd[READ_END]);
		close(stderr_DemoProgToUqcmpFd[WRITE_END]);
		execlp("uqcmp", "uqcmp", NULL);
		_exit(99);
	}
	// parent process

	close(stderr_TestProgToUqcmpFd[READ_END]);
	close(stderr_TestProgToUqcmpFd[WRITE_END]);
	
	close(stderr_DemoProgToUqcmpFd[READ_END]);
	close(stderr_DemoProgToUqcmpFd[WRITE_END]);

	pid_t* uqcmpPid = (pid_t*)malloc(sizeof(pid_t)*2);
	uqcmpPid[PID_UQCMP_STDOUT] = pidUqcmpStdout;
	uqcmpPid[PID_UQCMP__STDERR] = pidUqcmpStderr;
	return uqcmpPid;
}

void result_reporter(pid_t* uqcmpPid, int jobNo) {
	int statusStdoutUqcmp, statusStderrUqcmp;
	waitpid(uqcmpPid[PID_UQCMP_STDOUT], &statusStdoutUqcmp, 0);
	fflush(stdout);
	if (WIFEXITED(statusStdoutUqcmp)) {
		//exit statue will be here
		if (WEXITSTATUS(statusStdoutUqcmp) == 0) {
			printf("Job %d: Stdout matches\n", jobNo);
		}
	}
	waitpid(uqcmpPid[PID_UQCMP__STDERR], &statusStderrUqcmp, 0);
	if (WIFEXITED(statusStderrUqcmp)) {
		//exit statue will be here
		printf("Job %d: Stderr matches\n", jobNo);
	}
}



int main(int argc, char* argv[]) {
	cmdArg para_exist = pre_run_checking(argc, argv);
	
	FILE *testProgram = fopen(para_exist.testprogram, "r");
	if (testProgram == NULL) {
		exit_code_three(para_exist.testprogram);
	}
	fclose(testProgram);
	FILE *jobFile = fopen(para_exist.jobfile, "r");
	
	if (jobFile == NULL) {
		exit_code_three(para_exist.jobfile);
	}
	job_List jobList = line_splitter(jobFile, para_exist);

	//0 read(stdin), 1 write(stdout), 2error(stderr), 3 (uqcmp input), 4 (uqcmp input)
	for(int i = 0; i < jobList.size; i++) {
		printf("Starting job %d\n", i+1);
		pid_t* uqcmpPid = job_runner(para_exist, jobList.job[i]);
		sleep(2);
		result_reporter(uqcmpPid, i+1);
	}
	para_exist.jobfile = NULL;
	free(para_exist.jobfile);
	para_exist.testprogram = NULL;
	free(para_exist.testprogram);
	//free(testProgram); this already close!
	free(jobFile);
	return 0;
}