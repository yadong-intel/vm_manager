
/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef SRC_GUEST_VM_INTERFACE_H_
#define SRC_GUEST_VM_INTERFACE_H_

#include <string>
#include <vector>

#include "guest/config_parser.h"
#include "guest/vm_builder.h"

namespace vm_manager {

int StartVm(const char payload[]);
int ShutdownVm(const char payload[]);

}  // namespace vm_manager

#endif  // SRC_GUEST_VM_INTERFACE_H_
