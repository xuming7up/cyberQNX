#!/usr/bin/env bash

###############################################################################
# Copyright 2017 The Apollo Authors. All Rights Reserved.
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

#/*
# @Desc: Change History
# @Date: 2024-10-15
# @Version: 1.0.0
# @Feature List:
# -1.Nvidia xavier/orin platform support.
#
#*/
DOCKER_USER="${USER}"
DEV_CONTAINER="apollo_dev_${USER}"

xhost +local:root 1>/dev/null 2>&1

docker-exec () {
sudo nsenter --target $(docker inspect --format {{.State.Pid}} ${1}) -a /usr/sbin/chroot /new_root ${@:2};
}

if [ "$(uname -m)" = "aarch64" ] && product=$(lshw -short -c system) && [[ $product =~ .*"ProAI_Gen20" ]] || [[ $product =~ .*"e3550_t194"* ]] || [[ $product =~ .*"p3710-0010"* ]]; then
  docker-exec ${DEV_CONTAINER} su ${DOCKER_USER}
else
  docker exec \
      -u "${DOCKER_USER}" \
      -e HISTFILE=/apollo/.dev_bash_hist \
      -it "${DEV_CONTAINER}" \
      /bin/bash
fi


xhost -local:root 1>/dev/null 2>&1
