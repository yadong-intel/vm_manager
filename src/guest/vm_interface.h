
/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <string>

namespace vm_manager {
int StartVm(const char payload[]);
int ShutdownVm(std::string name);

}  // namespace vm_manager
