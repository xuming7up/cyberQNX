/******************************************************************************
 * Copyright 2024 The HyperLink Authors. All Rights Reserved.
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

#pragma once

// #include <boost/interprocess/managed_shared_memory.hpp>
// #include <boost/interprocess/sync/named_mutex.hpp>
// #include <boost/interprocess/sync/named_condition.hpp>
// #include <boost/interprocess/sync/scoped_lock.hpp>
// #include <boost/smart_ptr/make_shared.hpp>
// #include <boost/shared_ptr.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include <map>

// #include "cyber/cyber.h"
#include "cyber/transport/shm/segment_factory.h"
#include "cyber/common/util.h"

/**
 * @brief This template class create only one shared memory region
 *        in order to keep uniformity with shared memory interface,
 *        we use boost smart pointer.
 * @tparam T 
 */
template <typename T>
class SharedMemBuffer
{
public:
  SharedMemBuffer(std::string channel_name, bool reserve) {
    uint64_t channel_id = apollo::cyber::common::Hash(channel_name);
    segment_ = apollo::cyber::transport::SegmentFactory::CreateSegment(channel_id);
  }

  std::shared_ptr<apollo::cyber::transport::WritableBlock> getShmBlockToWrite(int size) {
    std::shared_ptr<apollo::cyber::transport::WritableBlock> wbPtr = 
      std::make_shared<apollo::cyber::transport::WritableBlock>();
    if (!segment_->AcquireBlockToWrite(size, wbPtr.get())) {
      AERROR << "acquire block failed.";
      return std::shared_ptr<apollo::cyber::transport::WritableBlock>();
    }
    ADEBUG << "block index: " << wbPtr->index;
    return wbPtr;
  }
  void releaseWrittenBlock(apollo::cyber::transport::WritableBlock wb) {
    segment_->ReleaseWrittenBlock(wb);
  }

  std::shared_ptr<apollo::cyber::transport::ReadableBlock> getShmBlockToRead(int blockIndex) {
    std::shared_ptr<apollo::cyber::transport::ReadableBlock> rbPtr = 
      std::make_shared<apollo::cyber::transport::ReadableBlock>();
    rbPtr->index = blockIndex;
    if (!segment_->AcquireBlockToRead(rbPtr.get())) {
      AERROR << "failed to acquire block, index: " << rbPtr->index;
      return std::shared_ptr<apollo::cyber::transport::ReadableBlock>();
    }
    return rbPtr;
  }

  void releaseReadBlock(apollo::cyber::transport::ReadableBlock rb) {
    segment_->ReleaseReadBlock(rb);
  }

  bool transmit(const T* srcBuf, int size, int* blockIndex) {
    std::shared_ptr<apollo::cyber::transport::WritableBlock> wbPtr =
      std::make_shared<apollo::cyber::transport::WritableBlock>();

    if (!segment_->AcquireBlockToWrite(size, wbPtr.get())) {
      AERROR << "acquire block failed.";
      return false;
    }
    ADEBUG << "block index: " << wbPtr->index;
    // copy to block
    memcpy(wbPtr->buf, srcBuf, size);
    wbPtr->block->set_msg_size(size);
    *blockIndex = wbPtr->index;
    segment_->ReleaseWrittenBlock(*wbPtr);
    return true;
  }

  bool recvBuf(int blockIndex, std::function<void(T*)> handler) {
    std::shared_ptr<apollo::cyber::transport::ReadableBlock> rbPtr = 
      std::make_shared<apollo::cyber::transport::ReadableBlock>();
    rbPtr->index = blockIndex;
    if (!segment_->AcquireBlockToRead(rbPtr.get())) {
      AERROR << "failed to acquire block, index: " << rbPtr->index;
      return false;
    }
    // handle rbPtr->buf
    if (handler) {
      handler(reinterpret_cast<T*>(rbPtr->buf));
    }
    // release
    segment_->ReleaseReadBlock(*rbPtr);
    return true;
  }


  
  // SharedMemBuffer(std::string shm_name = "SharedMemBuffer", int shm_size = 1024) 
  //   : m_shared_mem_name_{shm_name}, m_shared_mem_size_{shm_size} {}
  // ~SharedMemBuffer() {
  //     // destroy shared memory
  //     if (m_shared_mem_name_ != "" && m_remove) {
  //         boost::interprocess::shared_memory_object::remove(m_shared_mem_name_.c_str());
  //     }
  // }

