
/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <mutex>
#include <utility>
#include <memory>
#include <fstream>

#include <boost/process.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include "guest/vm_builder.h"
#include "guest/vm_builder_qemu.h"
#include "guest/config_parser.h"
#include "guest/vsock_cid_pool.h"
#include "guest/vm_process.h"
#include "guest/aaf.h"
#include "services/message.h"
#include "utils/log.h"
#include "utils/utils.h"

namespace vm_manager {
constexpr const char *kPciDevicePath = "/sys/bus/pci/devices/";
constexpr const char *kPciDriverPath = "/sys/bus/pci/drivers/";
constexpr const char *kPciDriverProbe = "/sys/bus/pci/drivers_probe";

constexpr const char *kIntelGpuBdf = "0000:00:02.0";
constexpr const char *kIntelGpuDevPath = "/sys/bus/pci/devices/0000:00:02.0";
constexpr const char *kIntelGpuDevice = "/sys/bus/pci/devices/0000:00:02.0/device";
constexpr const char *kIntelGpuDriver = "/sys/bus/pci/devices/0000:00:02.0/driver";
constexpr const char *kIntelGpuDriverUnbind = "/sys/bus/pci/devices/0000:00:02.0/driver/unbind";
constexpr const char *kIntelGpuSriovTotalVfs = "/sys/bus/pci/devices/0000:00:02.0/sriov_totalvfs";
constexpr const char *kIntelGpuSriovAutoProbe = "/sys/bus/pci/devices/0000:00:02.0/sriov_drivers_autoprobe";
constexpr const char *kDrmCard0DevSriovNumVfs = "/sys/class/drm/card0/device/sriov_numvfs";

constexpr const char *kVfioPciNewId =    "/sys/bus/pci/drivers/vfio-pci/new_id";
constexpr const char *kVfioPciRemoveId = "/sys/bus/pci/drivers/vfio-pci/remove_id";
constexpr const char *kVfioPciUnbind =   "/sys/bus/pci/drivers/vfio-pci/unbind";

constexpr const char *kGvtgMdevTypePath = "/sys/bus/pci/devices/mdev_supported_types/";
constexpr const char *kGvtgMdevV51Path = "/sys/bus/pci/devices/mdev_supported_types/i915-GVTg_V5_1/";
constexpr const char *kGvtgMdevV52Path = "/sys/bus/pci/devices/mdev_supported_types/i915-GVTg_V5_2/";
constexpr const char *kGvtgMdevV54Path = "/sys/bus/pci/devices/mdev_supported_types/i915-GVTg_V5_4/";
constexpr const char *kGvtgMdevV58Path = "/sys/bus/pci/devices/mdev_supported_types/i915-GVTg_V5_8/";

constexpr const char *kSys2MFreeHugePages = "/sys/kernel/mm/hugepages/hugepages-2048kB/free_hugepages";
constexpr const char *kSys2MNrHugePages = "/sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages";

static bool CheckUuid(std::string uuid) {
    try {
        boost::uuids::string_generator gen;
        boost::uuids::uuid u = gen(uuid);
        if (u.size() != 16)
            return false;
        else
            return true;
    } catch (std::exception &e) {
        LOG(error) << "Invalid uuid: " << uuid;
        return false;
    }
}

void VmBuilderQemu::SoundCardHook(void) {
    /* FIXME: Remove the remove snd-tgl-snd module before
        graphics passthrough and inserts the module snd-tgl-snd at the
        end before exiting. When we unbind the i915 driver,
        the audio driver must be unloaded and reloaded when the i915
        driver is rebound. Since this is not working as expcted when
        we give launch android with graphics passthrough, kernel crahses
        which reboots the host by itself and android launch fails. This
        is known issue from audio/1915 driver side.
        This issue is not seen with HDA sound card. Issue is seen only
        when sof-hda sound card is enabled on the host. So in that case,
        we are removing snd-sof-pci-intel-tgl module before graphics
        passthrough when sof-hda is enabled on host and reinsert
        snd-sof-pci-intel-tgl module before exit from guest.
    */
    if (!boost::process::system("cat /proc/asound/cards | grep sof")) {
        LOG(info) << "Removing snd-sof-pci-intel-tgl ...";
        if (!boost::process::system("modprobe -r snd-sof-pci-intel-tgl")) {
            end_call_.emplace([](){
                LOG(info) << "Probing snd-sof-pci-intel-tgl ...";
                boost::process::system("modprobe snd-sof-pci-intel-tgl");
            });
        }
    }
}

static bool IsVfioDriver(const char *path) {
    try {
        boost::filesystem::path p(path);
        if (boost::filesystem::is_symlink(p)) {
            boost::filesystem::path s(boost::filesystem::read_symlink(p));
            return (s.filename().compare("vfio-pci") == 0);
        }
        return false;
    } catch (std::exception &e) {
        return false;
    }
}

static int ReadSysFile(const char *sys_file, std::ios_base::fmtflags base) {
    try {
        std::ifstream ifs(sys_file, std::ios_base::in);
        ifs.setf(base, std::ios_base::basefield);
        int ret;
        ifs >> ret;
        return ret;
    } catch (std::exception &e) {
        LOG(error) << e.what();
        return -1;
    }
}

static int WriteSysFile(const char *sys_file, const std::string &str) {
    try {
        std::ofstream of(sys_file, std::ios_base::out);
        of << str;
        return 0;
    } catch (std::exception &e) {
        LOG(error) << e.what();
        return errno;
    }
}

static bool SetupHugePages(const std::string &mem_size) {
    if (mem_size.empty())
        return false;

    int mem_mb = 0;
    try {
        mem_mb = std::stoi(mem_size, nullptr, 0);
    } catch (std::exception &e) {
        LOG(error) << e.what();
        return false;
    }

    if (mem_mb <= 0)
        return false;

    switch (toupper(mem_size.back())) {
    case '0' ... '9':
    case 'A' ... 'F':
    case 'M':
        break;
    case 'G':
        if (mem_mb > INT32_MAX/1024)
            return false;
        else
            mem_mb *= 1024;
        break;
    default:
        return false;
    }

    /* Get free Huge pages */
    int free_hp = ReadSysFile(kSys2MFreeHugePages, std::ios_base::dec);

    if (free_hp >= mem_mb/2)
        return true;

    /* Get nr hugepages */
    int nr_hp = ReadSysFile(kSys2MNrHugePages, std::ios_base::dec);
    int required_hp = nr_hp - free_hp + mem_mb/2;

    /* Try to allocate required huge pages */
    WriteSysFile(kSys2MNrHugePages, std::to_string(required_hp));

    /* check nr huge pages */
    while (nr_hp != required_hp) {
        int wait_cnt = 0;
        nr_hp = ReadSysFile(kSys2MNrHugePages, std::ios_base::dec);
        if (wait_cnt < 200) {
            usleep(10000);
            wait_cnt++;
        } else {
            LOG(error) << "Hugepage cannot achieve required size!\n";
            return false;
        }
    }

    return true;
}

static int SetAvailableVf(void) {
    int totalvfs = ReadSysFile(kIntelGpuSriovTotalVfs, std::ios_base::dec);
    if (totalvfs <= 0)
        return -1;

    /* Limit total VFs to conserve memory */
    if (totalvfs > 4) {
        totalvfs = 4;
        /* Re-Probe SRIOV with limited VFs */
        WriteSysFile(kIntelGpuSriovAutoProbe, "0");
        WriteSysFile(kDrmCard0DevSriovNumVfs, std::to_string(totalvfs));
        WriteSysFile(kIntelGpuSriovAutoProbe, "1");
    }

    int dev_id = ReadSysFile(kIntelGpuDevice, std::ios_base::hex);
    if (dev_id < 0)
        return -1;

    std::string id("8086 " + (boost::format("%x") % dev_id).str());

    WriteSysFile(kVfioPciNewId, id);

    if (IsVfioDriver(kIntelGpuDriver)) {
        WriteSysFile(kVfioPciRemoveId, id);
    }
    WriteSysFile(kIntelGpuDriverUnbind, kIntelGpuBdf);

    int errno_saved = WriteSysFile(kVfioPciNewId, id);
    if (errno_saved == EEXIST) {
        WriteSysFile(kVfioPciRemoveId, id);
        WriteSysFile(kVfioPciNewId, id);
    } else if (errno_saved != 0) {
        return false;
    }

    std::string sriov_dev(kIntelGpuDevPath);
    for (int i = 0; i < totalvfs; i++) {
        sriov_dev.append("." + std::to_string(i) + "/enable");
        int status = ReadSysFile(sriov_dev.c_str(), std::ios_base::dec);
        if (status == 0)
            return i;
    }

    return -1;
}

bool VmBuilderQemu::SetupSriov(void) {
    std::string vgpu_mon_id = cfg_.GetValue(kGroupVgpu, kVgpuMonId);
    if (vgpu_mon_id.empty()) {
        emul_cmd_.append(" -display gtk,gl=on");
    } else {
        emul_cmd_.append(" -display gtk,gl=on,monitor=" + vgpu_mon_id);
    }

    std::string mem_size = cfg_.GetValue(kGroupMem, kMemSize);
    boost::trim(mem_size);

    if (!SetupHugePages(mem_size))
        return false;
    int vf = SetAvailableVf();
    if (vf < 0)
        return false;

    emul_cmd_.append(" -device virtio-vga,max_outputs=1,blob=true"
                    " -device vfio-pci,host=0000:00:02." + std::to_string(vf) +
                    " -object memory-backend-memfd,hugetlb=on,id=mem_sriov,size=" + mem_size +
                    " -machine memory-backend=mem_sriov");

    return true;
}

enum PciPassthroughAction {
    kPciPassthrough,
    kPciRestore
};

static bool PassthroughOnePciDev(const char *pci_id, PciPassthroughAction action) {
    if (!pci_id)
        return false;

    boost::filesystem::path p(kPciDevicePath);
    p.append(pci_id).append("/iommu_group/device");

    if (!boost::filesystem::exists(p) || boost::filesystem::is_directory(p)) {
        return false;
    }

    for (boost::filesystem::directory_entry& x : boost::filesystem::directory_iterator(p)) {
        std::cout << "  " << x.path() << "\n";
        std::string str_dev(x.path().string() + "/device");
        int device = ReadSysFile(str_dev.c_str(), std::ios_base::hex);
        std::string str_ven(x.path().string() + "/vendor");
        int vendor = ReadSysFile(str_ven.c_str(), std::ios_base::hex);
        std::string ven_dev(std::to_string(vendor) + " " + std::to_string(device));

        boost::filesystem::path driver(x.path().string() + "/driver");

        if (action == kPciRestore) {
            if (IsVfioDriver(driver.c_str())) {
                WriteSysFile(kVfioPciRemoveId, ven_dev);
                WriteSysFile(kVfioPciUnbind, x.path().filename().string());
            }
            /* TODO: check whether unbind successfully instead of sleep */
            sleep(1);

            if (WriteSysFile(kPciDriverProbe, x.path().filename().string()))
                return false;
            continue;
        }

        if (boost::filesystem::exists(driver)) {
            if (IsVfioDriver(driver.c_str())) {
                WriteSysFile(kVfioPciRemoveId, ven_dev);
            }
            std::string drv_unbind = driver.string() + "/unbind";
            WriteSysFile(kVfioPciUnbind, x.path().filename().string());

            /* TODO: check whether unbind successfully instead of sleep */
            sleep(1);

            int errno_saved = WriteSysFile(kVfioPciNewId, ven_dev);
            if (errno_saved == EEXIST) {
                WriteSysFile(kVfioPciRemoveId, ven_dev);
                WriteSysFile(kVfioPciNewId, ven_dev);
            } else if (errno_saved != 0) {
                return false;
            }
        }
    }

    return true;
}

void VmBuilderQemu::PassthroughPciDevices(void) {
    std::string pt_pci = cfg_.GetValue(kGroupPciPt, kPciPtDev);
    boost::trim(pt_pci);
    if (pt_pci.empty())
        return;

    std::vector<std::string> vec;
    boost::split(vec, pt_pci, boost::is_any_of(","), boost::token_compress_on);

    if (boost::process::system("modprobe vfio"))
        return;

    if (boost::process::system("modprobe vfio-pci"))
        return;

    for (auto it=vec.begin(); it != vec.end(); ++it) {
        boost::trim(*it);
        if (PassthroughOnePciDev(it->c_str(), kPciPassthrough)) {
            pci_pt_dev_set_.insert(std::move(*it)); //Need to check whether need to shift it after move it
            emul_cmd_.append(" -device vfio-pci,host=" + *it + ",x-no-kvm-intx=on");
        }
    }

    if (pci_pt_dev_set_.empty())
        return;

    end_call_.emplace([this](){
        LOG(info) << "Restore passthroughed PCI devices ...";
        for (auto it=pci_pt_dev_set_.begin(); it != pci_pt_dev_set_.end(); ++it) {
            PassthroughOnePciDev(it->c_str(), kPciRestore);
        }
    });
}

bool VmBuilderQemu::PassthroughGpu(void) {
    if (PassthroughOnePciDev(kIntelGpuBdf, kPciPassthrough)) {
        pci_pt_dev_set_.insert(kIntelGpuBdf);
        return true;
    }
    return false;
}

bool VmBuilderQemu::BuildVmArgs(void) {
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
    co_procs_.emplace_back(std::make_unique<VmCoProcRpmb>(std::move(rpmb_bin), std::move(rpmb_data), env_data_));

    //co_procs_.emplace_back(std::make_unique<VmCoProcSimple>(" qemu-system-x86_64 -device virtio-gpu", env_data_));
    //co_procs_.emplace_back(std::make_unique<VmCoProcSimple>("  qemu-system-x86_64 -device virtio-gpu", env_data_));


    std::string vtpm_bin = cfg_.GetValue(kGroupVtpm, kVtpmBinPath);
    std::string vtpm_data = cfg_.GetValue(kGroupVtpm, kVtpmDataDir);
    if (!vtpm_bin.empty() && !vtpm_data.empty()) {
        emul_cmd_.append(" -chardev socket,id=chrtpm,path=" +
                         vtpm_data + "/" + kVtpmSock +
                         " -tpmdev emulator,id=tpm0,chardev=chrtpm -device tpm-crb,tpmdev=tpm0");
    }
    co_procs_.emplace_back(std::make_unique<VmCoProcVtpm>(std::move(vtpm_bin), std::move(vtpm_data), env_data_));

    std::unique_ptr<Aaf> aaf_cfg;
    std::string aaf_path = cfg_.GetValue(kGroupAaf, kAafPath);
    if (!aaf_path.empty()) {
        emul_cmd_.append(" -virtfs local,mount_tag=Download9p,security_model=none,addr=3,path=" + aaf_path);
        aaf_cfg = std::make_unique<Aaf>(aaf_path.c_str());

        std::string aaf_suspend = cfg_.GetValue(kGroupAaf, kAafSuspend);
        if (!aaf_suspend.empty()) {
            aaf_cfg->Set(kAafKeySuspend, aaf_suspend);
        }

        std::string aaf_audio_type = cfg_.GetValue(kGroupAaf, kAafAudioType);
        if (!aaf_audio_type.empty()) {
            aaf_cfg->Set(kAafKeyAudioType, aaf_audio_type);
        }
    }

    std::string vgpu_type = cfg_.GetValue(kGroupVgpu, kVgpuType);
    if (!vgpu_type.empty()) {
        if (vgpu_type.compare(kVgpuGvtG)) {
            std::string vgpu_uuid = cfg_.GetValue(kGroupVgpu, kVgpuUuid);
            if (vgpu_uuid.empty()) {
                LOG(error) << "Empty VGPU UUID!";
                return false;
            }
            if (!CheckUuid(vgpu_uuid)) {
                return false;
            }
            emul_cmd_.append(" -display gtk,gl=on");
            emul_cmd_.append(" -device vfio-pci-nohotplug,ramfb=on,display=on,addr=2.0,x-igd-opregion=on,sysfsdev=" +
                             std::string(kIntelGpuDevPath) + vgpu_uuid);
            aaf_cfg->Set(kAafKeyGpuType, "gvtg");
        } else if (vgpu_type.compare(kVgpuGvtD)) {
            SoundCardHook();
            if (!PassthroughGpu()) {
                return false;
            }
            emul_cmd_.append(" -vga none -nographic"
                " -device vfio-pci,host=00:02.0,x-igd-gms=2,id=hostdev0,bus=pcie.0,addr=0x2,x-igd-opregion=on");
            aaf_cfg->Set(kAafKeyGpuType, "gvtd");
        } else if (vgpu_type.compare(kVgpuVirtio)) {
            emul_cmd_.append(" -display gtk,gl=on -device virtio-vga-gl");
            aaf_cfg->Set(kAafKeyGpuType, "virtio");
        } else if (vgpu_type.compare(kVgpuRamfb)) {
            emul_cmd_.append(" -display gtk,gl=on -device ramfb");
        } else if (vgpu_type.compare(kVgpuVirtio2D)) {
            emul_cmd_.append(" -display gtk,gl=on -device virtio-vga");
            aaf_cfg->Set(kAafKeyGpuType, "virtio");
        } else if (vgpu_type.compare(kVgpuSriov)) {
            if (!SetupSriov())
                return false;
            aaf_cfg->Set(kAafKeyGpuType, "virtio");
        } else {
            LOG(warning) << "Invalid Graphics config";
            return false;
        }
    }

    emul_cmd_.append(" -m " + cfg_.GetValue(kGroupMem, kMemSize));

    emul_cmd_.append(" -smp " + cfg_.GetValue(kGroupVcpu, kVcpuNum));

    std::string firm_type = cfg_.GetValue(kGroupFirm, kFirmType);
    if (firm_type.empty())
        return false;
    if (firm_type.compare(kFirmUnified)) {
        emul_cmd_.append(" -drive if=pflash,format=raw,file=" + cfg_.GetValue(kGroupFirm, kFirmUnified));
    } else if (firm_type.compare(kFirmSplited)) {
        emul_cmd_.append(" -drive if=pflash,format=raw,readonly,file=" + cfg_.GetValue(kGroupFirm, kFirmCode));
        emul_cmd_.append(" -drive if=pflash,format=raw,file=" + cfg_.GetValue(kGroupFirm, kFirmVars));
    } else {
        LOG(error) << "Invalid virtual firmware";
    }

    emul_cmd_.append(" -drive file="+ cfg_.GetValue(kGroupDisk, kDiskPath) +
        ",if=none,id=disk1,discard=unmap,detect-zeroes=unmap"
        "-device virtio-blk-pci,drive=disk1,bootindex=1");

    PassthroughPciDevices();

    aaf_cfg->Flush();

    return true;
}

void VmBuilderQemu::StartVm() {
    LOG(info) << "Emulator command:" << emul_cmd_;

    for (size_t i = 0; i < co_procs_.size(); ++i) {
        co_procs_[i]->Run();
    }
}

void VmBuilderQemu::StopVm() {
    for (size_t i = 0; i < co_procs_.size(); ++i) {
        co_procs_[i]->Stop();
    }
    while (!end_call_.empty()) {
        end_call_.front()();
        end_call_.pop();
    }
}

VmBuilderQemu::~VmBuilderQemu() {
    StopVm();
}

}  //  namespace vm_manager
