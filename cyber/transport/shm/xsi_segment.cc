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
* -1. add QNX support for shared memory operation (POSIX interface)
* 
*/

#include "cyber/transport/shm/xsi_segment.h"

#include <sys/ipc.h>

#ifdef __QNX__
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <aarch64/mmu.h>
#include <unistd.h>
#else
#include <sys/shm.h>
#endif

#include <sys/types.h>

#include "cyber/common/log.h"
#include "cyber/common/util.h"
#include "cyber/transport/shm/segment.h"
#include "cyber/transport/shm/shm_conf.h"

namespace apollo {
namespace cyber {
namespace transport {

XsiSegment::XsiSegment(uint64_t channel_id) : Segment(channel_id) {
  key_ = static_cast<key_t>(channel_id);
#ifdef __QNX__
  qnx_key_str_ = "/"+std::to_string(key_);
#endif
}

XsiSegment::~XsiSegment() { Destroy(); }

bool XsiSegment::OpenOrCreate() {
  if (init_) {
    return true;
  }

  // create managed_shm_
  int retry = 0;
  int shmid = 0;
  while (retry < 2) {
  #ifdef __QNX__
    shmid = shm_open(qnx_key_str_.c_str(), O_RDWR | O_CREAT | O_EXCL, 0644);
  #else
    shmid = shmget(key_, conf_.managed_shm_size(), 0644 | IPC_CREAT | IPC_EXCL);
  #endif
    if (shmid != -1) {
      break;
    }

    if (EINVAL == errno) {
      AINFO << "need larger space, recreate.";
      Reset();
      Remove();
      ++retry;
    } else if (EEXIST == errno) {
      ADEBUG << "shm already exist, open only.";
      return OpenOnly();
    } else {
      break;
    }
  }

  if (shmid == -1) {
    AERROR << "create shm failed, error code: " << strerror(errno);
    return false;
  }

#ifdef __QNX__
  if (ftruncate( shmid, conf_.managed_shm_size()) == -1) {
    AERROR << "ftruncate failed: " << strerror(errno);
    close(shmid);
    return false;
  }
#endif

  // attach managed_shm_
#ifdef __QNX__ 
  managed_shm_ = mmap(nullptr, conf_.managed_shm_size(), PROT_READ | PROT_WRITE,
                      MAP_SHARED, shmid, 0);
  if (managed_shm_ == MAP_FAILED) {
    AERROR << "attach shm failed: " << strerror(errno);
    close(shmid);
    shm_unlink(qnx_key_str_.c_str());
    return false;
  }

  close(shmid);
#else
  managed_shm_ = shmat(shmid, nullptr, 0);
  if (managed_shm_ == reinterpret_cast<void*>(-1)) {
    AERROR << "attach shm failed, error: " << strerror(errno);
    shmctl(shmid, IPC_RMID, 0);
    return false;
  }
#endif

  // create field state_
  state_ = new (managed_shm_) State(conf_.ceiling_msg_size());
  if (state_ == nullptr) {
    AERROR << "create state failed.";
  #ifdef __QNX__
    if (munmap(managed_shm_, conf_.managed_shm_size()) == -1) {
      AERROR << "munmap failed.";
    }
    managed_shm_ = nullptr;
    shm_unlink(qnx_key_str_.c_str());
  #else
    shmdt(managed_shm_);
    managed_shm_ = nullptr;
    shmctl(shmid, IPC_RMID, 0);
  #endif
    return false;
  }

  conf_.Update(state_->ceiling_msg_size());

  // create field blocks_
  blocks_ = new (static_cast<char*>(managed_shm_) + sizeof(State))
      Block[conf_.block_num()];
  if (blocks_ == nullptr) {
    AERROR << "create blocks failed.";
    state_->~State();
    state_ = nullptr;
  #ifdef __QNX__
    if (munmap(managed_shm_, conf_.managed_shm_size()) == -1) {
      AERROR << "munmap failed.";
    }
    managed_shm_ = nullptr;
    shm_unlink(qnx_key_str_.c_str());
  #else
    shmdt(managed_shm_);
    managed_shm_ = nullptr;
    shmctl(shmid, IPC_RMID, 0);
  #endif
    return false;
  }

  // create block buf
  uint32_t i = 0;
  for (; i < conf_.block_num(); ++i) {
    uint8_t* addr =
        new (static_cast<char*>(managed_shm_) + sizeof(State) +
             conf_.block_num() * sizeof(Block) + i * conf_.block_buf_size())
            uint8_t[conf_.block_buf_size()];

    std::lock_guard<std::mutex> _g(block_buf_lock_);
    block_buf_addrs_[i] = addr;
  }

  if (i != conf_.block_num()) {
    AERROR << "create block buf failed.";
    state_->~State();
    state_ = nullptr;
    blocks_ = nullptr;
    {
      std::lock_guard<std::mutex> _g(block_buf_lock_);
      block_buf_addrs_.clear();
    }
  #ifdef __QNX__
    if (munmap(managed_shm_, conf_.managed_shm_size()) == -1) {
      AERROR << "munmap failed.";
    }
    managed_shm_ = nullptr;
    shm_unlink(qnx_key_str_.c_str());
  #else
    shmdt(managed_shm_);
    managed_shm_ = nullptr;
    shmctl(shmid, IPC_RMID, 0);
  #endif
    return false;
  }

  state_->IncreaseReferenceCounts();
  init_ = true;
  ADEBUG << "open or create true.";
  return true;
}

bool XsiSegment::OpenOnly() {
  if (init_) {
    return true;
  }

  // get managed_shm_
#ifdef __QNX__
  int shmid = shm_open(qnx_key_str_.c_str(), O_RDWR, 0644);
#else 
  int shmid = shmget(key_, 0, 0644);
#endif
  if (shmid == -1) {
    AERROR << "get shm failed. error: " << strerror(errno);
    return false;
  }

  // attach managed_shm_
#ifdef __QNX__
  struct stat file_attr;
  if (fstat(shmid, &file_attr) < 0) {
    AERROR << "fstat failed: " << strerror(errno);
    close(shmid);
    return false;
  }

  managed_shm_ = mmap(nullptr, file_attr.st_size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, shmid, 0);
  if (managed_shm_ == MAP_FAILED) {
    AERROR << "attach shm failed: " << strerror(errno);
    close(shmid);
    return false;
  }

  close(shmid);
#else
  managed_shm_ = shmat(shmid, nullptr, 0);
  if (managed_shm_ == reinterpret_cast<void*>(-1)) {
    AERROR << "attach shm failed, error: " << strerror(errno);
    return false;
  }
#endif

  // get field state_
  state_ = reinterpret_cast<State*>(managed_shm_);
  if (state_ == nullptr) {
    AERROR << "get state failed.";
#ifdef __QNX__
    if (munmap(managed_shm_, file_attr.st_size) == -1) {
      AERROR << "munmap failed.";
    }
#else 
    shmdt(managed_shm_);
#endif 
    managed_shm_ = nullptr;
    return false;
  }

  conf_.Update(state_->ceiling_msg_size());

  // get field blocks_
  blocks_ = reinterpret_cast<Block*>(static_cast<char*>(managed_shm_) +
                                     sizeof(State));
  if (blocks_ == nullptr) {
    AERROR << "get blocks failed.";
    state_ = nullptr;
#ifdef __QNX__
    if (munmap(managed_shm_, conf_.managed_shm_size()) == -1) {
      AERROR << "munmap failed.";
    }
#else
    shmdt(managed_shm_);
#endif
    managed_shm_ = nullptr;
    return false;
  }

  // get block buf
  uint32_t i = 0;
  for (; i < conf_.block_num(); ++i) {
    uint8_t* addr = reinterpret_cast<uint8_t*>(
        static_cast<char*>(managed_shm_) + sizeof(State) +
        conf_.block_num() * sizeof(Block) + i * conf_.block_buf_size());

    if (addr == nullptr) {
      break;
    }
    std::lock_guard<std::mutex> _g(block_buf_lock_);
    block_buf_addrs_[i] = addr;
  }

  if (i != conf_.block_num()) {
    AERROR << "open only failed.";
    state_->~State();
    state_ = nullptr;
    blocks_ = nullptr;
    {
      std::lock_guard<std::mutex> _g(block_buf_lock_);
      block_buf_addrs_.clear();
    }
#ifdef __QNX__
    if (munmap(managed_shm_, conf_.managed_shm_size()) == -1) {
      AERROR << "munmap failed.";
    }
    managed_shm_ = nullptr;
    shm_unlink(qnx_key_str_.c_str());
#else 
    shmdt(managed_shm_);
    managed_shm_ = nullptr;
    shmctl(shmid, IPC_RMID, 0);
#endif
    return false;
  }

  state_->IncreaseReferenceCounts();
  init_ = true;
  ADEBUG << "open only true.";
  return true;
}

bool XsiSegment::Remove() {
#ifdef __QNX__
  if (shm_unlink(qnx_key_str_.c_str()) < 0) {
    AERROR << "shm_unlink failed: " << strerror(errno);
    return false;
  }
#else
  int shmid = shmget(key_, 0, 0644);
  if (shmid == -1 || shmctl(shmid, IPC_RMID, 0) == -1) {
    AERROR << "remove shm failed, error code: " << strerror(errno);
    return false;
  }
#endif
  ADEBUG << "remove success.";
  return true;
}

void XsiSegment::Reset() {
  state_ = nullptr;
  blocks_ = nullptr;
  {
    std::lock_guard<std::mutex> _g(block_buf_lock_);
    block_buf_addrs_.clear();
  }
  if (managed_shm_ != nullptr) {
#ifdef __QNX__
    if (munmap(managed_shm_, conf_.managed_shm_size()) == -1) {
      AERROR << "munmap failed.";
    }
#else 
    shmdt(managed_shm_);
#endif
    managed_shm_ = nullptr;
    return;
  }
}

}  // namespace transport
}  // namespace cyber
}  // namespace apollo
