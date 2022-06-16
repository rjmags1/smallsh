#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/types.h>
#include "main.h"

int fgStatSigNo;
int bgAllowed = 1;

int main() {
    
    char input[MAX_CMD_LEN + 1];
    char cmd[MAX_CMD_LEN + 1];
    char args[MAX_ARGS][MAX_ARG_LEN + 1];
    char infile[MAX_FILENAME_LEN + 1];
    char outfile[MAX_FILENAME_LEN + 1];
    int commentOrNewline = 0;
    int bg = 0;
    int bgStatSig;
    pid_t bgSpawnPid;
    int status = -1;
    int termSignal = -1;
    
    clearMainBuffers(input, cmd, args, infile, outfile);

    // shell process ignore SIGINT signal
    struct sigaction sigactIgnore = {0};
    sigactIgnore.sa_handler = SIG_IGN;
    sigfillset(&sigactIgnore.sa_mask);
    sigactIgnore.sa_flags = 0;
    sigaction(SIGINT, &sigactIgnore, NULL);

    // shell process toggle foreground only mode on SIGTSTP signal
    struct sigaction sigactStopSignal = {0};
    sigactStopSignal.sa_handler = &handleStopSignal;
    sigfillset(&sigactStopSignal.sa_mask);
    sigactStopSignal.sa_flags = 0;
    sigaction(SIGTSTP, &sigactStopSignal, NULL);
    
    pid_t ppid = getpid();
    while (1) { // main loop
        
        // get raw command string and parse it
        getInput(input);
        commentOrNewline = parseCommand(
            input, cmd, args, infile, outfile, &bg, (int) ppid);

        // execute command
        if (!commentOrNewline)  {
            if (strcmp(cmd, "exit") == 0) break;
            else if (strcmp(cmd, "cd") == 0) execCd(args[0], &status, &termSignal, bg);
            else if (strcmp(cmd, "status") == 0) execStatus(status, termSignal);
            else execNonBuiltin(cmd, args, infile, outfile, bg, &status, &termSignal);
        }

        // report on any finished background commands before next shell prompt
        bgSpawnPid = waitpid(-1, &bgStatSig, WNOHANG);
        while (bgSpawnPid > 0) {
            if (WIFEXITED(bgStatSig)) {
                printf("background pid %d is done: exited with value %d\n", 
                    bgSpawnPid, WEXITSTATUS(bgStatSig));
            }
            else if (WIFSIGNALED(bgStatSig)) {
                printf("background pid %d is done: terminated by signal %d\n", 
                    bgSpawnPid, WTERMSIG(bgStatSig));
            }
            fflush(stdout);
            bgSpawnPid = waitpid(-1, &bgStatSig, WNOHANG);
        }
        
        clearMainBuffers(input, cmd, args, infile, outfile);
    }

    return 0;
}


void getInput(char* input) {
    
    printf(": ");
    fflush(stdout);
    fgets(input, MAX_CMD_LEN + 1, stdin);
    input[strlen(input) - 1] = '\0'; // get rid of terminating newline
}


int parseCommand(char* input, char* cmd, char args[][MAX_ARG_LEN + 1], 
    char* infile, char* outfile, int* bg, int ppid) {
    
    char* tokenBuffer;
    char* delim = " ";
    char expanded[MAX_ARG_LEN];
    int i = 0, j = 0, k = 0;
    int tokenLen;
    char sppid[10]; // stringified ppid (parent process id AKA shell process id)
    sprintf(sppid, "%d", ppid);

    tokenBuffer = strtok(input, delim);
    // return 1 for whitespace only command
    if (!tokenBuffer || tokenBuffer[0] == '#') return 1; 

    // parse command string with strtok, expanding $$ into ppid,
    //    checking for input/output redirection, and
    //    if it should be run in the background
    while (tokenBuffer) {
        expand(expanded, tokenBuffer, sppid);

        if (cmd[0] == '\0') strcpy(cmd, expanded);
        else if (strcmp(expanded, "<") == 0) strcpy(
            infile, expand(expanded, strtok(NULL, delim), sppid));
        else if (strcmp(expanded, ">") == 0) strcpy(
                outfile, expand(expanded, strtok(NULL, delim), sppid));
        else strcpy(args[j++], expanded);

        tokenBuffer = strtok(NULL, delim);
    }
    *bg = strcmp(args[j - 1], "&") == 0 ? 1 : 0;
    if (*bg) args[j - 1][0] = '\0';
    
    return 0;
}


void clearMainBuffers(char* input, char* cmd, char args[][MAX_ARG_LEN + 1], 
    char* infile, char* outfile) {
    
    memset(input, '\0', MAX_CMD_LEN + 1);
    memset(cmd, '\0', MAX_CMD_LEN + 1);
    memset(infile, '\0', MAX_FILENAME_LEN + 1);
    memset(outfile, '\0', MAX_FILENAME_LEN + 1);
    for (int i = 0; i < MAX_ARGS; i++) args[i][0] = '\0';
}


