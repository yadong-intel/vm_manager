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
#include <memory>

#include <boost/thread.hpp>
#include <boost/asio.hpp>

#include "utils/log.h"

namespace vm_manager {

extern const char *kRpmbData;
extern const char *kRpmbSock;
extern const char *kVtpmSock;

class VmProcess {
 public:
    virtual void Run(void) = 0;
    virtual void Stop(void) = 0;
    virtual ~VmProcess() = default;
};

class VmCoProcSimple : public VmProcess {
 public:
    VmCoProcSimple(std::string cmd, std::vector<std::string> env) :
                    cmd_(cmd), env_data_(env) {}
    virtual void Run(void);
    virtual void Stop(void);
    virtual ~VmCoProcSimple();
 protected:
    VmCoProcSimple(const VmCoProcSimple&) = delete;
    VmCoProcSimple& operator=(const VmCoProcSimple&) = delete;

    void ThreadMon(void);

    std::string cmd_;
    std::vector<std::string> env_data_;

    std::unique_ptr<boost::process::child> c_;
 private:
    std::unique_ptr<boost::thread> mon_;
};

class VmCoProcRpmb : public VmCoProcSimple {
 public:
    VmCoProcRpmb(std::string bin, std::string data_dir, std::vector<std::string> env) :
          VmCoProcSimple("", env), bin_(bin), data_dir_(data_dir) {}

    virtual void Run(void);
    void Stop(void);
    ~VmCoProcRpmb();

 private:
    VmCoProcRpmb(const VmCoProcRpmb&) = delete;
    VmCoProcRpmb& operator=(const VmCoProcRpmb&) = delete;

    std::string bin_;
    std::string data_dir_;
};

class VmCoProcVtpm : public VmCoProcSimple {
 public:
    VmCoProcVtpm(std::string bin, std::string data_dir, std::vector<std::string> env) :
          VmCoProcSimple("", env), bin_(bin), data_dir_(data_dir) {}

    virtual void Run(void);
    void Stop(void);
    ~VmCoProcVtpm();

 private:
    VmCoProcVtpm(const VmCoProcVtpm&) = delete;
    VmCoProcVtpm& operator=(const VmCoProcVtpm&) = delete;

    std::string bin_;
    std::string data_dir_;
};

}  //  namespace vm_manager

#endif  // SRC_GUEST_VM_PROCESS_H_
