/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <signal.h>
#include <sys/socket.h>
#include <linux/vm_sockets.h>

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
#include <boost/thread/mutex.hpp>

#include <grpcpp/grpcpp.h>

#include "services/server.h"
#include "services/message.h"
#include "guest/vm_builder_qemu.h"
#include "utils/log.h"
#include "utils/utils.h"
#include "include/constants/vm_manager.h"

namespace vm_manager {

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

void RunListenerService(grpc::Service* listener,
                        const char *listener_address,
                        boost::latch *listener_ready,
                        std::shared_ptr<grpc::Server>* server_copy) {
    grpc::ServerBuilder builder;
    LOG(info) << "Startup Listener listen@" << listener_address;
    builder.AddListeningPort(listener_address, grpc::InsecureServerCredentials());
    builder.RegisterService(listener);

    std::shared_ptr<grpc::Server> server(builder.BuildAndStart().release());

    *server_copy = server;
    listener_ready->try_count_down();

    if (server) {
        server->Wait();
    }
}

bool Server::SetupStartupListenerService(void) {
    boost::latch listener_ready(1);
    char listener_address[50] = { 0 };
    snprintf(listener_address, sizeof(listener_address) - 1, "vsock:%u:%u", VMADDR_CID_ANY, kCiVStartupListenerPort);

    startup_listener_.thread =
        std::make_unique<boost::thread>(RunListenerService,
                                        &startup_listener_.listener,
                                        listener_address,
                                        &listener_ready,
                                        &startup_listener_.server);
    listener_ready.wait();
    return true;
}

int Server::ListVm(const char payload[]) {
    boost::interprocess::managed_shared_memory shm(
        boost::interprocess::open_only,
        payload);

    shm.destroy<bstring>("ListVms");
    shm.zero_free_memory();

    bstring *vm_lists = shm.construct<bstring>
                ("ListVms")
                [vmis_.size()]
                (shm.get_segment_manager());

    for (size_t i = 0; i < vmis_.size(); ++i) {
        std::string s;
        switch (vmis_[i]->GetState()) {
        case VmBuilder::VmState::kVmEmpty:
            s.assign(vmis_[i]->GetName() + ":Empty");
            break;
        case VmBuilder::VmState::kVmCreated:
            s.assign(vmis_[i]->GetName() + ":Created");
            break;
        case VmBuilder::VmState::kVmBooting:
            s.assign(vmis_[i]->GetName() + ":Booting");
            break;
        case VmBuilder::VmState::kVmRunning:
            s.assign(vmis_[i]->GetName() + ":Running");
            break;
        case VmBuilder::VmState::kVmPaused:
            s.assign(vmis_[i]->GetName() + ":Paused");
            break;
        }
        vm_lists[i].assign(s.c_str());
    }

    return 0;
}

int Server::ImportVm(const char payload[]) {
    boost::interprocess::managed_shared_memory shm(
        boost::interprocess::open_read_only,
        payload);

    auto cfg_path = shm.find<bstring>("ImportVmCfgPath");

    std::string p(cfg_path.first->c_str());

    if (p.empty())
        return -1;

    CivConfig cfg;
    if (!cfg.ReadConfigFile(p)) {
        LOG(error) << "Failed to read config file";
        return -1;
    }

    std::string vm_name(cfg.GetValue(kGroupGlob, kGlobName));
    if (vm_name.empty())
        return -1;

    auto id = FindVmInstance(vm_name);
    if (id != -1UL) {
        VmBuilder::VmState st = vmis_[id].get()->GetState();
        if ((st != VmBuilder::VmState::kVmCreated) && (st != VmBuilder::VmState::kVmEmpty)) {
            LOG(error) << "CiV: " << vm_name << " is running! Cannot overwrite!";
            return -1;
        }
        LOG(warning) << "Overwrite existed CiV: " << vm_name;
        DeleteVmInstance(vm_name);
    }

    std::vector<std::unique_ptr<VmBuilder>>::iterator vmi;
    if (cfg.GetValue(kGroupEmul, kEmulType) == kEmulTypeQemu) {
        std::unique_ptr<VmBuilderQemu> vbq = std::make_unique<VmBuilderQemu>(vm_name, std::move(cfg));
        if (!vbq->BuildVmArgs())
            return -1;
        vmi = vmis_.insert(vmis_.end(), std::move(vbq));
    } else {
        /* Default try to contruct for QEMU */
        std::unique_ptr<VmBuilderQemu> vbq = std::make_unique<VmBuilderQemu>(vm_name, std::move(cfg));
        if (!vbq->BuildVmArgs())
            return -1;
        vmi = vmis_.insert(vmis_.end(), std::move(vbq));
    }
    return 0;
}

void Server::VmThread(VmBuilder *vb, boost::latch *wait_continue, bool *vm_ready) {
    startup_listener_.listener.AddPendingVM(vb->GetCid(), [vb](){
        vb->SetVmReady();
    });

    LOG(info) << "Starting VM:  " << vb->GetName();
    /* Start VM */
    vb->StartVm();

    if (vb->WaitVmReady()) {
        *vm_ready = true;
        wait_continue->try_count_down();
        vb->WaitVmExit();
        DeleteVmInstance(vb->GetName());
    } else {
        DeleteVmInstance(vb->GetName());
        wait_continue->try_count_down();
    }
}

int Server::StartVm(const char payload[]) {
    boost::interprocess::managed_shared_memory shm(
        boost::interprocess::open_read_only,
        payload);

    auto vm_name = shm.find<bstring>("StartVmName");
    auto id = FindVmInstance(std::string(vm_name.first->c_str()));
    if (id == -1UL) {
        LOG(warning) << "CiV: [" << vm_name.first->c_str() << "] Not Exists!";
        return -1;
    }

    std::pair<bstring *, int> vm_env = shm.find<bstring>("StartVmEnv");
    std::vector<std::string> env_data;

    for (int i = 0; i < vm_env.second; i++) {
        env_data.push_back(std::string(vm_env.first[i].c_str()));
    }

    VmBuilder *vb = vmis_[id].get();
    vb->SetProcessEnv(std::move(env_data));

    boost::latch wait_continue = 1;
    bool vm_ready = false;

    boost::thread t([this, vb, &wait_continue, &vm_ready]() {
        VmThread(vb, &wait_continue, &vm_ready);
    });
    t.detach();

    wait_continue.wait();

    if (vm_ready) {
        return 0;
    }
    return -1;
#if 0
    if (wait_continue.wait_for(boost::chrono::seconds(200)) == boost::cv_status::timeout) {
        LOG(error) << "CiV[" << vb->GetName() << "]" << " Failed to bootup!";
        startup_listener_.listener.RemovePendingVM(vb->GetCid());
        return -1;
    }
    LOG(info) << "CiV[" << vb->GetName() << "]" << " is ready!";
    t.detach();

    return 0;
#endif
}

static void HandleSIG(int num) {
    LOG(info) << "Signal(" << num << ") received!";
    Server::Get().Stop();
}

void Server::Start(void) {
    try {
        signal(SIGINT, HandleSIG);
        signal(SIGTERM, HandleSIG);

        SetupStartupListenerService();

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
                case kCivMsgListVm:
                    if (ListVm(data.first->payload) == 0) {
                        data.first->type = kCivMsgRespondSuccess;
                    } else {
                        data.first->type = kCivMsgRespondFail;
                    }
                    break;
                case kCivMsgImportVm:
                    if (ImportVm(data.first->payload) == 0) {
                        data.first->type = kCivMsgRespondSuccess;
                    } else {
                        data.first->type = kCivMsgRespondFail;
                    }
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
                case kCivMsgGetVmState:
                    //GetVmState(data.first->payload);
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
    if (startup_listener_.server)
        startup_listener_.server->Shutdown();
}

Server &Server::Get(void) {
    static Server server_;
    return server_;
}

}  // namespace vm_manager
