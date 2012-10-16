//Last modified: 04/04/11 12:34:52(CEST) by Fabian Holler
#include <click/config.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>

#include "cpustats.hh"

CLICK_DECLS

//return 0 on success, -1 on error
int
CPUStats::get_usage(const pid_t pid, struct pstat* result){
  int i;
  //convert  pid to string
  char pid_s[20];
  snprintf(pid_s, sizeof(pid_s), "%d", pid);
  char stat_filepath[30] = "/proc/";
  strncat(stat_filepath, pid_s, sizeof(stat_filepath) - strlen(stat_filepath) -1);
  strncat(stat_filepath, "/stat", sizeof(stat_filepath) - strlen(stat_filepath) -1);

  //open /proc/pid/stat
  FILE *fpstat = fopen(stat_filepath, "r");
  if(fpstat == NULL){
      printf("FOPEN ERROR pid stat %s:\n", stat_filepath);
      return -1;
  }

  //open /proc/stat
  FILE *fstat = fopen("/proc/stat", "r");
  if(fstat == NULL){
      printf("FOPEN ERROR");
      fclose(fpstat);
      return -1;
  }
  bzero(result, sizeof(struct pstat));

  //read values from /proc/pid/stat
  if(fscanf(fpstat, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu %ld %ld", &result->utime_ticks, &result->stime_ticks, &result->cutime_ticks, &result->cstime_ticks) == EOF){
      fclose(fpstat);
      fclose(fstat);
      return -1;
  }
  fclose(fpstat);

  //read+calc cpu total time from /proc/stat, on linux 2.6.35-23 x86_64 the cpu row has 10values could differ on different architectures :/
  long unsigned int cpu_time[10];
  bzero(cpu_time, sizeof(cpu_time));
  if(fscanf(fstat, "%*s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu", &cpu_time[0], &cpu_time[1], &cpu_time[2], &cpu_time[3], &cpu_time[4], &cpu_time[5], &cpu_time[6], &cpu_time[7], &cpu_time[8], &cpu_time[9]) == EOF){
      fclose(fstat);
      return -1;
  }
  fclose(fstat);

  for(i=0; i < 10;i++){
      result->cpu_total_time += cpu_time[i];
  }

  return 0;
}

/*
* calculates the actual CPU usage(cur_usage - last_usage) in percent
* cur_usage, last_usage: both last measured get_usage() results
* ucpu_usage, scpu_usage: result parameters: user and sys cpu usage in %
*/
#if CLICK_USERLEVEL
void
CPUStats::calc_cpu_usage(struct pstat* cur_usage, struct pstat* last_usage, float* ucpu_usage, float* scpu_usage, float* cpu_usage){
  *ucpu_usage = 100 * (((cur_usage->utime_ticks + cur_usage->cutime_ticks) - (last_usage->utime_ticks + last_usage->cutime_ticks)) / (float)(cur_usage->cpu_total_time - last_usage->cpu_total_time));
  *scpu_usage = 100 * (((cur_usage->stime_ticks + cur_usage->cstime_ticks) - (last_usage->stime_ticks + last_usage->cstime_ticks)) / (float)(cur_usage->cpu_total_time - last_usage->cpu_total_time));
  *cpu_usage = *ucpu_usage + *scpu_usage;
}
#endif

void
CPUStats::calc_cpu_usage_int(struct pstat* cur_usage, struct pstat* last_usage,
                             uint32_t* ucpu_usage, uint32_t* scpu_usage, uint32_t* cpu_usage, uint32_t accuracy_factor ) {
  uint32_t time_diff = (uint32_t)(cur_usage->cpu_total_time - last_usage->cpu_total_time);
  *ucpu_usage = (accuracy_factor * 100 * ((cur_usage->utime_ticks + cur_usage->cutime_ticks) -
                                         (last_usage->utime_ticks + last_usage->cutime_ticks)));
  *scpu_usage = (accuracy_factor * 100 * ((cur_usage->stime_ticks + cur_usage->cstime_ticks) -
                                         (last_usage->stime_ticks + last_usage->cstime_ticks)));
  *cpu_usage = (*ucpu_usage + *scpu_usage) / time_diff;
  *ucpu_usage /= time_diff;
  *scpu_usage /= time_diff;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
ELEMENT_PROVIDES(CPUStats)