  /**
   * @brief this method only find a shared memory segment by buf_name, 
   * 
   * @param shm_name    shared memory segment name
   * @param remove      indicate whether remove shared memory segment when object detructed, 
   *                    default value is false which indicates not to remove shared memory object
   * @return true       succeed to find shared memory segment
   * @return false      failed to find shared memory
   */
  // bool OpenBuf(std::string shm_name, bool remove = false) {
  //   try
  //   {
  //     managed_shm_ptr_ = boost::make_shared<boost::interprocess::managed_shared_memory>(
  //       boost::interprocess::open_only, shm_name.c_str());
  //   }
  //   catch(const std::exception& e)
  //   {
  //     std::cerr << e.what() << '\n';
  //     return false;
  //   }
  //   m_remove = remove;

  //   return true;
  // }

  /**
   * @brief this method open or create a shared memory segment and generate default buf_name array
   * 
   * @param buf_size    the the size of each buffer object region
   * @param num_of_buf  the number of buffer object region
   * @return true 
   * @return false 
   */
  // bool OpenBuf(int buf_size[], int num_of_buf) {
  //   std::string name_prefix = "buf";
  //   std::string * tmp_name = new std::string[num_of_buf];
  //   for (int i = 0; i < num_of_buf; i++) {
  //     tmp_name[i] = name_prefix + std::to_string(i);
  //   }
  //   return OpenBuf(tmp_name, buf_size, num_of_buf);
  // }

  // bool OpenBuf(std::vector<std::string>& buf_name, std::vector<int>& buf_size) {
  //   if (buf_name.size() != buf_size.size()) {
  //     std::cerr << "parameter size error" << '\n';
  //     return false;
  //   }
  //   std::shared_ptr<std::string> all_buf_names_ptr(new std::string[buf_name.size()], std::default_delete<std::string[]>());
  //   std::shared_ptr<int> all_buf_size_ptr(new int[buf_size.size()], std::default_delete<int[]>());
  //   for (std::size_t i = 0; i < buf_name.size(); i++) {
  //     *(all_buf_names_ptr.get() + i) = buf_name[i];
  //     *(all_buf_size_ptr.get() + i) = buf_size[i];
  //   }

  //   return OpenBuf(all_buf_names_ptr.get(), all_buf_size_ptr.get(), buf_name.size());
  // }

  /**
   * @brief this method open or create a shared memory segment
   * 
   * @param buf_name    the name of each buffer object region
   * @param buf_size    the size of each buffer object region
   * @param num_of_buf  the number of each buffer object region
   * @return true 
   * @return false 
   */
  // bool OpenBuf(std::string buf_name[], int buf_size[], int num_of_buf) {
  //   int tmp_shared_mem_size = 0;
  //   int i;
  //   for(i = 0; i < num_of_buf; i++) {
  //     tmp_shared_mem_size += buf_size[i];
  //   }
  //   tmp_shared_mem_size += buf_size[i-1];
  //   managed_shm_ptr_ = boost::make_shared<boost::interprocess::managed_shared_memory>(
  //     boost::interprocess::open_or_create, m_shared_mem_name_.c_str(), 
  //     std::max(tmp_shared_mem_size, m_shared_mem_size_));
    
