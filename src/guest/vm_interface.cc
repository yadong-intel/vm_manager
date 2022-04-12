
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
#include "guest/vm_interface.h"
#include "services/message.h"
#include "utils/log.h"

namespace vm_manager {

int ShutdownVm(std::string name) {
    // Shutdown VM
    return 0;
}

void GuestThread(std::vector<std::string> env_data) {
    LOG(info) << "Thread-0x" << std::hex << std::this_thread::get_id() << " Starting";
    std::error_code ec;
    boost::asio::io_context ioc;
    std::future<std::string> buf_o, buf_e;
    boost::process::environment env_;

    for (std::string s : env_data) {
        env_.set(s.substr(0, s.find('=')), s.substr(s.find('=') + 1));
    }

    boost::process::child c(//boost::process::search_path("qemu-system-x86_64"),
                            //boost::process::args = { "-device", "virtio-gpu" },
                            "qemu-system-x86_64 -device virtio-gpu", 
                            env_,
                            boost::process::std_out > buf_o,
                            boost::process::std_err > buf_e,
                            ioc);

    LOG(info) << "Running=" << c.running(ec) << ", PID="<< c.id();
    LOG(info) << ec.message();

    ioc.run();

    int result = c.exit_code();

    LOG(info) << "Child-" << c.id() << " exited! code=" << result;
    LOG(info) << "Out: " << buf_o.get().c_str() << "Err:" << buf_e.get().c_str();
    LOG(info) << "Thread-0x" << std::hex << std::this_thread::get_id() << " Exiting";
}

int StartVm(const char payload[]) {
    boost::interprocess::managed_shared_memory shm(
        boost::interprocess::open_only,
        payload);
    std::pair<bstring*, boost::interprocess::managed_shared_memory::size_type> vm_name;
    vm_name = shm.find<bstring>("StartVmName");

    std::pair<bstring*, boost::interprocess::managed_shared_memory::size_type> vm_env;
    vm_env = shm.find<bstring>("StartVmEnv");
    std::vector<std::string> env_data;

    for (int i = 0; i < vm_env.second; i++) {
        env_data.push_back(std::string(vm_env.first[i].c_str()));
    }
    VmBuilder vb(vm_name.first->c_str());

    std::thread gt(GuestThread, env_data);
    std::thread::id tid = gt.get_id();
    gt.detach();

    if (vb.BuildVmArgs()) {
        vb.StartVm();
    } else {
        LOG(error) << "Failed to build args";
        return -1;
    }
    return 0;
}

}  //  namespace vm_manager
