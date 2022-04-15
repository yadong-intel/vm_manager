
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

void VmBuilder::StartVm() {
    main_th_ = boost::thread([this](){
        LOG(info) << "Emulator command:" << emul_cmd_;

        //boost::process::group g;
        //boost::thread t;
        for (VmProcess *vp : co_procs_) {
            tg_.add_thread(&vp->Run(&sub_proc_grp_));
        }
        //g.terminate();
    });
}

void VmBuilder::StopVm() {
    //sub_proc_grp_.terminate();
    for (VmProcess *vp : co_procs_) {
        vp->Stop();
    }
}

}  //  namespace vm_manager
