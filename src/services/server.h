
/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef __SERVER_H__
#define __SERVER_H__

#include <boost/asio.hpp>

namespace vm_manager {

const char server_sock[] = "/tmp/civ_hs.socket";

class Server final {
 public:
  static Server &Get(uid_t suid, gid_t sgid, bool daemon);
  void Start(void);
  void Stop(void);

 private:
  Server(uid_t suid, gid_t sgid, bool daemon) :
    suid_(suid), sgid_(sgid),
    acceptor_(ioc_),
    signals_(ioc_) {}

  ~Server() {

  }
  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;

  void Accept();

  void AsyncWaitSignal(void);

  uid_t suid_;
  gid_t sgid_;

  boost::asio::io_context ioc_;
  boost::asio::local::stream_protocol::acceptor acceptor_;
  boost::asio::signal_set signals_;
};

}  // namespace vm_manager

#endif /* __SERVER_H__ */
