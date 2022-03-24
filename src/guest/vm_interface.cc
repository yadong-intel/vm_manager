
/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include "guest/vm_builder.h"
#include "guest/vm_interface.h"
#include "utils/log.h"

namespace vm_manager {

int ShutdownVm(std::string name) {
    // Shutdown VM
    return 0;
}

int StartVm(std::string name) {
    VmBuilder vb(name);
    if (vb.BuildVmArgs()) {
        vb.StartVm();
    } else {
        LOG(error) << "Failed to build args";
        return -1;
    }
    return 0;
}

}  //  namespace vm_manager
