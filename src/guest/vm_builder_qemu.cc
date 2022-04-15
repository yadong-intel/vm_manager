
/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <mutex>
#include <utility>

#include <boost/process.hpp>

#include "guest/vm_builder.h"
#include "guest/vm_builder_qemu.h"
#include "guest/config_parser.h"
#include "guest/vsock_cid_pool.h"
#include "guest/vm_process.h"
#include "services/message.h"
#include "utils/log.h"
#include "utils/utils.h"

namespace vm_manager {

bool VmBuilderQemu::BuildVmArgs(CivConfig cfg_) {
    LOG(info) << "build qemu vm args";

    std::string str_emul_path = cfg_.GetValue(kGroupEmul, kEmulPath);
    boost::filesystem::path emul_path;
    if (boost::filesystem::exists(str_emul_path)) {
        emul_path = boost::filesystem::absolute(str_emul_path);
    } else {
        if (str_emul_path.empty()) {
            str_emul_path = "qemu-system-x86_64";
        }
        LOG(info) << "Trying to search" << str_emul_path << " from environment PATH";
        emul_path = boost::process::search_path(str_emul_path);
    }
    if (emul_path.empty()) {
        return false;
    }
    emul_cmd_.assign(emul_path.c_str());

    const std::string fixed_args = (
        " -M q35"
        " -machine kernel_irqchip=on"
        " -k en-us"
        " -cpu host,-waitpkg"
        " -enable-kvm"
        " -device qemu-xhci,id=xhci,p2=8,p3=8"
        " -device usb-mouse"
        " -device usb-kbd"
        " -device e1000,netdev=net0"
        " -device intel-iommu,device-iotlb=on,caching-mode=on"
        " -nodefaults ");
    emul_cmd_.append(fixed_args);

    std::string vm_name = cfg_.GetValue(kGroupGlob, kGlobName);
    if (vm_name.empty()) {
        emul_cmd_.clear();
        return false;
    }
    emul_cmd_.append(" -name " + vm_name);
    emul_cmd_.append(" -qmp unix:" + std::string(GetConfigPath()) + "/." + vm_name + ".qmp.unix.socket,server,nowait");

    std::string net_arg = " -netdev user,id=net0";
    std::string adb_port = cfg_.GetValue(kGroupGlob, kGlobAdbPort);
    if (!adb_port.empty())
        net_arg.append(",hostfwd=tcp::" + adb_port + "-:5555");
    std::string fb_port = cfg_.GetValue(kGroupGlob, kGlobFastbootPort);
    if (!fb_port.empty())
        net_arg.append(",hostfwd=tcp::" + adb_port + "-:5554");
    emul_cmd_.append(net_arg);

    std::string str_cid = cfg_.GetValue(kGroupGlob, kGlobCid);
    if (str_cid.empty()) {
        vsock_cid_ = VsockCidPool::Pool().GetCid();
        if (vsock_cid_ == 0) {
            emul_cmd_.clear();
            return false;
        }
    } else {
        try {
            vsock_cid_ = VsockCidPool::Pool().GetCid(std::stoul(str_cid));
            if (vsock_cid_ == 0) {
                LOG(error) << "Cannot acquire cid(" << str_cid << ") from cid pool!";
                emul_cmd_.clear();
                return false;
            }
        } catch (std::exception &e) {
            LOG(error) << "Invalid Cid!" << e.what();
            emul_cmd_.clear();
            return false;
        }
    }
    emul_cmd_.append(" -device vhost-vsock-pci,id=vhost-vsock-pci0,bus=pcie.0,addr=0x20,guest-cid=" +
                    std::to_string(vsock_cid_));

    std::string rpmb_bin = cfg_.GetValue(kGroupRpmb, kRpmbBinPath);
    std::string rpmb_data = cfg_.GetValue(kGroupRpmb, kRpmbDataDir);
    if (!rpmb_bin.empty() && !rpmb_data.empty()) {
        emul_cmd_.append(" -device virtio-serial,addr=1"
                         " -device virtserialport,chardev=rpmb0,name=rpmb0,nr=1"
                         " -chardev socket,id=rpmb0,path=" +
                           rpmb_data + "/" + std::string(kRpmbSock));
    }
    //VmProcess *vcpr = new VmCoProcRpmb(std::move(rpmb_bin), std::move(rpmb_data), env_data_);
    //co_procs.push_back(vcpr);

    VmProcess *test = new VmCoProcSimple("qemu-system-x86_64 -device virtio-gpu", env_data_);
    co_procs_.push_back(test);
    VmProcess *test2 = new VmCoProcSimple("qemu-system-x86_64 -device virtio-gpu", env_data_);
    co_procs_.push_back(test2);

    std::string vtpm_bin = cfg_.GetValue(kGroupVtpm, kVtpmBinPath);
    std::string vtpm_data = cfg_.GetValue(kGroupVtpm, kVtpmDataDir);
    if (!vtpm_bin.empty() && !vtpm_data.empty()) {
        emul_cmd_.append(" -chardev socket,id=chrtpm,path=" +
                         vtpm_data.append("swtpm-sock") +
                         " -tpmdev emulator,id=tpm0,chardev=chrtpm -device tpm-crb,tpmdev=tpm0");
    }

    return true;
}

}  //  namespace vm_manager
