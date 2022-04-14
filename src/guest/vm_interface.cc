
/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <utility>
#include <vector>

#include <boost/process.hpp>
#include <boost/process/environment.hpp>

#include "guest/vm_builder.h"
#include "guest/vm_builder_qemu.h"
#include "guest/vm_interface.h"
#include "services/message.h"
#include "utils/log.h"
#include "utils/utils.h"

namespace vm_manager {

std::vector<std::pair<std::string, std::thread>> thread_pairs;

static int FindVmThread(std::string name) {
    for (int i = 0; i < thread_pairs.size(); ++i) {
        LOG(info) << "thread: " << thread_pairs[i].first << " id=" << std::hex << thread_pairs[i].second.get_id();
        if (thread_pairs[i].first.compare(name) == 0)
            return i;
    }
    return -1;
}

int ShutdownVm(const char payload[]) {
    boost::interprocess::managed_shared_memory shm(
        boost::interprocess::open_only,
        payload);
    std::pair<bstring*, boost::interprocess::managed_shared_memory::size_type> vm_name;
    vm_name = shm.find<bstring>("StopVmName");
    int id = FindVmThread(std::string(vm_name.first->c_str()));
    if (id != -1)
        LOG(info) << "thread joinable?  " << thread_pairs[id].second.joinable();
    if (id == -1)
        LOG(warning) << "CiV: " << vm_name.first->c_str() << " is not running!";
    return 0;
}

int StartVm(const char payload[]) {
    boost::interprocess::managed_shared_memory shm(
        boost::interprocess::open_only,
        payload);
    std::pair<bstring*, boost::interprocess::managed_shared_memory::size_type> vm_name;
    vm_name = shm.find<bstring>("StartVmName");
    std::string cfg_file = GetConfigPath() + std::string("/") + vm_name.first->c_str() + ".ini";

    if (FindVmThread(std::string(vm_name.first->c_str())) != -1) {
        LOG(warning) << "CiV: " << vm_name.first->c_str() << " is already running!";
        return -1;
    }

    std::pair<bstring*, boost::interprocess::managed_shared_memory::size_type> vm_env;
    vm_env = shm.find<bstring>("StartVmEnv");
    std::vector<std::string> env_data;

    for (int i = 0; i < vm_env.second; i++) {
        env_data.push_back(std::string(vm_env.first[i].c_str()));
    }

    CivConfig cfg;
    if (!cfg.ReadConfigFile(cfg_file)) {
        LOG(error) << "Failed to read config file";
        return -1;
    }

    VmBuilder *vb = nullptr;
    if (cfg.GetValue(kGroupEmul, kEmulType) == kEmulTypeQemu) {
        vb = new VmBuilderQemu(std::move(cfg), std::move(env_data));
    } else {
        /* Default try to build args for QEMU */
        vb = new VmBuilderQemu(std::move(cfg), std::move(env_data));
    }

    if (vb->BuildVmArgs()) {
        std::thread t = vb->StartVm();
        t.detach();
        thread_pairs.push_back(make_pair(std::string(vm_name.first->c_str()), std::move(t)));
    } else {
        LOG(error) << "Failed to build args";
        return -1;
    }
    return 0;
}

}  //  namespace vm_manager
