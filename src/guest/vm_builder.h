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

#include "guest/config_parser.h"
#include "guest/vm_process.h"
#include "utils/log.h"

namespace vm_manager {

class VmBuilder {
 public:
    explicit VmBuilder(CivConfig cfg, std::vector<std::string> env) : cfg_(cfg), env_data_(env) {}
    virtual bool BuildVmArgs() = 0;
    virtual std::thread StartVm();

 protected:
    CivConfig cfg_;
    uint32_t vsock_cid_;
    std::vector<VmProcess *> co_procs;
    std::string emul_cmd_;
    std::vector<std::string> env_data_;
};

}  // namespace vm_manager

#endif  // SRC_GUEST_VM_BUILDER_H_
