
/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef __VM_BUILDER_H__
#define __VM_BUILDER_H__

#include <string>

#include "guest/config_parser.h"

namespace vm_manager {

class VmBuilder {
 public:
    explicit VmBuilder(std::string name);
    bool BuildVmArgs();
    void StartVm();
 private:
    CivConfig cfg_;
    std::string emul_cmd_;
    bool BuildQemuVmArgs();
};

}  // namespace vm_manager

#endif /* __VM_BUILDER_H__ */