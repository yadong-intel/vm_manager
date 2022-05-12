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
#include <vector>

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/process/environment.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/string.hpp>

#include "services/server.h"
#include "services/message.h"
#include "guest/vm_builder_qemu.h"
#include "utils/log.h"
#include "utils/utils.h"

namespace vm_manager {

const char *kCivServerMemName = "CivServerShm";
const char *kCivServerObjSync = "Civ Message Sync";
const char *kCivServerObjData = "Civ Message Data";
const int kCivSharedMemSize = 20480U;

size_t Server::FindVmInstance(std::string name) {
    for (size_t i = 0; i < vmis_.size(); ++i) {
        if (vmis_[i]->GetName().compare(name) == 0)
            return i;
    }
    return -1;
}

void Server::DeleteVmInstance(std::string name) {
    std::scoped_lock lock(vmis_mutex_);
    int index = FindVmInstance(name);
    if (index == -1)
        return;
    vmis_.erase(vmis_.begin() + index);
}

int Server::StopVm(const char payload[]) {
    boost::interprocess::managed_shared_memory shm(
        boost::interprocess::open_only,
        payload);
    std::pair<bstring *, int> vm_name;
    vm_name = shm.find<bstring>("StopVmName");
    int id = FindVmInstance(std::string(vm_name.first->c_str()));
    if (id != -1) {
        vmis_[id]->StopVm();
    }
    if (id == -1)
        LOG(warning) << "CiV: " << vm_name.first->c_str() << " is not running!";
    return 0;
}

void Server::VmThread(VmBuilder *vb) {
    if (!vb)
        return;

    /* Build VM releated Arguments */
    if (!vb->BuildVmArgs())
        return;

    LOG(info) << "Starting VM:  " << vb->GetName();
    /* Start VM */
    vb->StartVm();

    /* Wait VM to exit */
    vb->WaitVm();

    DeleteVmInstance(vb->GetName());
}

int Server::StartVm(const char payload[]) {
    boost::interprocess::managed_shared_memory shm(
        boost::interprocess::open_read_only,
        payload);

    auto vm_name = shm.find<bstring>("StartVmName");
    if (FindVmInstance(std::string(vm_name.first->c_str())) != -1UL) {
        LOG(warning) << "CiV: " << vm_name.first->c_str() << " is already running!";
        return -1;
    }

    std::string cfg_file = GetConfigPath() + std::string("/") + vm_name.first->c_str() + ".ini";

    CivConfig cfg;
    if (!cfg.ReadConfigFile(cfg_file)) {
        LOG(error) << "Failed to read config file";
        return -1;
    }

    std::pair<bstring *, int> vm_env = shm.find<bstring>("StartVmEnv");
    std::vector<std::string> env_data;

    for (int i = 0; i < vm_env.second; i++) {
        env_data.push_back(std::string(vm_env.first[i].c_str()));
    }

    std::vector<std::unique_ptr<VmBuilder>>::iterator vmi;
    if (cfg.GetValue(kGroupEmul, kEmulType) == kEmulTypeQemu) {
        vmi = vmis_.emplace(vmis_.end(),
                    std::make_unique<VmBuilderQemu>(vm_name.first->c_str(), std::move(cfg), std::move(env_data)));
    } else {
        /* Default try to build args for QEMU */
        vmi = vmis_.emplace(vmis_.end(),
                    std::make_unique<VmBuilderQemu>(vm_name.first->c_str(), std::move(cfg), std::move(env_data)));
    }

    VmBuilder *vb = vmi->get();
    boost::thread t([this, vb]() {
        VmThread(vb);
    });

    t.detach();

    return 0;
}

static void HandleSIG(int num) {
    LOG(info) << "Signal(" << num << ") received!";
    Server::Get().Stop();
}

void Server::Start(void) {
    try {
        signal(SIGINT, HandleSIG);
        signal(SIGTERM, HandleSIG);

        struct shm_remove {
            shm_remove() { boost::interprocess::shared_memory_object::remove(kCivServerMemName); }
            ~shm_remove() { boost::interprocess::shared_memory_object::remove(kCivServerMemName); }
        } remover;

        boost::interprocess::permissions unrestricted_permissions;
        unrestricted_permissions.set_unrestricted();
        boost::interprocess::managed_shared_memory shm(
            boost::interprocess::create_only,
            kCivServerMemName,
            sizeof(CivMsgSync) + sizeof(CivMsg) + 1024,
            0,
            unrestricted_permissions);

        sync_ = shm.construct<CivMsgSync>
                (kCivServerObjSync)
                ();

        while (!stop_server_) {
            boost::interprocess::scoped_lock <boost::interprocess::interprocess_mutex> lock(sync_->mutex_cond);
            sync_->cond_s.wait(lock);

            std::pair<CivMsg*, boost::interprocess::managed_shared_memory::size_type> data;
            data = shm.find<CivMsg>(kCivServerObjData);

            if (!data.first)
                continue;

            switch (data.first->type) {
                case kCiVMsgStopServer:
                    stop_server_ = true;
                    data.first->type = kCivMsgRespondSuccess;
                    break;
                case kCivMsgStartVm:
                    if (StartVm(data.first->payload) == 0) {
                        data.first->type = kCivMsgRespondSuccess;
                    } else {
                        data.first->type = kCivMsgRespondFail;
                    }
                    break;
                case kCivMsgStopVm:
                    if (StopVm(data.first->payload) == 0) {
                        data.first->type = kCivMsgRespondSuccess;
                    } else {
                        data.first->type = kCivMsgRespondFail;
                    }
                    break;
                case kCivMsgTest:
                    break;
                default:
                    LOG(error) << "vm-manager: received unknown message type: " << data.first->type;
                    break;
            }
            sync_->cond_c.notify_one();
        }

        shm.destroy_ptr(sync_);

        LOG(info) << "CiV Server exited!";
    } catch (std::exception &e) {
        LOG(error) << "CiV Server: Exception:" << e.what() << ", pid=" << getpid();
    }
}

void Server::Stop(void) {
    LOG(info) << "Stop CiV Server!";
    stop_server_ = true;
    sync_->cond_s.notify_one();
}

Server &Server::Get(void) {
    static Server server_;
    return server_;
}

}  // namespace vm_manager
