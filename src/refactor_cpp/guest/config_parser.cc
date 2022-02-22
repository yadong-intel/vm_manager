/*
 * Copyright (c) 2021 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <assert.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <map>
#include <vector>
#include <string_view>
#include <filesystem>

#include <boost/property_tree/ini_parser.hpp>

#include "config_parser.h"
#include "log.h"

namespace vm_manager {

using std::map;
using std::vector;
using std::string_view;
using boost::property_tree::ptree;

map<string_view, vector<string_view>> kConfigMap = {
    { kGroupGlob,    { kGlobName, kGlobFlashfiles, kGlobAdbPort, kGlobFastbootPort, kGlobCid } },
    { kGroupEmul,    { kEmulType, kEmulPath } },
    { kGroupMem,     { kMemSize } },
    { kGroupVcpu,    { kVcpuNum } },
    { kGroupFirm,    { kFirmType, kFirmPath, kFirmCode, kFirmVars } },
    { kGroupDisk,    { kDiskSize, kDiskPath } },
    { kGroupVgpu,    { kVgpuType, kVgpuGvtgVer, kVgpuUuid } },
    { kGroupVtpm,    { kVtpmBinPath, kVtpmDataDir } },
    { kGroupRpmb,    { kRpmbBinPath, kRpmbDataDir } },
    { kGroupAaf,     { kAafPath, kAafSuspend }},
    { kGroupPciPt,   { kPciDev }},
    { kGroupMed,     { kMedBattery, kMedThermal } },
    { kGroupService, { kServTimeKeep, kServPmCtrl } },
    { kGroupExtra,   { kExtraCmd, kExtraService } }
};

bool CivConfig::SanitizeOpts(void) {
    for (auto& section : cfg_data_) {
        auto group = kConfigMap.find(section.first);
        if (group != kConfigMap.end()) {
            for (auto& subsec : section.second) {
                auto key = std::find(group->second.begin(), group->second.end(), subsec.first);
                if (key != group->second.end()) {
                    continue;
                } else {
                    LOG(error) << "Invalid key: " << section.first << "." << subsec.first << "\n";
                    return false;
                }
            }
        } else {
            LOG(error) << "Invalid group: " << section.first << "\n";
            return false;
        }
    }
    return true;
}

bool CivConfig::ReadConfigFile(string path) {
    if (!std::filesystem::exists(path)){
        LOG(error) << "File not exists: " << path << std::endl;
        return false;
    }

    if (!std::filesystem::is_regular_file(path)) {
        LOG(error) << "File is not a regular file: " << path << std::endl;
        return false;
    }

    cfg_data_.clear();
    read_ini(path, cfg_data_);

    if (!SanitizeOpts()) {
        cfg_data_.clear();
        return false;
    }
    return true;
}

bool CivConfig::WriteConfigFile(string path) {
    if (!std::filesystem::exists(path)){
        LOG(error) << "File not exists: " << path << std::endl;
        return false;
    }

    if (!std::filesystem::is_regular_file(path)) {
        LOG(error) << "File is not a regular file: " << path << std::endl;
        return false;
    }

    if (cfg_data_.empty()) {
        LOG(warning) << "No data to write, skip!" << std::endl;
        return true;
    }

    if (!SanitizeOpts()) {
        return false;
    }

    write_ini(path, cfg_data_);
    return true;
}

string CivConfig::GetValue(string group, string key) {
    try {
        string val = cfg_data_.get<string>(group + "." + key);
        return val;
    } catch(const std::exception& e) {
        LOG(warning) << e.what();
        return std::string();
    }
}

}  // namespace vm_manager