  //   // initialize container
  //   try
  //   {
  //     for (i = 0; i < num_of_buf; i++) {
  //     T* buf = managed_shm_ptr_->find_or_construct<T>(buf_name[i].c_str())[buf_size[i]]();
  //     auto mutex = boost::make_shared<boost::interprocess::named_mutex>(
  //       boost::interprocess::open_or_create, (buf_name[i]+"mtx").c_str());
  //     buf_name2ptrs_[buf_name[i]] = buf;
  //     buf_name2mutexes_[buf_name[i]] = mutex;
  //   }
  //   }
  //   catch(const std::exception& e)
  //   {
  //     if (m_shared_mem_name_ != "") {
  //         boost::interprocess::shared_memory_object::remove(m_shared_mem_name_.c_str());
  //     }
  //     std::cerr << e.what() << '\n';
  //     return false;
  //   }
    
  //   m_num_of_buf_ = num_of_buf;

  //   return true;
  // }

  /**
   * @brief write data from address srcBuf to shared memory buffer region named buf_name
   * 
   * @param buf_name  the name of shared memory buffer, created by OpenBuf with parameter buf_name array
   * @param srcBuf    the local source data address
   * @param size      the length of data
   * @return true 
   * @return false 
   */
  // bool WriteBuf(std::string buf_name, T* srcBuf, int size) {
  //   boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(*buf_name2mutexes_[buf_name]);
  //   try
  //   {
  //     memcpy(getBuf(buf_name), srcBuf, size);
  //   }
  //   catch(const std::exception& e)
  //   {
  //     std::cerr << e.what() << '\n';
  //     return false;
  //   }
  //   return true;
  // }

  /**
   * @brief the message reveiver handle buf_name buffer with handler
   *        Not Recommented!!!
   * @param buf_name 
   * @param handler 
   * @return true 
   * @return false 
   */
  // bool recvBuf(std::string buf_name, std::function<void(T*)> handler) {
  //   auto mutex = boost::make_shared<boost::interprocess::named_mutex>(
  //     boost::interprocess::open_or_create, (buf_name+"mtx").c_str());
  //   boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(*mutex);
  //   // handler(getBuf(buf_name));
  //   handler(findBuf(buf_name).first);

  //   return true;
  // }

  // T* getBuf(std::string buf_name) {
  //   return buf_name2ptrs_[buf_name];
  // }

  // T* getBuf(std::string buf_name, int buf_size) {
  //   return managed_shm_ptr_->find_or_construct<T>(buf_name.c_str())[buf_size]();
  // }

  /**
   * @brief Get the shared memory buffer object to Local object
   * 
   * @param buf_name 
   * @return std::pair<std::shared_ptr<T>, std::size_t> 
   */
  // std::pair<std::shared_ptr<T>, std::size_t> getBuf2Local(std::string buf_name) {
  //   auto mutex = boost::make_shared<boost::interprocess::named_mutex>(
  //     boost::interprocess::open_or_create, (buf_name+"mtx").c_str());
  //   boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(*mutex);
  //   auto shmbuf = findBuf(buf_name);
  //   std::pair<std::shared_ptr<T>, std::size_t> tmpBuf;
  //   std::shared_ptr<T> bufMemory(new T[shmbuf.second], std::default_delete<T[]>());
  //   memcpy(bufMemory.get(), shmbuf.first, shmbuf.second);
  //   tmpBuf.first = bufMemory;
  //   tmpBuf.second = shmbuf.second;
    
  //   return tmpBuf;
  // }

  // std::pair<T*, std::size_t> findBuf(std::string buf_name) {
  //   return managed_shm_ptr_->find<T>(buf_name.c_str());
  // }

private:
  // int memcpy_neon(T* src, T* dst, int size) { return 0; }

private:
  // boost::shared_ptr<boost::interprocess::managed_shared_memory> managed_shm_ptr_;
  // std::string m_shared_mem_name_;
  // int m_shared_mem_size_;
  // std::map<std::string, T*> buf_name2ptrs_;
  // std::map<std::string, boost::shared_ptr<boost::interprocess::named_mutex>> buf_name2mutexes_;
  // int m_num_of_buf_;
  // bool m_remove = true;

private:
  apollo::cyber::transport::SegmentPtr segment_;
  
};