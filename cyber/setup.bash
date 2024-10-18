#! /usr/bin/env bash

# /*
# * @Desc: Change History
# * @Date: 2024-10-15
# * @Version: 1.0.0
# * @Feature List:
# * -1.Auto config IP address on network_interface(defined in dev_start.sh) and assign it to variable CYBER_IP
# * -2.Cgroup setup on Linux system
# */

TOP_DIR="$(cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd -P)"
source ${TOP_DIR}/scripts/apollo.bashrc

export APOLLO_BAZEL_DIST_DIR="${APOLLO_CACHE_DIR}/distdir"
export CYBER_PATH="${APOLLO_ROOT_DIR}/cyber"

bazel_bin_path="${APOLLO_ROOT_DIR}/bazel-bin"
cyber_bin_path="${bazel_bin_path}/cyber"
cyber_tool_path="${bazel_bin_path}/cyber/tools"
recorder_path="${cyber_tool_path}/cyber_recorder"
launch_path="${cyber_tool_path}/cyber_launch"
channel_path="${cyber_tool_path}/cyber_channel"
node_path="${cyber_tool_path}/cyber_node"
service_path="${cyber_tool_path}/cyber_service"
monitor_path="${cyber_tool_path}/cyber_monitor"
visualizer_path="${bazel_bin_path}/modules/tools/visualizer"
rosbag_to_record_path="${bazel_bin_path}/modules/data/tools/rosbag_to_record"

# TODO(all): place all these in one place and add_to_path
for entry in "${cyber_bin_path}" \
    "${recorder_path}" "${monitor_path}"  \
    "${channel_path}" "${node_path}" \
    "${service_path}" \
    "${launch_path}" \
    "${visualizer_path}" \
    "${rosbag_to_record_path}" ; do
    add_to_path "${entry}"
done

# ${CYBER_PATH}/python
export PYTHONPATH=${bazel_bin_path}/cyber/python/internal:${PYTHONPATH}
export CYBER_DOMAIN_ID=80
############################lwc added for auto config ip address on eth0:0 interface on pegasus product################
if hash lshw 2>/dev/null; then
    product=$(lshw -short -c system)
fi

if [[ ! -z "$network_interface" ]] && tmp_network=$(ifconfig | grep $network_interface) && [[ ! -z tmp_network ]]; then
    tmp_ip=`ifconfig $network_interface |grep -e 'inet ' |awk '{print $2}'|grep -oE '[0-9.]*'`
    if [[ ! -z "$tmp_ip" ]]; then
    	export CYBER_IP="${tmp_ip}"
    	echo "Gonna use address ${CYBER_IP}"
    else
    	export CYBER_IP=127.0.0.1
    	echo "Gonna use default address ${CYBER_IP}"
    fi
else 
    export CYBER_IP=127.0.0.1
    echo "Gonna use default address ${CYBER_IP}"
fi
#####################################################################################################
#export CYBER_IP=127.0.0.1
#DualOS IP config:
#XA: CYBER_IP=192.168.9.152
#XB: CYBER_IP=192.168.9.153

export GLOG_log_dir="${APOLLO_ROOT_DIR}/data/log"
export GLOG_alsologtostderr=0
export GLOG_colorlogtostderr=1
export GLOG_minloglevel=0

export sysmo_start=0

# for DEBUG log
#export GLOG_minloglevel=-1
#export GLOG_v=4

source ${CYBER_PATH}/tools/cyber_tools_auto_complete.bash

OS=`uname`;

if [ "$OS" = "Linux" ]; then
    ARCH=`uname -i`
fi

if [ -d "/sys/fs/cgroup/cpuset/system" ]; then
    if [ "$OS" = "Linux" ] && [ "$ARCH" = "aarch64" ]; then
        sudo echo $$ > /sys/fs/cgroup/cpuset/system/tasks 2>/dev/null;
    else
        sudo bash -c "echo $$ > /sys/fs/cgroup/cpuset/system/tasks 2>/dev/null";
    fi
fi

source ${APOLLO_ROOT_DIR}/install.$(uname).$(uname -p)/setup.rc
