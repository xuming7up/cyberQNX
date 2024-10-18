#! /usr/bin/env bash

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

TOP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"

install_dir=""

function print_help() {
    echo "Please use below command to clear the special caches."
    echo "cmake_clean.sh linux: clear the current linux build cache."
    echo "         qnx: clear the qnx build cache."
    exit 1
}

function rm_installed() {
    rm ${install_dir}/bin/cyber* ${install_dir}/bin/mainboard ${install_dir}/bin/tcp_echo* ${install_dir}/bin/udp_echo*
    rm ${install_dir}/lib/libcyber*
    rm -rf ${install_dir}/share/conf ${install_dir}/share/examples
}

case $# in
  0)
    print_help
    ;;
  1)
    cmd=$1
    ;;
  *)
    print_help
    ;;
esac

if [ $cmd == "linux" ]; then
    install_dir=${TOP_DIR}/install.$(uname).$(uname -p)
    echo "Clearing $(uname) directory build.$(uname).$(uname -p) and install.$(uname).$(uname -p)..."
    rm -rf ${TOP_DIR}/build.$(uname).$(uname -p)
    rm_installed
    echo "Clear end."
elif [ $cmd == "qnx" ]; then
    install_dir=${TOP_DIR}/install.QNX.aarch64
    echo "Clearing QNX directory build.QNX.* and install.QNX.aarch64 directories..."
    rm -rf ${TOP_DIR}/build.QNX.*
    rm_installed
    echo "Clear end."
else
    echo "Unknown cmd: ${cmd}"
fi
