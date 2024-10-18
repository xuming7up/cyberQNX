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
* -2. add QNX Support for pthread_setaffinity_np
* 
*/

#ifndef CYBER_SCHEDULER_COMMON_PIN_THREAD_H_
#define CYBER_SCHEDULER_COMMON_PIN_THREAD_H_

#include <string>
#include <thread>
#include <vector>

#include "cyber/common/log.h"

namespace apollo {
namespace cyber {
namespace scheduler {

#ifdef __QNX__

typedef struct cpu_set {
  uint32_t count;
} cpu_set_t;

#define CPU_ZERO(cpusetp) \
  ({ memset(cpusetp, '\0', sizeof(cpu_set_t)); })

#define CPU_SET(cpu, cpusetp) \
  ({ (cpusetp)->count |= (1 << cpu); })

#define CPU_ISSET(cpu, cpusetp)  \
  ({ (cpusetp)->count & (1 << cpu); })

int pthread_setaffinity_np(pthread_t thread, size_t cpusetsize, const cpu_set_t *cpuset);

#endif

void ParseCpuset(const std::string& str, std::vector<int>* cpuset);

void SetSchedAffinity(std::thread* thread, const std::vector<int>& cpus,
                      const std::string& affinity, int cpu_id = -1);

void SetSchedAffinity(pthread_t p_tid, const std::vector<int>& cpus,
                      const std::string& affinity, int cpu_id = -1);

void SetSchedPolicy(std::thread* thread, std::string spolicy,
                    int sched_priority, pid_t tid = -1);

void SetSchedPolicy(pthread_t p_tid, std::string spolicy,
                    int sched_priority, pid_t tid = -1);

}  // namespace scheduler
}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_SCHEDULER_COMMON_PIN_THREAD_H_
