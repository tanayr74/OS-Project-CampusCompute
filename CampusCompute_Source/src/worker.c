#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "common.h"

void run_worker(const char *mode)
{
    pid_t child;
    int status;
    int simulate_failure = strcmp(mode, "fail") == 0;

    printf("Worker test: %s\n", simulate_failure ? "controlled simulated failure" : "normal completion");
    fflush(stdout);
    child = fork();
    if (child == -1) {
        perror("fork");
        return;
    }
    if (child == 0) {
        printf("Child PID %ld: running simulated work.\n", (long)getpid());
        fflush(stdout);
        if (simulate_failure) {
            fprintf(stderr, "Child PID %ld: simulated error; exiting with status 7.\n", (long)getpid());
            _exit(7);
        }
        _exit(0);
    }

    printf("Parent PID %ld: waiting for child PID %ld.\n", (long)getpid(), (long)child);
    if (waitpid(child, &status, 0) == -1) {
        perror("waitpid");
        return;
    }
    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        printf("Parent: child exited normally with status %d.\n", exit_code);
        if (exit_code == 0) puts("Worker result: success.");
        else puts("Worker result: simulated failure detected (non-zero exit status).");
    } else if (WIFSIGNALED(status)) {
        printf("Parent: child terminated by signal %d.\n", WTERMSIG(status));
    } else {
        puts("Parent: child ended in an unclassified state.");
    }
}
