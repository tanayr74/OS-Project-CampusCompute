#ifndef CAMPUSCOMPUTE_COMMON_H
#define CAMPUSCOMPUTE_COMMON_H

void print_usage(const char *program_name);
int run_schedule(const char *policy, const char *jobs_file);
void run_logger(void);
void run_ipc(void);
void run_banker(const char *mode);
void run_worker(const char *mode);
void run_resource_limit(void);

#endif
