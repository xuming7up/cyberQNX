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
* -1.Cyber recorder will replace lidar and camera shared memory buffer with the real data during recording
* -2.Cyber recorder will notify camera driver to compress the images with h264 format before recording the camera driver data
*/

#ifndef CYBER_TOOLS_CYBER_RECORDER_RECORDER_H_
#define CYBER_TOOLS_CYBER_RECORDER_RECORDER_H_

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "cyber/base/signal.h"
#include "cyber/cyber.h"
#include "cyber/message/raw_message.h"
#include "cyber/proto/record.pb.h"
#include "cyber/proto/topology_change.pb.h"
#include "cyber/record/record_writer.h"
#include "proto/lidar/pointcloud.pb.h"
#include "modules/common/shmlib/shared_mem_buffer.h"
#include "cyber/base/thread_safe_queue.h"

using apollo::cyber::Node;
using apollo::cyber::ReaderBase;
using apollo::cyber::base::Connection;
using apollo::cyber::message::RawMessage;
using apollo::cyber::proto::ChangeMsg;
using apollo::cyber::proto::RoleAttributes;
using apollo::cyber::proto::RoleType;
using apollo::cyber::service_discovery::ChannelManager;
using apollo::cyber::service_discovery::TopologyManager;

using apollo::cyber::proto::H264CompressorNotify;

namespace apollo {
namespace cyber {
namespace record {

class Recorder : public std::enable_shared_from_this<Recorder> {
 public:
  Recorder(const std::string& output, bool all_channels,
           const std::vector<std::string>& channel_vec);
  Recorder(const std::string& output, bool all_channels,
           const std::vector<std::string>& channel_vec,
           const proto::Header& header);
  ~Recorder();
  bool Start();
  bool Stop();

 private:
  bool is_started_ = false;
  bool is_stopping_ = false;
  std::shared_ptr<Node> node_ = nullptr;
  std::shared_ptr<RecordWriter> writer_ = nullptr;
  std::shared_ptr<std::thread> display_thread_ = nullptr;
  Connection<const ChangeMsg&> change_conn_;
  std::string output_;
  bool all_channels_ = true;
  std::vector<std::string> channel_vec_;
  proto::Header header_;
  std::unordered_map<std::string, std::shared_ptr<ReaderBase>>
      channel_reader_map_;
  uint64_t message_count_;
  uint64_t message_time_;
  /////////////////////////////////////////////////////////////
  //For Point cloud:
  std::set<std::string> point_cloud_channels_;
  std::map<std::string,std::shared_ptr<SharedMemBuffer<uint8_t>>> pointcloud_shm_map_;
  apollo::cyber::base::ThreadSafeQueue<std::shared_ptr<apollo::drivers::PointCloud>> point_cloud_que_;
  void Init();
  bool Transfer(const apollo::drivers::PointCloud& src,apollo::drivers::PointCloud& dst);
  /////////////////////////////////////////////////////////////
  bool InitReadersImpl();

  bool FreeReadersImpl();

  bool InitReaderImpl(const std::string& channel_name,
                      const std::string& message_type);

  void TopologyCallback(const ChangeMsg& msg);

  void ReaderCallback(const std::shared_ptr<RawMessage>& message,
                      const std::string& channel_name);

  void FindNewChannel(const RoleAttributes& role_attr);

  void ShowProgress();

#if defined(_ON_ORIN_) || defined(_ON_PEGASUS_)
  void NotifyCameraCompressor(const bool start, const std::string &channel_name = "");
#endif
};

}  // namespace record
}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_TOOLS_CYBER_RECORDER_RECORDER_H_
