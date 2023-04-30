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

// Used for controling the number of job in linear run
/*
 * actual job size that has been run
 * (violatileJobSize <= JobsList.size)
 */
int violatileJobSize = -1;
/*
 * number of job that has been run
 */
int jobRun = 0;

typedef struct CmdArg {
    char flag[4];
    char* testprogram;
    char* jobfile; 
} CmdArg;

typedef struct Job {
    char* inputFileName;
    FILE* inputFile;
    char** args;
    int numArgs;
    int lineNum;
} Job;

typedef struct JobsList {
    Job job[MAX_JOBS];
    int size;
} JobsList;

void exit_code_two() {
    fprintf(stderr, "Usage: testuqwordiply [--quiet] [--parallel]"
            " testprogram jobfile\n");
    exit(2);
}

void exit_code_three(char* filename) {
    fprintf(stderr, "testuqwordiply: Unable to open job file"
            " \"%s\"\n", filename);
    exit(3);
}

void exit_code_four(char* filename, int lineNum) {
    fprintf(stderr, "testuqwordiply: syntax error on line %d "
            "of \"%s\"\n", lineNum, filename);
    exit(4);
}

void exit_code_five(char* inputFileName, char* filename, int lineNum) {
    fprintf(stderr, "testuqwordiply: unable to open file \"%s\" specified on"
            " line %d of \"%s\"\n", inputFileName, lineNum, filename);
    exit(5);
}

void exit_code_six(char* filename) {
    fprintf(stderr, "testuqwordiply: no jobs found in \"%s\"\n", filename);
    exit(6);
}

CmdArg pre_run_checking(int argc, char* argv[]) {
    //printf("argc = %d\n", argc);
    CmdArg paraExist;
    paraExist.flag[QUIET] = 0;
    paraExist.flag[PARALLEL] = 0;
    paraExist.testprogram = NULL;
    paraExist.jobfile = NULL;
    if (argc < 3) {
        exit_code_two();
    }
    for (int i = 1; i < argc; i++) {
	//printf("argv[%d] = %s\n", i, argv[i]);
    	if (strstr(argv[i], "--") != NULL) {
    	    if (paraExist.testprogram != NULL || paraExist.jobfile != NULL) {
    	        exit_code_two();
    	    }   
    	    if (!strcmp(argv[i], "--quiet") && !paraExist.flag[QUIET]) {
    	        paraExist.flag[QUIET] = 1; // mark that quiet is set
	        continue;
	    }
            if (!strcmp(argv[i], "--parallel") && !paraExist.flag[PARALLEL]) {
                paraExist.flag[PARALLEL] = 1; // mark that parallel is set
                continue;
            }
            exit_code_two();
        } else {
            if (i == argc - 2 && 
                    paraExist.testprogram == NULL) { // second last argument
                paraExist.testprogram = argv[i];
                continue;
            } else if (i == argc - 1 && 
                    paraExist.jobfile == NULL) { // last argument
                paraExist.jobfile = argv[i];
                continue;
            }
            exit_code_two();
        }
    }
    if (paraExist.jobfile == NULL || paraExist.testprogram == NULL) {
	exit_code_two();
    }
    return paraExist;
}

JobsList line_splitter(FILE* jobFile, CmdArg paraExist) {
    JobsList jobList;
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
		    exit_code_four(paraExist.jobfile, lineNum);
	        }    
	    }
 	}
	char** splitedResult = split_line(line,',');
        if (splitedResult[0] == NULL 
                || splitedResult[1] == NULL
		|| !strcmp(splitedResult[0],"")) {
	    free(splitedResult);
            exit_code_four(paraExist.jobfile, lineNum);
        }
	jobList.job[i].inputFileName = splitedResult[0];
	//6 Lines below help to relocate all element to its position + 1
	//because first arg will be 'name of program' when using exec
	//and last arg will be NULL
	char** jobArgs = split_space_not_quote(splitedResult[1], 
                &jobList.job[i].numArgs);
	jobList.job[i].args =
            (char**)malloc(sizeof(char*)*(++jobList.job[i].numArgs + 1));
	jobList.job[i].args[0] = jobList.job[i].inputFileName;
	for(int argLoc = 0; argLoc < jobList.job[i].numArgs -1; argLoc++) {
	    jobList.job[i].args[argLoc +1] = jobArgs[argLoc];
	}
	jobList.job[i].args[jobList.job[i].numArgs] = (char*)0;		
	jobList.job[i].lineNum = lineNum;
	if((jobList.job[i].inputFile = 
                    fopen(jobList.job[i].inputFileName, "r")) == NULL) {
	    exit_code_five(jobList.job[i].inputFileName, 
                    paraExist.jobfile, lineNum);
	}
	//close(jobList.job[i].inputFile);
	jobList.size++;
        i++;
	
        //noBrackets can not be free internally...
        free(splitedResult);
    }
    free(line);
    if (jobList.size == 0) {
        exit_code_six(paraExist.jobfile);
    }
    return jobList;
}

