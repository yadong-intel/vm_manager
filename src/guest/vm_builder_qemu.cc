
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
        std::ifstream ifs(sys_file, std::ofstream::in);
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
        std::ofstream of(sys_file, std::ofstream::out);
        of << str;
        of.flush();
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

    if (boost::process::system("modprobe vfio"))
        return false;

    if (boost::process::system("modprobe vfio-pci"))
        return false;

    boost::filesystem::path p(kPciDevicePath);
    p.append(pci_id).append("/iommu_group/devices");

    if (!boost::filesystem::exists(p) || !boost::filesystem::is_directory(p)) {
        return false;
    }

    for (boost::filesystem::directory_entry& x : boost::filesystem::directory_iterator(p)) {
        std::cout << "  " << x.path() << "\n";
        std::string str_dev(x.path().string() + "/device");
        int device = ReadSysFile(str_dev.c_str(), std::ios_base::hex);
        std::string str_ven(x.path().string() + "/vendor");
        int vendor = ReadSysFile(str_ven.c_str(), std::ios_base::hex);
        std::string ven_dev((boost::format("%x") % vendor).str() + " " + (boost::format("%x") % device).str());

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
            LOG(info) << "Unbind PCI driver - " << x.path().filename().string();
            WriteSysFile(drv_unbind.c_str(), x.path().filename().string());

            /* wait driver to be unbinded */
            constexpr const int kCheckUnbindRepeatCount = 2000;
            int cnt = 0;
            while (cnt < kCheckUnbindRepeatCount) {
                if (!boost::filesystem::exists(driver))
                    break;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                cnt++;
            }
            if (cnt >= kCheckUnbindRepeatCount) {
                LOG(error) << "Failed to unbind - " << x.path().filename().string();
                return false;
            }
        }

        int errno_saved = WriteSysFile(kVfioPciNewId, ven_dev);
        if (errno_saved == EEXIST) {
            WriteSysFile(kVfioPciRemoveId, ven_dev);
            WriteSysFile(kVfioPciNewId, ven_dev);
        } else if (errno_saved != 0) {
            return false;
        }
    }

    return true;
}

void VmBuilderQemu::BuildPtPciDevicesCmd(void) {
    std::string pt_pci = cfg_.GetValue(kGroupPciPt, kPciPtDev);
    boost::trim(pt_pci);
    if (pt_pci.empty())
        return;

    std::vector<std::string> vec;
    boost::split(vec, pt_pci, boost::is_any_of(","), boost::token_compress_on);

    for (auto it=vec.begin(); it != vec.end(); ++it) {
        boost::trim(*it);
        if (PassthroughOnePciDev(it->c_str(), kPciPassthrough)) {
            pci_pt_dev_set_.insert(*it);
            emul_cmd_.append(" -device vfio-pci,host=" + *it + ",x-no-kvm-intx=on");
        } else {
            LOG(warning) << "Failed to passthrough: " << *it;
        }
    }
}

