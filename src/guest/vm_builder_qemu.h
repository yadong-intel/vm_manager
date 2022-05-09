
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
#include <memory>
#include <queue>

#include <boost/ptr_container/ptr_vector.hpp>

#include "guest/config_parser.h"
#include "guest/vm_builder.h"

namespace vm_manager {

class VmBuilderQemu : public VmBuilder {
 public:
    explicit VmBuilderQemu(std::string name, CivConfig cfg, std::vector<std::string> env) :
                       VmBuilder(name), cfg_(cfg), env_data_(env) {}
    ~VmBuilderQemu();
    bool BuildVmArgs(void);
    void StartVm(void);
    void StopVm(void);

 private:
    void soundcard_hook(void);
    bool passthrough_gpu(void);
    bool setup_sriov(void);

    CivConfig cfg_;
    uint32_t vsock_cid_;
    std::vector<std::unique_ptr<VmProcess>> co_procs_;
    std::string emul_cmd_;
    std::vector<std::string> env_data_;
    std::vector<std::string> pci_pt_dev_;
    std::queue<std::function<void(void)>> end_call_;
};

}  // namespace vm_manager

#endif  // SRC_GUEST_VM_BUILDER_QEMU_H_
