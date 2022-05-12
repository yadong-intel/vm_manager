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
#include <exception>

#include <boost/thread.hpp>
#include <boost/process.hpp>

#include "guest/config_parser.h"
#include "guest/vm_process.h"
#include "utils/log.h"

namespace vm_manager {

class VmBuilder {
 public:
    VmBuilder(std::string name) : name_(name) {}
    virtual ~VmBuilder() = default;
    virtual bool BuildVmArgs(void) = 0;
    virtual void StartVm(void) = 0;
    virtual void WaitVm(void) = 0;
    virtual void StopVm(void) = 0;
    std::string GetName(void);
 protected:
    std::string name_;
};

}  // namespace vm_manager

#endif  // SRC_GUEST_VM_BUILDER_H_
