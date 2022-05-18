/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef SRC_SERVICES_SERVER_H_
#define SRC_SERVICES_SERVER_H_

#include <string>
#include <vector>
#include <memory>

#include "utils/log.h"
#include "guest/vm_builder.h"
#include "services/message.h"
#include "services/startup_listener_impl.h"

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

    size_t FindVmInstance(std::string name);
    void DeleteVmInstance(std::string name);

    int StartVm(const char payload[]);
    int StopVm(const char payload[]);

    void VmThread(VmBuilder *vb);

    void Accept();

    void AsyncWaitSignal(void);

    bool SetupListenerService();

    bool stop_server_ = false;

    CivMsgSync *sync_;

    std::vector<std::unique_ptr<VmBuilder>> vmis_;

    std::mutex vmis_mutex_;

    StartupListenerImpl startup_listener_;
    std::unique_ptr<boost::thread> listener_thread_;
    std::shared_ptr<grpc::Server> listener_server_;
};

}  // namespace vm_manager

#endif  // SRC_SERVICES_SERVER_H_
