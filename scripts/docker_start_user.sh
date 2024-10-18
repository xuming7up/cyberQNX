#!/usr/bin/env bash

###############################################################################
# Copyright 2020 The Apollo Authors. All Rights Reserved.
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
#
###############################################################################

#/*
# @Desc: Change History
# @Date: 2024-10-15
# @Version: 1.0.0
# @Feature List:
# -1.Nvidia xavier/orin platform support.
#
#*/

function _create_user_account() {
  local user_name="$1"
  local uid="$2"
  local group_name="$3"
  local gid="$4"
  addgroup --gid "${gid}" "${group_name}"

  adduser --disabled-password --force-badname --gecos '' \
    "${user_name}" --uid "${uid}" --gid "${gid}" # 2>/dev/null

  usermod -aG sudo "${user_name}"
}

function setup_user_bashrc() {
  local uid="$1"
  local gid="$2"
  local user_home="/home/$3"
  # cp -rf /etc/skel/.{profile,bash*} "${user_home}"
  local RCFILES_DIR="/opt/apollo/rcfiles"
  local rc
  if [[ -d "${RCFILES_DIR}" ]]; then
    for entry in ${RCFILES_DIR}/*; do
      rc=$(basename "${entry}")
      if [[ "${rc}" = user.* ]]; then
        cp -rf "${entry}" "${user_home}/${rc##user}"
      fi
    done
  fi
  # Set user files ownership to current user, such as .bashrc, .profile, etc.
  # chown -R "${uid}:${gid}" "${user_home}"
  chown -R "${uid}:${gid}" ${user_home}/.*
}

function setup_user_account_if_not_exist() {
  local user_name="$1"
  local uid="$2"
  local group_name="$3"
  local gid="$4"
  if grep -q "^${user_name}:" /etc/passwd; then
    echo "User ${user_name} already exist. Skip setting user account."
    return
  fi
  _create_user_account "$@"
  setup_user_bashrc "${uid}" "${gid}" "${user_name}"
}

function grant_device_permissions() {
  # setup GPS device
  [ -e /dev/novatel0 ] && chmod a+rw /dev/novatel0
  [ -e /dev/novatel1 ] && chmod a+rw /dev/novatel1
  [ -e /dev/novatel2 ] && chmod a+rw /dev/novatel2

  [ -e /dev/ttyACM0 ] && chmod a+rw /dev/ttyACM0
  [ -e /dev/imu ] && chmod a+rw /dev/imu

  # setup camera device
  [ -e /dev/camera/obstacle ] && chmod a+rw /dev/camera/obstacle
  [ -e /dev/camera/trafficlights ] && chmod a+rw /dev/camera/trafficlights

  # setup audio device
  [ -e /dev/snd ] && usermod -a -G audio "$1"
}

function setup_apollo_directories() {
  local apollo_dir="/opt/apollo"
  [[ -d "${apollo_dir}" ]] || mkdir -p "${apollo_dir}"
  # chown -R "${uid}:${gid}" "${apollo_dir}"
}

##===================== Main ==============================##
function main() {
  local user_name="$1"
  local uid="$2"
  local group_name="$3"
  local gid="$4"

  if [ "${uid}" != "${gid}" ]; then
    echo "Warning: uid(${uid}) != gid(${gid}) found."
  fi
  if [ "${user_name}" != "${group_name}" ]; then
    echo "Warning: user_name(${user_name}) != group_name(${group_name}) found."
  fi
  setup_user_account_if_not_exist "$@"
  setup_apollo_directories "${uid}" "${gid}"
  grant_device_permissions "${user_name}"

  TARGET_ARCH="$(uname -m)"

  # for jetson orin
  if [[ "$product" =~ .*"Orin"* ]]; then
    find /usr/local/ -type l -follow -printf "" 2>&1 | sed -e "s/find: '\(.*\)':.*/\1/" | xargs rm
  fi
  # for proai pegsus and drive orin
  if [[ $5 =~ .*"ProAI_Gen20" ]] || [[ $5 =~ .*"e3550_t194"* ]] || [ "$5" = "p3710-0010" ]; then
    find /usr/local/ -type l -follow -printf "" 2>&1 | sed -e "s/find: ‘\(.*\)’:.*/\1/" | xargs rm 
    echo "export product=$5" | cat - /home/${user_name}/.bashrc > temp && mv temp /home/${user_name}/.bashrc
    echo "export network_interface=$7" | cat - /home/${user_name}/.bashrc > temp && mv temp /home/${user_name}/.bashrc
    echo "export tune_performance=$6" >> /home/${user_name}/.bashrc
  fi

  echo "export necessary_tf_cuda_compute_capabilities=$8" >> /home/${user_name}/.bashrc

  if [ "${TARGET_ARCH}" = "aarch64" ] && [ "$5" = "p3710-0010" ] || [[ "$product" =~ .*"Orin"* ]]; then
    # setup video device
    usermod -aG video "$1"
    echo "export MESA_GL_VERSION_OVERRIDE=3.3" >> /home/${user_name}/.bashrc
  fi
  if [ "${TARGET_ARCH}" = "aarch64" ] && [[ "$product" =~ .*"Orin"* ]]; then
    echo '/usr/lib/aarch64-linux-gnu/tegra/' >> /etc/ld.so.conf.d/qt.conf
    ldconfig 2>/dev/null
  fi
}

if [ "${DOCKER_USER}" == "" ]; then
  DOCKER_USER="$1"
  DOCKER_USER_ID="$2"
  DOCKER_GRP="$3"
  DOCKER_GRP_ID="$4"

  echo '/usr/lib/aarch64-linux-gnu/tegra/aarch64-linux-gnu/' >> /etc/ld.so.conf.d/qt.conf
  echo '/apollo/third_party/can_card_library/hermes_can/lib_aarch64' >> /etc/ld.so.conf.d/qt.conf
  echo '/usr/local/driveworks-2.2/lib' >> /etc/ld.so.conf.d/nvidia.conf
  echo '/usr/local/driveworks-5.4/lib' >> /etc/ld.so.conf.d/nvidia.conf
  echo '/lib/aarch64-linux-gnu' >> /etc/ld.so.conf.d/nvidia.conf
  ldconfig 2>/dev/null
fi

main "${DOCKER_USER}" "${DOCKER_USER_ID}" "${DOCKER_GRP}" "${DOCKER_GRP_ID}" "$5" "$6" "$7" "$8"
