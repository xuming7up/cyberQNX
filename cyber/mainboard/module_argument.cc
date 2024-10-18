/******************************************************************************
 * Copyright 2018 The Apollo Authors. All Rights Reserved.
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

#include "cyber/mainboard/module_argument.h"

#include <getopt.h>
#include <libgen.h>

using apollo::cyber::common::GlobalData;
using apollo::cyber::common::GetAbsolutePath;
using apollo::cyber::common::GetProtoFromFile;
using apollo::cyber::common::PathExists;
using apollo::cyber::common::WorkRoot;

namespace apollo {
namespace cyber {
namespace mainboard {

void ModuleArgument::DisplayUsage() {
  AINFO << "Usage: \n    " << binary_name_ << " [OPTION]...\n"
        << "Description: \n"
        << "    -h, --help : help information \n"
        << "    -d, --dag_conf=CONFIG_FILE : module dag config file\n"
        << "    -p, --process_group=process_group: the process "
           "namespace for running this module, default in manager process\n"
        << "    -s, --sched_name=sched_name: sched policy "
           "conf for hole process, sched_name should be conf in cyber.pb.conf\n"
        << "Example:\n"
        << "    " << binary_name_ << " -h\n"
        << "    " << binary_name_ << " -d dag_conf_file1 -d dag_conf_file2 "
        << "-p process_group -s sched_name\n";
}

bool ModuleArgument::CheckFile(const std::string& conf_path) {
  auto cfg_file = GetAbsolutePath(WorkRoot(), conf_path);
  apollo::cyber::proto::CyberConfig cfg;
  if (PathExists(cfg_file) && GetProtoFromFile(cfg_file, &cfg)) {
    return true;
  }
  return false;
}

void ModuleArgument::GetCgroup(const std::string& conf_path, uint8_t cgroup_type, std::string& cgroup) {
  auto cfg_file = GetAbsolutePath(WorkRoot(), conf_path);
  apollo::cyber::proto::CyberConfig cfg;
  if (PathExists(cfg_file) && GetProtoFromFile(cfg_file, &cfg)) {
    switch (cgroup_type) {
      case static_cast<uint8_t>(Cgroup_Type::CPU_SET): {
        if (cfg.scheduler_conf().has_cpuset_cgroup()) {
          cgroup = cfg.scheduler_conf().cpuset_cgroup();
        }
        else {
          cgroup = "system";
        }
        break;
      }
      case static_cast<uint8_t>(Cgroup_Type::CPU_USAGE): {
        if (cfg.scheduler_conf().has_cpuusage_cgroup()) {
          cgroup = cfg.scheduler_conf().cpuusage_cgroup();
        }
        break;
      }
      default: break;
    }
  }
}

void ModuleArgument::LimitCpuAccess(const std::string& conf_path) {
  std::string cgroup;
  GetCgroup(conf_path, static_cast<uint8_t>(Cgroup_Type::CPU_SET), cgroup);
  int pid = getpid();
  std::string cmd{"sudo bash -c "};
  std::string internal_cmd = std::string{"\"echo "} + std::to_string(pid) + std::string{" > /sys/fs/cgroup/cpuset/"} 
                            + cgroup + std::string{"/tasks\""};
  cmd.append(internal_cmd);
  AINFO << "cmd is " << cmd;
  system(cmd.c_str());
}

// to limit the cpu usage of the program
void ModuleArgument::LimitCpuUsage(const std::string& conf_path) {
  std::string cgroup;
  GetCgroup(conf_path, static_cast<uint8_t>(Cgroup_Type::CPU_USAGE), cgroup);
  if (!cgroup.empty()) {
    int pid = getpid();
    std::string cmd{"sudo bash -c "};
    std::string internal_cmd = std::string{"\"echo "} + std::to_string(pid) + std::string{" > /sys/fs/cgroup/cpu/"} 
                              + cgroup + std::string{"/tasks\""};
    cmd.append(internal_cmd);
    AINFO << "cmd is " << cmd;
    system(cmd.c_str());
  }
}

void ModuleArgument::ParseArgument(const int argc, char* const argv[]) {
  binary_name_ = std::string(basename(argv[0]));
  GetOptions(argc, argv);
  std::string tune_performance{getenv("tune_performance")};
  if (tune_performance.empty()) {
    tune_performance = "no";
  }
  std::string conf;

  // ensure that the process_group is valid when adding -p flag to lauch the component
  if (!process_group_.empty()) {
    conf = std::string{"conf/"} + process_group_ + std::string{".conf"};
    if (!CheckFile(conf)) {
      AWARN << "sched conf "<< conf << " not found, use default one " << DEFAULT_process_group_;
      process_group_ = DEFAULT_process_group_;
      conf = std::string{"conf/"} + process_group_ + std::string{".conf"};
    }
  }
  else {
    for (const auto & dag : dag_conf_list_) {
      std::string dag_name{dag.substr(dag.find_last_of('/')+1, dag.substr(dag.find_last_of('/')+1).find_last_of('.'))};
      std::string process_group = dag_name + std::string{"_sched_classic"};
      conf = std::string{"conf/"} + process_group + std::string{".conf"};
      if (CheckFile(conf)) {
        process_group_ = process_group;
        break;
      }
    }
    if (process_group_.empty()) {
      process_group_ = DEFAULT_process_group_;
      conf = std::string{"conf/"} + process_group_ + std::string{".conf"};
    }
  }

  if (tune_performance == "yes") {
    LimitCpuAccess(conf);
    LimitCpuUsage(conf);
  }

  if (sched_name_.empty()) {
    sched_name_ = DEFAULT_sched_name_;
  }

  GlobalData::Instance()->SetProcessGroup(process_group_);
  GlobalData::Instance()->SetSchedName(sched_name_);
  AINFO << "binary_name_ is " << binary_name_ << ", process_group_ is "
        << process_group_ << ", has " << dag_conf_list_.size() << " dag conf";
  for (std::string& dag : dag_conf_list_) {
    AINFO << "dag_conf: " << dag;
  }
}

void ModuleArgument::GetOptions(const int argc, char* const argv[]) {
  opterr = 0;  // extern int opterr
  int long_index = 0;
  const std::string short_opts = "hd:p:s:";
  static const struct option long_opts[] = {
      {"help", no_argument, nullptr, 'h'},
      {"dag_conf", required_argument, nullptr, 'd'},
      {"process_name", required_argument, nullptr, 'p'},
      {"sched_name", required_argument, nullptr, 's'},
      {NULL, no_argument, nullptr, 0}};

  // log command for info
  std::string cmd("");
  for (int i = 0; i < argc; ++i) {
    cmd += argv[i];
    cmd += " ";
  }
  AINFO << "command: " << cmd;

  if (1 == argc) {
    DisplayUsage();
    exit(0);
  }

  do {
    int opt =
        getopt_long(argc, argv, short_opts.c_str(), long_opts, &long_index);
    if (opt == -1) {
      break;
    }
    switch (opt) {
      case 'd':
        dag_conf_list_.emplace_back(std::string(optarg));
        for (int i = optind; i < argc; i++) {
          if (*argv[i] != '-') {
            dag_conf_list_.emplace_back(std::string(argv[i]));
          } else {
            break;
          }
        }
        break;
      case 'p':
        process_group_ = std::string(optarg);
        break;
      case 's':
        sched_name_ = std::string(optarg);
        break;
      case 'h':
        DisplayUsage();
        exit(0);
      default:
        break;
    }
  } while (true);

  if (optind < argc) {
    AINFO << "Found non-option ARGV-element \"" << argv[optind++] << "\"";
    DisplayUsage();
    exit(1);
  }

  if (dag_conf_list_.empty()) {
    AINFO << "-d parameter must be specified";
    DisplayUsage();
    exit(1);
  }
}

}  // namespace mainboard
}  // namespace cyber
}  // namespace apollo
