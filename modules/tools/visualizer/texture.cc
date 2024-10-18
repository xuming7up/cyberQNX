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
* -1. Put big data (PointCloud, Image) into another shared memory not the cyber message object
*     to accelerate transmission speed.
* 
*/

#include "modules/tools/visualizer/texture.h"
#include "cyber/cyber.h"
#include "modules/common/shmlib/shared_mem_buffer.h"
#include <iostream>

static std::shared_ptr<SharedMemBuffer<uint8_t>> shm_buffer_ptr_= nullptr;

Texture::Texture()
    : is_size_changed_(false),
      is_dirty_(false),
      texture_format_(0),
      image_width_(0),
      image_height_(0),
      data_size_(0),
      data_(nullptr) {
        shm_buffer_ptr_ = std::make_shared<SharedMemBuffer<uint8_t>>("DwCameraBuffer", true);
      }

bool Texture::UpdateData(const QImage& img) {
  if (data_size_ < img.byteCount()) {
    if (!data_) {
      delete[] data_;
    }

    data_ = new GLubyte[img.byteCount()];
    if (data_ == nullptr) {
      data_size_ = 0;
      return false;
    }
    data_size_ = img.byteCount();
    is_size_changed_ = true;
  }

  image_height_ = img.height();
  image_width_ = img.width();

  memcpy(data_, img.bits(), img.byteCount());
  is_dirty_ = true;

  texture_format_ = GL_RGBA;
  return true;
}

bool Texture::UpdateData(
    const std::shared_ptr<const apollo::drivers::Image>& imgData) {
  std::size_t imgSize = imgData->width() * imgData->height() * 3;

  if (static_cast<std::size_t>(data_size_) < imgSize) {
    if (!data_) {
      delete[] data_;
    }

    data_ = new GLubyte[imgSize];
    if (data_ == nullptr) {
      data_size_ = 0;
      return false;
    }
    data_size_ = static_cast<GLsizei>(imgSize);
    is_size_changed_ = true;
  }

  texture_format_ = GL_RGB;
  ///////////////////////////////////////////////////////////////////////////////////
  //Shared memory support
  const uint8_t *data;
  std::size_t data_size = 0;
  std::shared_ptr<apollo::cyber::transport::ReadableBlock> rbPtr;
  if (imgData->has_block_index()) {
    rbPtr = shm_buffer_ptr_->getShmBlockToRead(imgData->block_index());
    data = reinterpret_cast<const uint8_t*>(rbPtr->buf);
    data_size = imgData->step() * imgData->height();
  } else {
    data = reinterpret_cast<const uint8_t*>(imgData->data().data());
    data_size = imgData->data().size();
  }
  ///////////////////////////////////////////////////////////////////////////////////

  if (imgData->encoding() == std::string("yuyv")) {
    const GLubyte* src =
        reinterpret_cast<const GLubyte*>(data);

    GLubyte* dst = data_;
    for (std::size_t i = 0; i < data_size; i += 4) {
      int y = 298 * (src[i] - 16);
      int u = src[i + 1] - 128;
      int u1 = 516 * u;
      int v = src[i + 3] - 128;
      int v1 = 208 * v;

      u *= 100;
      v *= 409;

#define CLAMP(v) static_cast<GLubyte>((v) > 255 ? 255 : ((v) < 0 ? 0 : v))
      *dst++ = CLAMP((y + v + 128) >> 8);
      *dst++ = CLAMP((y - u - v1 + 128) >> 8);
      *dst++ = CLAMP((y + u1 + 128) >> 8);

      y = 298 * (src[i + 2] - 16);

      *dst++ = CLAMP((y + v + 128) >> 8);
      *dst++ = CLAMP((y - u - v1 + 128) >> 8);
      *dst++ = CLAMP((y + u1 + 128) >> 8);
#undef CLAMP
    }
  } else if (imgData->encoding() == std::string("rgb8")) {
    memcpy(data_, data, imgSize);
  } else if (imgData->encoding() == std::string("bgr8")) {
    memcpy(data_, data, imgSize);
    texture_format_ = GL_BGR;
  } else {
    memset(data_, 0, imgSize);
    std::cerr << "Cannot support this format (" << imgData->encoding()
              << ") image" << std::endl;
  }
  is_dirty_ = true;
  ///////////////////////////////////////////////////////////////////////////////////
  //Shared memory support
  if (imgData->has_block_index()) {
    shm_buffer_ptr_->releaseReadBlock(*rbPtr);
  }
  ///////////////////////////////////////////////////////////////////////////////////

  image_height_ = imgData->height();
  image_width_ = imgData->width();

  return true;
}
