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

/*
* @Desc: Change History
* @Date: 2024-10-15
* @Version: 1.0.0
* @Feature List:
* -1.Using Driveworks middleware framework to decode h264 frames on Drive AGX Orin platform
*/

#ifndef CYBER_TOOLS_CYBER_RECORDER_PLAYER_PLAY_TASK_CONSUMER_H_
#define CYBER_TOOLS_CYBER_RECORDER_PLAYER_PLAY_TASK_CONSUMER_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>

#include "cyber/tools/cyber_recorder/player/play_task_buffer.h"
#if defined(_ON_ORIN_) || defined(_ON_PEGASUS_)
#include "cyber/cyber.h"
#include "proto/camera/sensor_image.pb.h"
#include "cyber/message/raw_message.h"
#include "cyber/node/node.h"
#include "cyber/node/writer.h"
#include "play_task_producer.h"
#include "dwcamera_deserializer.h"
#include "modules/drivers/dwcamera_sensing_world/neonutility.h"
#endif

namespace apollo {
namespace cyber {
namespace record {

#if defined(_ON_ORIN_) || defined(_ON_PEGASUS_)
using apollo::drivers::Image;
#endif
class PlayTaskConsumer {
 public:
  using ThreadPtr = std::unique_ptr<std::thread>;
  using TaskBufferPtr = std::shared_ptr<PlayTaskBuffer>;

  explicit PlayTaskConsumer(const TaskBufferPtr& task_buffer,
                            double play_rate = 1.0);
  virtual ~PlayTaskConsumer();

#if defined(_ON_ORIN_) || defined(_ON_PEGASUS_)
  static PlayTaskConsumer* getInstance() {
    return sInstance_;
  }
#endif

  void Start(uint64_t begin_time_ns);
  void Stop();
  void Pause() { is_paused_.exchange(true); }
  void PlayOnce() { is_playonce_.exchange(true); }
  void Continue() { is_paused_.exchange(false); }

  uint64_t base_msg_play_time_ns() const { return base_msg_play_time_ns_; }
  uint64_t base_msg_real_time_ns() const { return base_msg_real_time_ns_; }
  uint64_t last_played_msg_real_time_ns() const {
    return last_played_msg_real_time_ns_;
  }

#if defined(_ON_ORIN_) || defined(_ON_PEGASUS_)
  void writeImageToComponent(const std::string& channel_name, uint8_t* img, uint8_t channel_index, int width, int height, double measurement_time, double header_timestamp);
#endif

 private:
  void ThreadFunc();

#if defined(_ON_ORIN_) || defined(_ON_PEGASUS_)
  static PlayTaskConsumer* sInstance_;
#endif
  double play_rate_;
  ThreadPtr consume_th_;
  TaskBufferPtr task_buffer_;
  std::atomic<bool> is_stopped_;
  std::atomic<bool> is_paused_;
  std::atomic<bool> is_playonce_;
  uint64_t begin_time_ns_;
  uint64_t base_msg_play_time_ns_;
  uint64_t base_msg_real_time_ns_;
  uint64_t last_played_msg_real_time_ns_;
  static const uint64_t kPauseSleepNanoSec;
  static const uint64_t kWaitProduceSleepNanoSec;
  static const uint64_t MIN_SLEEP_DURATION_NS;
#if defined(_ON_ORIN_) || defined(_ON_PEGASUS_)
  std::shared_ptr<Node> node_;
  std::map<std::string, std::shared_ptr<cyber::Writer<apollo::drivers::Image>>> h264_decompressed_writers_;
#endif
};

}  // namespace record
}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_TOOLS_CYBER_RECORDER_PLAYER_PLAY_TASK_CONSUMER_H_
