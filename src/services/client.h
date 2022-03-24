/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <boost/asio.hpp>

#include "services/message.h"

namespace vm_manager {
class Client {
 public:
  Client() : sock_(ioc_) {}
  ~Client() {
    sock_.close();
    ioc_.stop();
  }
  bool Connect(void);
  bool Send(CivMsg msg);
  bool Receive(CivMsg *msg);

 private:
  boost::asio::io_context ioc_;
  boost::asio::local::stream_protocol::socket sock_;
};

}  // namespace vm_manager

#endif /* __CLIENT_H__ */