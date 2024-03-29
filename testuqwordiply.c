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
#define SLEEP_TIME_SEC 2
#define SLEEP_TIME_NANO 0

/*
 * flag of interupt signal
 * 0 = not interupted
 * 1 = interupted
 */
int interuptionFlag = 0;

/*
 * struct for command line argument
 * flag[QUIET] = 1 if --quiet is set
 * flag[PARALLEL] = 1 if --parallel is set
 * flag[]
 * testprogram = name of test program
 * jobfile = name of job file
 */
typedef struct CmdArg {
    char flag[4]; // may change
    char* testprogram;
    char* jobfile; 
} CmdArg;

/*
 * struct for job
 * inputFileName = name of input file
 * inputFile = pointer to input file
 * args = array of arguments
 * numArgs = number of arguments
 * lineNum = line number of job in job file
 */
typedef struct Job {
    char* inputFileName;
    FILE* inputFile;
    char** args;
    int numArgs;
    int lineNum;
} Job;

/*
 * struct for job list (array of jobs)
 * job = array of jobs
 * size = number of jobs
 */
typedef struct JobsList {
    Job job[MAX_JOBS];
    int size;
} JobsList;

/*
 * exit_code_two()
 * --------------------
 * exit with code 2 and its message of testuqwordiply usage
 */
void exit_code_two() {
    fprintf(stderr, "Usage: testuqwordiply [--quiet] [--parallel]"
            " testprogram jobfile\n");
    exit(2);
}

/*
 * exit_code_three()
 * --------------------
 * exit with code 3 and the message: unable to open job file
 */
void exit_code_three(char* filename) {
    fprintf(stderr, "testuqwordiply: Unable to open job file"
            " \"%s\"\n", filename);
    exit(3);
}

/*
 * exit_code_four()
 * --------------------
 * exit with code 4 and its message of syntax error
 */
void exit_code_four(char* filename, int lineNum) {
    fprintf(stderr, "testuqwordiply: syntax error on line %d "
            "of \"%s\"\n", lineNum, filename);
    exit(4);
}

/*
 * exit_code_five()
 * --------------------
 * exit with code 5 and its message of unable to open file
 */
void exit_code_five(char* inputFileName, char* filename, int lineNum) {
    fprintf(stderr, "testuqwordiply: unable to open file \"%s\" specified on"
            " line %d of \"%s\"\n", inputFileName, lineNum, filename);
    exit(5);
}

/*
 * exit_code_six()
 * --------------------
 * exit with code 6 and its message of no jobs found
 */
void exit_code_six(char* filename) {
    fprintf(stderr, "testuqwordiply: no jobs found in \"%s\"\n", filename);
    exit(6);
}

/*
 * pre_run_checking()
 * --------------------
 * check the validity of command line argument
 * and pre-defind all the arguments
 * 
 * argc: number of command line argument
 * argv: array of command line argument
 * 
 * Returns: a struct the contains the information of command line argument
 *        (see CmdArg struct)
 */
