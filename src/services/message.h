/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>

namespace vm_manager {

extern const char *kCivSharedMemName;
extern const char *kCivSharedObjSync;
extern const char *kCivSharedObjData;

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

struct CivMsgSync {
    boost::interprocess::interprocess_mutex mutex;
    boost::interprocess::interprocess_condition cond_empty;
    boost::interprocess::interprocess_condition cond_full;
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