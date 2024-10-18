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
* -1.Discrete arch support,add host name to channel.
* 
*/

#ifndef CYBER_NODE_NODE_H_
#define CYBER_NODE_NODE_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "cyber/node/node_channel_impl.h"
#include "cyber/node/node_service_impl.h"
#include "cyber/proto/component_conf.pb.h"

namespace apollo {
namespace cyber {

template <typename M0, typename M1, typename M2, typename M3>
class Component;
class TimerComponent;

/**
 * @class Node
 * @brief Node is the fundamental building block of Cyber RT.
 * every module contains and communicates through the node.
 * A module can have different types of communication by defining
 * read/write and/or service/client in a node.
 * @warning Duplicate name is not allowed in topo objects, such as node,
 * reader/writer, service/clinet in the topo.
 */
class Node {
 public:
  template <typename M0, typename M1, typename M2, typename M3>
  friend class Component;
  friend class TimerComponent;
  friend bool Init(const char*);
  friend std::unique_ptr<Node> CreateNode(const std::string&,
                                          const std::string&);
  virtual ~Node();

  /**
   * @brief Get node's name.
   * @warning duplicate node name is not allowed in the topo.
   */
  const std::string& Name() const;

  /**
   * @brief Create a Writer with specific message type.
   *
   * @tparam MessageT Message Type
   * @param role_attr is a protobuf message RoleAttributes, which includes the
   * channel name and other info.
   * @return std::shared_ptr<Writer<MessageT>> result Writer Object
   */
  template <typename MessageT>
  auto CreateWriter(const proto::RoleAttributes& role_attr)
      -> std::shared_ptr<Writer<MessageT>>;

  /**
   * @brief Create a Writer with specific message type.
   *
   * @tparam MessageT Message Type
   * @param channel_name the channel name to be published.
   * @return std::shared_ptr<Writer<MessageT>> result Writer Object
   */
  template <typename MessageT>
  auto CreateWriter(const std::string& channel_name)
      -> std::shared_ptr<Writer<MessageT>>;

  /**
   * @brief Create a Reader with specific message type with channel name
   * qos and other configs used will be default
   *
   * @tparam MessageT Message Type
   * @param channel_name the channel of the reader subscribed.
   * @param reader_func invoked when message receive
   * invoked when the message is received.
   * @return std::shared_ptr<cyber::Reader<MessageT>> result Reader Object
   */
  template <typename MessageT>
  auto CreateReader(const std::string& channel_name,
                    const CallbackFunc<MessageT>& reader_func = nullptr)
      -> std::shared_ptr<cyber::Reader<MessageT>>;

  /**
   * @brief Create a Reader with specific message type with reader config
   *
   * @tparam MessageT Message Type
   * @param config instance of `ReaderConfig`,
   * include channel name, qos and pending queue size
   * @param reader_func invoked when message receive
   * @return std::shared_ptr<cyber::Reader<MessageT>> result Reader Object
   */
  template <typename MessageT>
  auto CreateReader(const ReaderConfig& config,
                    const CallbackFunc<MessageT>& reader_func = nullptr)
      -> std::shared_ptr<cyber::Reader<MessageT>>;

  /**
   * @brief Create a Reader object with `RoleAttributes`
   *
   * @tparam MessageT Message Type
   * @param role_attr instance of `RoleAttributes`,
   * includes channel name, qos, etc.
   * @param reader_func invoked when message receive
   * @return std::shared_ptr<cyber::Reader<MessageT>> result Reader Object
   */
  template <typename MessageT>
  auto CreateReader(const proto::RoleAttributes& role_attr,
                    const CallbackFunc<MessageT>& reader_func = nullptr)
      -> std::shared_ptr<cyber::Reader<MessageT>>;

  /**
   * @brief Create a Service object with specific `service_name`
   *
   * @tparam Request Message Type of the Request
   * @tparam Response Message Type of the Response
   * @param service_name specific service name to a serve
   * @param service_callback invoked when a service is called
   * @return std::shared_ptr<Service<Request, Response>> result `Service`
   */
  template <typename Request, typename Response>
  auto CreateService(const std::string& service_name,
                     const typename Service<Request, Response>::ServiceCallback&
                         service_callback)
      -> std::shared_ptr<Service<Request, Response>>;

  /**
   * @brief Create a Client object to request Service with `service_name`
   *
   * @tparam Request Message Type of the Request
   * @tparam Response Message Type of the Response
   * @param service_name specific service name to a Service
   * @return std::shared_ptr<Client<Request, Response>> result `Client`
   */
  template <typename Request, typename Response>
  auto CreateClient(const std::string& service_name)
      -> std::shared_ptr<Client<Request, Response>>;

  /**
   * @brief Observe all readers' data
   */
  void Observe();

  /**
   * @brief clear all readers' data
   */
  void ClearData();

  /**
   * @brief Get the Reader object that subscribe `channel_name`
   *
   * @tparam MessageT Message Type
   * @param channel_name channel name
   * @return std::shared_ptr<Reader<MessageT>> result reader
   */
  template <typename MessageT>
  auto GetReader(const std::string& channel_name)
      -> std::shared_ptr<Reader<MessageT>>;

