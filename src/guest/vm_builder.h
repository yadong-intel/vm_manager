/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef SRC_GUEST_VM_BUILDER_H_
#define SRC_GUEST_VM_BUILDER_H_

#include <string>
#include <vector>
#include <utility>

#include <boost/thread.hpp>
#include <boost/process.hpp>

#include "guest/config_parser.h"
#include "guest/vm_process.h"
#include "utils/log.h"

namespace vm_manager {

class VmBuilder {
 public:
    explicit VmBuilder(std::vector<std::string> env) : env_data_(env) {}
    virtual bool BuildVmArgs(CivConfig cfg) = 0;
    virtual void StartVm();
    void StopVm();

 protected:
    CivConfig cfg__;
    uint32_t vsock_cid_;
    std::vector<VmProcess *> co_procs_;
    std::string emul_cmd_;
    std::vector<std::string> env_data_;
    boost::thread main_th_;
    boost::thread_group tg_;
    boost::process::group sub_proc_grp_;
    bool shut_vm = false;
};

}  // namespace vm_manager

#endif  // SRC_GUEST_VM_BUILDER_H_