pid_t* job_runner(CmdArg paraExist, Job job, int jobNo) {
    int stdoutTestProgToUqcmpFd[2];
    int stdoutDemoProgToUqcmpFd[2];
    int stderrTestProgToUqcmpFd[2];
    int stderrDemoProgToUqcmpFd[2];
    if (pipe(stdoutTestProgToUqcmpFd) == -1) {
        perror("Creating pipe stdout from test program to uqcmp error!");
        exit(1);
    }
    if (pipe(stderrTestProgToUqcmpFd) == -1) {
    	perror("Creating pipe stderr from test program to uqcmp error!");
        exit(1);
    }
    pid_t pidTestProg = fork();
    if (pidTestProg == 0) {
    	// testprogram child process
    	int inputTest = open(job.inputFileName, O_RDONLY);
    	dup2(inputTest, STDIN_FILENO);
        dup2(stdoutTestProgToUqcmpFd[WRITE_END], STDOUT_FILENO);
    	close(stdoutTestProgToUqcmpFd[READ_END]);
    	close(stdoutTestProgToUqcmpFd[WRITE_END]);
	
        dup2(stderrTestProgToUqcmpFd[WRITE_END], STDERR_FILENO);
        close(stderrTestProgToUqcmpFd[READ_END]);
        close(stderrTestProgToUqcmpFd[WRITE_END]);
    	execvp(paraExist.testprogram, job.args);
        _exit(99);
    }
    if (pipe(stdoutDemoProgToUqcmpFd) == -1) {
        perror("Creating pipe stdout from Demo to uqcmp error!");
        exit(1);
    }
    if (pipe(stderrDemoProgToUqcmpFd) == -1) {
    	perror("Creating pipe stderr from Demo to uqcmp error!");
        exit(1);
    }
    pid_t pidDemo = fork();
    if (pidDemo == 0) {
    	// demo child process
    	close(stdoutTestProgToUqcmpFd[READ_END]);
    	close(stdoutTestProgToUqcmpFd[WRITE_END]);
    	close(stderrTestProgToUqcmpFd[READ_END]);
	close(stderrTestProgToUqcmpFd[WRITE_END]);

    	int inputTest = open(job.inputFileName, O_RDONLY);
        dup2(inputTest, STDIN_FILENO);
        dup2(stdoutDemoProgToUqcmpFd[WRITE_END], STDOUT_FILENO);
        close(stdoutDemoProgToUqcmpFd[READ_END]);
        close(stdoutDemoProgToUqcmpFd[WRITE_END]);
        
        dup2(stderrDemoProgToUqcmpFd[WRITE_END], STDERR_FILENO);
        close(stderrDemoProgToUqcmpFd[READ_END]);
        close(stderrDemoProgToUqcmpFd[WRITE_END]);
        execvp("demo-uqwordiply", job.args);
        _exit(99);
    }
    pid_t pidUqcmpStdout = fork();
    if (pidUqcmpStdout == 0) {
    	// uqcmp stdout child process
    	close(stderrTestProgToUqcmpFd[READ_END]);
	close(stderrTestProgToUqcmpFd[WRITE_END]);
    	close(stderrDemoProgToUqcmpFd[READ_END]);
	close(stderrDemoProgToUqcmpFd[WRITE_END]);
	    //dup2(devNull, STDIN_FILENO); // ignore stdin
        if (paraExist.flag[QUIET] == 1) {
            int devNull = open("/dev/null", O_WRONLY);
	    dup2(devNull, STDOUT_FILENO); // ignore stdout
        }
    	dup2(stdoutTestProgToUqcmpFd[READ_END], UQCMP1_END);
        close(stdoutTestProgToUqcmpFd[READ_END]);
        close(stdoutTestProgToUqcmpFd[WRITE_END]);

        dup2(stdoutDemoProgToUqcmpFd[READ_END], UQCMP2_END);
        close(stdoutDemoProgToUqcmpFd[READ_END]);
        close(stdoutDemoProgToUqcmpFd[WRITE_END]);
        char uqcmpName[UQCMP_PREFIX_LEN];
        sprintf(uqcmpName, "Job %d stdout", jobNo);
        execlp("uqcmp", "uqcmp", uqcmpName, NULL);
        _exit(99);
    }
    // parent process
    close(stdoutTestProgToUqcmpFd[READ_END]);
    close(stdoutTestProgToUqcmpFd[WRITE_END]);
	
    close(stdoutDemoProgToUqcmpFd[READ_END]);
    close(stdoutDemoProgToUqcmpFd[WRITE_END]);
    pid_t pidUqcmpStderr = fork();
    if (pidUqcmpStderr == 0) {
        // uqcmp stderr child process
        if (paraExist.flag[QUIET] == 1) {
	    int devNull = open("/dev/null", O_WRONLY);
	    dup2(devNull, STDOUT_FILENO); // ignore stdout
	}
	dup2(stderrTestProgToUqcmpFd[READ_END], UQCMP1_END);
	close(stderrTestProgToUqcmpFd[READ_END]);
	close(stderrTestProgToUqcmpFd[WRITE_END]);
	dup2(stderrDemoProgToUqcmpFd[READ_END], UQCMP2_END);
	close(stderrDemoProgToUqcmpFd[READ_END]);
	close(stderrDemoProgToUqcmpFd[WRITE_END]);
	char uqcmpName[UQCMP_PREFIX_LEN];
	sprintf(uqcmpName, "Job %d stderr", jobNo);
	execlp("uqcmp", "uqcmp", uqcmpName, NULL);
	_exit(99);
    }
    // parent process

    close(stderrTestProgToUqcmpFd[READ_END]);
    close(stderrTestProgToUqcmpFd[WRITE_END]);
	
    close(stderrDemoProgToUqcmpFd[READ_END]);
    close(stderrDemoProgToUqcmpFd[WRITE_END]);

    pid_t* uqcmpPid = (pid_t*)malloc(sizeof(pid_t) * 4);
    uqcmpPid[PID_TESTPROG] = pidTestProg;
    uqcmpPid[PID_DEMO] = pidDemo;
    uqcmpPid[PID_UQCMP_STDOUT] = pidUqcmpStdout;
    uqcmpPid[PID_UQCMP__STDERR] = pidUqcmpStderr;
    return uqcmpPid;
}

