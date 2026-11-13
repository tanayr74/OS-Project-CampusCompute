#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include "common.h"

#define MAX_JOBS 8
#define MAX_SEGMENTS 128

typedef enum {
    JOB_NEW = 0,
    JOB_READY,
    JOB_RUNNING,
    JOB_COMPLETED
} JobState;

typedef struct {
    int id;
    int arrival;
    int burst;
    int priority;
    int remaining;
    int start;
    int completion;
    JobState state;
} Job;

typedef struct {
    int job_id;
    int start;
    int end;
} Segment;

static const Job sample_jobs[] = {
    {1, 0, 5, 2, 5, -1, -1, JOB_NEW},
    {2, 1, 3, 1, 3, -1, -1, JOB_NEW},
    {3, 2, 8, 3, 8, -1, -1, JOB_NEW},
    {4, 3, 6, 2, 6, -1, -1, JOB_NEW}
};

static const char *state_name(JobState state)
{
    switch (state) {
        case JOB_NEW: return "NEW";
        case JOB_READY: return "READY";
        case JOB_RUNNING: return "RUNNING";
        case JOB_COMPLETED: return "COMPLETED";
        default: return "UNKNOWN";
    }
}

static int load_jobs(const char *path, Job jobs[], int *count)
{
    if (path == NULL) {
        *count = (int)(sizeof(sample_jobs) / sizeof(sample_jobs[0]));
        for (int i = 0; i < *count; ++i) jobs[i] = sample_jobs[i];
        return 0;
    }

    FILE *file = fopen(path, "r");
    char line[256];
    int line_number = 0;
    *count = 0;
    if (file == NULL) {
        fprintf(stderr, "Scheduling input error: cannot open '%s': %s\n", path, strerror(errno));
        return -1;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        int id, arrival, burst, priority;
        char extra;
        int fields;
        ++line_number;
        fields = sscanf(line, " %d %d %d %d %c", &id, &arrival, &burst, &priority, &extra);
        if (fields == EOF || line[0] == '\0') continue;
        {
            char *first = line;
            while (*first == ' ' || *first == '\t' || *first == '\r' || *first == '\n') ++first;
            if (*first == '\0' || *first == '#') continue;
        }
        if (fields != 4) {
            fprintf(stderr, "Scheduling input error: line %d must contain exactly: id arrival burst priority\n", line_number);
            fclose(file);
            return -1;
        }
        if (id <= 0 || arrival < 0 || burst <= 0 || priority < 0) {
            fprintf(stderr, "Scheduling input error: line %d has invalid values (id > 0, arrival >= 0, burst > 0, priority >= 0 required).\n", line_number);
            fclose(file);
            return -1;
        }
        if (*count >= MAX_JOBS) {
            fprintf(stderr, "Scheduling input error: at most %d jobs are supported.\n", MAX_JOBS);
            fclose(file);
            return -1;
        }
        for (int i = 0; i < *count; ++i) {
            if (jobs[i].id == id) {
                fprintf(stderr, "Scheduling input error: duplicate job ID %d on line %d.\n", id, line_number);
                fclose(file);
                return -1;
            }
        }
        jobs[*count] = (Job){id, arrival, burst, priority, burst, -1, -1, JOB_NEW};
        ++(*count);
    }

    if (ferror(file)) {
        fprintf(stderr, "Scheduling input error: failed while reading '%s'.\n", path);
        fclose(file);
        return -1;
    }
    fclose(file);
    if (*count == 0) {
        fprintf(stderr, "Scheduling input error: '%s' contains no job records.\n", path);
        return -1;
    }
    return 0;
}

static void reset_jobs(const Job source[], Job jobs[], int count)
{
    for (int i = 0; i < count; ++i) {
        jobs[i] = source[i];
        jobs[i].remaining = jobs[i].burst;
        jobs[i].start = -1;
        jobs[i].completion = -1;
        jobs[i].state = JOB_NEW;
    }
}

