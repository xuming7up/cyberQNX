#!/bin/sh

###############################################################################
# Copyright 2024 The HyperLink Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
###############################################################################

# Usage: ./tune_performance.sh [0 or 1]
#         0 for teardown, 1 for setup

camera_usage=150
lidar_usage=150
default_num=10000

OS=`uname`;

if [ "$OS" = "Linux" ]; then
    ARCH=`uname -i`
fi

SUDO=""
if [ "$OS" = "Linux" ] && [ "$ARCH" = "aarch64" ]; then
    ECHO=echo
    SUDO="eval $ECHO nvidia | sudo -S"
else
    SUDO=sudo
fi

systemCpuset=/sys/fs/cgroup/cpuset/system
sensorCpuset=/sys/fs/cgroup/cpuset/sensor
perceptionCpuset=/sys/fs/cgroup/cpuset/perception
cameraCpucontrol=/sys/fs/cgroup/cpu/camera
lidarCpucontrol=/sys/fs/cgroup/cpu/lidar
# test for 2.5 core
# cameraCputest=/sys/fs/cgroup/cpu/camera
# radarCputest=/sys/fs/cgroup/cpu/radar
# commonCpuset=/sys/fs/cgroup/cpuset/common

setupSystemCpuset()
{
    $SUDO mkdir -p $systemCpuset
    $SUDO chown -R ${USER}:${USER} $systemCpuset
    $SUDO echo 0 > $systemCpuset/cpuset.mem_exclusive
    $SUDO echo 0 > $systemCpuset/cpuset.mems
    $SUDO echo 0-2 > $systemCpuset/cpuset.cpus
    $SUDO echo $$ > $systemCpuset/tasks
    tasks=`$SUDO cat /sys/fs/cgroup/cpuset/tasks`
    for l in $tasks; do
        $SUDO echo $l > $systemCpuset/tasks 2>/dev/null;
    done
}

start()
{
    echo "Tuning system for perf"
    $SUDO chown -R ${USER}:${USER} /sys/fs/cgroup/cpuset

    if [ ! -d "$systemCpuset" ]; then
        setupSystemCpuset
    fi

    # Expect to have had this shell put into system, if not, reconfig the
    # system cpuset
    cpuset=`cat /proc/$$/cpuset`
    if [ "$cpuset" != "/system" ]; then
        setupSystemCpuset
    fi

    if [ ! -d "$sensorCpuset" ]; then
        $SUDO mkdir $sensorCpuset
        $SUDO chown -R ${USER}:${USER} $sensorCpuset
        $SUDO echo 0 > $sensorCpuset/cpuset.mem_exclusive
        $SUDO echo 0 > $sensorCpuset/cpuset.mems
        $SUDO echo 3-4 > $sensorCpuset/cpuset.cpus
    fi

    if [ ! -d "$perceptionCpuset" ]; then
        $SUDO mkdir $perceptionCpuset
        $SUDO chown -R ${USER}:${USER} $perceptionCpuset
        $SUDO echo 0 > $perceptionCpuset/cpuset.mem_exclusive
        $SUDO echo 0 > $perceptionCpuset/cpuset.mems
        $SUDO echo 5-7 > $perceptionCpuset/cpuset.cpus
    fi

    if [ ! -d "$cameraCpucontrol" ]; then
        $SUDO mkdir $cameraCpucontrol
        $SUDO chown -R ${USER}:${USER} $cameraCpucontrol
        $SUDO echo $camera_usage > $cameraCpucontrol/cpu.shares
        $SUDO echo 1000000 > $cameraCpucontrol/cpu.cfs_period_us
        $SUDO echo `expr $camera_usage \* $default_num` > $cameraCpucontrol/cpu.cfs_quota_us
    fi

    if [ ! -d "$lidarCpucontrol" ]; then
        $SUDO mkdir $lidarCpucontrol
        $SUDO chown -R ${USER}:${USER} $lidarCpucontrol
        $SUDO echo $lidar_usage > $lidarCpucontrol/cpu.shares
        $SUDO echo 1000000 > $lidarCpucontrol/cpu.cfs_period_us
        $SUDO echo `expr $lidar_usage \* $default_num` > $lidarCpucontrol/cpu.cfs_quota_us
    fi

    #---------------------------test for 2.5 core--------------------------------------------------------
    # if [ ! -d "$commonCpuset" ]; then
    #     $SUDO mkdir $commonCpuset
    #     $SUDO chown -R ${USER}:${USER} $commonCpuset
    #     $SUDO echo 0 > $commonCpuset/cpuset.mem_exclusive
    #     $SUDO echo 0 > $commonCpuset/cpuset.mems
    #     $SUDO echo 6 > $commonCpuset/cpuset.cpus
    # fi


    # if [ ! -d "$cameraCputest" ]; then
    #     $SUDO mkdir $cameraCputest
    #     $SUDO chown -R ${USER}:${USER} $cameraCputest
    #     $SUDO echo 25 > $cameraCputest/cpu.shares
    #     $SUDO echo 1000000 > $cameraCputest/cpu.cfs_period_us
    #     $SUDO echo 250000 > $cameraCputest/cpu.cfs_quota_us
    # fi

    # if [ ! -d "$radarCputest" ]; then
    #     $SUDO mkdir $radarCputest
    #     $SUDO chown -R ${USER}:${USER} $radarCputest
    #     $SUDO echo 25 > $radarCputest/cpu.shares
    #     $SUDO echo 1000000 > $radarCputest/cpu.cfs_period_us
    #     $SUDO echo 250000 > $radarCputest/cpu.cfs_quota_us
    # fi
    #---------------------------end test for 2.5 core--------------------------------------------------------
    echo "Done tuning."
}

end()
{
    echo "Cleaning up perf system tuning"
    set +e
    # Cleanup cpusets
    tasks=`$SUDO cat $systemCpuset/tasks 2>/dev/null` 
    for l in $tasks; do
        $SUDO echo $l > /sys/fs/cgroup/cpuset/tasks 2>/dev/null;
    done
    $SUDO rmdir $systemCpuset 2>/dev/null;
    $SUDO rmdir $sensorCpuset 2>/dev/null;
    $SUDO rmdir $perceptionCpuset 2>/dev/null;
    $SUDO rmdir $cameraCpucontrol 2>/dev/null;
    $SUDO rmdir $lidarCpucontrol 2>/dev/null;
    # test for 2.5 core --------------------------------------------
    # $SUDO rmdir $commonCpuset 2>/dev/null;
    # $SUDO rmdir $cameraCputest 2>/dev/null;
    # $SUDO rmdir $radarCputest 2>/dev/null;
}

if [ "$1" = "1" ]; then
    start
else
    end
fi
