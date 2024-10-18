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

#只有在docker-compose 1.xx版本才能正常运行，
#在docker-compose 2.xx版本container name的名称不一样，不是下划线"_"，而是短横线"-"
CONTAINER_NAME="qnx_proai-nvidia-qnx-6.0-soc_1"

docker exec -it $CONTAINER_NAME /bin/bash