void VmBuilderQemu::SetPciDevicesCallback(void) {
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

void VmBuilderQemu::RunMediationSrv(void) {
    std::string batt_med = cfg_.GetValue(kGroupMed, kMedBattery);
    if (batt_med.empty())
        return;
    co_procs_.emplace_back(std::make_unique<VmCoProcSimple>(batt_med));

    std::string ther_med = cfg_.GetValue(kGroupMed, kMedThermal);
    if (ther_med.empty())
        return;
    co_procs_.emplace_back(std::make_unique<VmCoProcSimple>(ther_med));
}

void VmBuilderQemu::BuildGuestTimeKeepCmd(void) {
    std::string tk = cfg_.GetValue(kGroupService, kServTimeKeep);
    if (tk.empty())
        return;
    co_procs_.emplace_back(std::make_unique<VmCoProcSimple>(tk));
    emul_cmd_.append(" -qmp pipe:/tmp/qmp-time-keep-pipe");
}

void VmBuilderQemu::BuildGuestPmCtrlCmd(void) {
    std::string pm = cfg_.GetValue(kGroupService, kServPmCtrl);
    if (pm.empty())
        return;
    co_procs_.emplace_back(std::make_unique<VmCoProcSimple>(pm));
    emul_cmd_.append(" -qmp unix:/tmp/qmp-pm-sock,server=on,wait=off -no-reboot");
}

static int GetUid(void) {
    char *suid = NULL;
    int real_uid;
    suid = getenv("SUDO_UID");
    if (suid) {
        real_uid = atoi(suid);
    } else {
        real_uid = getuid();
    }
    return real_uid;
}

void VmBuilderQemu::BuildAudioCmd(void) {
    int uid = GetUid();
    emul_cmd_.append(" -device intel-hda"
                     " -device hda-duplex,audiodev=android_spk"
                     " -audiodev id=android_spk,timer-period=5000,driver=pa,"
                     "in.fixed-settings=off,out.fixed-settings=off,server=/var/run/pulse/native");
}

void VmBuilderQemu::BuildExtraCmd(void) {
    std::string ex_cmd = cfg_.GetValue(kGroupExtra, kExtraCmd);
    if (ex_cmd.empty())
        return;
    emul_cmd_.append(" " + ex_cmd);
}

void VmBuilderQemu::SetExtraServices(void) {
    std::string ex_srvs = cfg_.GetValue(kGroupExtra, kExtraService);
    boost::trim(ex_srvs);
    if (ex_srvs.empty())
        return;
    std::vector<std::string> vec;
    boost::split(vec, ex_srvs, boost::is_any_of(";"), boost::token_compress_on);

    for (auto it=vec.begin(); it != vec.end(); ++it) {
        boost::trim(*it);
        if (it->empty())
            continue;

        co_procs_.emplace_back(std::make_unique<VmCoProcSimple>(*it));
    }
}

bool VmBuilderQemu::BuildEmulPath(void) {
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
    return true;
}

void VmBuilderQemu::BuildFixedCmd(void) {
    emul_cmd_.append(
        " -M q35"
        " -machine kernel_irqchip=on"
        " -k en-us"
        " -cpu host,-waitpkg"
        " -enable-kvm"
        " -device qemu-xhci,id=xhci,p2=8,p3=8"
        " -device usb-mouse"
        " -device usb-kbd");
    emul_cmd_.append(
        /* Make sure this device be the last argument */
        " -device intel-iommu,device-iotlb=on,caching-mode=on"
        " -nodefaults ");
}

bool VmBuilderQemu::BuildNameQmp(void) {
    std::string vm_name = cfg_.GetValue(kGroupGlob, kGlobName);
    boost::trim(vm_name);
    if (vm_name.empty()) {
        emul_cmd_.clear();
        return false;
    }
    emul_cmd_.append(" -name " + vm_name);
    emul_cmd_.append(" -qmp unix:" + std::string(GetConfigPath()) + "/." + vm_name + ".qmp.unix.socket,server,nowait");
    return true;
}

void VmBuilderQemu::BuildNetCmd(void) {
    std::string model = cfg_.GetValue(kGroupNet, kNetModel);
    if (model.empty())
        return;

    std::string net_arg = " -netdev user,id=net0";
    std::string adb_port = cfg_.GetValue(kGroupNet, kNetAdbPort);
    if (!adb_port.empty())
        net_arg.append(",hostfwd=tcp::" + adb_port + "-:5555");
    std::string fb_port = cfg_.GetValue(kGroupNet, kNetFastbootPort);
    if (!fb_port.empty())
        net_arg.append(",hostfwd=tcp::" + fb_port + "-:5554");

    emul_cmd_.append(net_arg);
    emul_cmd_.append(" -device e1000,netdev=net0,bus=pcie.0,addr=0xA");
}

bool VmBuilderQemu::BuildVsockCmd(void) {
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
    emul_cmd_.append(" -device vhost-vsock-pci,id=vhost-vsock-pci0,bus=pcie.0,addr=0x10,guest-cid=" +
                    std::to_string(vsock_cid_));
    return true;
}

void VmBuilderQemu::BuildRpmbCmd(void) {
    std::string rpmb_bin = cfg_.GetValue(kGroupRpmb, kRpmbBinPath);
    std::string rpmb_data = cfg_.GetValue(kGroupRpmb, kRpmbDataDir);
    if (!rpmb_bin.empty() && !rpmb_data.empty()) {
        emul_cmd_.append(" -device virtio-serial,addr=1"
                         " -device virtserialport,chardev=rpmb0,name=rpmb0,nr=1"
                         " -chardev socket,id=rpmb0,path=" +
                           rpmb_data + "/" + std::string(kRpmbSock));
    }
    co_procs_.emplace_back(std::make_unique<VmCoProcRpmb>(std::move(rpmb_bin), std::move(rpmb_data)));
}

void VmBuilderQemu::BuildVtpmCmd(void) {
    std::string vtpm_bin = cfg_.GetValue(kGroupVtpm, kVtpmBinPath);
    std::string vtpm_data = cfg_.GetValue(kGroupVtpm, kVtpmDataDir);
    if (!vtpm_bin.empty() && !vtpm_data.empty()) {
        emul_cmd_.append(" -chardev socket,id=chrtpm,path=" +
                         vtpm_data + "/" + kVtpmSock +
                         " -tpmdev emulator,id=tpm0,chardev=chrtpm -device tpm-crb,tpmdev=tpm0");
    }
    co_procs_.emplace_back(std::make_unique<VmCoProcVtpm>(std::move(vtpm_bin), std::move(vtpm_data)));
}

void VmBuilderQemu::BuildAafCfg(void) {
    std::string aaf_path = cfg_.GetValue(kGroupAaf, kAafPath);
    if (!aaf_path.empty()) {
        emul_cmd_.append(" -virtfs local,mount_tag=aaf,security_model=none,path=" + aaf_path);
        aaf_cfg_ = std::make_unique<Aaf>(aaf_path.c_str());

        std::string aaf_suspend = cfg_.GetValue(kGroupAaf, kAafSuspend);
        if (!aaf_suspend.empty()) {
            aaf_cfg_->Set(kAafKeySuspend, aaf_suspend);
        }

        std::string aaf_audio_type = cfg_.GetValue(kGroupAaf, kAafAudioType);
        if (!aaf_audio_type.empty()) {
            aaf_cfg_->Set(kAafKeyAudioType, aaf_audio_type);
        }
    }
}

bool VmBuilderQemu::BuildVgpuCmd(void) {
    std::string vgpu_type = cfg_.GetValue(kGroupVgpu, kVgpuType);
    if (!vgpu_type.empty()) {
        if (vgpu_type.compare(kVgpuGvtG) == 0) {
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
            if (aaf_cfg_)
                aaf_cfg_->Set(kAafKeyGpuType, "gvtg");
        } else if (vgpu_type.compare(kVgpuGvtD) == 0) {
            SoundCardHook();
            if (!PassthroughGpu()) {
                return false;
            }
            emul_cmd_.append(" -vga none -nographic"
                " -device vfio-pci,host=00:02.0,x-igd-gms=2,id=hostdev0,bus=pcie.0,addr=0x2,x-igd-opregion=on");
            if (aaf_cfg_)
                aaf_cfg_->Set(kAafKeyGpuType, "gvtd");
        } else if (vgpu_type.compare(kVgpuVirtio) == 0) {
            emul_cmd_.append(" -display gtk,gl=on -device virtio-vga-gl");
            if (aaf_cfg_)
                aaf_cfg_->Set(kAafKeyGpuType, "virtio");
        } else if (vgpu_type.compare(kVgpuRamfb) == 0) {
            emul_cmd_.append(" -display gtk,gl=on -device ramfb");
        } else if (vgpu_type.compare(kVgpuVirtio2D)) {
            emul_cmd_.append(" -display gtk,gl=on -device virtio-vga");
            if (aaf_cfg_)
                aaf_cfg_->Set(kAafKeyGpuType, "virtio");
        } else if (vgpu_type.compare(kVgpuSriov) == 0) {
            if (!SetupSriov())
                return false;
            if (aaf_cfg_)
                aaf_cfg_->Set(kAafKeyGpuType, "virtio");
        } else {
            LOG(warning) << "Invalid Graphics config";
            return false;
        }
    }
    return true;
}

void VmBuilderQemu::BuildMemCmd(void) {
    emul_cmd_.append(" -m " + cfg_.GetValue(kGroupMem, kMemSize));
}

void VmBuilderQemu::BuildVcpuCmd(void) {
    emul_cmd_.append(" -smp " + cfg_.GetValue(kGroupVcpu, kVcpuNum));
}

bool VmBuilderQemu::BuildFirmwareCmd(void) {
    std::string firm_type = cfg_.GetValue(kGroupFirm, kFirmType);
    if (firm_type.empty())
        return false;
    if (firm_type.compare(kFirmUnified) == 0) {
        emul_cmd_.append(" -drive if=pflash,format=raw,file=" + cfg_.GetValue(kGroupFirm, kFirmPath));
    } else if (firm_type.compare(kFirmSplited) == 0) {
        emul_cmd_.append(" -drive if=pflash,format=raw,readonly,file=" + cfg_.GetValue(kGroupFirm, kFirmCode));
        emul_cmd_.append(" -drive if=pflash,format=raw,file=" + cfg_.GetValue(kGroupFirm, kFirmVars));
    } else {
        LOG(error) << "Invalid virtual firmware";
        return false;
    }
    return true;
}

void VmBuilderQemu::BuildVdiskCmd(void) {
    emul_cmd_.append(
        " -drive file="+ cfg_.GetValue(kGroupDisk, kDiskPath) + ",if=none,id=disk1,discard=unmap,detect-zeroes=unmap"
        " -device virtio-blk-pci,drive=disk1,bootindex=1");
}

bool VmBuilderQemu::BuildVmArgs(void) {
    LOG(info) << "build qemu vm args";

    if (!BuildEmulPath())
        return false;

    BuildRpmbCmd();

    BuildAafCfg();

    if (!BuildNameQmp())
        return false;

    BuildNetCmd();

    if (!BuildVsockCmd())
        return false;

    BuildVtpmCmd();

    if (!BuildVgpuCmd())
        return false;

    BuildMemCmd();

    BuildVcpuCmd();

    if (!BuildFirmwareCmd())
        return false;

    BuildVdiskCmd();

    BuildPtPciDevicesCmd();
    SetPciDevicesCallback();

    RunMediationSrv();

    BuildGuestTimeKeepCmd();

    BuildGuestPmCtrlCmd();

    BuildAudioCmd();

    BuildExtraCmd();

    BuildFixedCmd();

    SetExtraServices();

    if (aaf_cfg_)
        aaf_cfg_->Flush();

    main_proc_ = std::make_unique<VmCoProcSimple>(emul_cmd_);

    state_ = VmBuilder::VmState::kVmCreated;

    return true;
}

void VmBuilderQemu::SetProcessEnv(std::vector<std::string> env) {
    main_proc_->SetEnv(env);
}

void VmBuilderQemu::StartVm() {
    LOG(info) << "Emulator command:" << emul_cmd_;

    for (size_t i = 0; i < co_procs_.size(); ++i) {
        if (!co_procs_[i]->Running())
            co_procs_[i]->Run();
    }

    if (main_proc_) {
        main_proc_->Run();
        LOG(info) << "Main Proc is started";
        state_ = VmBuilder::VmState::kVmBooting;
    }
}

bool VmBuilderQemu::WaitVmReady(void) {
    int wait_cnt = 0;
    while (wait_cnt++ < 200) {
        if (!main_proc_->Running())
            return false;
        vm_ready_latch_.wait_for(boost::chrono::seconds(1));

        if (state_ == VmBuilder::VmState::kVmRunning)
            return true;
    }

    return false;
}

void VmBuilderQemu::SetVmReady(void) {
    vm_ready_latch_.try_count_down();

    state_ = VmBuilder::VmState::kVmRunning;
}

void VmBuilderQemu::PauseVm(void) {

}

void VmBuilderQemu::WaitVmExit() {
    if (main_proc_) {
        main_proc_->Join();
        //StopVm();
    }
}

void VmBuilderQemu::StopVm() {
    if (main_proc_)
        main_proc_->Stop();

    for (size_t i = 0; i < co_procs_.size(); ++i) {
        co_procs_[i]->Stop();
    }

    VsockCidPool::Pool().ReleaseCid(vsock_cid_);

    while (!end_call_.empty()) {
        end_call_.front()();
        end_call_.pop();
    }
}

VmBuilderQemu::~VmBuilderQemu() {
    StopVm();
}

}  //  namespace vm_manager
