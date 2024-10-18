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
* -1.Using FFMPEG to decode h264 frames on platforms except Drive AGX Orin
*/

#ifndef CYBER_TOOLS_CYBER_RECORDER_PLAYER_PLAY_TASK_PRODUCER_H_
#define CYBER_TOOLS_CYBER_RECORDER_PLAYER_PLAY_TASK_PRODUCER_H_

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "cyber/message/raw_message.h"
#include "cyber/node/node.h"
#include "cyber/node/writer.h"
#include "cyber/record/record_reader.h"
#include "cyber/tools/cyber_recorder/player/play_param.h"
#include "cyber/tools/cyber_recorder/player/play_task_buffer.h"
#include "proto/camera/sensor_image.pb.h"

#if defined(_ON_ORIN_) || defined(_ON_PEGASUS_)
#include "dwcamera_deserializer.h"
#include "modules/drivers/dwcamera_sensing_world/neonutility.h"
#endif

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>
#include <libavutil/opt.h>
#include <libavutil/avassert.h>
#include <libavutil/imgutils.h>
#include <libavutil/mem.h>
#include <libswscale/swscale.h>
// #include <linux/videodev2.h>
}

#include <libavcodec/version.h>
#if LIBAVCODEC_VERSION_MAJOR < 55
#define AV_CODEC_ID_MJPEG CODEC_ID_MJPEG
#endif

#define IMAGE_SIZE_PER_BUFFER 5

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
#define av_frame_alloc  avcodec_alloc_frame
#endif

namespace apollo {
namespace cyber {
namespace record {

using apollo::drivers::Image;
using apollo::drivers::CompressedImage;
#if defined(_ON_ORIN_) || defined(_ON_PEGASUS_)
class DwCameraDeserializer;
#endif
class PlayTaskProducer {
 public:
  using NodePtr = std::shared_ptr<Node>;
  using ThreadPtr = std::unique_ptr<std::thread>;
  using TaskBufferPtr = std::shared_ptr<PlayTaskBuffer>;
  using RecordReaderPtr = std::shared_ptr<RecordReader>;
  using WriterPtr = std::shared_ptr<Writer<message::RawMessage>>;
  using WriterMap = std::unordered_map<std::string, WriterPtr>;
  using MessageTypeMap = std::unordered_map<std::string, std::string>;
  using H264CompressedImageChannelMap = std::unordered_map<std::string, std::vector<std::shared_ptr<CompressedImage>>>;

  PlayTaskProducer(const TaskBufferPtr& task_buffer,
                   const PlayParam& play_param);
  virtual ~PlayTaskProducer();

  static PlayTaskProducer* getInstance() {
    return sInstance_;
  }

  bool Init();
  void Start();
  void Stop();

  const PlayParam& play_param() const { return play_param_; }
  bool is_stopped() const { return is_stopped_.load(); }
  void PushDecodedDataToBuffer(uint8_t* img, uint8_t channel_index, int img_width, int img_height); 
#if defined(_ON_ORIN_) || defined(_ON_PEGASUS_)
  std::shared_ptr<DwCameraDeserializer> GetDWH264Decoder(const std::string & channel_name) {return dw_h264_decoders_[channel_name];}
  std::vector<std::shared_ptr<DwCameraDeserializer>> GetAllDWH264Decoders() {
    std::vector<std::shared_ptr<DwCameraDeserializer>> res;
    for (const auto& iter : dw_h264_decoders_) {
      res.emplace_back(iter.second);
    }
    return res;
  }
#endif

 private:
  static PlayTaskProducer* sInstance_;
  bool ReadRecordInfo();
  bool UpdatePlayParam();
  bool CreateWriters();
  void ThreadFunc();

  void PreloadH264DecompressedChannels();
  void InitDWH264Decoder();
  void InitFFMPEGH264Decompresser(const int channel_index, const std::string &channel_name);
  void InitDWH264Decompresser(const int channel_index, const std::string &filename);
  void Decode(const int channel_index, AVCodecContext *dec_ctx, AVFrame *frame, AVFrame *pFrameBGR, AVPacket *pkt);
  void AVFrame2Img(const int channel_index, AVFrame *pFrame, AVFrame *pFrameBGR);


