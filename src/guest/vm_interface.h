
/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef SRC_GUEST_VM_INTERFACE_H_
#define SRC_GUEST_VM_INTERFACE_H_

#include <string>
#include <vector>

#include "guest/config_parser.h"
#include "guest/vm_builder.h"

namespace vm_manager {

class VmInstance {
 public:
    VmInstance(std::string name, CivConfig c, std::vector<std::string> env) : name_(name), cfg_(c), env_data_(env) {}
    void Build(void);
    void Start(void);
    void Stop(void);
    std::string GetName() {
        return name_;
    }

 private:
    std::string name_;
    VmBuilder *vb_;
    CivConfig cfg_;
    std::vector<std::string> env_data_;
    boost::thread t_;
};

int StartVm(const char payload[]);
int ShutdownVm(const char payload[]);

}  // namespace vm_manager

#endif  // SRC_GUEST_VM_INTERFACE_H_
