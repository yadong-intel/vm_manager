/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <signal.h>

#include <iostream>
#include <exception>
#include <utility>
#include <string>
#include <memory>
#include <filesystem>

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

#include "services/server.h"
#include "services/message.h"
#include "guest/vm_interface.h"
#include "guest/vm_builder.h"
#include "utils/log.h"

namespace vm_manager {

const char *kCivSharedMemName = "CivVmServerShm";
const char *kCivSharedObjSync = "Civ Message Sync";
const char *kCivSharedObjData = "Civ Message Data";
const int kCivSharedMemSize = 10240U;

static void HandleSIG(int num) {
    LOG(info) << "Signal(" << num << ") received!";
    Server::Get().Stop();
}

void Server::Start(void) {
    try {
        signal(SIGINT, HandleSIG);
        signal(SIGTERM, HandleSIG);

        struct shm_remove {
            shm_remove() { boost::interprocess::shared_memory_object::remove("CivVmServerShm"); }
            ~shm_remove() { boost::interprocess::shared_memory_object::remove("CivVmServerShm"); }
        } remover;

          boost::interprocess::managed_shared_memory shm(
            boost::interprocess::create_only,
            kCivSharedMemName,
            kCivSharedMemSize);

        sync_ = shm.construct<CivMsgSync>
                (kCivSharedObjSync)
                ();

        while (!stop_server) {
            boost::interprocess::scoped_lock <boost::interprocess::interprocess_mutex> lock(sync_->mutex);
            sync_->cond_full.wait(lock);
            std::pair<CivMsg*, boost::interprocess::managed_shared_memory::size_type> data;
            data = shm.find<CivMsg>(kCivSharedObjData);
            if (!data.first)
                continue;
            switch (data.first->type) {
                case kCiVMsgStopServer:
                    stop_server = true;
                    break;
                case kCivMsgStartVm:
                    StartVm(data.first->vm_pay_load);
                    break;
                case kCivMsgTest:
                    break;
                default:
                    LOG(error) << "vm-manager: received unknown message type: " << data.first->type;
                    return;
            }
        }

        LOG(info) << "CiV Server exited!";
    } catch (std::exception &e) {
        LOG(error) << "CiV Server: Exception:" << e.what() << ", pid=" << getpid();
    }
}

void Server::Stop(void) {
    LOG(info) << "Stop CiV Server!";
    stop_server = true;
    sync_->cond_full.notify_one();
}

Server &Server::Get(void) {
    static Server server_;
    return server_;
}

}  // namespace vm_manager
