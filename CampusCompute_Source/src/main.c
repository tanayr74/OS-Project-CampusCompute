#include <stdio.h>
#include <string.h>
#include "common.h"

void print_usage(const char *program_name)
{
    printf("CampusCompute - OS concepts demonstration\n\n");
    printf("Usage:\n");
    printf("  %s help\n", program_name);
    printf("  %s schedule fcfs|rr|priority|compare [jobs_file]\n", program_name);
    printf("  %s logger\n", program_name);
    printf("  %s ipc\n", program_name);
    printf("  %s banker safe|unsafe\n", program_name);
    printf("  %s worker normal|fail\n", program_name);
    printf("  %s limit\n", program_name);
    printf("  %s integrated\n", program_name);
    printf("\nNotes:\n");
    printf("  Scheduling uses the same built-in sample jobs for repeatable tests.\n");
    printf("  'worker fail' is a controlled simulated failure, not an OS crash.\n");
}

static int is_one_of(const char *value, const char *a, const char *b,
                     const char *c, const char *d)
{
    return strcmp(value, a) == 0 || strcmp(value, b) == 0 ||
           (c != NULL && strcmp(value, c) == 0) ||
           (d != NULL && strcmp(value, d) == 0);
}

static void run_integrated(void)
{
    puts("=== Integrated CampusCompute demonstration ===");
    (void)run_schedule("compare", NULL);
    run_logger();
    run_ipc();
    run_banker("safe");
    run_banker("unsafe");
    run_worker("normal");
    run_worker("fail");
    run_resource_limit();
    puts("=== Integrated demonstration finished ===");
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "help") == 0) {
        print_usage(argv[0]);
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "integrated") == 0) {
        run_integrated();
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "logger") == 0) {
        run_logger();
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "ipc") == 0) {
        run_ipc();
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "limit") == 0) {
        run_resource_limit();
        return 0;
    }
    if ((argc == 3 || argc == 4) && strcmp(argv[1], "schedule") == 0 &&
        is_one_of(argv[2], "fcfs", "rr", "priority", "compare")) {
        return run_schedule(argv[2], argc == 4 ? argv[3] : NULL) == 0 ? 0 : 1;
    }
    if (argc == 3 && strcmp(argv[1], "banker") == 0 &&
        is_one_of(argv[2], "safe", "unsafe", NULL, NULL)) {
        run_banker(argv[2]);
        return 0;
    }
    if (argc == 3 && strcmp(argv[1], "worker") == 0 &&
        is_one_of(argv[2], "normal", "fail", NULL, NULL)) {
        run_worker(argv[2]);
        return 0;
    }

    fprintf(stderr, "Error: invalid command or arguments.\n\n");
    print_usage(argv[0]);
    return 2;
}
