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

#include "cyber/tools/cyber_recorder/player/play_task_producer.h"

#include <iostream>
#include <limits>

#include "cyber/common/log.h"
#include "cyber/common/time_conversion.h"
#include "cyber/cyber.h"
#include "cyber/message/protobuf_factory.h"
#include "cyber/record/record_viewer.h"

#define INBUF_SIZE 4096

namespace apollo {
namespace cyber {
namespace record {

PlayTaskProducer* PlayTaskProducer::sInstance_ = nullptr;

const uint32_t PlayTaskProducer::kMinTaskBufferSize = 500;
const uint32_t PlayTaskProducer::kPreloadTimeSec = 3;
const uint64_t PlayTaskProducer::kSleepIntervalNanoSec = 1000000;

PlayTaskProducer::PlayTaskProducer(const TaskBufferPtr& task_buffer,
                                   const PlayParam& play_param)
    : play_param_(play_param),
      task_buffer_(task_buffer),
      produce_th_(nullptr),
      is_initialized_(false),
      is_stopped_(true),
      node_(nullptr),
      earliest_begin_time_(std::numeric_limits<uint64_t>::max()),
      latest_end_time_(0),
      total_msg_num_(0) {}

PlayTaskProducer::~PlayTaskProducer() { Stop(); }

bool PlayTaskProducer::Init() {
  if (is_initialized_.exchange(true)) {
    AERROR << "producer has been initialized.";
    return false;
  }

  if (!ReadRecordInfo() || !UpdatePlayParam() || !CreateWriters()) {
    is_initialized_.store(false);
    return false;
  }

  PlayTaskProducer::sInstance_ = this;
  return true;
  
}

// void PlayTaskProducer::Start() {
//   if (!is_initialized_.load()) {
//     AERROR << "please call Init firstly.";
//     return;
//   }

//   if (!is_stopped_.exchange(false)) {
//     AERROR << "producer has been started.";
//     return;
//   }

//   produce_th_.reset(new std::thread(&PlayTaskProducer::ThreadFunc, this));
// }

void PlayTaskProducer::Start() {
  if (!is_initialized_.load()) {
    AERROR << "please call Init firstly.";
    return;
  }
  if (!is_stopped_.exchange(false)) {
    AERROR << "producer has been started.";
    return;
  }
  std::cout << "Preloading camera compressed h264 data............................................." << std::endl;
  PreloadH264DecompressedChannels();
  if (h264_channels.size() != 0) {
#if defined(_ON_ORIN_) || defined(_ON_PEGASUS_)
    InitDWH264Decoder();
#else
    auto iter = h264_channels.begin();
    for (uint8_t i = 0; i < h264_channels.size(); ++i) {
      std::thread t{&PlayTaskProducer::InitFFMPEGH264Decompresser, this, i, iter->first};
      t.detach();
      iter++;
    }
#endif
    std::cout << "Preloading camera compressed h264 data finished" << std::endl;
  }
  produce_th_.reset(new std::thread(&PlayTaskProducer::ThreadFunc, this));
}

void PlayTaskProducer::Stop() {
  if (!is_stopped_.exchange(true)) {
    return;
  }
  if (produce_th_ != nullptr && produce_th_->joinable()) {
    produce_th_->join();
    produce_th_ = nullptr;
  }
  break_all_wait_ = true;
  for (uint8_t i = 0; i < 16; i++) {
    loop_completion_cvs[i].notify_all();
    image_buffer_ready_cvs[i].notify_all();
  }
}

#if defined(_ON_ORIN_) || defined(_ON_PEGASUS_)
void PlayTaskProducer::InitDWH264Decoder() {
  for (const auto & iter : h264_channels) {
    std::ofstream h264_file;
    auto ch = iter.first;
    auto right_boundary = ch.substr(0, ch.find_last_of("/")).find_last_of("/");
    auto filename = ch.substr(0, right_boundary).substr(ch.substr(0, right_boundary).find_last_of("/")+1);
    h264_file.open(std::string{"/apollo/"} + filename + std::string{".h264"});
    if (h264_file.fail()) {
      std::cerr << "Open File Error: " << strerror(errno);
      exit(1);
    }
    for (const auto &compressed_image : iter.second) {
      h264_file.write(((char*)(&compressed_image->data()[0])), compressed_image->data().size());
    }
    h264_file.close();
    auto compressed_ch = iter.first;
    auto uncompressed_ch = compressed_ch.substr(0, compressed_ch.find_last_of('/'));
    auto dw_decoder = std::make_shared<DwCameraDeserializer>(std::string{"video=/apollo/"} + filename + std::string{".h264"});
    dw_h264_decoders_[uncompressed_ch] = dw_decoder;
    dw_decoder->onInitialize();
  }
}
#endif

// void PlayTaskProducer::PushDecodedDataToBuffer(uint8_t* img, uint8_t channel_index, int img_width, int img_height) {
//   int img_size = 3 * img_width * img_height;
//   auto rgb = (char*)malloc(img_size);
//   auto lpb_image = std::make_shared<Image>();
//   lpb_image->set_width(img_width);
//   lpb_image->set_height(img_height);
//   lpb_image->mutable_data()->reserve(img_size);
//   std::string img_encoding = "rgb8";
//   lpb_image->set_encoding(img_encoding);
//   lpb_image->set_step(3 * img_width);
//   rgba2rgb_neon((const uint8_t*)img, (uint8_t*)rgb, img_width, img_height);
//   lpb_image->set_data(rgb, img_size);
//   std::lock_guard<std::mutex> guard(image_buffer_locks[channel_index]);
//   lpb_image_buffer[channel_index].push(lpb_image);
//   free(rgb);
//   decompressed_data_counter[channel_index]++;
// }


void PlayTaskProducer::PreloadH264DecompressedChannels() {
  auto record_viewer = std::make_shared<RecordViewer>(
      record_readers_, play_param_.begin_time_ns, play_param_.end_time_ns,
      play_param_.channels_to_play);
  auto itr = record_viewer->begin();
  auto itr_end = record_viewer->end();

  for (; itr != itr_end && !is_stopped_.load(); ++itr) {
    auto channel_name = itr->channel_name;
    auto search = writers_.find(channel_name);
    if (search == writers_.end()) {
      continue;
    }
    if (channel_name.substr(channel_name.find_last_of('/') + 1) == "h264_compressed") {
      auto raw_msg = std::make_shared<message::RawMessage>(itr->content);
      auto compressed_msg = std::make_shared<CompressedImage>();
      compressed_msg->ParseFromString(raw_msg->message);
      compressed_msg->set_measurement_time(itr->time);
      if (h264_channels.find(channel_name) == h264_channels.end()) {
        h264_channels[channel_name] = std::vector<std::shared_ptr<CompressedImage>>{compressed_msg};
      }
      else {
        h264_channels[channel_name].push_back(compressed_msg);
      }
    }
  }
  for (const auto & iter : h264_channels) {
    static uint8_t channel_index = 0;
    auto channel_name = iter.first;
    std::string uncompressed_channel_name = channel_name.substr(0, channel_name.find_last_of('/'));
    channel_name_map[uncompressed_channel_name] = channel_index++;
  }

  rgb_buffers.resize(h264_channels.size());
  yuv2rgb_swscale_ctxs.resize(h264_channels.size());
  lpb_image_buffer.resize(h264_channels.size());
  decompressed_data_counter.resize(h264_channels.size());
}

void PlayTaskProducer::AVFrame2Img(const int channel_index, AVFrame *pFrame, AVFrame *pFrameBGR)
{ 
  int frameHeight = pFrame->height;
  int frameWidth = pFrame->width;
  int channels = 3;
  auto lpb_image = std::make_shared<Image>();
  if (rgb_buffers[channel_index] == NULL) {
    int BGRsize = avpicture_get_size(AV_PIX_FMT_BGR24, frameWidth, frameHeight);
    // for optimization, av_malloc will use posix_memalign to memalign memory on 64 bytes
    // thus, if the image step size, eg, 3848 * 3, can not be placed in a 64 bytes memory-aligned block, 
    // need to modify the image step size size to make sure it is 64 bytes memory-aligned
    if (frameWidth * channels % 64) {
      int mem_aligned_step = (frameWidth * channels + 63) & ~(decltype(frameWidth * channels))63;
      BGRsize = mem_aligned_step * frameHeight;
    }
    rgb_buffers[channel_index] = (uint8_t *) av_malloc(BGRsize);
    avpicture_fill((AVPicture *) pFrameBGR, rgb_buffers[channel_index], AV_PIX_FMT_BGR24,
                    frameWidth, frameHeight);
    yuv2rgb_swscale_ctxs[channel_index] = sws_getContext(frameWidth, frameHeight, AV_PIX_FMT_YUV420P, frameWidth, frameHeight, AV_PIX_FMT_RGB24, 0, 0, 0, 0);
    
  }
  lpb_image->set_width(frameWidth);
  lpb_image->set_height(frameHeight);
  lpb_image->mutable_data()->reserve(frameHeight * frameWidth * channels);
  std::string img_encoding = "rgb8";
  lpb_image->set_encoding(img_encoding);
  lpb_image->set_step(3 * frameWidth);
  lpb_image->set_measurement_time(pFrame->pts);
  /*********************************** yuv to rgb with sws_scale *******************************/
  sws_scale(yuv2rgb_swscale_ctxs[channel_index], (const uint8_t *const *)pFrame->data, pFrame->linesize, 0, frameHeight, pFrameBGR->data, pFrameBGR->linesize);
  lpb_image->set_data(rgb_buffers[channel_index], frameHeight * frameWidth * 3);
  // lpb_image->mutable_header()->set_timestamp_sec(cyber::Time::Now().ToSecond());
  std::lock_guard<std::mutex> guard(image_buffer_locks[channel_index]);
  lpb_image_buffer[channel_index].push(lpb_image);
  image_buffer_ready_cvs[channel_index].notify_one();
  
  // h264_decompressed_writers_[channel_index]->Write(lpb_image);
  decompressed_data_counter[channel_index]++;
  /*********************************** yuv to rgb with sws_scale *******************************/
}

void PlayTaskProducer::Decode(const int channel_index, AVCodecContext *dec_ctx, AVFrame *frame, AVFrame *pFrameBGR, AVPacket *pkt)
{           
    int ret;

    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error sending a packet for decoding\n");
        exit(1);
    }
    
    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return;
        }
        else if (ret < 0) {
            fprintf(stderr, "Error during decoding\n");
            exit(1);
        }
        AVFrame2Img(channel_index, frame, pFrameBGR);
    }
}

