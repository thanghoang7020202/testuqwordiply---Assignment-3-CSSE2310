#include <stdlib.h>
//#include <unistd.h>
#include <stdio.h>
//#include <sys/wait.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
//#include <csse2010a3.h>

#define QUIET 0
#define PARALLEL 1
#define TESTPROGRAM 2
#define JOBFILE 3

char* pre_run_checking(int argc, char* argv[]) {
	//printf("argc = %d\n", argc);
	char para_exist[4] = {0, 0, 0 ,0}; 
	// 0 for quiet, 1 for parallel, 2 for testprogram, 3 for jobfile
	if(argc < 3) {
		exit(2);
	}
	for (int i = 1 ; i < argc ; i++) {
		//printf("argv[%d] = %s\n", i, argv[i]);
		if (strstr(argv[i], "--") != NULL) {
			if (!strcmp(argv[i], "--quiet") && !para_exist[QUIET]) {
				printf("in 1\n");
				para_exist[QUIET] = 1; // mark that quiet is set
				continue;
			}
			if (!strcmp(argv[i], "--parallel") && !para_exist[PARALLEL]) {
				printf("why it in 2\n");
				para_exist[PARALLEL] = 1; // mark that parallel is set
				continue;
			}
				exit(2);
		} else {
			if(i == argc - 2) { // second last argument
				para_exist[TESTPROGRAM] = 1;
				continue;
			} else if (i == argc - 1) { // last argument
				para_exist[JOBFILE] = 1;
				continue;
			}
			exit(2);
		}
	}
	return para_exist;
}

int main(int argc, char* argv[]) {
	char para_exist[] = pre_run_checking(argc, argv);
	
	FILE *testProgram = fopen(argv[argc - 2], "r");
	if (testProgram == NULL) {
		perror("testuqwordiply: Unable to open job file \"%s\"", argv[argc - 2]);
		exit(3);
	}
	FILE *jobFile = fopen(argv[argc - 1], "r");
	if (jobFile == NULL) {
		perror("testuqwordiply: Unable to open job file \"%s\"", argv[argc - 1]);
		exit(3);
	}

	close(testProgram);
	close(jobFile);
	return 0;
}

