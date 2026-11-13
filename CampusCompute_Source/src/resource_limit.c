#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "common.h"

void run_resource_limit(void)
{
    const char *test_file = "resource_limit_test.tmp";
    pid_t child;
    int status;

    puts("Resource limit demonstration: RLIMIT_FSIZE = 1024 bytes in a child process.");
    fflush(stdout);
    child = fork();
    if (child == -1) {
        perror("fork");
        return;
    }
    if (child == 0) {
        struct rlimit limit;
        char buffer[2048];
        ssize_t written;
        int fd;

        (void)signal(SIGXFSZ, SIG_IGN);
        limit.rlim_cur = 1024;
        limit.rlim_max = 1024;
        if (setrlimit(RLIMIT_FSIZE, &limit) == -1) {
            perror("child setrlimit");
            _exit(20);
        }
        memset(buffer, 'C', sizeof(buffer));
        fd = open(test_file, O_CREAT | O_TRUNC | O_WRONLY, 0600);
        if (fd == -1) {
            perror("child open test file");
            _exit(21);
        }
        written = write(fd, buffer, sizeof(buffer));
        if (written == -1) {
            perror("child write (limit reached)");
            (void)close(fd);
            _exit(22);
        }
        printf("Child: requested %zu bytes; write() accepted %zd bytes under the limit.\n",
               sizeof(buffer), written);
        fflush(stdout);
        if (close(fd) == -1) perror("child close test file");
        _exit(0);
    }

    if (waitpid(child, &status, 0) == -1) perror("waitpid resource-limit child");
    else if (WIFEXITED(status)) printf("Parent: resource-limit test child exit status %d.\n", WEXITSTATUS(status));
    else if (WIFSIGNALED(status)) printf("Parent: resource-limit test child terminated by signal %d.\n", WTERMSIG(status));
    if (unlink(test_file) == -1 && errno != ENOENT) perror("unlink resource-limit test file");
    puts("Resource-limit test file removed; the limit was confined to the child process.");
}
