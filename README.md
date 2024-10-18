Welcome to HyperLink's GitHub page!

HyperLink is a high performance, flexible architecture which accelerates the development, testing, and deployment of Autonomous Vehicles.

# Table of Contents

1. [Introduction](#introduction)
2. [Major Features and Improvements](#major-features-and-improvements)
3. [Build on PC Linux x86_64](#build-on-pc-linux-x86_64)
4. [Build on SoC(ARM64) Linux aarch64](#build-on-soc_arm64-linux-aarch64)
5. [Cross build on PC for target system QNX aarch64](#cross-build-on-pc-for-target-system-qnx-aarch64)
6. [Run example commands](#run-example-commands)
7. [Questions](#questions)
8. [Copyright and License](#copyright-and-license)
9. [Disclaimer](#disclaimer)
10. [Connect with us](#connect-with-us)

## Introduction

This release is a middleware for secondary development based on Baidu Apollo 6.0 cyberRT. We mainly implements support for hardware platforms(x86_64,arm64) and operating systems(Linux,QNX) with different architectures.

We have validated on these platforms: x86_64 PC Linux, Drive Orin Linux/QNX, Jetson Orin Linux.

## Major Features and Improvements

This chapter lists the main features, please refer to comments in the relevant code files for details.The keywords in the comments: @Feature List.

* For QNX aarch64 platform support
  * Fixed fastrtps1.5.0 cannot communicate in qnx  
    changed files:(src/cpp/transport/UDPv4Transport.cpp,UDPv6Transport.cpp)
  * Fixed phread_setaffinity_np not support in qnx  
    changed files:(cyber/scheduler/common/pin_thread.h,pin_thread.cc,cyber/scheduler/scheduler.cc,processor.cc)
  * Fixed dispatchers(rtps,shm,intra) different behaviour in linux and qnx  
    sample refer to(cyber/examples/common_component_example/CMakeLists.txt)
  * Implementation of shm IPC mechanism in qnx  
    changed files:(cyber/transport/shm/xsi_segment.h,xsi_segment.cc,posix_segment.cc,condition_notifier.h,condition_notifier.cc)
  * Fixed epoll not support in qnx  
    changed files:(cyber/io/poll_data.h,poll_handler.cc,poller.cc,poller.h,session.cc)
  * Fixed cyber_monitor failed in qnx  
    Add terminfo files to qnx host（refer to [cross build for qnx](#cross-build-on-pc-for-target-system-qnx-aarch64)）
  * Fixed the clock different behaviour in linux and qnx  
    changed files:(cyber/time/time.cc)
  * Fixed the issue entry->d_type is not support in qnx  
    changed files:(cyber/common/file.h,file.cc)
* For Linux aarch64 platform support
  * Nvidia xavier/orin docker environment support  
    changed files:(docker/scripts/dev_start.sh,dev_into.sh,scripts/docker_start_user.sh)
  * Fixed the issue of cyber crashed in Linux aarch64 environment  
    changed files:(CMakeLists.txt)
* To improve portability and flexibility
  * Support multi platform compilation and re-implement the compilation system using cmake replace bazel  
    added files:(all the CMakeLists.txt,scripts/cmake_build.sh,cmake_clean.sh,setup.sh.in,cyber/setup.bash,cyber/common/global_data.cc)
  * Implementation of discrete arch cyber communication  
    changed files:(docker/scripts/dev_start.sh,scripts/docker_start_user.sh,cyber/setup.bash)

## Build on PC Linux x86_64

### Prerequisites

Host(PC development environment) for x86_64 Linux

* A machine with a 8-core processor and 16GB memory minimum
* OS: Ubuntu 18.04 and above
* NVIDIA driver version 440.33.01 and above ([Web link](https://www.nvidia.com/Download/index.aspx?lang=en-us))
* Docker-CE version 19.03 and above ([Official doc](https://docs.docker.com/engine/install/ubuntu/))  
  add user to docker group: sudo gpasswd -a ${USER} docker

Docker image envirement

* Docker image: dev-x86_64-18.04-20200914_0742
* cmake version 3.16.8 and above
* protoc version 3.12.3
* g++ gcc version 7.5.0 and above

### Build step

* cd docker/scripts
* ./dev_start.sh  
  *Note*: If docker images download fails, you can try to modify the docker repository:  
  vim docker/scripts/dev\_start.sh  
  DOCKER_REPO="apolloauto/apollo"`->`DOCKER_REPO="registry.baidubce.com/apolloauto/apollo"`
* ./dev_into.sh
* ./scripts/cmake_build.sh linux
* output files: ./install.Linux.x86_64/

### Run examples

Environment:

* Target system: PC Linux x86_64
* The example can run within docker environment：./dev_into.sh  
  *Note*: The command 'source ./install.Linux.x86_64/setup. rc' will be automatically executed
* cd ./install.Linux.x86_64/

[Run examples](#run-example-commands)

## Build on SoC_ARM64 Linux aarch64

### Prerequisites

Host(ARM64) for aarch64 Linux

* A machine with a 8-core processor and 16GB memory minimum
* OS: Ubuntu 18.04LTS（arm64）and above
* Docker-CE version 19.03 and above ([Official doc](https://docs.docker.com/engine/install/ubuntu/))  
  add user to docker group: sudo gpasswd -a ${USER} docker

Docker image envirement

* Docker image: dev-aarch64-18.04-20200915_0106
* cmake version 3.16.8 and above
* protoc version 3.12.3
* g++ gcc version 7.5.0 and above

### Build step

* cd docker/scripts
* ./dev_start.sh  
  *Note*: If docker images download fails, you can try to modify the docker repository:  
  vim docker/scripts/dev\_start.sh  
  DOCKER_REPO="apolloauto/apollo"`->`DOCKER_REPO="registry.baidubce.com/apolloauto/apollo"`
* ./dev_into.sh
* ./scripts/cmake_build.sh linux
* output files: ./install.Linux.aarch64/

### Run examples

Environment:

* Target system: ARM Linux aarch64
* The example can run within docker environment：./dev_into.sh  
  *Note*: The command 'source ./install.Linux.aarch64/setup. rc' will be automatically executed
* cd ./install.Linux.aarch64/

[Run examples](#run-example-commands)

## Cross build on PC for target system QNX aarch64

### Prerequisites

Host(PC development environment) for QNX

* A machine with a 8-core processor and 16GB memory minimum
* OS: Ubuntu 18.04 and above
* Docker-CE version 19.03 and above ([Official doc](https://docs.docker.com/engine/install/ubuntu/))  
  add user to docker group: sudo gpasswd -a ${USER} docker
* docker-compose version 1.29.2 ([Officaial doc](https://www.digitalocean.com/community/tutorials/how-to-install-and-use-docker-compose-on-ubuntu-20-04))
* QNX license available (/home/${USER}/.qnx/license)

Docker image envirement

* Docker image: (you must get the docker image from blackberry with QNX7.1 SDP)
* cmake version 3.16.8 and above
* protoc version 3.12.3
* Modified the ./docker/scripts/qnx/docker-compose.yml line 32:  
  e.g. IMAGE: your_image_REPOSITORY:your_image_TAG
* Make sure QNX license exist.

### Build step

* cd docker/scripts/qnx
* ./dev_start.sh -l
* ./dev_into.sh
* ./scripts/cmake_build.sh qnx
* output files: ./install.QNX.aarch64/

### Run examples step

Environment:

* Target system: ARM QNX aarch64
* The terminfo files should be complete(for cyber_monitor):  
  (host):/opt/qnx710/target/qnx7/usr/lib/terminfo -> (target):/usr/lib/terminfo
* The python3.8 should be available(for cyber_launch):  
  (host): /opt/qnx710/target/qnx7/usr/lib/python3.8/* -> (target): /usr/lib/
* Deploy bin files to SoC:  
  e.g. scp -r ./install.QNX.aarch64/* USER@hostIP:/test
* ssh to the SoC and source the rc file:  
  ssh USER@hostIP  
  cd /test  
  . ./setup.rc(must be run in current console)

[Run examples](#run-example-commands)

## Run example commands

**cyber comm**:

* cyber_example_talker
* cyber_example_listener

**cyber service**:

* cyber_example_service

cyber mainboard:

* mainboard -d share/examples/timer_component_example/timer.dag
* cyber_monitor

cyber launch:

* cyber_launch start share/examples/timer_component_example/timer.launch
* cyber_monitor

## Questions

You are welcome to submit questions and bug reports as [GitHub Issues](https://github.com/ApolloAuto/apollo/issues "https://github.com/ApolloAuto/apollo/issues").

## Copyright and License

This release is provided under the [Apache-2.0 license](https://github.com/ApolloAuto/apollo/blob/master/LICENSE).

## Disclaimer

This release only has the source code for cyber，tools，examples and all third-party libraries are provided in binary format.

## Connect with us

Interested in our turnKey solutions or partnering with us Mail us at: [xxxpartner@google.com](mailto:xxxpartner@google.com "mailto:xxxpartner@google.com")
