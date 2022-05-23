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
    enum class VmState {
       kVmEmpty = 0,
       kVmCreated,
       kVmBooting,
       kVmRunning,
       kVmPaused,
    };

 public:
    explicit VmBuilder(std::string name) : name_(name) {}
    virtual ~VmBuilder() = default;
    virtual bool BuildVmArgs(void) = 0;
    virtual void StartVm(void) = 0;
    virtual void WaitVmExit(void) = 0;
    virtual void StopVm(void) = 0;
    virtual void PauseVm(void) = 0;
    virtual bool WaitVmReady(void) = 0;
    virtual void SetVmReady(void) = 0;
    std::string GetName(void);
    uint32_t GetCid(void);
    VmState GetState(void);

 protected:
    std::string name_;
    uint32_t vsock_cid_;
    VmState state_ = VmBuilder::VmState::kVmEmpty;
    std::mutex state_lock_;
};

}  // namespace vm_manager

#endif  // SRC_GUEST_VM_BUILDER_H_
