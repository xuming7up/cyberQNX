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

#include "cyber/node/node.h"

#include "cyber/common/global_data.h"
#include "cyber/time/time.h"

namespace apollo {
namespace cyber {

using proto::RoleType;

Node::Node(const std::string& node_name, const std::string& name_space, const std::string& host_name)
    : node_name_(node_name), name_space_(name_space), host_name_(host_name) {
  node_channel_impl_.reset(new NodeChannelImpl(node_name));
  node_service_impl_.reset(new NodeServiceImpl(node_name));
}

Node::Node(const std::string& node_name,const proto::FixedChannels& fixed_channels, const std::string& name_space,const std::string& host_name)
     : node_name_(node_name), name_space_(name_space), host_name_(host_name){
  node_channel_impl_.reset(new NodeChannelImpl(node_name));
  node_service_impl_.reset(new NodeServiceImpl(node_name));

  for (const std::string& reader : fixed_channels.readers()){
    fixed_reader_set_.insert(reader);
  }

  for (const std::string& writer : fixed_channels.writers()){
    fixed_writer_set_.insert(writer);
  }
}

Node::~Node() {}

const std::string& Node::Name() const { return node_name_; }

void Node::Observe() {
  for (auto& reader : readers_) {
    reader.second->Observe();
  }
}

void Node::ClearData() {
  for (auto& reader : readers_) {
    reader.second->ClearData();
  }
}

  std::string Node::FormatChannelName(const std::string& channel_name,bool is_reader){
    std::string result = channel_name;
    bool is_fixed = false;
    if (is_reader){
      is_fixed = (fixed_reader_set_.count(channel_name) == 1);
    }else{
      is_fixed = (fixed_writer_set_.count(channel_name) == 1);
    }

    if (!is_fixed){
      result += host_name_;
    }
    
    return result;
  }

}  // namespace cyber
}  // namespace apollo