void execCd(char* destDir, int* status, int* termSignal, int bg) {
    // builtin implementation of cd command
    
    if (destDir[0] == '\0') destDir = getenv("HOME");
    int result = chdir(destDir);
    if (!bg) {
        *status = result;
        *termSignal = -1;
    }
}


void execStatus(int status, int termSignal) {
    // builtin implementation of status command
    
    if (status > -1) printf("exit value %d\n", status);
    else if (termSignal > -1) printf("terminated by signal %d\n", termSignal);
    fflush(stdout);
}


char* expand(char expanded[], char* tokenBuffer, char sppid[]) {
    // expand instances of $$ in raw command string into shell process id
    
    int i = 0, k = 0;
    int tokenLen = strlen(tokenBuffer);
        while (i < tokenLen) {
            if (i < tokenLen - 1 && tokenBuffer[i] == '$' && tokenBuffer[i + 1] == '$') {
                i++;
                int n = 0;
                while (sppid[n]) expanded[k++] = sppid[n++];
            }
            else expanded[k++] = tokenBuffer[i];
            i++;
        }
        expanded[k] = '\0';

    return expanded;
}


void execNonBuiltin(char cmd[], char args[][MAX_ARG_LEN + 1], char* infile, 
    char* outfile, int bg, int* status, int* termSignal) {
    // executes any non-builtin commands using fork and execvp sys calls

    char *argv[MAX_ARGS];
    argv[0] = cmd;
    int i = 0;
    while (++i < MAX_ARGS) { // fill argv with cmd and args for execvp
        argv[i] = args[i - 1][0] == '\0' ? NULL : args[i - 1];
    }
    
    pid_t spawnPid = fork();
    switch(spawnPid) {

        // spawned child process executes this
        case 0: 
            // do any necessary input/output redirection
            if (infile[0] == '\0' && bg && bgAllowed) inputRedirect("/dev/null");
            else if (infile[0] != '\0') inputRedirect(infile);
            if (outfile[0] == '\0' && bg && bgAllowed) outputRedirect("/dev/null");
            else if (outfile[0] != '\0') outputRedirect(outfile);

            // spawned child processes should not ignore SIGINT
            struct sigaction sigactDefault = {0};
            sigactDefault.sa_handler = SIG_DFL;
            sigfillset(&sigactDefault.sa_mask);
            sigactDefault.sa_flags = 0;
            sigaction(SIGINT, &sigactDefault, NULL);

            // now we can execvp the non-builtin command
            if (execvp(cmd, argv) == -1) {
                perror("exec failed");
                exit(1); // spawned child exit with status 1 on exec failure
            }

        // shell process executes this on fork failure
        case -1: 
            perror("fork failed");
            exit(1); 

        // shell process executes this on fork success
        default:
            if (bg && bgAllowed) {
                // report background child pid to user if not in foreground-only mode
                //    and user specified background execution for this command
                printf("background pid is %d\n", spawnPid);
                fflush(stdout);
            }

            // suspend parent process execution if command not running in background
            waitpid(spawnPid, &fgStatSigNo, bg && bgAllowed ? WNOHANG : 0);
            
            if (!bg) {
                // update status and termSignal based on outcome of most 
                // recent foreground command
                *status = WIFEXITED(fgStatSigNo) ? WEXITSTATUS(fgStatSigNo) : -1;
                *termSignal = WIFSIGNALED(fgStatSigNo) ? WTERMSIG(fgStatSigNo) : -1;
                
                if (*termSignal > -1)  execStatus(*status, *termSignal);
            }
    }
}


void handleStopSignal(int _) {
    // toggle foreground only mode and notify user
    
    if (bgAllowed) {
        bgAllowed = 0;
        char* fgOnlyMsg = "\nForeground only mode turned on. & now ignored.\n";
        write(STDOUT_FILENO, fgOnlyMsg, 47);
    }
    else if (!bgAllowed) {
        bgAllowed = 1;
        char* fgOnlyMsg = "\nForeground only mode turned off. & now respected.\n";
        write(STDOUT_FILENO, fgOnlyMsg, 50);
    }
    fflush(stdout);
}


void inputRedirect(char* redirected) {
    
    FILE* fp = fopen(redirected, "r");
    if (fp == NULL) {
        dprintf(STDERR_FILENO, "couldn't open %s for input redirection\n", redirected);
        exit(1);
    }
    if (dup2(fileno(fp), STDIN_FILENO) == -1) {
        fclose(fp);
        dprintf(STDERR_FILENO, "couldn't redirect %s to stdin\n", redirected);
        exit(1);
    }
    fclose(fp);
}


void outputRedirect(char* redirected) {
    
    FILE* fp = fopen(redirected, "w");
    if (fp == NULL) {
        dprintf(STDERR_FILENO, "couldn't open %s for output redirection\n", redirected);
        exit(1);
    }
    if (dup2(fileno(fp), STDOUT_FILENO) == -1) {
        fclose(fp);
        dprintf(STDERR_FILENO, "couldn't redirect stdout to %s\n", redirected);
        exit(1);
    }
    fclose(fp);
}