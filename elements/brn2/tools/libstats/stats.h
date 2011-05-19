#include<stdlib.h> 
#include <sys/types.h>
#include<stdio.h>
#include<strings.h>
#include<string.h>

struct pstat{
    long unsigned int utime_ticks;
    long int cutime_ticks;
    long unsigned int stime_ticks;
    long int cstime_ticks;

    long unsigned int cpu_total_time;
};

//return 0 on success, -1 on error
int get_usage(const pid_t pid, struct pstat* result);
void calc_cpu_usage(struct pstat* cur_usage, struct pstat* last_usage, float* ucpu_usage, float* scpu_usage);
