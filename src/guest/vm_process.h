/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef SRC_GUEST_VM_PROCESS_H_
#define SRC_GUEST_VM_PROCESS_H_

#include <string>
#include <vector>
#include <thread>

namespace vm_manager {

extern const char *kRpmbData;
extern const char *kRpmbSock;

class VmProcess {
 public:
    virtual std::thread Run(void) = 0;
};

class VmCoProcSimple : public VmProcess {
 public:
    VmCoProcSimple(std::string cmd, std::vector<std::string> env) :
                    cmd_(cmd), env_data_(env) {}
    virtual std::thread Run(void);
 protected:
    std::string cmd_;
    std::vector<std::string> env_data_;
};

class VmCoProcRpmb : public VmCoProcSimple {
 public:
    VmCoProcRpmb(std::string bin, std::string data_dir, std::vector<std::string> env) :
          VmCoProcSimple("", env), bin_(bin), data_dir_(data_dir) {}

    std::thread Run(void);

 private:
    std::string bin_;
    std::string data_dir_;
};

}  //  namespace vm_manager

#endif  // SRC_GUEST_VM_PROCESS_H_
