#ifndef CPUSTATS_HH
#define CPUSTATS_HH

#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>

CLICK_DECLS

struct pstat{
  long unsigned int utime_ticks;
  long int cutime_ticks;
  long unsigned int stime_ticks;
  long int cstime_ticks;

  long unsigned int cpu_total_time;
};

class CPUStats
{

 public:

  CPUStats() {};
  ~CPUStats() {};

  //return 0 on success, -1 on error
  static int get_usage(const pid_t pid, struct pstat* result);
#if CLICK_USERLEVEL
  static void calc_cpu_usage(struct pstat* cur_usage, struct pstat* last_usage, float* ucpu_usage, float* scpu_usage, float* cpu_usage);
#endif
  static void calc_cpu_usage_int(struct pstat* cur_usage, struct pstat* last_usage, uint32_t* ucpu_usage, uint32_t* scpu_usage, uint32_t* cpu_usage );
};

CLICK_ENDDECLS
#endif
