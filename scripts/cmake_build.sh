#!/usr/bin/env bash

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
install_dir=${TOP_DIR}/install
build_out=${TOP_DIR}/build
build_target_os="unknown"
target_arch="aarch64"

function printHelp() {
    echo "Please use below command to build third-party libraries:"
    echo "script/cmake_build.sh linux(or qnx)"
    exit 1
}

case $# in
  0)
    printHelp
    ;;
  1)
    build_target_os=$1
    ;;
  *)
    printHelp
    ;;
esac

if [ $build_target_os == "qnx" ]; then
    build_target_os="QNX"
elif [ $build_target_os == "linux" ]; then
    build_target_os="Linux"
    target_arch=$(arch)
else
    printHelp
fi

build_out="${build_out}.${build_target_os}.${target_arch}"
install_dir="${install_dir}.${build_target_os}.${target_arch}"

if [ ! -d $build_out ];then
  mkdir -p $build_out
fi




cd $build_out
echo "Running cmake..."
if [ $build_target_os == "QNX" ]; then
    time cmake .. -DCMAKE_TOOLCHAIN_FILE=${TOP_DIR}/cmake/qnx710_cross_compile.cmake -DCMAKE_INSTALL_PREFIX=${install_dir}
else
    time cmake .. -DCMAKE_INSTALL_PREFIX=${install_dir}/
fi

echo "Running make -j$(nproc)..."  
time make -j$(nproc)

echo "Running make install..."  
time make install
