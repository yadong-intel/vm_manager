
/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef SRC_SERVICES_SERVER_H_
#define SRC_SERVICES_SERVER_H_

#include "utils/log.h"
#include "services/message.h"

namespace vm_manager {

class Server final {
 public:
  static Server &Get(void);
  void Start(void);
  void Stop(void);

 private:
  Server() = default;

  ~Server() = default;

  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;

  void Accept();

  void AsyncWaitSignal(void);

  bool stop_server = false;
  CivMsgSync *sync_;
};

}  // namespace vm_manager

#endif  // SRC_SERVICES_SERVER_H_
