#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <semaphore.h>
#include "common.h"

#define MESSAGE_SIZE 256

typedef struct {
    char message[MESSAGE_SIZE];
} SharedData;

static int wait_for_semaphore(sem_t *sem)
{
    int result;
    do {
        result = sem_wait(sem);
    } while (result == -1 && errno == EINTR);
    return result;
}

void run_ipc(void)
{
    char shm_name[64];
    char ready_name[64];
    char done_name[64];
    int fd = -1;
    SharedData *shared = MAP_FAILED;
    sem_t *ready = SEM_FAILED;
    sem_t *done = SEM_FAILED;
    pid_t child;
    int status = 0;
    int shm_created = 0, ready_created = 0, done_created = 0;

    (void)snprintf(shm_name, sizeof(shm_name), "/campuscompute_shm_%ld", (long)getpid());
    (void)snprintf(ready_name, sizeof(ready_name), "/campuscompute_ready_%ld", (long)getpid());
    (void)snprintf(done_name, sizeof(done_name), "/campuscompute_done_%ld", (long)getpid());

    fd = shm_open(shm_name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd == -1) { perror("shm_open"); goto cleanup; }
    shm_created = 1;
    if (ftruncate(fd, (off_t)sizeof(SharedData)) == -1) { perror("ftruncate"); goto cleanup; }
    shared = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shared == MAP_FAILED) { perror("mmap"); goto cleanup; }
    memset(shared, 0, sizeof(*shared));

    ready = sem_open(ready_name, O_CREAT | O_EXCL, 0600, 0);
    if (ready == SEM_FAILED) { perror("sem_open ready"); goto cleanup; }
    ready_created = 1;
    done = sem_open(done_name, O_CREAT | O_EXCL, 0600, 0);
    if (done == SEM_FAILED) { perror("sem_open done"); goto cleanup; }
    done_created = 1;

    puts("IPC: created POSIX shared memory and two named semaphores.");
    fflush(stdout);
    child = fork();
    if (child == -1) { perror("fork"); goto cleanup; }
    if (child == 0) {
        if (wait_for_semaphore(ready) == -1) {
            perror("child sem_wait");
            _exit(10);
        }
        printf("Child: read from shared memory: '%s'\n", shared->message);
        fflush(stdout);
        (void)snprintf(shared->message, sizeof(shared->message),
                       "ACK from child process %ld", (long)getpid());
        if (sem_post(done) == -1) {
            perror("child sem_post");
            _exit(11);
        }
        _exit(0);
    }

    (void)snprintf(shared->message, sizeof(shared->message),
                   "Hello from parent process %ld", (long)getpid());
    printf("Parent: wrote to shared memory: '%s'\n", shared->message);
    if (sem_post(ready) == -1) { perror("parent sem_post ready"); }
    else if (wait_for_semaphore(done) == -1) { perror("parent sem_wait done"); }
    else { printf("Parent: received shared-memory response: '%s'\n", shared->message); }

    if (waitpid(child, &status, 0) == -1) perror("waitpid");
    else if (WIFEXITED(status)) printf("Parent: child exited with status %d.\n", WEXITSTATUS(status));
    else if (WIFSIGNALED(status)) printf("Parent: child terminated by signal %d.\n", WTERMSIG(status));

cleanup:
    if (shared != MAP_FAILED && munmap(shared, sizeof(SharedData)) == -1) perror("munmap");
    if (fd != -1 && close(fd) == -1) perror("close shared-memory fd");
    if (ready != SEM_FAILED && sem_close(ready) == -1) perror("sem_close ready");
    if (done != SEM_FAILED && sem_close(done) == -1) perror("sem_close done");
    if (ready_created && sem_unlink(ready_name) == -1) perror("sem_unlink ready");
    if (done_created && sem_unlink(done_name) == -1) perror("sem_unlink done");
    if (shm_created && shm_unlink(shm_name) == -1) perror("shm_unlink");
    puts("IPC cleanup: unmapped memory, closed handles, and unlinked program-created IPC names.");
}
