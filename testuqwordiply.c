#include <stdlib.h>
//#include <unistd.h>
#include <stdio.h>
//#include <sys/wait.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
//#include <csse2310a3.h>

#define QUIET 0
#define PARALLEL 1
#define MAX_JOBS 5000

typedef struct cmdArg{
	char flag[4];
	char* testprogram;
	char* jobfile; 
} cmdArg;

typedef struct job{
	char* inputFileName;
	char** args;
	int numArgs;
	int lineNum;
	FILE* inputFile;
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
			if (!strcmp(argv[i], "--quiet") && !para_exist.flag[QUIET]) {
				printf("in 1\n");
				para_exist.flag[QUIET] = 1; // mark that quiet is set
				continue;
			}
			if (!strcmp(argv[i], "--parallel") && !para_exist.flag[PARALLEL]) {
				printf("why it in 2\n");
				para_exist.flag[PARALLEL] = 1; // mark that parallel is set
				continue;
			}
				exit_code_two();
		} else {
			if(i == argc - 2) { // second last argument
				para_exist.testprogram = argv[i];
				continue;
			} else if (i == argc - 1) { // last argument
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
		if(line[0] == '#' || line == ""){
			continue; // skip it if it is a comment line or empty line
		}
		char** splitedResult = split_line(line,',');
        if (splitedResult[0] == NULL || splitedResult[1] == NULL) {
            // edit later!
			free(splitedResult);
            exit_code_four(para_exist.jobfile, lineNum);
        }
		jobList.job[i].inputFileName = splitedResult[0];
        char* noBrackets = (char*)malloc(sizeof(char)*(strlen(splitedResult[1]) - 2));
        strncpy(noBrackets,splitedResult[1] +1, strlen(splitedResult[1]) - 2);
		jobList.job[i].args = 
				split_space_not_quote(noBrackets, &jobList.job[i].numArgs);
		jobList.job[i].lineNum = lineNum;
		if((jobList.job[i].inputFile = fopen(jobList.job[i].inputFileName, "r"))) {
			exit_code_five(jobList.job[i].inputFileName, para_exist.jobfile, lineNum);
		}
		jobList.size++;
        i++;

		//noBrackets can not be free internally...
		free(splitedResult);
	}
	
	free(line);
	return jobList;
}

int main(int argc, char* argv[]) {
	cmdArg para_exist = pre_run_checking(argc, argv);
	
	FILE *testProgram = fopen(para_exist.testprogram, "r");
	if (testProgram == NULL) {
		exit_code_three(para_exist.testprogram);
	}
	FILE *jobFile = fopen(para_exist.jobfile, "r");
	
	if (jobFile == NULL) {
		exit_code_three(para_exist.jobfile);
	}

	job_List jobList = line_splitter(jobFile, para_exist);
	
	for (int i = 0; i < jobList.size; i++) {
		printf("there are %d argument(s) on line %d: ",jobList.job[i].numArgs, jobList.job[i].lineNum );
        for (int j = 0 ;j < jobList.job[i].numArgs; j++) {
            printf("[%s] ", jobList.job[i].args[j]);
        }
        printf("\n");
    }

	para_exist.jobfile = NULL;
	free(para_exist.jobfile);
	para_exist.testprogram = NULL;
	free(para_exist.testprogram);
	free(testProgram);
	free(jobFile);
	return 0;
}