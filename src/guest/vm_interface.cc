
/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <utility>
#include <vector>
#include <memory>

#include <boost/process.hpp>
#include <boost/process/environment.hpp>

#include "guest/vm_builder.h"
#include "guest/vm_builder_qemu.h"
#include "guest/vm_interface.h"
#include "services/message.h"
#include "utils/log.h"
#include "utils/utils.h"

namespace vm_manager {

std::vector<std::unique_ptr<VmBuilder>> vm_instances;

static int FindVmInstance(std::string name) {
    for (int i = 0; i < vm_instances.size(); ++i) {
        if (vm_instances[i]->GetName().compare(name) == 0)
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
    int id = FindVmInstance(std::string(vm_name.first->c_str()));
    if (id != -1) {
        vm_instances[id]->StopVm();
        vm_instances.erase(vm_instances.begin() + id);
    }
    if (id == -1)
        LOG(warning) << "CiV: " << vm_name.first->c_str() << " is not running!";
    return 0;
}

int StartVm(const char payload[]) {
    boost::interprocess::managed_shared_memory shm(
        boost::interprocess::open_only,
        payload);

    auto vm_name = shm.find<bstring>("StartVmName");
    std::string cfg_file = GetConfigPath() + std::string("/") + vm_name.first->c_str() + ".ini";

    if (FindVmInstance(std::string(vm_name.first->c_str())) != -1) {
        LOG(warning) << "CiV: " << vm_name.first->c_str() << " is already running!";
        return -1;
    }

    auto vm_env = shm.find<bstring>("StartVmEnv");
    std::vector<std::string> env_data;

    for (int i = 0; i < vm_env.second; i++) {
        env_data.push_back(std::string(vm_env.first[i].c_str()));
    }

    CivConfig cfg;
    if (!cfg.ReadConfigFile(cfg_file)) {
        LOG(error) << "Failed to read config file";
        return -1;
    }

    std::vector<std::unique_ptr<VmBuilder>>::iterator vmi;
    if (cfg.GetValue(kGroupEmul, kEmulType) == kEmulTypeQemu) {
        vmi = vm_instances.emplace(vm_instances.end(),
                    std::make_unique<VmBuilderQemu>(vm_name.first->c_str(), std::move(cfg), std::move(env_data)));
    } else {
        /* Default try to build args for QEMU */
        vmi = vm_instances.emplace(vm_instances.end(),
                    std::make_unique<VmBuilderQemu>(vm_name.first->c_str(), std::move(cfg), std::move(env_data)));
    }
    vmi->get()->BuildVmArgs();
    vmi->get()->StartVm();

    return 0;
}

}  //  namespace vm_manager