  PlayParam play_param_;
  TaskBufferPtr task_buffer_;
  ThreadPtr produce_th_;
  

  std::atomic<bool> is_initialized_;
  std::atomic<bool> is_stopped_;

  NodePtr node_;
  WriterMap writers_;
  MessageTypeMap msg_types_;
  std::vector<RecordReaderPtr> record_readers_;
  H264CompressedImageChannelMap h264_channels;
  std::unordered_map<std::string, uint8_t> channel_name_map;
  std::vector<std::shared_ptr<cyber::Writer<apollo::drivers::Image>>> h264_decompressed_writers_;
  std::vector<std::queue<std::shared_ptr<Image>> >lpb_image_buffer;
  std::array<std::mutex, 16> image_buffer_locks;
  std::vector<struct SwsContext *>yuv2rgb_swscale_ctxs;
  std::vector<uint8_t*> rgb_buffers;
  std::vector<int> decompressed_data_counter;
  std::array<std::mutex, 16> loop_completion_locks;
  std::array<std::condition_variable, 16> loop_completion_cvs;
  std::array<std::condition_variable, 16> image_buffer_ready_cvs;
  volatile bool break_all_wait_ = false;

#if defined(_ON_ORIN_) || defined(_ON_PEGASUS_)
  std::map<std::string, std::shared_ptr<DwCameraDeserializer>> dw_h264_decoders_;
#endif
  std::map<std::string, bool> dw_decoder_start_flag;
  // the upper limit of the time dw deserializer takes to start the camera, statistics are acquired by some experiments
  // camera starting time varies on 8mp camera and 2mp camera
  std::map<std::string, int64_t> dw_decoder_start_time = {
    {"/apollo/sensor/camera/front_long/image", 160000000},
    {"/apollo/sensor/camera/front_medium/image", 160000000},
    {"/apollo/sensor/camera/front_short/image", 160000000},
    {"/apollo/sensor/camera/front_fisheye/image", 65000000},
    {"/apollo/sensor/camera/left_fisheye/image", 65000000},
    {"/apollo/sensor/camera/front_left_side/image", 65000000},
    {"/apollo/sensor/camera/rear_left_side/image", 65000000},
    {"/apollo/sensor/camera/rear_medium_short/image", 160000000},
    {"/apollo/sensor/camera/rear_fisheye/image", 65000000},
    {"/apollo/sensor/camera/right_fisheye/image", 65000000},
    {"/apollo/sensor/camera/front_right_side/image", 65000000},
    {"/apollo/sensor/camera/rear_right_side/image", 65000000}};

  // the time CPU takes to process the frame acquired from camera, unit ns
  // image processing time varies on 8mp camera and 2mp camera
  std::map<std::string, int64_t> dw_decoder_iamge_process_time = {
    {"/apollo/sensor/camera/front_long/image", 32000000},
    {"/apollo/sensor/camera/front_medium/image", 32000000},
    {"/apollo/sensor/camera/front_short/image", 32000000},
    {"/apollo/sensor/camera/front_fisheye/image", 2700000},
    {"/apollo/sensor/camera/left_fisheye/image", 2700000},
    {"/apollo/sensor/camera/front_left_side/image", 2700000},
    {"/apollo/sensor/camera/rear_left_side/image", 2700000},
    {"/apollo/sensor/camera/rear_medium_short/image", 32000000},
    {"/apollo/sensor/camera/rear_fisheye/image", 2700000},
    {"/apollo/sensor/camera/right_fisheye/image", 2700000},
    {"/apollo/sensor/camera/front_right_side/image", 2700000},
    {"/apollo/sensor/camera/rear_right_side/image", 2700000}};

  uint64_t earliest_begin_time_;
  uint64_t latest_end_time_;
  uint64_t total_msg_num_;

  static const uint32_t kMinTaskBufferSize;
  static const uint32_t kPreloadTimeSec;
  static const uint64_t kSleepIntervalNanoSec;
};

}  // namespace record
}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_TOOLS_CYBER_RECORDER_PLAYER_PLAY_TASK_PRODUCER_H_