/*
 * result_reporter()
 * -----------------
 * This function will report the result of the job.
 * compare the stdout and stderr of test program 
 * and demo-uqwordiply with uqcmp as well as the exit status
 * of test program and demo-uqwordiply.
 * 
 * uqcmpPid: the array of pid of test program, demo-uqwordiply and uqcmp
 * jobNo: the job number
 * 
 * Return: true if all the result matches, false otherwise
 */
bool result_reporter(pid_t* pids, int jobNo) {
    int statusStdoutUqcmp = 0;
    int statusStderrUqcmp = 0;
    int statusTestprog = 0;
    int statusDemo = 0;
    int passCounter = 0;
    waitpid(pids[PID_UQCMP_STDOUT], &statusStdoutUqcmp, 0);
    fflush(stdout);
    if (WIFEXITED(statusStdoutUqcmp)) {
	//exit statue will be here
    	if (WEXITSTATUS(statusStdoutUqcmp) == 0) {
            printf("Job %d: Stdout matches\n", jobNo);
	    fflush(stdout);
    	    passCounter++;
	} else {
            printf("Job %d: Stdout differs\n", jobNo);
            fflush(stdout);
        }
    }
    waitpid(pids[PID_UQCMP__STDERR], &statusStderrUqcmp, 0);
    if (WIFEXITED(statusStderrUqcmp)) {
	//exit statue will be here
	if (WEXITSTATUS(statusStderrUqcmp) == 0) {
    	    printf("Job %d: Stderr matches\n", jobNo);
	    fflush(stdout);
	    passCounter++;
        } else {
            printf("Job %d: Stderr differs\n", jobNo);
            fflush(stdout);
        }
    }
    waitpid(pids[PID_TESTPROG], &statusTestprog, 0);
    waitpid(pids[PID_DEMO], &statusDemo, 0);
    if (WEXITSTATUS(statusTestprog) == WEXITSTATUS(statusDemo)) {
        passCounter++;
    	printf("Job %d: Exit status matches\n", jobNo);
        fflush(stdout);
    } else {
        printf("Job %d: Exit status differs\n", jobNo);
    	fflush(stdout);
    }
    return (passCounter == 3);
}

