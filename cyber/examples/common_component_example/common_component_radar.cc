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
#include "cyber/examples/common_component_example/common_component_radar.h"

bool ComponentRadar::Init() {
  AINFO << "Commontest component init";
  int pid = getpid();
	AINFO << "pid is " << pid;
  std::thread t1{&ComponentRadar::calculate_prime, this};
  t1.detach();
  calculate_prime();
  return true;
}

bool ComponentRadar::Proc(const std::shared_ptr<Driver>& msg0,
                                 const std::shared_ptr<Driver>& msg1) {
  AINFO << "Start common component Proc [" << msg0->msg_id() << "] ["
        << msg1->msg_id() << "]";
  return true;
}
