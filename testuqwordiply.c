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
#include <csse2310a3.h>

#define QUIET 0
#define PARALLEL 1
#define MAX_JOBS 5000
#define READ_END 0
#define WRITE_END 1
#define ERROR_END 2
#define UQCMP1_END 3
#define UQCMP2_END 4
#define PID_TESTPROG 0
#define PID_DEMO 1
#define PID_UQCMP_STDOUT 2
#define PID_UQCMP__STDERR 3
#define UQCMP_PREFIX_LEN 13
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
		int commaNum = 0;
		for(int count = 0; line[count] ; count++) {
			if(line[count]== ',') {
				if (commaNum < 1) {
					commaNum++;
				} else {
					exit_code_four(para_exist.jobfile, lineNum);
				} 
			}
 		}
		char** splitedResult = split_line(line,',');
        if (splitedResult[0] == NULL 
			|| splitedResult[1] == NULL
			|| !strcmp(splitedResult[0],"")) {
			free(splitedResult);
            exit_code_four(para_exist.jobfile, lineNum);
        }
		jobList.job[i].inputFileName = splitedResult[0];
		//6 Lines below help to relocate all element to its position + 1
		//because first arg will be 'name of program' when using exec
		//and last arg will be NULL
		char** jobArgs = split_space_not_quote(splitedResult[1], &jobList.job[i].numArgs);
		jobList.job[i].args = (char**)malloc(sizeof(char*)*(++jobList.job[i].numArgs + 1));
		jobList.job[i].args[0] = jobList.job[i].inputFileName;
		for( int argLoc = 0; argLoc < jobList.job[i].numArgs -1; argLoc++) {
			jobList.job[i].args[argLoc +1] = jobArgs[argLoc];
		}
		jobList.job[i].args[jobList.job[i].numArgs] = (char*)0;		
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

pid_t* job_runner(cmdArg para_exist, job job, int jobNo) {
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
		char uqcmpName[UQCMP_PREFIX_LEN];
		sprintf (uqcmpName,"Job %d stdout",jobNo);
		execlp("uqcmp", "uqcmp", uqcmpName, NULL);
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
		char uqcmpName[UQCMP_PREFIX_LEN];
		sprintf (uqcmpName,"Job %d stderr",jobNo);
		execlp("uqcmp", "uqcmp", uqcmpName, NULL);
		_exit(99);
	}
	// parent process

	close(stderr_TestProgToUqcmpFd[READ_END]);
	close(stderr_TestProgToUqcmpFd[WRITE_END]);
	
	close(stderr_DemoProgToUqcmpFd[READ_END]);
	close(stderr_DemoProgToUqcmpFd[WRITE_END]);

	pid_t* uqcmpPid = (pid_t*)malloc(sizeof(pid_t)*4);
	uqcmpPid[PID_TESTPROG] = pidTestProg;
	uqcmpPid[PID_DEMO] = pidDemo;
	uqcmpPid[PID_UQCMP_STDOUT] = pidUqcmpStdout;
	uqcmpPid[PID_UQCMP__STDERR] = pidUqcmpStderr;
	return uqcmpPid;
}

