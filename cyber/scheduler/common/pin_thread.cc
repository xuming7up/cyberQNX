/******************************************************************************
 * Copyright 2019 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

/*
* @Desc: Change History
* @Date: 2024-10-15
* @Version: 1.0.0
* @Feature List:
* -1. add new function: 
*     void SetSchedAffinity(pthread_t p_tid, const std::vector<int>& cpus, 
*                           const std::string& affinity, int cpu_id = -1);
*     void SetSchedPolicy(pthread_t p_tid, std::string spolicy, 
*                           int sched_priority, pid_t tid = -1);
* -2. add QNX Support for pthread_setaffinity_np function.
* 
*/

#include "cyber/scheduler/common/pin_thread.h"

#include <sched.h>
#include <sys/resource.h>
#ifdef __QNX__
#include <sys/syspage.h>
#include <sys/neutrino.h>
#include <sys/procmgr.h>
#endif

namespace apollo {
namespace cyber {
namespace scheduler {

/* QNX */
#ifdef __QNX__
  
int pthread_setaffinity_np(pthread_t thread, size_t cpusetsize, const cpu_set_t *cpuset) {
  unsigned num_elements = 0;
  int *rsizep, masksize_bytes, size;
  unsigned *rmaskp, *imaskp;
  void *my_data;

  /* Determine the number of array elements required to hold
   * the runmasks, based on the number of CPUs in the system.*/
  num_elements = RMSK_SIZE(_syspage_ptr->num_cpu);

  /* Determine the size of the runmask, in bytes. */
  masksize_bytes = num_elements * sizeof(unsigned);

  /* Allocate memory for the data structure that we'll pass 
   * to ThreadCtl(). We need space for an integer (the number 
   * of elements in each mask array) and the two masks
   * (runmask and inherit mask). 
   * 
   * struct _thread_runmask {
   * int size;
   * unsigned runmask[size];
   * unsigned inherit_mask[size];
   * };
   */

  size = sizeof(int) + 2 * masksize_bytes;
  if ((my_data = malloc(size)) == NULL) {
    /* Not enough memory. ... */
    AERROR << "Not enough memory for thread runmask!!!";
    return -1;
  } else {
    memset(my_data, 0x00, size);

    /* Set up pointers to the "members" of the structure. */
    rsizep = (int *)my_data;
    rmaskp = (unsigned *)(rsizep + 1);
    imaskp = rmaskp + num_elements;

    /* Set the size. */
    *rsizep = num_elements;

    /* Set the runmask. */
    uint32_t cpus = cpuset->count;
    int cpui = 0;
    while (cpus) {
      if (cpus & 0x1) {
        RMSK_SET(cpui, rmaskp);
        RMSK_SET(cpui, imaskp);
      }
      cpus = cpus >> 1;
      cpui++;
    }

    procmgr_ability(0, PROCMGR_ADN_ROOT|PROCMGR_AOP_ALLOW|PROCMGR_AID_XPROCESS_DEBUG, PROCMGR_AID_EOL);
    if (ThreadCtlExt(0, thread, _NTO_TCTL_RUNMASK_GET_AND_SET_INHERIT, my_data) == -1) {
      /* Something went wrong. */
      AERROR << "Something went wrong, when set thread affinity (ThreadCtlExt)";
      return -1;
    }
  }
  return 0;
}

#endif

void ParseCpuset(const std::string& str, std::vector<int>* cpuset) {
  std::vector<std::string> lines;
  std::stringstream ss(str);
  std::string l;
  while (getline(ss, l, ',')) {
    lines.push_back(l);
  }
  for (auto line : lines) {
    std::stringstream ss(line);
    std::vector<std::string> range;
    while (getline(ss, l, '-')) {
      range.push_back(l);
    }
    if (range.size() == 1) {
      cpuset->push_back(std::stoi(range[0]));
    } else if (range.size() == 2) {
      for (int i = std::stoi(range[0]), e = std::stoi(range[1]); i <= e; i++) {
        cpuset->push_back(i);
      }
    } else {
      ADEBUG << "Parsing cpuset format error.";
      exit(0);
    }
  }
}

void SetSchedAffinity(std::thread* thread, const std::vector<int>& cpus,
                      const std::string& affinity, int cpu_id) {
  SetSchedAffinity(thread->native_handle(),cpus,affinity,cpu_id);
}

void SetSchedAffinity(pthread_t p_tid, const std::vector<int>& cpus,
                      const std::string& affinity, int cpu_id) {
  cpu_set_t set;
  CPU_ZERO(&set);

  if (cpus.size()) {
    if (!affinity.compare("range")) {
      for (const auto cpu : cpus) {
        CPU_SET(cpu, &set);
      }
      pthread_setaffinity_np(p_tid, sizeof(set), &set);
      AINFO << "thread:" << p_tid << " set range affinity";
    } else if (!affinity.compare("1to1")) {
      if (cpu_id == -1 || (uint32_t)cpu_id >= cpus.size()) {
        return;
      }
      CPU_SET(cpus[cpu_id], &set);
      pthread_setaffinity_np(p_tid, sizeof(set), &set);
      AINFO << "thread:" << p_tid << " set 1to1 affinity";
    }
  }
}

void SetSchedPolicy(std::thread* thread, std::string spolicy,
                    int sched_priority, pid_t tid) {
    SetSchedPolicy(thread->native_handle(),spolicy,sched_priority,tid);
}

void SetSchedPolicy(pthread_t p_tid, std::string spolicy,
                    int sched_priority, pid_t tid) {
  struct sched_param sp;
  int policy;

  memset(reinterpret_cast<void*>(&sp), 0, sizeof(sp));
  sp.sched_priority = sched_priority;

  if (!spolicy.compare("SCHED_FIFO")) {
    policy = SCHED_FIFO;
    pthread_setschedparam(p_tid, policy, &sp);
    AINFO << "thread " << p_tid << " set sched_policy: " << spolicy;
  } else if (!spolicy.compare("SCHED_RR")) {
    policy = SCHED_RR;
    pthread_setschedparam(p_tid, policy, &sp);
    AINFO << "thread " << p_tid << " set sched_policy: " << spolicy;
  } else if (!spolicy.compare("SCHED_OTHER")) {
    setpriority(PRIO_PROCESS, tid, sched_priority);
    AINFO << "thread " << tid << " set sched_policy: " << spolicy;
  }
}

}  // namespace scheduler
}  // namespace cyber
}  // namespace apollo
