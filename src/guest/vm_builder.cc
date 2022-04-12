
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
#include "guest/config_parser.h"
#include "services/message.h"
#include "utils/log.h"
#include "utils/utils.h"

namespace vm_manager {

class VsockCidPool {
#define CIV_MAX_CID_NUM 2048U

 public:
    uint32_t GetCid() {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t pos = bs_._Find_first();
        if (pos == CIV_MAX_CID_NUM)
            return 0;
        bs_.reset(pos);
        return kCiVCidStart + pos;
    }

    uint32_t GetCid(uint32_t cid) {
        std::lock_guard<std::mutex> lock(mutex_);
        if ((cid < kCiVCidStart) || (cid >= (kCiVCidStart + CIV_MAX_CID_NUM)))
            return 0;
        if (!bs_[cid - kCiVCidStart])
            return 0;

        bs_.reset(cid - kCiVCidStart);
        return cid;
    }

    bool ReleaseCid(uint32_t cid) {
        if ((cid < kCiVCidStart) || (cid >= (kCiVCidStart + CIV_MAX_CID_NUM)))
            return false;
        std::lock_guard<std::mutex> lock(mutex_);
        bs_.set(cid - kCiVCidStart);
        return true;
    }

    static VsockCidPool &Pool(void) {
        static VsockCidPool vcp_;
        return vcp_;
    }

 private:
    VsockCidPool() = default;
    ~VsockCidPool() = default;
    VsockCidPool(const VsockCidPool &) = delete;
    VsockCidPool& operator=(const VsockCidPool&) = delete;

    const uint32_t kCiVCidStart = 1024U;
    std::bitset<CIV_MAX_CID_NUM> bs_ = std::move(std::bitset<CIV_MAX_CID_NUM>{}.set());
    std::mutex mutex_;
};

bool VmBuilder::BuildQemuVmArgs() {
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
                           rpmb_data.append("/rpmb-sock"));
    }

    return true;
}

bool VmBuilder::BuildVmArgs() {
    if (cfg_.GetValue(kGroupEmul, kEmulType) == kEmulTypeQemu) {
        return BuildQemuVmArgs();
    } else {
        /* Default try to build args for QEMU */
        return BuildQemuVmArgs();
    }
    return false;
}

void VmBuilder::StartVm() {
    LOG(info) << "Emulator command:" << emul_cmd_;
}

VmBuilder::VmBuilder(const char *name) {
    LOG(info) << "Config file path:" << GetConfigPath()  << std::string("/") << name << ".ini";
    if (!cfg_.ReadConfigFile(GetConfigPath() + std::string("/") + name + ".ini")) {
        LOG(error) << "Failed to read config file";
    }
}

}  //  namespace vm_manager