void PlayTaskProducer::InitFFMPEGH264Decompresser(const int channel_index, const std::string &channel_name) {
    const AVCodec *codec;
    AVCodecParserContext *parser;
    AVCodecContext *c= NULL;
    AVFrame *frame;
    AVFrame *pFrameBGR;
    uint8_t *data;
    size_t   data_size;
    int ret;
    AVPacket *pkt;
    uint64_t index = 0;

    pkt = av_packet_alloc();
    if (!pkt) {
        fprintf(stderr, "failed to allocate av packet\n");
        exit(1);
    }

    codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        exit(1);
    }

    parser = av_parser_init(codec->id);
    if (!parser) {
        fprintf(stderr, "parser not found\n");
        exit(1);
    }

    c = avcodec_alloc_context3(codec);
    if (!c) {
        fprintf(stderr, "Could not allocate video codec context\n");
        exit(1);
    }

    /* open it */
    if (avcodec_open2(c, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }

    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }
    pFrameBGR = av_frame_alloc();
    if (!pFrameBGR) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }

    do {
        /* read raw data */
        image_buffer_locks[channel_index].lock();
        if (lpb_image_buffer[channel_index].size() >= IMAGE_SIZE_PER_BUFFER){
            image_buffer_locks[channel_index].unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        else {
            image_buffer_locks[channel_index].unlock();
        }
        
        // one loop finished, need to clear leftover data in the buffer
        if (index == h264_channels[channel_name].size()) {
            // std::cout << "channel_name = " << channel_name << ", msg count = " << decompressed_data_counter[channel_index] << std::endl;
            std::unique_lock<std::mutex> lock(loop_completion_locks[channel_index]);
            loop_completion_cvs[channel_index].wait(lock);
            std::lock_guard<std::mutex> guard(image_buffer_locks[channel_index]);
            while(!lpb_image_buffer[channel_index].empty()) {
              lpb_image_buffer[channel_index].pop();
            }
            index = 0;
        }

        /* use the parser to split the data into frames */
        auto compressed_img = h264_channels[channel_name][index];
        data = (uint8_t*)(&compressed_img->data()[0]);
        data_size = compressed_img->data().size();
        while (data_size > 0) {
            ret = av_parser_parse2(parser, c, &pkt->data, &pkt->size,
                                   data, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
            if (ret < 0) {
                fprintf(stderr, "Error while parsing\n");
                exit(1);
            }
            data      += ret;
            data_size -= ret;

            if (pkt->size) {
                pkt->pts = (int64_t)compressed_img->measurement_time();
                Decode(channel_index, c, frame, pFrameBGR, pkt);
            }
        }
        index++;
    } while (!is_stopped_.load());

    /* flush the decoder */
    Decode(channel_index, c, frame, pFrameBGR, pkt);

    av_parser_close(parser);
    avcodec_free_context(&c);
    av_frame_free(&frame);
    av_frame_free(&pFrameBGR);
    av_packet_free(&pkt);
    av_free(rgb_buffers[channel_index]);

}

// void PlayTaskProducer::InitDWH264Decompresser(const int channel_index, const std::string &filename) {
//   std::cout << "filename = " << filename << std::endl;
//   auto dw_deserializer = std::make_shared<apollo::drivers::dwcamera::DwCameraDeserializer>(filename, channel_index);
//   while (!is_stopped_.load()) {
//     dw_deserializer->run();
//   }
//   dw_deserializer->stop();
// }

bool PlayTaskProducer::ReadRecordInfo() {
  if (play_param_.files_to_play.empty()) {
    AINFO << "no file to play.";
    return false;
  }

  auto pb_factory = message::ProtobufFactory::Instance();

  // loop each file
  for (auto& file : play_param_.files_to_play) {
    auto record_reader = std::make_shared<RecordReader>(file);
    if (!record_reader->IsValid()) {
      continue;
    }
    if (!record_reader->GetHeader().is_complete()) {
      std::cout << "file: " << file << " is not complete." << std::endl;
      continue;
    }

    record_readers_.emplace_back(record_reader);

    auto channel_list = record_reader->GetChannelList();
    // loop each channel info
    for (auto& channel_name : channel_list) {
      // std::cout << "channel_name = " << channel_name << ", msg num = " << record_reader->GetMessageNumber(channel_name) << std::endl;
      if (play_param_.black_channels.find(channel_name) !=
          play_param_.black_channels.end()) {
        // minus the black message number from record file header
        total_msg_num_ -= record_reader->GetMessageNumber(channel_name);
        continue;
      }

      auto& msg_type = record_reader->GetMessageType(channel_name);
      msg_types_[channel_name] = msg_type;

      if (!play_param_.is_play_all_channels &&
          play_param_.channels_to_play.count(channel_name) > 0) {
        total_msg_num_ += record_reader->GetMessageNumber(channel_name);
      }
      auto& proto_desc = record_reader->GetProtoDesc(channel_name);
      pb_factory->RegisterMessage(proto_desc);
    }

    auto& header = record_reader->GetHeader();
    if (play_param_.is_play_all_channels) {
      total_msg_num_ += header.message_number();
    }

    if (header.begin_time() < earliest_begin_time_) {
      earliest_begin_time_ = header.begin_time();
    }
    if (header.end_time() > latest_end_time_) {
      latest_end_time_ = header.end_time();
    }

    auto begin_time_s = static_cast<double>(header.begin_time()) / 1e9;
    auto end_time_s = static_cast<double>(header.end_time()) / 1e9;
    auto begin_time_str =
        common::UnixSecondsToString(static_cast<int>(begin_time_s));
    auto end_time_str =
        common::UnixSecondsToString(static_cast<int>(end_time_s));

    std::cout << "file: " << file << ", chunk_number: " << header.chunk_number()
              << ", begin_time: " << header.begin_time() << " ("
              << begin_time_str << ")"
              << ", end_time: " << header.end_time() << " (" << end_time_str
              << ")"
              << ", message_number: " << header.message_number() << std::endl;
  }

  std::cout << "earliest_begin_time: " << earliest_begin_time_
            << ", latest_end_time: " << latest_end_time_
            << ", total_msg_num: " << total_msg_num_ << std::endl;

  return true;
}

bool PlayTaskProducer::UpdatePlayParam() {
  if (play_param_.begin_time_ns < earliest_begin_time_) {
    play_param_.begin_time_ns = earliest_begin_time_;
  }
  if (play_param_.start_time_s > 0) {
    play_param_.begin_time_ns += static_cast<uint64_t>(
        static_cast<double>(play_param_.start_time_s) * 1e9);
  }
  if (play_param_.end_time_ns > latest_end_time_) {
    play_param_.end_time_ns = latest_end_time_;
  }
  if (play_param_.begin_time_ns >= play_param_.end_time_ns) {
    AERROR << "begin time are equal or larger than end time"
           << ", begin_time_ns=" << play_param_.begin_time_ns
           << ", end_time_ns=" << play_param_.end_time_ns;
    return false;
  }
  if (play_param_.preload_time_s == 0) {
    AINFO << "preload time is zero, we will use defalut value: "
          << kPreloadTimeSec << " seconds.";
    play_param_.preload_time_s = kPreloadTimeSec;
  }
  return true;
}

bool PlayTaskProducer::CreateWriters() {
  std::string node_name = "cyber_recorder_play_" + std::to_string(getpid());
  node_ = apollo::cyber::CreateNode(node_name);
  if (node_ == nullptr) {
    AERROR << "create node failed.";
    return false;
  }

  for (auto& item : msg_types_) {
    auto& channel_name = item.first;
    auto& msg_type = item.second;

    if (play_param_.is_play_all_channels ||
        play_param_.channels_to_play.count(channel_name) > 0) {
      if (play_param_.black_channels.find(channel_name) !=
          play_param_.black_channels.end()) {
        continue;
      }
      proto::RoleAttributes attr;
      attr.set_channel_name(channel_name);
      attr.set_message_type(msg_type);
      auto writer = node_->CreateWriter<message::RawMessage>(attr);
      if (writer == nullptr) {
        AERROR << "create writer failed. channel name: " << channel_name
               << ", message type: " << msg_type;
        return false;
      }
      writers_[channel_name] = writer;
    }
  }

  return true;
}

// void PlayTaskProducer::ThreadFunc() {
//   const uint64_t loop_time_ns =
//       play_param_.end_time_ns - play_param_.begin_time_ns;
//   uint64_t avg_interval_time_ns = kSleepIntervalNanoSec;
//   if (total_msg_num_ > 0) {
//     avg_interval_time_ns = loop_time_ns / total_msg_num_;
//   }

//   double avg_freq_hz = static_cast<double>(total_msg_num_) /
//                        (static_cast<double>(loop_time_ns) * 1e-9);
//   uint32_t preload_size = (uint32_t)avg_freq_hz * play_param_.preload_time_s;
//   AINFO << "preload_size: " << preload_size;
//   if (preload_size < kMinTaskBufferSize) {
//     preload_size = kMinTaskBufferSize;
//   }

//   auto record_viewer = std::make_shared<RecordViewer>(
//       record_readers_, play_param_.begin_time_ns, play_param_.end_time_ns,
//       play_param_.channels_to_play);

//   uint32_t loop_num = 0;
//   while (!is_stopped_.load()) {
//     uint64_t plus_time_ns = loop_num * loop_time_ns;
//     auto itr = record_viewer->begin();
//     auto itr_end = record_viewer->end();

//     while (itr != itr_end && !is_stopped_.load()) {
//       while (!is_stopped_.load() && task_buffer_->Size() > preload_size) {
//         std::this_thread::sleep_for(
//             std::chrono::nanoseconds(avg_interval_time_ns));
//       }
//       for (; itr != itr_end && !is_stopped_.load(); ++itr) {
//         if (task_buffer_->Size() > preload_size) {
//           break;
//         }

//         auto search = writers_.find(itr->channel_name);
//         if (search == writers_.end()) {
//           continue;
//         }

//         auto raw_msg = std::make_shared<message::RawMessage>(itr->content);
//         auto task = std::make_shared<PlayTask>(
//             raw_msg, search->second, itr->time, itr->time + plus_time_ns);
//         task_buffer_->Push(task);
//       }
//     }

//     if (!play_param_.is_loop_playback) {
//       is_stopped_.store(true);
//       break;
//     }
//     ++loop_num;
//   }
// }

void PlayTaskProducer::ThreadFunc() {
  const uint64_t loop_time_ns =
      play_param_.end_time_ns - play_param_.begin_time_ns;
  uint64_t avg_interval_time_ns = kSleepIntervalNanoSec;
  if (total_msg_num_ > 0) {
    avg_interval_time_ns = loop_time_ns / total_msg_num_;
  }

  double avg_freq_hz = static_cast<double>(total_msg_num_) /
                       (static_cast<double>(loop_time_ns) * 1e-9);
  uint32_t preload_size = (uint32_t)avg_freq_hz * play_param_.preload_time_s;
  AINFO << "preload_size: " << preload_size;
  if (preload_size < kMinTaskBufferSize) {
    preload_size = kMinTaskBufferSize;
  }

  auto record_viewer = std::make_shared<RecordViewer>(
      record_readers_, play_param_.begin_time_ns, play_param_.end_time_ns,
      play_param_.channels_to_play);

  uint32_t loop_num = 0;
  while (!is_stopped_.load()) {
    uint64_t plus_time_ns = loop_num * loop_time_ns;
    auto itr = record_viewer->begin();
    auto itr_end = record_viewer->end();

    while (itr != itr_end && !is_stopped_.load()) {
      while (!is_stopped_.load() && task_buffer_->Size() > preload_size) {
        std::this_thread::sleep_for(
            std::chrono::nanoseconds(avg_interval_time_ns));
      }
      for (; itr != itr_end && !is_stopped_.load(); ++itr) {
        if (task_buffer_->Size() > preload_size) {
          break;
        }

        auto search = writers_.find(itr->channel_name);
        if (search == writers_.end()) {
          continue;
        }

        auto raw_msg = std::make_shared<message::RawMessage>(itr->content);
        std::shared_ptr<PlayTask> task;
#if (!defined(_ON_ORIN_) && !defined(_ON_PEGASUS_))
        // Make a one to one match of the original data and the decoded data
        // So that the decoded data will have the same timestamp as the original data
        if (channel_name_map.find(itr->channel_name) != channel_name_map.end()) {
          auto shm_image = std::make_shared<Image>();
          shm_image->ParseFromString(raw_msg->message);
          auto channel_index = channel_name_map[itr->channel_name];
          std::unique_lock<std::mutex> buffer_lock(image_buffer_locks[channel_index]);
          if (lpb_image_buffer[channel_index].empty()) {
            image_buffer_ready_cvs[channel_index].wait_for(buffer_lock, std::chrono::seconds(1), 
                                  [channel_index, this]() { return break_all_wait_ || !lpb_image_buffer[channel_index].empty(); });
            if (lpb_image_buffer[channel_index].empty()) {
              std::cout << "h264 decompressor reached end of stream, thus skip this frame" << std::endl;
              continue;
            }
          }
          auto decompressed_image = lpb_image_buffer[channel_index].front();
          // cases when current decoded frame is within the same timestamp as the shared memory frame
          // pop the current decoded frame from the image buffer, and construct a new frame with the shared memory frame timestamp and the decoded frame content 
          if (decompressed_image->measurement_time() < itr->time && itr->time / 1000000UL - decompressed_image->measurement_time() / 1000000UL < 33) {
            lpb_image_buffer[channel_index].pop();
            buffer_lock.unlock();
            decompressed_image->mutable_header()->set_timestamp_sec(shm_image->mutable_header()->timestamp_sec());
            decompressed_image->set_measurement_time(shm_image->measurement_time());
            std::string tmp;
            decompressed_image->SerializeToString(&tmp);
            auto decompressed_raw_message = std::make_shared<message::RawMessage>(tmp);
            task = std::make_shared<PlayTask>(
                decompressed_raw_message, search->second, itr->time, itr->time + plus_time_ns);
          }
          // cases when current decoded frame is out of sync with the shared memory frame
          else {
            buffer_lock.unlock();

            /* case 1: missing decoded frame
            eg: shared memory frame timestamp: 10 -> 40 -> 70 -> 100 -> 130 ....
                decoded frame timestamp:       8  -> 38 -> 68 -> 400 ..... 
            in this case, skipping the shared memory frame by making a empty task, 
            until the next shared memory frame is within the same timestamp as the current decoded frame */ 
            if (decompressed_image->measurement_time() > itr->time) {
              // nothing, just skip this frame
            }

            /* case 2: missing shared memory frame
            eg: shared memory frame timestamp: 10 -> 40 -> 70 -> 300 -> 330 ....
                decoded frame timestamp:       8  -> 38 -> 68 -> 98  -> 128 ...
            in this case, keep popping the decoded frame in the decoded frame buffer, 
            until the next decoded frame in the decoded frame buffer that has relatively same timestamp as the shared memory image */
            else {
              bool found_next_frame = false;
              buffer_lock.lock();
              while(lpb_image_buffer[channel_index].size() > 0) {
                auto decompressed_image = lpb_image_buffer[channel_index].front();
                if (decompressed_image->measurement_time() < itr->time && itr->time / 1000000UL - decompressed_image->measurement_time() / 1000000UL < 33) {
                  lpb_image_buffer[channel_index].pop();
                  buffer_lock.unlock();
                  decompressed_image->mutable_header()->set_timestamp_sec(shm_image->mutable_header()->timestamp_sec());
                  decompressed_image->set_measurement_time(shm_image->measurement_time());
                  std::string tmp;
                  decompressed_image->SerializeToString(&tmp);
                  auto decompressed_raw_message = std::make_shared<message::RawMessage>(tmp);
                  task = std::make_shared<PlayTask>(
                      decompressed_raw_message, search->second, itr->time, itr->time + plus_time_ns);
                  found_next_frame = true;
                  break;
                }
                else {
                  lpb_image_buffer[channel_index].pop();
                }
              }
              if (!found_next_frame) {
                buffer_lock.unlock();
              }
            }
          }
        }
        else {
          task = std::make_shared<PlayTask>(
              raw_msg, search->second, itr->time, itr->time + plus_time_ns);
        }
#else
        if (itr->channel_name.substr(itr->channel_name.find_last_of('/') + 1) == "image") {
          if (dw_decoder_start_flag.find(itr->channel_name) == dw_decoder_start_flag.end()) {
            dw_decoder_start_flag[itr->channel_name] = true;
            task = std::make_shared<PlayTask>(
            raw_msg, search->second, itr->time - dw_decoder_start_time[itr->channel_name] - dw_decoder_iamge_process_time[itr->channel_name], 
            itr->time + plus_time_ns - dw_decoder_start_time[itr->channel_name] - dw_decoder_iamge_process_time[itr->channel_name]);
          }
          else {
            task = std::make_shared<PlayTask>(
            raw_msg, search->second, itr->time - dw_decoder_iamge_process_time[itr->channel_name], 
            itr->time + plus_time_ns - dw_decoder_iamge_process_time[itr->channel_name]);
          }
        }
        else {
          task = std::make_shared<PlayTask>(
            raw_msg, search->second, itr->time, itr->time + plus_time_ns);
        }
#endif
        task_buffer_->Push(task);
      }
    }

    if (!play_param_.is_loop_playback) {
      is_stopped_.store(true);
      break;
    }
    ++loop_num;
#if (!defined(_ON_ORIN_) && !defined(_ON_PEGASUS_))
    for (uint i = 0; i < h264_channels.size(); i++) {
      std::lock_guard<std::mutex> lock(loop_completion_locks[i]);
      loop_completion_cvs[i].notify_one();
    }
#endif   
  }
}

}  // namespace record
}  // namespace cyber
}  // namespace apollo
