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

#include "cyber/tools/cyber_recorder/player/play_task_consumer.h"
#include "cyber/common/log.h"
#include "cyber/time/time.h"

namespace apollo {
namespace cyber {
namespace record {

#if defined(_ON_ORIN_) || defined(_ON_PEGASUS_)
PlayTaskConsumer* PlayTaskConsumer::sInstance_ = nullptr;
#endif
const uint64_t PlayTaskConsumer::kPauseSleepNanoSec = 100000000UL;
const uint64_t PlayTaskConsumer::kWaitProduceSleepNanoSec = 5000000UL;
const uint64_t PlayTaskConsumer::MIN_SLEEP_DURATION_NS = 200000000UL;

PlayTaskConsumer::PlayTaskConsumer(const TaskBufferPtr& task_buffer,
                                   double play_rate)
    : play_rate_(play_rate),
      consume_th_(nullptr),
      task_buffer_(task_buffer),
      is_stopped_(true),
      is_paused_(false),
      is_playonce_(false),
      base_msg_play_time_ns_(0),
      base_msg_real_time_ns_(0),
      last_played_msg_real_time_ns_(0){
  if (play_rate_ <= 0) {
    AERROR << "invalid play rate: " << play_rate_
           << " , we will use default value(1.0).";
    play_rate_ = 1.0;
  }
#if defined(_ON_ORIN_) || defined(_ON_PEGASUS_)
  node_ = nullptr;
#endif
}

PlayTaskConsumer::~PlayTaskConsumer() { Stop(); }

void PlayTaskConsumer::Start(uint64_t begin_time_ns) {
  if (!is_stopped_.exchange(false)) {
    return;
  }
  begin_time_ns_ = begin_time_ns;
#if defined(_ON_ORIN_) || defined(_ON_PEGASUS_)
  PlayTaskConsumer::sInstance_ = this;
  std::string node_name = "cyber_recorder_consume_" + std::to_string(getpid());
  node_ = apollo::cyber::CreateNode(node_name);
  if (node_ == nullptr) {
    AERROR << "create node failed.";
    return;
  }
#endif
  consume_th_.reset(new std::thread(&PlayTaskConsumer::ThreadFunc, this));
}

void PlayTaskConsumer::Stop() {
  if (is_stopped_.exchange(true)) {
    return;
  }
#if defined(_ON_ORIN_) || defined(_ON_PEGASUS_)
  for (const auto& decoder : PlayTaskProducer::getInstance()->GetAllDWH264Decoders()) {
    decoder->onRelease();
  }
#endif
  if (consume_th_ != nullptr && consume_th_->joinable()) {
    consume_th_->join();
    consume_th_ = nullptr;
  }
}

// void PlayTaskConsumer::ThreadFunc() {
//   uint64_t base_real_time_ns = 0;
//   uint64_t accumulated_pause_time_ns = 0;

//   while (!is_stopped_.load()) {
//     auto task = task_buffer_->Front();
//     if (task == nullptr) {
//       std::this_thread::sleep_for(
//           std::chrono::nanoseconds(kWaitProduceSleepNanoSec));
//       continue;
//     }

//     uint64_t sleep_ns = 0;

//     if (base_msg_play_time_ns_ == 0) {
//       base_msg_play_time_ns_ = task->msg_play_time_ns();
//       base_msg_real_time_ns_ = task->msg_real_time_ns();
//       if (base_msg_play_time_ns_ > begin_time_ns_) {
//         sleep_ns = static_cast<uint64_t>(
//             static_cast<double>(base_msg_play_time_ns_ - begin_time_ns_) /
//             play_rate_);
//         while (sleep_ns > MIN_SLEEP_DURATION_NS && !is_stopped_.load()) {
//           std::this_thread::sleep_for(
//               std::chrono::nanoseconds(MIN_SLEEP_DURATION_NS));
//           sleep_ns -= MIN_SLEEP_DURATION_NS;
//         }

//         if (is_stopped_.load()) {
//           break;
//         }

//         std::this_thread::sleep_for(std::chrono::nanoseconds(sleep_ns));
//       }
//       base_real_time_ns = Time::Now().ToNanosecond();
//       ADEBUG << "base_msg_play_time_ns: " << base_msg_play_time_ns_
//              << "base_real_time_ns: " << base_real_time_ns;
//     }

//     uint64_t task_interval_ns = static_cast<uint64_t>(
//         static_cast<double>(task->msg_play_time_ns() - base_msg_play_time_ns_) /
//         play_rate_);
//     uint64_t real_time_interval_ns = Time::Now().ToNanosecond() -
//                                      base_real_time_ns -
//                                      accumulated_pause_time_ns;
//     if (task_interval_ns > real_time_interval_ns) {
//       sleep_ns = task_interval_ns - real_time_interval_ns;
//       std::this_thread::sleep_for(std::chrono::nanoseconds(sleep_ns));
//     }

//     task->Play();
//     is_playonce_.store(false);

//     last_played_msg_real_time_ns_ = task->msg_real_time_ns();
//     while (is_paused_.load() && !is_stopped_.load()) {
//       if (is_playonce_.load()) {
//         break;
//       }
//       std::this_thread::sleep_for(std::chrono::nanoseconds(kPauseSleepNanoSec));
//       accumulated_pause_time_ns += kPauseSleepNanoSec;
//     }
//     task_buffer_->PopFront();
//   }
// }

#if defined(_ON_ORIN_) || defined(_ON_PEGASUS_)
void PlayTaskConsumer::writeImageToComponent(const std::string& channel_name, uint8_t* img, uint8_t channel_index, int width, int height, double measurement_time, double header_timestamp) {
  int img_size = 3 * width * height;
  auto rgb = (char*)malloc(img_size);
  auto lpb_image = std::make_shared<Image>();
  lpb_image->set_width(width);
  lpb_image->set_height(height);
  lpb_image->mutable_data()->reserve(img_size);
  lpb_image->mutable_header()->set_timestamp_sec(header_timestamp);
  lpb_image->set_measurement_time(measurement_time);
  std::string img_encoding = "rgb8";
  lpb_image->set_encoding(img_encoding);
  lpb_image->set_step(3 * width);
  rgba2rgb_neon((const uint8_t*)img, (uint8_t*)rgb, width, height);
  lpb_image->set_data(rgb, img_size);
  h264_decompressed_writers_[channel_name]->Write(lpb_image);
  free(rgb);
}
#endif

void PlayTaskConsumer::ThreadFunc() {
  uint64_t base_real_time_ns = 0;
  uint64_t accumulated_pause_time_ns = 0;

  while (!is_stopped_.load()) {
    auto task = task_buffer_->Front();
    if (task == nullptr) {
      std::this_thread::sleep_for(
          std::chrono::nanoseconds(kWaitProduceSleepNanoSec));
      continue;
    }

    uint64_t sleep_ns = 0;

    if (base_msg_play_time_ns_ == 0) {
      base_msg_play_time_ns_ = task->msg_play_time_ns();
      base_msg_real_time_ns_ = task->msg_real_time_ns();
      if (base_msg_play_time_ns_ > begin_time_ns_) {
        sleep_ns = static_cast<uint64_t>(
            static_cast<double>(base_msg_play_time_ns_ - begin_time_ns_) /
            play_rate_);
        while (sleep_ns > MIN_SLEEP_DURATION_NS && !is_stopped_.load()) {
          std::this_thread::sleep_for(
              std::chrono::nanoseconds(MIN_SLEEP_DURATION_NS));
          sleep_ns -= MIN_SLEEP_DURATION_NS;
        }

        if (is_stopped_.load()) {
          break;
        }

        std::this_thread::sleep_for(std::chrono::nanoseconds(sleep_ns));
      }
      base_real_time_ns = Time::Now().ToNanosecond();
      ADEBUG << "base_msg_play_time_ns: " << base_msg_play_time_ns_
             << "base_real_time_ns: " << base_real_time_ns;
    }

    uint64_t task_interval_ns = static_cast<uint64_t>(
        static_cast<double>(task->msg_play_time_ns() - base_msg_play_time_ns_) /
        play_rate_);
    uint64_t real_time_interval_ns = Time::Now().ToNanosecond() -
                                     base_real_time_ns -
                                     accumulated_pause_time_ns;
    if (task_interval_ns > real_time_interval_ns) {
      sleep_ns = task_interval_ns - real_time_interval_ns;
      std::this_thread::sleep_for(std::chrono::nanoseconds(sleep_ns));
    }

#if defined(_ON_ORIN_) || defined(_ON_PEGASUS_)
    auto channel_name = task->GetWriter()->GetChannelName();
    if (channel_name.substr(channel_name.find_last_of('/') + 1) == "image") {
      if (h264_decompressed_writers_.find(channel_name) == h264_decompressed_writers_.end()) {
        // auto construction_before = Time::Now().ToNanosecond();
        std::string decompressed_channel_name = channel_name;
        auto writer = node_->CreateWriter<apollo::drivers::Image>(decompressed_channel_name);
        h264_decompressed_writers_[channel_name] = writer;
        auto left_boundary = channel_name.substr(0, channel_name.find_last_of('/')).find_last_of('/')+1;
        auto filename = channel_name.substr(left_boundary).substr(0, channel_name.substr(left_boundary).find_last_of('/'));
        auto dw_decoder = PlayTaskProducer::getInstance()->GetDWH264Decoder(channel_name);
        dw_decoder->start();
        // auto construction_after = Time::Now().ToNanosecond();
        // std::cout << "start camera interval = " << construction_after - construction_before << std::endl;
      }
      // auto onprocess_before = Time::Now().ToNanosecond();
      auto uncompressed_image = std::make_shared<Image>();
      uncompressed_image->ParseFromString(task->GetMessage()->message);
      PlayTaskProducer::getInstance()->GetDWH264Decoder(channel_name)->onProcess(channel_name, uncompressed_image->measurement_time(), uncompressed_image->mutable_header()->timestamp_sec());
      // auto onprocess_after = Time::Now().ToNanosecond();
      // std::cout << "image processing time = " << onprocess_after - onprocess_before << std::endl;
    }
    else {
      task->Play();
    }
#else
    task->Play();
#endif
    is_playonce_.store(false);

    last_played_msg_real_time_ns_ = task->msg_real_time_ns();
    while (is_paused_.load() && !is_stopped_.load()) {
      if (is_playonce_.load()) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::nanoseconds(kPauseSleepNanoSec));
      accumulated_pause_time_ns += kPauseSleepNanoSec;
    }
    task_buffer_->PopFront();
  }
}

}  // namespace record
}  // namespace cyber
}  // namespace apollo