 private:
 explicit Node(const std::string& node_name,
                const std::string& name_space = "",
                const std::string& host_name = "");

  explicit Node(const std::string& node_name,
                const proto::FixedChannels& fixed_channels,
                const std::string& name_space = "",
                const std::string& host_name = "");
    /**
   * @brief Append the host_name to the channel name.
   *
   * @param channel_name Channel name string.
   * @param is_reader the channel_name is reader or writer.
   * @return new channel name.
   */
  std::string FormatChannelName(const std::string& channel_name,bool is_reader = true);

  std::string node_name_;
  std::string name_space_;
  std::string host_name_;//The host name string will be append to the channel name.
  std::set<std::string> fixed_reader_set_;//The  fixed reader channels.
  std::set<std::string> fixed_writer_set_;//The  fixed writer channels.

  std::mutex readers_mutex_;
  std::map<std::string, std::shared_ptr<ReaderBase>> readers_;

  std::unique_ptr<NodeChannelImpl> node_channel_impl_ = nullptr;
  std::unique_ptr<NodeServiceImpl> node_service_impl_ = nullptr;
};

template <typename MessageT>
auto Node::CreateWriter(const proto::RoleAttributes& role_attr)
    -> std::shared_ptr<Writer<MessageT>> {
  proto::RoleAttributes attr(role_attr);
  auto name = FormatChannelName(role_attr.channel_name(),false);
  attr.set_channel_name(name);
  return node_channel_impl_->template CreateWriter<MessageT>(attr);
}

template <typename MessageT>
auto Node::CreateWriter(const std::string& channel_name)
    -> std::shared_ptr<Writer<MessageT>> {
  auto name = FormatChannelName(channel_name,false);
  return node_channel_impl_->template CreateWriter<MessageT>(name);
}

template <typename MessageT>
auto Node::CreateReader(const proto::RoleAttributes& role_attr,
                        const CallbackFunc<MessageT>& reader_func)
    -> std::shared_ptr<Reader<MessageT>> {
  std::lock_guard<std::mutex> lg(readers_mutex_);
  proto::RoleAttributes attr(role_attr);
  auto name = FormatChannelName(role_attr.channel_name());
  attr.set_channel_name(name);
  if (readers_.find(attr.channel_name()) != readers_.end()) {
    AWARN << "Failed to create reader: reader with the same channel already "
             "exists.";
    return nullptr;
  }
  auto reader = node_channel_impl_->template CreateReader<MessageT>(
      attr, reader_func);
  if (reader != nullptr) {
    readers_.emplace(std::make_pair(attr.channel_name(), reader));
  }
  return reader;
}

template <typename MessageT>
auto Node::CreateReader(const ReaderConfig& config,
                        const CallbackFunc<MessageT>& reader_func)
    -> std::shared_ptr<cyber::Reader<MessageT>> {
  std::lock_guard<std::mutex> lg(readers_mutex_);
  ReaderConfig new_cfg(config);
  new_cfg.channel_name = FormatChannelName(new_cfg.channel_name);
  if (readers_.find(new_cfg.channel_name) != readers_.end()) {
    AWARN << "Failed to create reader: reader with the same channel already "
             "exists.";
    return nullptr;
  }
  auto reader =
      node_channel_impl_->template CreateReader<MessageT>(new_cfg, reader_func);
  if (reader != nullptr) {
    readers_.emplace(std::make_pair(new_cfg.channel_name, reader));
  }
  return reader;
}

template <typename MessageT>
auto Node::CreateReader(const std::string& channel_name,
                        const CallbackFunc<MessageT>& reader_func)
    -> std::shared_ptr<Reader<MessageT>> {
  std::lock_guard<std::mutex> lg(readers_mutex_);
  auto name = FormatChannelName(channel_name);
  if (readers_.find(name) != readers_.end()) {
    AWARN << "Failed to create reader: reader with the same channel already "
             "exists.";
    return nullptr;
  }
  auto reader = node_channel_impl_->template CreateReader<MessageT>(
      name, reader_func);
  if (reader != nullptr) {
    readers_.emplace(std::make_pair(name, reader));
  }
  return reader;
}

template <typename Request, typename Response>
auto Node::CreateService(
    const std::string& service_name,
    const typename Service<Request, Response>::ServiceCallback&
        service_callback) -> std::shared_ptr<Service<Request, Response>> {
  return node_service_impl_->template CreateService<Request, Response>(
      service_name, service_callback);
}

template <typename Request, typename Response>
auto Node::CreateClient(const std::string& service_name)
    -> std::shared_ptr<Client<Request, Response>> {
  return node_service_impl_->template CreateClient<Request, Response>(
      service_name);
}

template <typename MessageT>
auto Node::GetReader(const std::string& name)
    -> std::shared_ptr<Reader<MessageT>> {
  std::lock_guard<std::mutex> lg(readers_mutex_);
  auto new_name = FormatChannelName(name);
  auto it = readers_.find(new_name);
  if (it != readers_.end()) {
    return std::dynamic_pointer_cast<Reader<MessageT>>(it->second);
  }
  return nullptr;
}

}  // namespace cyber
}  // namespace apollo

#endif  // CYBER_NODE_NODE_H_
