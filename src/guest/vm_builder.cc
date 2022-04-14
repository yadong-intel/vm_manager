
/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <utility>

#include <boost/process.hpp>

#include "guest/vm_builder.h"
#include "guest/config_parser.h"
#include "services/message.h"
#include "utils/log.h"
#include "utils/utils.h"

namespace vm_manager {

std::thread VmBuilder::StartVm() {
    return std::thread ([this](){
        LOG(info) << "Emulator command:" << emul_cmd_;

        boost::process::group g;
        std::thread t;
        for (VmProcess *vp : co_procs) {
            t = vp->Run();
            t.detach();
        }
        //g.terminate();
    });
}

}  //  namespace vm_manager
