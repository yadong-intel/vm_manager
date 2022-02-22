
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
class Service final {
public:
    static std::unique_ptr<Service> Create();
    ~Service();
private:
    Service(const Service&) = delete;
    Service& operator=(const Service&) = delete;
    bool Init();
}

}  // vm_manager