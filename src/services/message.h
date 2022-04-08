/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include <string>

#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/string.hpp>

namespace vm_manager {

typedef boost::interprocess::allocator<char, boost::interprocess::managed_shared_memory::segment_manager> CharAllocator;
typedef boost::interprocess::basic_string<char, std::char_traits<char>, CharAllocator> bstring;

extern const char *kCivServerMemName;
extern const char *kCivServerObjSync;
extern const char *kCivServerObjData;


enum CivMsgType {
    kCiVMsgStopServer = 100U,
    kCivMsgStartVm,
    kCivMsgStopVm,
    kCivMsgGetVmStatus,
    kCivMsgTest,
    kCivMsgRespond = 500U,
};

struct CivMsgSync {
    boost::interprocess::interprocess_mutex mutex;
    boost::interprocess::interprocess_mutex mutex_cond;
    boost::interprocess::interprocess_condition cond_s;
    boost::interprocess::interprocess_condition cond_c;
};

struct CivMsg {
    enum { MaxPayloadSize = 1024U };

    CivMsgType type;
    char payload[MaxPayloadSize];
};

}  // namespace vm_manager

#endif /* __MESSAGE_H__ */