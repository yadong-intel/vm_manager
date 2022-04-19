/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <filesystem>
#include <ctime>

#include <boost/process.hpp>
#include <boost/process/environment.hpp>
#include <boost/thread.hpp>

#include "utils/log.h"
#include "guest/vm_process.h"

namespace vm_manager {

void VmCoProcSimple::ThreadMon(void) {
    std::error_code ec;

    time_t rawtime;
    struct tm timeinfo;
    char t_buf[80];
    time(&rawtime);
    localtime_r(&rawtime, &timeinfo);
    strftime(t_buf, 80 , "%Y-%m-%d_%T", &timeinfo);

    boost::process::environment env;
    for (std::string s : env_data_) {
        env.set(s.substr(0, s.find('=')), s.substr(s.find('=') + 1));
    }

    LOG(info) << "CMD: " << cmd_;
    size_t exe_pos_begin = cmd_.find_first_not_of(' ');
    size_t exe_pos_end = cmd_.find_first_of(' ', exe_pos_begin);
    std::string exe = cmd_.substr(exe_pos_begin, exe_pos_end - exe_pos_begin);

    std::string tid = boost::lexical_cast<std::string>(mon_->get_id());

    std::string f_out = "/tmp/" + std::string(basename(exe.c_str())) + "_" + t_buf + "_" + tid + "_out.log";

    c_ = std::make_unique<boost::process::child>(
                            cmd_,
                            boost::process::env = env,
                            (boost::process::std_out & boost::process::std_err) > f_out, ec);

    c_->wait(ec);

    int result = c_->exit_code();

    LOG(info) << "Thread-0x" << tid << " Exiting"
              << "\nChild-" << c_->id() << " exited, exit code=" << result
              << "\nlog: " << f_out;
}

void VmCoProcSimple::Run(void) {
    mon_ = std::make_unique<boost::thread>([this] { ThreadMon(); });
}

void VmCoProcSimple::Stop(void) {
    LOG(info) << "Terminate Simp Proc: " << c_->id();
    kill(c_->id(), SIGTERM);
    mon_->try_join_for(boost::chrono::seconds(10));
}

const char *kRpmbData = "RPMB_DATA";
const char *kRpmbSock = "rpmb_sock";

void VmCoProcRpmb::Run(void) {
    LOG(info) << bin_ << " " << data_dir_;
    if (!boost::filesystem::exists(data_dir_ + "/" + kRpmbData)) {
        std::error_code ec;
        boost::process::child init_data(bin_, "--dev " + data_dir_ + "/" + kRpmbData + " --init --size 2048");
        init_data.wait(ec);
        int ret = init_data.exit_code();
    }
    cmd_ = bin_ + " --dev " + data_dir_ + "/" + kRpmbData + " --sock " + data_dir_ + "/" + kRpmbSock;

    mon_ = std::make_unique<boost::thread>([this] { ThreadMon(); });
}

void VmCoProcRpmb::Stop(void) {
    LOG(info) << "Terminate Rpmb Proc: " << c_->id();
    kill(c_->id(), SIGTERM);
    mon_->try_join_for(boost::chrono::seconds(10));
    if (boost::filesystem::exists(data_dir_ + "/" + kRpmbSock)) {
        boost::filesystem::remove(data_dir_ + "/" + kRpmbSock);
    }
}

}  //  namespace vm_manager