static void print_results(const char *label, const Job jobs[], int count,
                          const Segment segments[], int segment_count)
{
    double total_tat = 0.0;
    double total_wt = 0.0;

    printf("\n--- %s ---\n", label);
    printf("Timeline: ");
    if (segment_count == 0) {
        printf("(no execution)\n");
    } else {
        for (int i = 0; i < segment_count; ++i) {
            printf("J%d[%d-%d]%s", segments[i].job_id, segments[i].start,
                   segments[i].end, i + 1 == segment_count ? "\n" : " -> ");
        }
    }
    printf("\n%-5s %-7s %-6s %-8s %-5s %-5s %-5s %-10s\n",
           "Job", "Arrival", "Burst", "Priority", "CT", "TAT", "WT", "Status");
    for (int i = 0; i < count; ++i) {
        int tat = jobs[i].completion - jobs[i].arrival;
        int wt = tat - jobs[i].burst;
        total_tat += tat;
        total_wt += wt;
        printf("J%-4d %-7d %-6d %-8d %-5d %-5d %-5d %-10s\n",
               jobs[i].id, jobs[i].arrival, jobs[i].burst, jobs[i].priority,
               jobs[i].completion, tat, wt, state_name(jobs[i].state));
    }
    printf("Average turnaround time: %.2f\n", total_tat / count);
    printf("Average waiting time:     %.2f\n", total_wt / count);
}

static void add_segment(Segment segments[], int *segment_count, int job_id,
                        int start, int end)
{
    if (*segment_count < MAX_SEGMENTS) {
        segments[*segment_count].job_id = job_id;
        segments[*segment_count].start = start;
        segments[*segment_count].end = end;
        ++(*segment_count);
    }
}

static void schedule_fcfs(Job jobs[], int count, Segment segments[], int *segment_count)
{
    int order[MAX_JOBS];
    int time = 0;
    *segment_count = 0;

    for (int i = 0; i < count; ++i) order[i] = i;
    for (int i = 0; i < count; ++i) {
        for (int j = i + 1; j < count; ++j) {
            int ai = jobs[order[i]].arrival;
            int aj = jobs[order[j]].arrival;
            int idi = jobs[order[i]].id;
            int idj = jobs[order[j]].id;
            if (aj < ai || (aj == ai && idj < idi)) {
                int tmp = order[i]; order[i] = order[j]; order[j] = tmp;
            }
        }
    }

    for (int p = 0; p < count; ++p) {
        Job *job = &jobs[order[p]];
        if (time < job->arrival) time = job->arrival;
        job->state = JOB_RUNNING;
        job->start = time;
        add_segment(segments, segment_count, job->id, time, time + job->burst);
        time += job->burst;
        job->completion = time;
        job->remaining = 0;
        job->state = JOB_COMPLETED;
    }
}

static void schedule_priority(Job jobs[], int count, Segment segments[], int *segment_count)
{
    int time = 0;
    int completed = 0;
    *segment_count = 0;

    while (completed < count) {
        int chosen = -1;
        for (int i = 0; i < count; ++i) {
            if (jobs[i].state == JOB_COMPLETED || jobs[i].arrival > time) continue;
            if (chosen == -1 || jobs[i].priority < jobs[chosen].priority ||
                (jobs[i].priority == jobs[chosen].priority &&
                 (jobs[i].arrival < jobs[chosen].arrival ||
                  (jobs[i].arrival == jobs[chosen].arrival && jobs[i].id < jobs[chosen].id)))) {
                chosen = i;
            }
        }
        if (chosen == -1) {
            int next_arrival = INT_MAX;
            for (int i = 0; i < count; ++i) {
                if (jobs[i].state != JOB_COMPLETED && jobs[i].arrival < next_arrival)
                    next_arrival = jobs[i].arrival;
            }
            if (next_arrival == INT_MAX) break;
            time = next_arrival;
            continue;
        }
        Job *job = &jobs[chosen];
        job->state = JOB_RUNNING;
        job->start = time;
        add_segment(segments, segment_count, job->id, time, time + job->burst);
        time += job->burst;
        job->completion = time;
        job->remaining = 0;
        job->state = JOB_COMPLETED;
        ++completed;
    }
}

static void enqueue_new_arrivals(Job jobs[], int count, int time, int queue[],
                                 int *tail, int *queue_count, int queued[])
{
    for (int i = 0; i < count; ++i) {
        if (jobs[i].state == JOB_NEW && jobs[i].remaining > 0 &&
            jobs[i].arrival <= time && !queued[i] && *queue_count < MAX_JOBS) {
            queue[*tail] = i;
            *tail = (*tail + 1) % MAX_JOBS;
            ++(*queue_count);
            queued[i] = 1;
            jobs[i].state = JOB_READY;
        }
    }
}