/*
 * prallel_runer()
 * ------------------
 * This function is used to run all the jobs in parallel.
 * 
 * paraExist: the struct that contains all the flags and arguments
 * jobList: the struct that contains all the jobs
 * passCounter: the counter that counts the number of passed jobs
 * 
 * Return: the number of passed jobs
 */
int prallel_runer(CmdArg paraExist, JobsList jobList, int passCounter) {
    pid_t** uqcmpPid = (pid_t**)malloc(sizeof(pid_t*) * jobList.size);
    violatileJobSize = jobList.size;
    for (; jobRun < violatileJobSize; jobRun++) {
    	printf("Starting job %d\n", jobRun + 1);
    	fflush(stdout);
    	uqcmpPid[jobRun] = 
                job_runner(paraExist, jobList.job[jobRun], jobRun + 1);
    }
    sleep(2);
    for (int i = 0; i < violatileJobSize; i++) {
        kill(uqcmpPid[i][PID_TESTPROG], SIGKILL);
    	kill(uqcmpPid[i][PID_DEMO], SIGKILL);
        kill(uqcmpPid[i][PID_UQCMP_STDOUT], SIGKILL);
    	kill(uqcmpPid[i][PID_UQCMP__STDERR], SIGKILL);
    }
    for (int i = 0; i < violatileJobSize; i++) {
        (result_reporter(uqcmpPid[i], i+1))? passCounter++ : passCounter;
    	free(uqcmpPid[i]);
    }
    free(uqcmpPid);
    return passCounter;
}

/*
 * linear_runner()
 * --------------------
 * paraExist: commandline argument's struct
 * jobList: list of job
 * passCounter: counter of passed job
 * 
 * Returns: number of job that is passed 
 */
int linear_runner(CmdArg paraExist, JobsList jobList, int passCounter) {
    violatileJobSize = jobList.size;
    for (; jobRun < violatileJobSize; jobRun++) {
    	printf("Starting job %d\n", jobRun + 1);
    	fflush(stdout);
    	pid_t* uqcmpPid = job_runner(paraExist, 
                jobList.job[jobRun], jobRun + 1);
    	sleep(2);
    	//kill
    	kill(uqcmpPid[PID_TESTPROG], SIGKILL);
    	kill(uqcmpPid[PID_DEMO], SIGKILL);
    	kill(uqcmpPid[PID_UQCMP_STDOUT], SIGKILL);
    	kill(uqcmpPid[PID_UQCMP__STDERR], SIGKILL);
    	(result_reporter(uqcmpPid, jobRun+1))? passCounter++ : passCounter;
    	free(uqcmpPid);
    }
    return passCounter;
}

/*
 * handle_sigint()
 * --------------------
 * Handling SIGINT signal (crtl + C)
 * 
 * stop accepting more jobs by break the loop
 * but still run the current jobs
 * 
 * sig : signal number 
 */
void handle_sigint(int sig) {
    violatileJobSize = jobRun + 1;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, handle_sigint);
    CmdArg paraExist = pre_run_checking(argc, argv);

    FILE* jobFile = fopen(paraExist.jobfile, "r");
    if (jobFile == NULL) {
    	exit_code_three(paraExist.jobfile);
    }
    JobsList jobList = line_splitter(jobFile, paraExist);
    int passCounter = 0;
    if (paraExist.flag[PARALLEL] == 1) { // parallel run
        passCounter = prallel_runer(paraExist, jobList, passCounter);
    } else { // linear run
	passCounter = linear_runner(paraExist, jobList, passCounter);
    }
    if (violatileJobSize == -1) {
	violatileJobSize = jobList.size;
    }
    printf("testuqwordiply: %d out of %d tests passed\n", 
            passCounter, violatileJobSize);
    paraExist.jobfile = NULL;
    free(paraExist.jobfile);
    paraExist.testprogram = NULL;
    free(paraExist.testprogram);
    //free(testProgram); this already close!
    free(jobFile);
    return (violatileJobSize - passCounter) ? 1 : 0;
}