bool result_reporter(pid_t* uqcmpPid, int jobNo) {
	int statusStdoutUqcmp = 0;
	int statusStderrUqcmp = 0;
	int statusTestprog = 0;
	int statusDemo = 0;
	int passCounter = 0;
	waitpid(uqcmpPid[PID_UQCMP_STDOUT], &statusStdoutUqcmp, 0);
	fflush(stdout);
	if (WIFEXITED(statusStdoutUqcmp)) {
		//exit statue will be here
		if (WEXITSTATUS(statusStdoutUqcmp) == 0) {
			printf("Job %d: Stdout matches\n", jobNo);
			fflush(stdout);
			passCounter++;
		}
		else if (WEXITSTATUS(statusStdoutUqcmp) == 5) {
			printf("Job %d: Stdout differs\n", jobNo);
			fflush(stdout);
		}
		else {
			printf("Job %d: Unable to execute test\n", jobNo);
			fflush(stdout);
		}
	}
	waitpid(uqcmpPid[PID_UQCMP__STDERR], &statusStderrUqcmp, 0);
	if (WIFEXITED(statusStderrUqcmp)) {
		//exit statue will be here
		if (WEXITSTATUS(statusStderrUqcmp) == 0) {
			printf("Job %d: Stderr matches\n", jobNo);
			fflush(stdout);
			passCounter++;
		}
		else if (WEXITSTATUS(statusStderrUqcmp) == 5) {
			printf("Job %d: Stderr differs\n", jobNo);
			fflush(stdout);
		}
		else {
			printf("Job %d: Unable to execute test\n", jobNo);
			fflush(stdout);
		}
	}
	waitpid(uqcmpPid[PID_TESTPROG], &statusTestprog, 0);
	waitpid(uqcmpPid[PID_DEMO], &statusDemo, 0);
	if (WEXITSTATUS(statusTestprog) == WEXITSTATUS(statusDemo)) {
		passCounter++;
		printf("Job %d: Exit status matches\n", jobNo);
		fflush(stdout);
	}
	else {
		printf("Job %d: Exit status differs\n", jobNo);
		fflush(stdout);
	}
	return (passCounter == 3);
}

int prallel_runer(cmdArg para_exist, job_List jobList, int passCounter) {
	pid_t** uqcmpPid = (pid_t**)malloc(sizeof(pid_t*)*jobList.size);
	for(int i = 0; i < jobList.size; i++) {
		printf("Starting job %d\n", i+1);
		fflush(stdout);
		uqcmpPid[i] = job_runner(para_exist, jobList.job[i], i+1);
	}
	sleep(2);
	//kill
	for (int i = 0; i < jobList.size; i++) {
		kill(uqcmpPid[i][PID_TESTPROG], SIGKILL);
		kill(uqcmpPid[i][PID_DEMO], SIGKILL);
		kill(uqcmpPid[i][PID_UQCMP_STDOUT], SIGKILL);
		kill(uqcmpPid[i][PID_UQCMP__STDERR], SIGKILL);
	}
	for (int i = 0; i < jobList.size; i++) {
		(result_reporter(uqcmpPid[i], i+1))? passCounter++ : passCounter;
		free(uqcmpPid[i]);
	}
	free(uqcmpPid);
	return passCounter;
}

int linear_runner(cmdArg para_exist, job_List jobList, int passCounter) {
	for (int i = 0; i < jobList.size; i++) {
		printf("Starting job %d\n", i+1);
		fflush(stdout);
		pid_t* uqcmpPid = job_runner(para_exist, jobList.job[i], i+1);
		sleep(2);
		//kill
		kill(uqcmpPid[PID_TESTPROG], SIGKILL);
		kill(uqcmpPid[PID_DEMO], SIGKILL);
		kill(uqcmpPid[PID_UQCMP_STDOUT], SIGKILL);
		kill(uqcmpPid[PID_UQCMP__STDERR], SIGKILL);
		(result_reporter(uqcmpPid, i+1))? passCounter++ : passCounter;
		free(uqcmpPid);
	}
	return passCounter;
}

int main(int argc, char* argv[]) {
	cmdArg para_exist = pre_run_checking(argc, argv);
	
	FILE *jobFile = fopen(para_exist.jobfile, "r");
	if (jobFile == NULL) {
		exit_code_three(para_exist.jobfile);
	}
	job_List jobList = line_splitter(jobFile, para_exist);
	int passCounter = 0;
	//0 read(stdin), 1 write(stdout), 2error(stderr), 3 (uqcmp input), 4 (uqcmp input)
	if (para_exist.flag[PARALLEL] == 1) { // parallel run
		passCounter = prallel_runer(para_exist, jobList, passCounter);
	} else { // linear run
		passCounter = linear_runner(para_exist, jobList, passCounter);
	}
	printf("testuqwordiply: %d out of %d tests passed\n", passCounter, jobList.size);
	para_exist.jobfile = NULL;
	free(para_exist.jobfile);
	para_exist.testprogram = NULL;
	free(para_exist.testprogram);
	//free(testProgram); this already close!
	free(jobFile);
	return (jobList.size - passCounter)? 1 : 0;
}