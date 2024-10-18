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

#include "cyber/tools/cyber_recorder/recorder.h"

#include "cyber/record/header_builder.h"
#define TEST_PERF 0

namespace apollo {
namespace cyber {
namespace record {

Recorder::Recorder(const std::string& output, bool all_channels,
                   const std::vector<std::string>& channel_vec)
    : output_(output), all_channels_(all_channels), channel_vec_(channel_vec) {
  header_ = HeaderBuilder::GetHeader();
  Init();
}

Recorder::Recorder(const std::string& output, bool all_channels,
                   const std::vector<std::string>& channel_vec,
                   const proto::Header& header)
    : output_(output),
      all_channels_(all_channels),
      channel_vec_(channel_vec),
      header_(header) {
        Init();
      }
//////////////////////////////////////////////////////////////////
//For point cloud
void Recorder::Init(){
  size_t max_points = 345600 + 38400 * 3;
  for(int i = 0; i < 4; ++i){
    auto point_cloud = std::make_shared<apollo::drivers::PointCloud>();
    point_cloud->mutable_point()->Reserve(max_points);
    for (size_t i = 0; i < max_points; i++) {
      point_cloud->add_point();
    }
    point_cloud_que_.Enqueue(point_cloud);
  }
}
//////////////////////////////////////////////////////////////////

Recorder::~Recorder() { Stop(); }

#if defined(_ON_ORIN_) || defined(_ON_PEGASUS_)
void Recorder::NotifyCameraCompressor(const bool start, const std::string &channel_name) {
  // if recording compressed channel, notify camera module to start transporting h264 coded data
  if (channel_name.substr(channel_name.find_last_of('/') + 1) == "h264_compressed" || channel_name.empty()) {
    std::cout << "recording channel " << channel_name << ", start notifying camera driver to start/stop compressing the raw image data" << std::endl;
    auto client = node_->CreateClient<H264CompressorNotify, H264CompressorNotify>("h264_compressor_notifier");
    auto msg = std::make_shared<H264CompressorNotify>();
    if (!channel_name.empty()) {
      msg->set_channel_name(channel_name);
    }
    msg->set_start(start);
    auto ip = apollo::cyber::common::GlobalData::Instance()->HostIp();
    msg->set_host_ip_addr(ip);
    int retry_times = 0;
    while (retry_times < 3) {
      auto res = client->SendRequest(msg);
      if (res != nullptr) {
        if (res->ack() && (channel_name.empty() || res->channel_name() == channel_name)) break;
        std::cout << "dwcamera_sending_world: responese: " << res->ShortDebugString() << std::endl;
      } else {
        std::cout << "cyber recorder: dwcamera_sending_world service may not ready, thus data on " << channel_name << " may not be synchronized" << std::endl;
      }
      retry_times++;
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << "notifying camera driver finished" << std::endl;
  }
}
#endif

// bool Recorder::Start() {
//   writer_.reset(new RecordWriter(header_));
//   if (!writer_->Open(output_)) {
//     AERROR << "Datafile open file error.";
//     return false;
//   }
//   std::string node_name = "cyber_recorder_record_" + std::to_string(getpid());
//   node_ = ::apollo::cyber::CreateNode(node_name);
//   if (node_ == nullptr) {
//     AERROR << "create node failed, node: " << node_name;
//     return false;
//   }
//   if (!InitReadersImpl()) {
//     AERROR << " _init_readers error.";
//     return false;
//   }
//   message_count_ = 0;
//   message_time_ = 0;
//   is_started_ = true;
//   display_thread_ =
//       std::make_shared<std::thread>([this]() { this->ShowProgress(); });
//   if (display_thread_ == nullptr) {
//     AERROR << "init display thread error.";
//     return false;
//   }
//   return true;
// }

bool Recorder::Start() {
  // if recording h264 compressed channel without the corresponding uncompressed channel, add the uncompressed channel to the channel list
  for (const auto &ch : channel_vec_) {
    if (ch.substr(ch.find_last_of('/') + 1) == "h264_compressed") {
      auto uncompressed_ch = ch.substr(0, ch.find_last_of('/'));
      // if recording h264_compressed channel data without the raw data channel, add the raw data channel to the record list
      if (std::find(channel_vec_.begin(), channel_vec_.end(), uncompressed_ch) == channel_vec_.end()) {
        channel_vec_.push_back(uncompressed_ch);
      }
    }
  }
  writer_.reset(new RecordWriter(header_));
  if (!writer_->Open(output_)) {
    AERROR << "Datafile open file error.";
    return false;
  }
  std::string node_name = "cyber_recorder_record_" + std::to_string(getpid());
  node_ = ::apollo::cyber::CreateNode(node_name);
  if (node_ == nullptr) {
    AERROR << "create node failed, node: " << node_name;
    return false;
  }
  if (!InitReadersImpl()) {
    AERROR << " _init_readers error.";
    return false;
  }
  message_count_ = 0;
  message_time_ = 0;
  is_started_ = true;
  display_thread_ =
      std::make_shared<std::thread>([this]() { this->ShowProgress(); });
  if (display_thread_ == nullptr) {
    AERROR << "init display thread error.";
    return false;
  }
  return true;
}

// bool Recorder::Stop() {
//   if (!is_started_ || is_stopping_) {
//     return false;
//   }
//   is_stopping_ = true;
//   if (!FreeReadersImpl()) {
//     AERROR << " _free_readers error.";
//     return false;
//   }
//   writer_->Close();
//   node_.reset();
//   if (display_thread_ && display_thread_->joinable()) {
//     display_thread_->join();
//     display_thread_ = nullptr;
//   }
//   is_started_ = false;
//   is_stopping_ = false;
//   return true;
// }

bool Recorder::Stop() {
  if (!is_started_ || is_stopping_) {
    return false;
  }
  is_stopping_ = true;
#if defined(_ON_ORIN_) || defined(_ON_PEGASUS_)
  for (const auto & iter : channel_reader_map_) {
    auto ch = iter.first;
    if (ch.substr(ch.find_last_of('/') + 1) == "h264_compressed") {
      NotifyCameraCompressor(false, ch);
    }
  }
#endif
  if (!FreeReadersImpl()) {
    AERROR << " _free_readers error.";
    return false;
  }
  writer_->Close();
  node_.reset();
  if (display_thread_ && display_thread_->joinable()) {
    display_thread_->join();
    display_thread_ = nullptr;
  }
  is_started_ = false;
  is_stopping_ = false;
  return true;
}

void Recorder::TopologyCallback(const ChangeMsg& change_message) {
  ADEBUG << "ChangeMsg in Topology Callback:" << std::endl
         << change_message.ShortDebugString();
  if (change_message.role_type() != apollo::cyber::proto::ROLE_WRITER) {
    ADEBUG << "Change message role type is not ROLE_WRITER.";
    return;
  }
  FindNewChannel(change_message.role_attr());
}

void Recorder::FindNewChannel(const RoleAttributes& role_attr) {
  if (!role_attr.has_host_ip() || role_attr.host_ip() != cyber::GlobalData::Instance()->HostIp()) {
    AWARN << "channel comes from remote ip, so to be discarded.";
    return;
  }
  if (!role_attr.has_channel_name() || role_attr.channel_name().empty()) {
    AWARN << "change message not has a channel name or has an empty one.";
    return;
  }
  if (!role_attr.has_message_type() || role_attr.message_type().empty()) {
    AWARN << "Change message not has a message type or has an empty one.";
    return;
  }
  if (!role_attr.has_proto_desc() || role_attr.proto_desc().empty()) {
    AWARN << "Change message not has a proto desc or has an empty one.";
    return;
  }
  if (!all_channels_ &&
      std::find(channel_vec_.begin(), channel_vec_.end(),
                role_attr.channel_name()) == channel_vec_.end()) {
    ADEBUG << "New channel was found, but not in record list.";
    return;
  }
  if (channel_reader_map_.find(role_attr.channel_name()) ==
      channel_reader_map_.end()) {
    if (!writer_->WriteChannel(role_attr.channel_name(),
                               role_attr.message_type(),
                               role_attr.proto_desc())) {
      AERROR << "write channel fail, channel:" << role_attr.channel_name();
    }
    InitReaderImpl(role_attr.channel_name(), role_attr.message_type());
  }
}

bool Recorder::InitReadersImpl() {
  std::shared_ptr<ChannelManager> channel_manager =
      TopologyManager::Instance()->channel_manager();

#if defined(_ON_ORIN_) || defined(_ON_PEGASUS_)
  // must make sure all camera start compressing data before recording
  if (all_channels_) {
      NotifyCameraCompressor(true);
  }
  else {
    for (const auto &ch : channel_vec_) {
      NotifyCameraCompressor(true, ch);
    }
  }
#endif

  // get historical writers
  std::vector<proto::RoleAttributes> role_attr_vec;
  channel_manager->GetWriters(&role_attr_vec);
  for (auto role_attr : role_attr_vec) {
    FindNewChannel(role_attr);
  }

  // listen new writers in future
  change_conn_ = channel_manager->AddChangeListener(
      std::bind(&Recorder::TopologyCallback, this, std::placeholders::_1));
  if (!change_conn_.IsConnected()) {
    AERROR << "change connection is not connected";
    return false;
  }
  return true;
}

bool Recorder::FreeReadersImpl() {
  std::shared_ptr<ChannelManager> channel_manager =
      TopologyManager::Instance()->channel_manager();

  channel_manager->RemoveChangeListener(change_conn_);

  return true;
}

bool Recorder::InitReaderImpl(const std::string& channel_name,
                              const std::string& message_type) {
  try {
    std::weak_ptr<Recorder> weak_this = shared_from_this();
    std::shared_ptr<ReaderBase> reader = nullptr;
    //////////////////////////////////////////////////////////////////
    //For point cloud
    if(message_type.find("PointCloud") != std::string::npos){
      point_cloud_channels_.insert(channel_name);
    }
    //////////////////////////////////////////////////////////////////
    auto callback = [weak_this, channel_name](
                        const std::shared_ptr<RawMessage>& raw_message) {
      auto share_this = weak_this.lock();
      if (!share_this) {
        return;
      }
      share_this->ReaderCallback(raw_message, channel_name);
    };
    ReaderConfig config;
    config.channel_name = channel_name;
    config.pending_queue_size =
        gflags::Int32FromEnv("CYBER_PENDING_QUEUE_SIZE", 50);
    reader = node_->CreateReader<RawMessage>(config, callback);
    if (reader == nullptr) {
      AERROR << "Create reader failed.";
      return false;
    }
    channel_reader_map_[channel_name] = reader;
    return true;
  } catch (const std::bad_weak_ptr& e) {
    AERROR << e.what();
    return false;
  }
}

void Recorder::ReaderCallback(const std::shared_ptr<RawMessage>& message,
                              const std::string& channel_name) {
  if (!is_started_ || is_stopping_) {
    AERROR << "record procedure is not started or stopping.";
    return;
  }

  if (message == nullptr) {
    AERROR << "message is nullptr, channel: " << channel_name;
    return;
  }
  
  /////////////////////////////////////////////////////////////
  //For Point cloud:
  bool is_pointcloud = (point_cloud_channels_.count(channel_name) == 1);
  std::shared_ptr<RawMessage> new_msg = nullptr;
  //当PointCloud使用共享内存信息拼接的时候RawMessge.ByteSize最大约为440多字节；
  //因为数据类型是RawMessge不能直接读到block_index && shared_mem_name 信息，
  //所以用ByteSize判断是否大于512来初步判断是否为共享内存数据(为减少直接反序列化带来的不必要的性能损耗)。
  if (is_pointcloud && message->ByteSize() < 512) {
#if TEST_PERF
    auto start_time_ns = apollo::cyber::Time::Now().ToNanosecond();
#endif
    std::shared_ptr<apollo::drivers::PointCloud> point_cloud_buf = nullptr;
    if (!point_cloud_que_.WaitDequeue(&point_cloud_buf)) {
      AERROR << "Get point cloud buf failed! ";
      return;
    }
    point_cloud_buf->Clear();
    point_cloud_buf->ParseFromString(message->message);

    if (point_cloud_buf->merged_point_clouds().size() > 0) {
      for (auto& p : point_cloud_buf->merged_point_clouds()) {
        bool ok = Transfer(p, *point_cloud_buf);
        if(!ok){
          AWARN << "Droped frame of merged lidar channel:" << channel_name;
          return;
        }
      }
    } else if (point_cloud_buf->has_shared_mem_name() && point_cloud_buf->has_block_index()) {
      bool ok = Transfer(*point_cloud_buf, *point_cloud_buf);
      if(!ok){
        AWARN << "Droped frame of lidar channel:" << channel_name;
        return;
      }
    } else {
      AERROR << "Unsupported point cloud data: " << point_cloud_buf->ShortDebugString();
      return;
    }

    point_cloud_buf->clear_merged_point_clouds();
    point_cloud_buf->clear_shared_mem_name();
    point_cloud_buf->clear_block_index();

    std::string content("");
    if (!point_cloud_buf->SerializeToString(&content)) {
      AERROR << "Failed to serialize point cloud, channel: " << channel_name;
      return;
    }
    new_msg = std::make_shared<RawMessage>(content, message->timestamp);
#if TEST_PERF
    double elps_ms = (apollo::cyber::Time::Now().ToNanosecond() - start_time_ns) / 1e6;
    AINFO << "ReaderCallback convert  lidar channel: " << channel_name
          << ", elps time(ms): " << elps_ms
          << ",point size: " << point_cloud_buf->point_size();
#endif
    point_cloud_que_.Enqueue(point_cloud_buf);
  }
  /////////////////////////////////////////////////////////////
  message_time_ = Time::Now().ToNanosecond();
  if (!writer_->WriteMessage(channel_name, ((is_pointcloud && new_msg != nullptr) ? new_msg : message), message_time_)) {
    AERROR << "write data fail, channel: " << channel_name;
    return;
  }

  message_count_++;
}

  /////////////////////////////////////////////////////////////
  //For Point cloud shared memory:
bool Recorder::Transfer(const apollo::drivers::PointCloud& src,apollo::drivers::PointCloud& dst)  {
  std::shared_ptr<SharedMemBuffer<uint8_t>> pointcloud_shm;
  if (pointcloud_shm_map_.find(src.shared_mem_name()) != pointcloud_shm_map_.end()) {
    pointcloud_shm = pointcloud_shm_map_.at(src.shared_mem_name());
  } else {
    pointcloud_shm = std::make_shared<SharedMemBuffer<uint8_t>>(src.shared_mem_name(), true);
    pointcloud_shm_map_.insert( std::make_pair(src.shared_mem_name(), pointcloud_shm));
  }
  auto rbPtr = pointcloud_shm->getShmBlockToRead(src.block_index());
  if (rbPtr == nullptr){
    return false;
  }
  for (unsigned int i = 0; i < src.width(); ++i) {
    const apollo::drivers::PointXYZIT& pt = *(( apollo::drivers::PointXYZIT*)(rbPtr->buf +  i * sizeof(apollo::drivers::PointXYZIT)));
    apollo::drivers::PointXYZIT *point = dst.add_point();
    point->CopyFrom(pt);
  }
  pointcloud_shm->releaseReadBlock(*rbPtr);
  return true;
}
/////////////////////////////////////////////////////////////

void Recorder::ShowProgress() {
  while (is_started_ && !is_stopping_) {
    std::cout << "\r[RUNNING]  Record Time: " << std::setprecision(3)
              << message_time_ / 1000000000
              << "    Progress: " << channel_reader_map_.size() << " channels, "
              << message_count_ << " messages";
    std::cout.flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  std::cout << std::endl;
}

}  // namespace record
}  // namespace cyber
}  // namespace apollo
