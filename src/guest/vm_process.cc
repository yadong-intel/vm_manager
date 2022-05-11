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
              << "\n\t\tChild-" << c_->id() << " exited, exit code=" << result
              << "\n\t\tlog: " << f_out;
}

void VmCoProcSimple::Run(void) {
    mon_ = std::make_unique<boost::thread>([this] { ThreadMon(); });
}

void VmCoProcSimple::Stop(void) {
    if (!mon_.get())
        return;
    LOG(info) << "Terminate CoProc: " << c_->id();
    kill(c_->id(), SIGTERM);
    mon_->try_join_for(boost::chrono::seconds(10));
    mon_.reset(nullptr);
}

bool VmCoProcSimple::Running(void) {
    if (c_)
        return c_->running();
    return false;
}

VmCoProcSimple::~VmCoProcSimple() {
    Stop();
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

    VmCoProcSimple::Run();
}

void VmCoProcRpmb::Stop(void) {
    VmCoProcSimple::Stop();

    /* TODO: this temp rpmb sock file is created by rpmb_dev, but rpmb_dev did not cleanup
             when exit, so here check and remove the sock file after the rpmb_dev process
             exited. Need to remove this block of code once the rpmb_dev fixed the issue.
     */
    if (boost::filesystem::exists(data_dir_ + "/" + kRpmbSock)) {
        LOG(info) << "Cleanup Rpmb CoProc stuff ...";
        boost::filesystem::remove(data_dir_ + "/" + kRpmbSock);
    }
}


const char *kVtpmSock = "swtpm-sock";

VmCoProcRpmb::~VmCoProcRpmb() {
    Stop();
}

void VmCoProcVtpm::Run(void) {
    LOG(info) << bin_ << " " << data_dir_;

    if (!boost::filesystem::exists(data_dir_)) {
        LOG(warning) << "Data path for Vtpm not exists!";
        return;
    }

    cmd_ = bin_ + " socket --tpmstate dir=" + data_dir_ +
           " --tpm2 --ctrl type=unixio,path=" + data_dir_ + "/" + kVtpmSock;

    VmCoProcSimple::Run();
}

void VmCoProcVtpm::Stop(void) {
    VmCoProcSimple::Stop();
}

VmCoProcVtpm::~VmCoProcVtpm() {
    Stop();
}

}  //  namespace vm_manager
