
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
#include <set>
#include <utility>
#include <memory>
#include <queue>

#include <boost/ptr_container/ptr_vector.hpp>

#include "guest/config_parser.h"
#include "guest/vm_builder.h"
#include "guest/aaf.h"

namespace vm_manager {

class VmBuilderQemu : public VmBuilder {
 public:
    explicit VmBuilderQemu(std::string name, CivConfig cfg, std::vector<std::string> env) :
                       VmBuilder(name), cfg_(cfg), env_data_(env) {}
    ~VmBuilderQemu();
    bool BuildVmArgs(void);
    void StartVm(void);
    void StopVm(void);
    void WaitVm(void);

 private:
    bool BuildEmulPath(void);
    void BuildFixedCmd(void);
    bool BuildNameQmp(void);
    void BuildNetCmd(void);
    bool BuildVsockCmd(void);
    void BuildRpmbCmd(void);
    void BuildVtpmCmd(void);
    void BuildAafCfg(void);
    bool BuildVgpuCmd(void);
    void BuildMemCmd(void);
    void BuildVcpuCmd(void);
    bool BuildFirmwareCmd(void);
    void BuildVdiskCmd(void);
    void BuildPtPciDevicesCmd(void);
    void BuildGuestTimeKeepCmd(void);
    void BuildGuestPmCtrlCmd(void);
    void BuildAudioCmd(void);
    void BuildExtraCmd(void);

    void SoundCardHook(void);
    bool PassthroughGpu(void);

    void SetPciDevicesCallback(void);
    bool SetupSriov(void);
    void RunMediationSrv(void);
    void SetExtraServices(void);

    CivConfig cfg_;
    std::unique_ptr<Aaf> aaf_cfg_;
    uint32_t vsock_cid_;
    bool vm_ready = false;
    std::unique_ptr<VmProcess> main_proc_;
    std::vector<std::unique_ptr<VmProcess>> co_procs_;
    std::string emul_cmd_;
    std::vector<std::string> env_data_;
    std::set<std::string> pci_pt_dev_set_;
    std::queue<std::function<void(void)>> end_call_;
};

}  // namespace vm_manager

#endif  // SRC_GUEST_VM_BUILDER_QEMU_H_
