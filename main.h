#ifndef MAIN_HEADER

#define MAIN_HEADER "main.h"
#define MAX_CMD_LEN 2048
#define MAX_ARGS 512
#define MAX_ARG_LEN 64
#define MAX_FILENAME_LEN 255

void getInput(char*);
int parseCommand(char*, char*, char[][MAX_ARG_LEN + 1],  char*, char*, int*, int);
void clearMainBuffers(char*, char*, char[][MAX_ARG_LEN + 1], char*, char*);
void execCd(char*, int*, int*, int);
void execStatus(int, int);
char* expand(char[], char*, char[]);
void execNonBuiltin(char[], char[][MAX_ARG_LEN + 1], char*, char*, int, int*, int*);
void handleStopSignal(int);
void inputRedirect(char*);
void outputRedirect(char*);

#endif