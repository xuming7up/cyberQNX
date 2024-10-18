/******************************************************************************
 * Copyright 2024 The HyperLink Authors. All Rights Reserved.
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
#include <memory>

#include "cyber/component/component.h"
#include "cyber/examples/proto/examples.pb.h"
#include <iostream>
#include <time.h>
#ifndef __QNX__
#include <sys/syscall.h>
#endif
#include <unistd.h>
#include <thread>
#include <omp.h>

using apollo::cyber::Component;
using apollo::cyber::ComponentBase;
using apollo::cyber::examples::proto::Driver;

class ComponentCamera : public Component<Driver, Driver> {
 public:
  bool Init() override;
  bool Proc(const std::shared_ptr<Driver>& msg0,
            const std::shared_ptr<Driver>& msg1) override;

    void calculate_prime() {
        //test core 2.5-----------------------------------------------
        // AINFO << "inside calculate_prime";
        // int pid = getpid();
        // int tid = static_cast<int>(syscall(SYS_gettid));
        // AINFO << "pid = " << pid << ", tid = " << tid;
        // if (pid != tid) {
        //     std::string cmd{"sudo bash -c "};
        //     std::string internal_cmd{"\"echo "};
        //     internal_cmd.append(std::to_string(tid));
        //     std::string cpuset_cmd{" > /sys/fs/cgroup/cpuset/common/tasks\""};
        //     internal_cmd.append(cpuset_cmd);
        //     cmd.append(internal_cmd);
        //     AINFO << "cmd is " << cmd;
        //     system(cmd.c_str());

        //     cmd = std::string{"sudo bash -c "};
        //     internal_cmd = std::string{"\"echo "};
        //     internal_cmd.append(std::to_string(tid));
        //     std::string cpucontrol_cmd{" > /sys/fs/cgroup/cpu/camera/tasks\""};
        //     internal_cmd.append(cpucontrol_cmd);
        //     cmd.append(internal_cmd);
        //     AINFO << "cmd is " << cmd;
        //     system(cmd.c_str());

        // }
        // test 2.5 core----------------------------------------
        double runTime;
        auto start = std::chrono::system_clock::now();
        int num = 1,primes = 0;

        int limit = 1000000;

    #pragma omp parallel for schedule(dynamic) reduction(+ : primes)
        for (num = 1; num <= limit; num++) { 
            int i = 2; 
            while(i <= num) { 
                if(num % i == 0)
                    break;
                i++; 
            }
            if(i == num)
                primes++;
    //      printf("%d prime numbers calculated\n",primes);
        }
        auto end = std::chrono::system_clock::now();
        runTime = std::chrono::duration<double, std::milli>(end - start).count();
        AINFO << "This machine calculated all " << primes << " prime numbers under " << limit << " in " << runTime <<"seconds";
    }

};
CYBER_REGISTER_COMPONENT(ComponentCamera)



// how to build this demo 
// g++ -o test cpu_test.cpp -lgomp -lpthread
