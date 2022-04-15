/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <filesystem>

#include <boost/process.hpp>
#include <boost/process/environment.hpp>
#include <boost/thread.hpp>

#include "utils/log.h"
#include "guest/vm_process.h"

namespace vm_manager {

static void RunProc(std::string cmd, std::vector<std::string> env_data, boost::asio::io_context *ioc, boost::process::group *g) {
    LOG(info) << "Thread-0x" << std::hex << std::this_thread::get_id() << " Starting";
    std::error_code ec;
    std::future<std::string> buf_o, buf_e;
    boost::process::environment env;

    for (std::string s : env_data) {
        env.set(s.substr(0, s.find('=')), s.substr(s.find('=') + 1));
    }

    LOG(info) << "CMD: " << cmd;

    boost::process::child c(cmd,
                            env,
                            boost::process::std_out > buf_o,
                            boost::process::std_err > buf_e,
                            *ioc, *g);

    LOG(info) << "Running=" << c.running(ec) << ", PID="<< c.id();
    LOG(info) << ec.message();

    ioc->run();

    if (c.running()) {
        c.terminate();
    }

    int result = c.exit_code();

    LOG(info) << "Child-" << c.id() << " exited! code=" << result;
    LOG(info) << "Out: " << buf_o.get().c_str() << "Err:" << buf_e.get().c_str();
    LOG(info) << "Thread-0x" << std::hex << std::this_thread::get_id() << " Exiting";
}

boost::thread &VmCoProcSimple::Run(boost::process::group *g) {
    mon_ = boost::thread(RunProc, cmd_, env_data_, &ioc_, g);
    return mon_;
}

const char *kRpmbData = "RPMB_DATA";
const char *kRpmbSock = "rpmb_sock";

boost::thread &VmCoProcRpmb::Run(boost::process::group *g) {
    LOG(info) << bin_ << " " << data_dir_;
    if (!boost::filesystem::exists(data_dir_ + "/" + kRpmbData)) {
        std::error_code ec;
        boost::process::child init_data(bin_, "--dev " + data_dir_ + "/" + kRpmbData + " --init --size 2048");
        init_data.wait(ec);
        int ret = init_data.exit_code();
        if (ret != 0) {
            mon_.~thread();
            return mon_;
        }
    }
    cmd_ = bin_ + " --dev " + data_dir_ + "/" + kRpmbData + " --sock " + data_dir_ + "/" + kRpmbSock;

    mon_ = boost::thread(RunProc, cmd_, env_data_, &ioc_, g);
    return mon_;
}

}  //  namespace vm_manager
