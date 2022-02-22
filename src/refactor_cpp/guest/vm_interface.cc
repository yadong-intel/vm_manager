
/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include "vm_builder.h"
#include "log.h"

namespace vm_manager {

int ShutdownVm(string name)
{
    VmBuilder vb(name);
    if (vb.BuildVmArgs()) {
        vb.StartVm();
    } else {
        LOG(error) << "Failed to build args";
        return -1;
    }
    return 0;
}

int StartVm(string name)
{
    VmBuilder vb(name);
    if (vb.BuildVmArgs()) {
        vb.StartVm();
    } else {
        LOG(error) << "Failed to build args";
        return -1;
    }
    return 0;
}

}  // vm_manager