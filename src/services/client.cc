/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <string>
#include <utility>

#include <boost/process/environment.hpp>

#include "services/client.h"
#include "services/message.h"
#include "services/server.h"
#include "utils/log.h"

namespace vm_manager {

void Client::PrepareStartGuestClientShm(const char *vm_name) {
    boost::process::environment env = boost::this_process::environment();

    client_shm_.destroy<bstring>("StartVmName");
    client_shm_.destroy<bstring>("StartVmEnv");
    client_shm_.zero_free_memory();

    bstring *var_name = client_shm_.construct<bstring>
                ("StartVmName")
                (vm_name, client_shm_.get_segment_manager());

    bstring *var_env = client_shm_.construct<bstring>
                ("StartVmEnv")
                [env.size()]
                (client_shm_.get_segment_manager());

    for (std::string s : env._data) {
        var_env->assign(s.c_str());
        var_env++;
    }
}

void Client::PrepareStopGuestClientShm(const char *vm_name) {
    boost::process::environment env = boost::this_process::environment();

    client_shm_.destroy<bstring>("StopVmName");
    client_shm_.zero_free_memory();

    bstring *var_name = client_shm_.construct<bstring>
                ("StopVmName")
                (vm_name, client_shm_.get_segment_manager());
}

bool Client::Notify(CivMsgType t) {
    std::pair<CivMsgSync*, boost::interprocess::managed_shared_memory::size_type> sync;
    sync = server_shm_.find<CivMsgSync>(kCivServerObjSync);
    if (!sync.first || (sync.second != 1)) {
        LOG(error) << "Failed to find sync block!" << sync.first << " size=" << sync.second;
        return false;
    }
    boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> lock_clients(sync.first->mutex);

    CivMsg *data = server_shm_.construct<CivMsg>
                (kCivServerObjData)
                ();

    data->type = t;
    strncpy(data->payload, client_shm_name_.c_str(), sizeof(data->payload));

    boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> lock_cond(sync.first->mutex_cond);
    sync.first->cond_s.notify_one();

    sync.first->cond_c.wait(lock_cond);

    server_shm_.destroy<CivMsg>(kCivServerObjData);
    server_shm_.zero_free_memory();

    return true;
}

Client::Client() {
    server_shm_ = boost::interprocess::managed_shared_memory(boost::interprocess::open_only, kCivServerMemName);

    client_shm_name_ = std::string("CivClientShm" + std::to_string(getpid()));
    boost::interprocess::permissions unrestricted_permissions;
    unrestricted_permissions.set_unrestricted();
    client_shm_ = boost::interprocess::managed_shared_memory(
            boost::interprocess::create_only,
            client_shm_name_.c_str(),
            65536,
            0,
            unrestricted_permissions);
}

Client::~Client() {
    boost::interprocess::shared_memory_object::remove(client_shm_name_.c_str());
}

}  // namespace vm_manager
