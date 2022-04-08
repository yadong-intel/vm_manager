
/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include "guest/vm_builder.h"
#include "guest/config_parser.h"
#include "services/message.h"
#include "utils/log.h"
#include "utils/utils.h"

namespace vm_manager {

bool VmBuilder::BuildQemuVmArgs() {
    LOG(info) << "build qemu vm args";
    return false;
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