static void schedule_rr(Job jobs[], int count, int quantum,
                        Segment segments[], int *segment_count)
{
    int queue[MAX_JOBS];
    int queued[MAX_JOBS] = {0};
    int head = 0, tail = 0, queue_count = 0;
    int time = 0, completed = 0;
    *segment_count = 0;

    while (completed < count) {
        enqueue_new_arrivals(jobs, count, time, queue, &tail, &queue_count, queued);
        if (queue_count == 0) {
            int next_arrival = INT_MAX;
            for (int i = 0; i < count; ++i) {
                if (jobs[i].state == JOB_NEW && jobs[i].remaining > 0 &&
                    jobs[i].arrival < next_arrival) next_arrival = jobs[i].arrival;
            }
            if (next_arrival == INT_MAX) break;
            if (time < next_arrival) time = next_arrival;
            enqueue_new_arrivals(jobs, count, time, queue, &tail, &queue_count, queued);
            if (queue_count == 0) continue;
        }

        int index = queue[head];
        head = (head + 1) % MAX_JOBS;
        --queue_count;
        queued[index] = 0;
        Job *job = &jobs[index];
        job->state = JOB_RUNNING;
        if (job->start < 0) job->start = time;
        int run_for = job->remaining < quantum ? job->remaining : quantum;
        int start = time;
        time += run_for;
        job->remaining -= run_for;
        add_segment(segments, segment_count, job->id, start, time);

        enqueue_new_arrivals(jobs, count, time, queue, &tail, &queue_count, queued);
        if (job->remaining == 0) {
            job->completion = time;
            job->state = JOB_COMPLETED;
            ++completed;
        } else {
            job->state = JOB_READY;
            if (queue_count < MAX_JOBS) {
                queue[tail] = index;
                tail = (tail + 1) % MAX_JOBS;
                ++queue_count;
                queued[index] = 1;
            }
        }
    }
}

static void run_one(const char *policy, const Job source[], int count)
{
    Job jobs[MAX_JOBS];
    Segment segments[MAX_SEGMENTS];
    int segment_count = 0;
    reset_jobs(source, jobs, count);

    if (strcmp(policy, "fcfs") == 0) {
        schedule_fcfs(jobs, count, segments, &segment_count);
        print_results("First-Come, First-Served (FCFS)", jobs, count, segments, segment_count);
    } else if (strcmp(policy, "rr") == 0) {
        const int quantum = 2;
        printf("Round Robin time quantum: %d\n", quantum);
        schedule_rr(jobs, count, quantum, segments, &segment_count);
        print_results("Round Robin (RR)", jobs, count, segments, segment_count);
    } else if (strcmp(policy, "priority") == 0) {
        puts("Priority rule: lower number means higher priority; ties use earlier arrival, then lower job ID.");
        schedule_priority(jobs, count, segments, &segment_count);
        print_results("Non-preemptive Priority Scheduling", jobs, count, segments, segment_count);
    }
}

int run_schedule(const char *policy, const char *jobs_file)
{
    Job jobs[MAX_JOBS];
    int count = 0;
    if (load_jobs(jobs_file, jobs, &count) != 0) return -1;

    printf("Scheduling input: %s (%d jobs)\n",
           jobs_file == NULL ? "built-in repeatable sample" : jobs_file, count);
    puts("Jobs: ID Arrival Burst Priority");
    for (int i = 0; i < count; ++i)
        printf("     J%d %d %d %d\n", jobs[i].id, jobs[i].arrival, jobs[i].burst, jobs[i].priority);

    if (strcmp(policy, "compare") == 0) {
        run_one("fcfs", jobs, count);
        run_one("rr", jobs, count);
        run_one("priority", jobs, count);
    } else if (strcmp(policy, "fcfs") == 0 || strcmp(policy, "rr") == 0 ||
               strcmp(policy, "priority") == 0) {
        run_one(policy, jobs, count);
    } else {
        fprintf(stderr, "Scheduling error: unsupported policy '%s'.\n", policy);
        return -1;
    }
    return 0;
}
