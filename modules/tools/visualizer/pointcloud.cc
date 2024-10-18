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

#include "modules/tools/visualizer/pointcloud.h"
#include "cyber/cyber.h"
#include "modules/common/shmlib/shared_mem_buffer.h"
#include <iostream>

static std::map<std::string,std::shared_ptr<SharedMemBuffer<uint8_t>>> shared_mem_map_;

PointCloud::PointCloud(
    int pointCount, int vertexElementCount,
    const std::shared_ptr<QOpenGLShaderProgram>& shaderProgram)
    : RenderableObject(pointCount, vertexElementCount, shaderProgram),
      buffer_(nullptr) {}

PointCloud::~PointCloud(void) {
  if (buffer_) {
    delete[] buffer_;
    buffer_ = nullptr;
  }
}

bool PointCloud::FillVertexBuffer(GLfloat* pBuffer) {
  if (buffer_ && pBuffer) {
    memcpy(pBuffer, buffer_, VertexBufferSize());
    delete[] buffer_;
    buffer_ = nullptr;
    return true;
  } else {
    std::cout << "---Error!!! cannot upload data to Graphics Card----"
              << std::endl;
    return false;
  }
}

/*bool PointCloud::FillData(
    const std::shared_ptr<const apollo::drivers::PointCloud>& pdata) {
  assert(vertex_count() == pdata->point_size());
  buffer_ = new GLfloat[vertex_count() * vertex_element_count()];
  if (buffer_) {
    GLfloat* tmp = buffer_;

    for (int i = 0; i < vertex_count(); ++i, tmp += vertex_element_count()) {
      const apollo::drivers::PointXYZIT& point = pdata->point(i);
      tmp[0] = point.x();
      tmp[1] = point.z();
      tmp[2] = -point.y();
      tmp[3] = static_cast<float>(point.intensity());
    }
    return true;
  }
  return false;
}*/

bool PointCloud::FillData(const std::vector<const apollo::drivers::PointCloud*>& point_cloud_arr){
  buffer_ = new GLfloat[vertex_count() * vertex_element_count()];
  if (!buffer_) {
    return false;
  }
  //AERROR << "###Fill data size: " << vertex_count() * vertex_element_count();

  GLfloat* tmp = buffer_;
  
  for(auto& pdata : point_cloud_arr){
    if (pdata->point_size() == 0 && pdata->has_shared_mem_name()) {

    std::shared_ptr<SharedMemBuffer<uint8_t>> shm_buffer_ptr =  nullptr;
    if(shared_mem_map_.find(pdata->shared_mem_name()) != shared_mem_map_.end()){
      shm_buffer_ptr = shared_mem_map_.at(pdata->shared_mem_name());
    }else{
      shm_buffer_ptr = std::make_shared<SharedMemBuffer<uint8_t>>(pdata->shared_mem_name(), true);
      shared_mem_map_.insert(std::make_pair(pdata->shared_mem_name(),shm_buffer_ptr));
    }

    auto rbPtr = shm_buffer_ptr->getShmBlockToRead(pdata->block_index());

    for (uint32_t i = 0; i < pdata->width();  ++i, tmp += vertex_element_count()) {
      const apollo::drivers::PointXYZIT& point = *((apollo::drivers::PointXYZIT*)(rbPtr->buf + i * sizeof(apollo::drivers::PointXYZIT)));
      tmp[0] = point.x();
      tmp[1] = point.z();
      tmp[2] = -point.y();
      tmp[3] = static_cast<float>(point.intensity() > 172 ? point.intensity() - 100 : point.intensity());
    }
    //AERROR << "-----Fill point cloud: " << pdata->has_shared_mem_name() << " points: " << pdata->width();
    shm_buffer_ptr->releaseReadBlock(*rbPtr);
  }else {
    for (int j = 0; j < vertex_count(); ++j, tmp += vertex_element_count()) {
      const apollo::drivers::PointXYZIT& point = pdata->point(j);
      tmp[0] = point.x();
      tmp[1] = point.z();
      tmp[2] = -point.y();
      tmp[3] = static_cast<float>(point.intensity() > 172 ? point.intensity() - 100 : point.intensity());
    }
  }

  }
  
  return true;
}
