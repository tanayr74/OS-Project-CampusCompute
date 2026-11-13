#include <stdio.h>
#include <string.h>
#include "common.h"

#define PROCESS_COUNT 5
#define RESOURCE_COUNT 3

typedef struct {
    int available[RESOURCE_COUNT];
    int allocation[PROCESS_COUNT][RESOURCE_COUNT];
    int maximum[PROCESS_COUNT][RESOURCE_COUNT];
} BankData;

static void print_vector(const char *label, const int values[RESOURCE_COUNT])
{
    printf("%s", label);
    for (int j = 0; j < RESOURCE_COUNT; ++j) printf(" %d", values[j]);
    putchar('\n');
}

void run_banker(const char *mode)
{
    BankData data = {
        .available = {3, 3, 2},
        .allocation = {
            {0, 1, 0},
            {2, 0, 0},
            {3, 0, 2},
            {2, 1, 1},
            {0, 0, 2}
        },
        .maximum = {
            {7, 5, 3},
            {3, 2, 2},
            {9, 0, 2},
            {2, 2, 2},
            {4, 3, 3}
        }
    };
    int need[PROCESS_COUNT][RESOURCE_COUNT];
    int work[RESOURCE_COUNT];
    int finished[PROCESS_COUNT] = {0};
    int sequence[PROCESS_COUNT];
    int sequence_count = 0;
    int progress;

    if (strcmp(mode, "unsafe") == 0) {
        data.available[0] = 0;
        data.available[1] = 0;
        data.available[2] = 0;
    }

    puts("Banker's safety algorithm (Need = Maximum - Allocation)");
    printf("State under test: %s\n", mode);
    print_vector("Available:", data.available);
    puts("Need matrix:");
    for (int i = 0; i < PROCESS_COUNT; ++i) {
        printf("P%d:", i);
        for (int j = 0; j < RESOURCE_COUNT; ++j) {
            need[i][j] = data.maximum[i][j] - data.allocation[i][j];
            printf(" %d", need[i][j]);
        }
        putchar('\n');
    }

    memcpy(work, data.available, sizeof(work));
    do {
        progress = 0;
        for (int i = 0; i < PROCESS_COUNT; ++i) {
            int can_finish = !finished[i];
            for (int j = 0; j < RESOURCE_COUNT && can_finish; ++j) {
                if (need[i][j] > work[j]) can_finish = 0;
            }
            if (can_finish) {
                for (int j = 0; j < RESOURCE_COUNT; ++j)
                    work[j] += data.allocation[i][j];
                finished[i] = 1;
                sequence[sequence_count++] = i;
                progress = 1;
            }
        }
    } while (progress);

    if (sequence_count == PROCESS_COUNT) {
        printf("Result: SAFE. Safe sequence: ");
        for (int i = 0; i < sequence_count; ++i)
            printf("P%d%s", sequence[i], (i + 1 == sequence_count) ? "\n" : " -> ");
    } else {
        printf("Result: UNSAFE. No complete safe sequence found; finished %d of %d processes.\n",
               sequence_count, PROCESS_COUNT);
    }
}
