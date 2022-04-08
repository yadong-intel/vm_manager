/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <boost/interprocess/managed_shared_memory.hpp>

#include "services/message.h"

namespace vm_manager {
class Client {
 public:
  Client();
  ~Client();

  void PrepareStartGuestClientShm(const char *vm_name);
  bool Notify(CivMsgType t);

 private:
  boost::interprocess::managed_shared_memory server_shm_;
  boost::interprocess::managed_shared_memory client_shm_;
  std::string client_shm_name_;
};

}  // namespace vm_manager

#endif /* __CLIENT_H__ */