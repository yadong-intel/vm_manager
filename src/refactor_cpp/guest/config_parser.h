/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef __CONFIG_PARSER_H__
#define __CONFIG_PARSER_H__

#include <string>

#include <boost/property_tree/ptree.hpp>

namespace vm_manager {

using std::string;

/* Groups */
constexpr char kGroupGlob[]    = "global";
constexpr char kGroupEmul[]    = "emulator";
constexpr char kGroupMem[]     = "memory";
constexpr char kGroupVcpu[]    = "vcpu";
constexpr char kGroupFirm[]    = "firmware";
constexpr char kGroupDisk[]    = "disk";
constexpr char kGroupVgpu[]    = "graphics";
constexpr char kGroupVtpm[]    = "vtpm";
constexpr char kGroupRpmb[]    = "rpmb";
constexpr char kGroupAaf[]     = "aaf";
constexpr char kGroupPciPt[]   = "passthrough";
constexpr char kGroupMed[]     = "mediation";
constexpr char kGroupService[] = "guest_control";
constexpr char kGroupExtra[]   = "extra";

/* Keys */
constexpr char kGlobName[]       = "name";
constexpr char kGlobFlashfiles[] = "flashfiles";
constexpr char kGlobAdbPort[]        = "adb_port";
constexpr char kGlobFastbootPort[]   = "fastboot_port";
constexpr char kGlobCid[]        = "vsock_cid";

constexpr char kEmulType[] = "type";
constexpr char kEmulPath[] = "path";

constexpr char kMemSize[] = "size";

constexpr char kVcpuNum[] = "num";

constexpr char kFirmType[] = "type";
constexpr char kFirmPath[] = "path";
constexpr char kFirmCode[] = "code";
constexpr char kFirmVars[] = "vars";

constexpr char kDiskSize[] = "size";
constexpr char kDiskPath[] = "path";

constexpr char kVgpuType[]    = "type";
constexpr char kVgpuGvtgVer[] = "gvtg_version";
constexpr char kVgpuUuid[]    = "vgpu_uuid";

constexpr char kVtpmBinPath[] = "bin_path";
constexpr char kVtpmDataDir[] = "data_dir";

constexpr char kRpmbBinPath[] = "bin_path";
constexpr char kRpmbDataDir[] = "data_dir";

constexpr char kAafPath[] =    "path";
constexpr char kAafSuspend[] = "support_suspend";

constexpr char kPciDev[] = "passthrough_pci";

constexpr char kMedBattery[] = "battery_med";
constexpr char kMedThermal[] = "thermal_med";

constexpr char kServTimeKeep[] = "time_keep";
constexpr char kServPmCtrl[]   = "pm_control";

constexpr char kExtraCmd[]     = "cmd";
constexpr char kExtraService[] = "service";

/* Options for Key to select */
constexpr char kEmulTypeQemu[] = "QEMU";

constexpr char kFirmUnified[] = "unified";
constexpr char kFirmSplited[] = "splited";

constexpr char kVgpuNone[]   = "headless";
constexpr char kVgpuVirtio[] = "virtio";
constexpr char kVgpuRamfb[]  = "ramfb";
constexpr char kVgpuGvtG[]   = "GVT-g";
constexpr char kVgpuGvtD[]   = "GVT-d";

constexpr char kGvtgV51[] = "i915-GVTg_V5_1";
constexpr char kGvtgV52[] = "i915-GVTg_V5_2";
constexpr char kGvtgV54[] = "i915-GVTg_V5_4";
constexpr char kGvtgV58[] = "i915-GVTg_V5_8";

constexpr char kSuspendEnable[]  = "enable";
constexpr char kSuspendDisable[] = "disable";


class CivConfig final {
 public:
  string GetValue(const string group, const string key);
  int SetValue(const string group, const string key, const string value);
  bool ReadConfigFile(const string path);
  bool WriteConfigFile(string path);
 private:
  bool SanitizeOpts(void);
  boost::property_tree::ptree cfg_data_;
};
}  // namespace vm_manager

#endif /* __CONFIG_PARSER_H__ */
