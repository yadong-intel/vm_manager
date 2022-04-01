/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef __MESSAGE_H__
#define __MESSAGE_H__

namespace vm_manager {

enum CivMsgType {
    kCiVMsgStopServer = 100U,
    kCivMsgStartVm,
    kCivMsgStopVm,
    kCivMsgGetVmStatus,
    kCivMsgTest,
    kCivMsgRespond = 500U,
};

struct StartVmPayload {
    char name[64];
    char env_disp[64];
    char env_xauth[128];
};

struct CivMsg {
    enum { MaxPayloadSize = 1024U };

    CivMsgType type;
    union {
        char payload[MaxPayloadSize];
        struct StartVmPayload vm_pay_load;
    };
};


}  // namespace vm_manager

#endif /* __MESSAGE_H__ */