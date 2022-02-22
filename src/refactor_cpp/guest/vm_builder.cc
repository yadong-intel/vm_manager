
/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include "vm_builder.h"
#include "vm_manager.h"
#include "config_parser.h"
#include "log.h"

namespace vm_manager {

bool VmBuilder::BuildQemuVmArgs()
{
    LOG(info) << "build qemu vm args";
    return false;
}

bool VmBuilder::BuildVmArgs()
{
    if (cfg_.GetValue(kGroupEmul, kEmulType) == kEmulTypeQemu) {
        return BuildQemuVmArgs();
    } else {
        /* Default try to build args for QEMU */
        return BuildQemuVmArgs();
    }
    return false;
}

void VmBuilder::StartVm()
{
    LOG(info) << "Emulator command:" << emul_cmd_;
}

VmBuilder::VmBuilder(string name)
{
    LOG(info) << "Config file path:" << GetConfigPath()  << string("/") << name << ".ini";
    if (!cfg_.ReadConfigFile(GetConfigPath() + string("/") + name + ".ini")) {
        LOG(error) << "Failed to read config file";
    }
}

}  // vm_manager