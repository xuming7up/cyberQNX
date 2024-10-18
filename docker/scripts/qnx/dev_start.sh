#!/bin/bash

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

SOC_TOP="$(cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd -P)"
DOCKER_DIR=$SOC_TOP/qnx

echo "----------> $SOC_TOP"

function stop() {
    docker-compose -f $DOCKER_DIR/docker-compose.yml -f $DOCKER_DIR/docker-compose.user.yml down
}

function restart() {
    docker-compose -f $DOCKER_DIR/docker-compose.yml -f $DOCKER_DIR/docker-compose.user.yml down
    docker-compose -f $DOCKER_DIR/docker-compose.yml -f $DOCKER_DIR/docker-compose.user.yml up -d proai-nvidia-qnx-6.0-soc
}

while [ $# -gt 0 ] ; do 
    opt=$1 ; shift
    echo "------------> $opt"
    case "${opt}" in 
    stop)
        stop
        exit 0
        ;;
    restart)
        restart
        ;;
    *)
        ;;
    esac
done # end while

docker-compose -f $DOCKER_DIR/docker-compose.yml -f $DOCKER_DIR/docker-compose.user.yml down
docker-compose -f $DOCKER_DIR/docker-compose.yml -f $DOCKER_DIR/docker-compose.user.yml up -d proai-nvidia-qnx-6.0-soc
