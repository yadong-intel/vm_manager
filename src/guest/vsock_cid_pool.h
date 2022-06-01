/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef SRC_GUEST_VSOCK_CID_POOL_H_
#define SRC_GUEST_VSOCK_CID_POOL_H_

#include <mutex>
#include <utility>
#include <bitset>

namespace vm_manager {

class VsockCidPool {
#define CIV_MAX_CID_NUM 2048U

 public:
    uint32_t GetCid();

    uint32_t GetCid(uint32_t cid);

    bool ReleaseCid(uint32_t cid);

    static VsockCidPool &Pool(void);

 private:
    VsockCidPool() = default;
    ~VsockCidPool() = default;
    VsockCidPool(const VsockCidPool &) = delete;
    VsockCidPool& operator=(const VsockCidPool&) = delete;

    const uint32_t kCiVCidStart = 1024U;
    std::bitset<CIV_MAX_CID_NUM> bs_ = std::move(std::bitset<CIV_MAX_CID_NUM>{}.set());
    std::mutex mutex_;
};

}  // namespace vm_manager

#endif  // SRC_GUEST_VSOCK_CID_POOL_H_
