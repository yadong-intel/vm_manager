
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

namespace vm_manager {

class VmBuilder {
 public:
    explicit VmBuilder(const char *name);
    bool BuildVmArgs();
    void StartVm();
 private:
    CivConfig cfg_;
    uint32_t vsock_cid_;
    std::vector<std::pair<std::string, std::string>> prev_cmds_;
    std::string emul_cmd_;
    bool BuildQemuVmArgs();
};

}  // namespace vm_manager

#endif  // SRC_GUEST_VM_BUILDER_H_
