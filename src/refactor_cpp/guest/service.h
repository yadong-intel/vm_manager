
/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef __SERVICE_H__
#define __SERVICE_H__

#include <string>
#include <memory>

#include <boost/asio.hpp>

#include "vm_builder.h"
#include "log.h"

#if 0
namespace vm_manager {

const char service_sock[] = "/tmp/civ_hs.socket";

class Service final {
 public:
  static Service *Get(uid_t suid, gid_t sgid, bool daemon);
  static Service *Get(void) { return s_; }
  bool Run(void);
  void Stop(void);

 private:
  Service(uid_t suid, gid_t sgid, bool daemon) : sudo_uid_(suid), sudo_gid_(sgid), daemon_(daemon) {}
  ~Service() = default;
  Service(const Service&) = delete;
  Service& operator=(const Service&) = delete;

  void DoAccept();

  void StopServer(void);
  void StartServer(void);

  static Service *s_;
  static std::mutex mutex_;
  uid_t sudo_uid_;
  gid_t sudo_gid_;
  bool daemon_ = false;

  boost::asio::io_context io_context;
  boost::asio::local::stream_protocol::acceptor acceptor_;
};

bool StopServer(void);

}  // namespace vm_manager

#endif

#endif /* __SERVICE_H__ */