CmdArg pre_run_checking(int argc, char* argv[]) {
    CmdArg paraExist;
    paraExist.flag[QUIET] = 0;
    paraExist.flag[PARALLEL] = 0;
    paraExist.testprogram = NULL;
    paraExist.jobfile = NULL;
    if (argc < 3) {
        exit_code_two();
    }
    for (int i = 1; i < argc; i++) { // check all the arguments
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

/*
 * comma_counter()
 * --------------------
 * count the number of comma in the line
 * 
 * line: pointer to the string that will be used to count
 * 
 * Returns: 1 if there is only one comma in the line
 */
int comma_counter(char* line) {
    int commaNum = 0;
    for (int count = 0; line[count]; count++) {
	if (line[count] == ',') {
	    if (commaNum < 1) {
                commaNum++;
	    } else {
                return 0;
	    }    
	}
    }
    return 1;
}

/*
 * line_splitter()
 * --------------------
 * split the line in job file into job
 * 
 * jobFile: pointer to job file
 * paraExist: a struct that contains the information of command line argument
 *       (see CmdArg struct)
 * 
 * Returns: a struct that contains the information of all the jobs
 */
JobsList line_splitter(FILE* jobFile, CmdArg paraExist) {
    JobsList jobList;
    jobList.size = 0; // initialize the size of job list
    char* line = NULL;
    for (int i = 0, lineNum = 1;
            (line = read_line(jobFile)) != NULL; lineNum++) {
	if (line[0] == '#' || !strcmp(line,"")) {
	    continue; // skip it if it is a comment line or empty line
	}
    if (!comma_counter(line)) {
        exit_code_four(paraExist.jobfile, lineNum);
    }
	char** splitedResult = split_line(line,',');
        if (splitedResult[0] == NULL || splitedResult[1] == NULL
		|| !strcmp(splitedResult[0],"")) {
	    free(splitedResult);
            exit_code_four(paraExist.jobfile, lineNum);
        }
	jobList.job[i].inputFileName = splitedResult[0];
	//6 Lines below help to relocate all element to its position + 1
	char** jobArgs = split_space_not_quote(splitedResult[1], 
                &jobList.job[i].numArgs);
	jobList.job[i].args =
            (char**)malloc(sizeof(char*)*(++jobList.job[i].numArgs + 1));
	jobList.job[i].args[0] = jobList.job[i].inputFileName;
	for (int argLoc = 0; argLoc < jobList.job[i].numArgs -1; argLoc++) {
	    jobList.job[i].args[argLoc +1] = jobArgs[argLoc];
	}
    //set last element to NULL
	jobList.job[i].args[jobList.job[i].numArgs] = (char*)0; 	
	jobList.job[i].lineNum = lineNum;
	if ((jobList.job[i].inputFile = 
                    fopen(jobList.job[i].inputFileName, "r")) == NULL) {
	    exit_code_five(jobList.job[i].inputFileName, 
                    paraExist.jobfile, lineNum);
	}
	jobList.size++;
        i++;
        free(splitedResult);
    }
    free(line);
    if (jobList.size == 0) {
        exit_code_six(paraExist.jobfile);
    }
    return jobList;
}

/*
 * test_prog_runner()
 * --------------------
 * run the test program
 * 
 * paraExist: a struct that contains the information of command line argument
 *      (see CmdArg struct)
 * job: a struct that contains the information of a job
 * stdoutTestProgToUqcmpFd: pointer to the file descriptor that will be used
 *     to write the stdout of test program to uqcmp
 * stderrTestProgToUqcmpFd: pointer to the file descriptor that will be used
 *    to write the stderr of test program to uqcmp
 * 
 * Returns: the pid of the test program
 */
pid_t test_prog_runner(CmdArg paraExist, Job job, 
        int* stdoutTestProgToUqcmpFd, int* stderrTestProgToUqcmpFd) {
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
    return pidTestProg;
}

/*
 * demo_prog_runner()
 * --------------------
 * run the demo-uqwordiply program
 * 
 * paraExist: a struct that contains the information of command line argument
 *      (see CmdArg struct)
 * job: a struct that contains the information of a job
 * stdoutDemoProgToUqcmpFd: pointer to the file descriptor that will be used
 *      to write the stdout of demo-uqwordiply program to uqcmp
 * stderrDemoProgToUqcmpFd: pointer to the file descriptor that will be used
 *      to write the stderr of demo-uqwordiply program to uqcmp
 * 
 * Returns: the pid of the demo-uqwordiply program
 */
pid_t demo_uqwordiply_runner(CmdArg paraExist, Job job, 
        int* stdoutDemoProgToUqcmpFd, int* stderrDemoProgToUqcmpFd) {
    pid_t pidDemo = fork();
    if (pidDemo == 0) {
    	// demo child process
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
    return pidDemo;
}

/*
 * stdout_uqcmp_runner()
 * --------------------
 * run the uqcmp program for stdout
 * 
 * paraExist: a struct that contains the information of command line argument
 *    (see CmdArg struct)
 *      job: a struct that contains the information of a job
 * stdoutTestProgToUqcmpFd: pointer to the file descriptor that will be used
 *      to write the stdout of test program to uqcmp
 * stdoutDemoProgToUqcmpFd: pointer to the file descriptor that will be used
 *      to write the stdout of demo-uqwordiply program to uqcmp
 * stderrTestProgToUqcmpFd: pointer to the file descriptor that will be used
 *      to write the stderr of test program to uqcmp
 * stderrDemoProgToUqcmpFd: pointer to the file descriptor that will be used
 *      to write the stderr of demo-uqwordiply program to uqcmp
 * 
 * Returns: the pid of the uqcmp program
 */
pid_t stdout_uqcmp_runner(int jobNo, CmdArg paraExist, Job job, 
        int* stdoutTestProgToUqcmpFd, int* stdoutDemoProgToUqcmpFd, 
        int* stderrTestProgToUqcmpFd, int* stderrDemoProgToUqcmpFd) {
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
    return pidUqcmpStdout;
}

/*
 * stderr_uqcmp_runner()
 * --------------------
 * run the uqcmp program for stderr
 * 
 * paraExist: a struct that contains the information of command line argument
 *   (see CmdArg struct)
 * job: a struct that contains the information of a job
 * stderrTestProgToUqcmpFd: pointer to the file descriptor that will be used
 *   to write the stderr of test program to uqcmp
 * stderrDemoProgToUqcmpFd: pointer to the file descriptor that will be used
 *  to write the stderr of demo-uqwordiply program to uqcmp
 * 
 * Returns: the pid of the uqcmp program
 */
pid_t stderr_uqcmp_runner(int jobNo, CmdArg paraExist, Job job, 
        int* stderrTestProgToUqcmpFd, int* stderrDemoProgToUqcmpFd) {
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
    return pidUqcmpStderr;
}

/*
 * job_runner()
 * --------------------
 * run the given job
 * 
 * paraExist: a struct that contains the information of command line argument
 *      (see CmdArg struct)
 * job: a struct that contains the information of a job
 * jobNo: the index of that job
 * 
 * Returns: a pointer to all pids created by this job
*/
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
    pid_t pidTestProg = test_prog_runner(paraExist, job,
            stdoutTestProgToUqcmpFd, stderrTestProgToUqcmpFd);
    
    if (pipe(stdoutDemoProgToUqcmpFd) == -1) {
        perror("Creating pipe stdout from Demo to uqcmp error!");
        exit(1);
    }
    if (pipe(stderrDemoProgToUqcmpFd) == -1) {
    	perror("Creating pipe stderr from Demo to uqcmp error!");
        exit(1);
    }

    pid_t pidDemo = demo_uqwordiply_runner(paraExist, job, 
            stdoutDemoProgToUqcmpFd, stderrDemoProgToUqcmpFd);
    pid_t pidUqcmpStdout = stdout_uqcmp_runner(jobNo, paraExist, job, 
            stdoutTestProgToUqcmpFd, stdoutDemoProgToUqcmpFd, 
            stderrTestProgToUqcmpFd, stderrDemoProgToUqcmpFd);
    
    close(stdoutTestProgToUqcmpFd[READ_END]);
    close(stdoutTestProgToUqcmpFd[WRITE_END]);
    close(stdoutDemoProgToUqcmpFd[READ_END]);
    close(stdoutDemoProgToUqcmpFd[WRITE_END]);
    pid_t pidUqcmpStderr = stderr_uqcmp_runner(jobNo, paraExist, job, 
            stderrTestProgToUqcmpFd, stderrDemoProgToUqcmpFd);
    
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
 * print_match()
 * --------------------
 * print the match message
 * 
 * jobNo: the index of that job
 * type: the type of the match message
 *     should be stdout/stderr/Exit status
 * 
 * Returns: void
 */
void print_match(int jobNo, char* type) {
    printf("Job %d: %s matches\n", jobNo, type);
    fflush(stdout);
}

/*
 * print_differ()
 * --------------------
 * print the differ message
 * 
 * jobNo: the index of that job
 * type: the type of the differ message
 *    should be stdout/stderr/Exit status
 * 
 * Returns: void
 */
void print_differ(int jobNo, char* type) {
    // type 
    printf("Job %d: %s differs\n", jobNo, type);
    fflush(stdout);
}

/*
 * print_error()
 * --------------------
 * print the error message
 * 
 * jobNo: the index of that job
 * 
 * Returns: void
 */
void print_error(int jobNo) {
    printf("Job %d : Unable to execute test\n",jobNo);
    fflush(stdout);
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
 * Returns: true if all the result matches, false otherwise
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
    	if (WEXITSTATUS(statusStdoutUqcmp) == 0) {
            print_match(jobNo, "Stdout");
    	    passCounter++;
	} else if (WEXITSTATUS(statusStdoutUqcmp) == 99) {
            print_error(jobNo);
            return false;
        } else {
            print_differ(jobNo, "Stdout");
        }
    }
    waitpid(pids[PID_UQCMP__STDERR], &statusStderrUqcmp, 0);
    if (WIFEXITED(statusStderrUqcmp)) {
	if (WEXITSTATUS(statusStderrUqcmp) == 0) {
            print_match(jobNo, "Stderr");
	    passCounter++;
        } else if (WEXITSTATUS(statusStderrUqcmp) == 99) {
            print_error(jobNo);
            return false;
        } else {
            print_differ(jobNo, "Stderr");
        }
    }
    waitpid(pids[PID_TESTPROG], &statusTestprog, 0);
    waitpid(pids[PID_DEMO], &statusDemo, 0);
    if (WEXITSTATUS(statusTestprog) == WEXITSTATUS(statusDemo)) {
        passCounter++;
    	print_match(jobNo, "Exit status");
    } else if (WEXITSTATUS(statusTestprog) == 99 || 
            WEXITSTATUS(statusDemo) == 99) {
        print_error(jobNo);
        return false;
    } else {
        print_differ(jobNo, "Exit status");
    }
    return (passCounter == 3);
}

/*
 * process_killer()
 * ----------------
 * This function will kill all the processes 
 * that are still running in one job.
 * 
 * uqcmpPid: the array of pid of test program, demo-uqwordiply and uqcmp
 * 
 * Returns: true if all the processes are killed, false otherwise
 */
bool process_killer(pid_t* uqcmpPid) {
    if (kill(uqcmpPid[PID_TESTPROG], SIGKILL)
            || kill(uqcmpPid[PID_DEMO], SIGKILL)
            || kill(uqcmpPid[PID_UQCMP_STDOUT], SIGKILL)
            || kill(uqcmpPid[PID_UQCMP__STDERR], SIGKILL)) {
        return false;
    } else {
        return true;
    }
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
 * Returns: the number of passed jobs
 */
int prallel_runer(CmdArg paraExist, JobsList* jobList, int passCounter) {
    pid_t** uqcmpPid = (pid_t**)malloc(sizeof(pid_t*) * jobList->size);
    int ignore[jobList->size]; //ignore the jobs that are not executable
    int jobRun = 0;
    for (; jobRun < jobList->size; jobRun++) {
    	printf("Starting job %d\n", jobRun + 1);
    	fflush(stdout);
    	uqcmpPid[jobRun] = 
                job_runner(paraExist, jobList->job[jobRun], jobRun + 1);
    }
    struct timespec start = {SLEEP_TIME_SEC, SLEEP_TIME_NANO};
    struct timespec remaining = {0, 0};
    int finishedSleep = nanosleep(&start, &remaining);
        while(true) {
            if (interuptionFlag == 1) {
                finishedSleep = nanosleep(&remaining, &remaining);
                jobList->size = jobRun + 1;
                interuptionFlag = 0;
            }
            if (finishedSleep == 0) {
                break;
            }
        }
    for (int i = 0; i < jobList->size; i++) {
        if (!process_killer(uqcmpPid[i])) {
            printf("Job %d : Unable to execute test\n", i + 1);
            fflush(stdout);
            ignore[i] = 1;
        }
        ignore[i] = 0;
    }
    for (int i = 0; i < jobList->size; i++) {
        if (!ignore[i]) {
            (result_reporter(uqcmpPid[i], i + 1))? passCounter++ : passCounter;
    	    free(uqcmpPid[i]);
        }
    }
    free(uqcmpPid);
    return passCounter;
}

/*
 * linear_runner()
 * --------------------
 * paraExist: commandline argument's struct
 * jobList: pointer tolist of job
 * passCounter: counter of passed job
 * 
 * Returns: number of job that is passed 
 */
int linear_runner(CmdArg paraExist, JobsList* jobList, int passCounter) {
    for (int jobRun = 0; jobRun < jobList->size; jobRun++) {
    	printf("Starting job %d\n", jobRun + 1);
    	fflush(stdout);
    	pid_t* uqcmpPid = job_runner(paraExist, 
                jobList->job[jobRun], jobRun + 1);
        struct timespec start = {SLEEP_TIME_SEC, SLEEP_TIME_NANO};
        struct timespec remaining = {0, 0};
        int finishedSleep = nanosleep(&start, &remaining);
        while(true) {
            if (interuptionFlag == 1) {
                finishedSleep = nanosleep(&remaining, &remaining);
                jobList->size = jobRun + 1;
                interuptionFlag = 0;
            }
            if (finishedSleep == 0) {
                break;
            }
        }
    	//kill
    	if (!process_killer(uqcmpPid)) {
            printf("One or more processes can't be killed\n");
            fflush(stdout);
            continue; //skip the rest of the loop
        }
    	(result_reporter(uqcmpPid, jobRun + 1))? passCounter++ : passCounter;
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
    interuptionFlag = 1;
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
        passCounter = prallel_runer(paraExist, &jobList, passCounter);
    } else { // linear run
	passCounter = linear_runner(paraExist, &jobList, passCounter);
    }
    printf("testuqwordiply: %d out of %d tests passed\n", 
            passCounter, jobList.size);
    paraExist.jobfile = NULL;
    free(paraExist.jobfile);
    paraExist.testprogram = NULL;
    free(paraExist.testprogram);
    free(jobFile);
    return (jobList.size - passCounter) ? 1 : 0;
}
