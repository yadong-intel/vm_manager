
/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef SRC_GUEST_VM_BUILDER_QEMU_H_
#define SRC_GUEST_VM_BUILDER_QEMU_H_

#include <string>
#include <vector>
#include <utility>

#include "guest/config_parser.h"
#include "guest/vm_builder.h"

namespace vm_manager {

class VmBuilderQemu : public VmBuilder {
 public:
    explicit VmBuilderQemu(std::vector<std::string> env) : VmBuilder(env) {}
    bool BuildVmArgs(CivConfig cfg_);
};

}  // namespace vm_manager

#endif  // SRC_GUEST_VM_BUILDER_QEMU_H_
