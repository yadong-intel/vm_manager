/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <guest/vsock_cid_pool.h>

namespace vm_manager {
#define CIV_MAX_CID_NUM 2048U

uint32_t VsockCidPool::GetCid() {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t pos = bs_._Find_first();
    if (pos == CIV_MAX_CID_NUM)
        return 0;
    bs_.reset(pos);
    return kCiVCidStart + pos;
}

uint32_t VsockCidPool::GetCid(uint32_t cid) {
    std::lock_guard<std::mutex> lock(mutex_);
    if ((cid < kCiVCidStart) || (cid >= (kCiVCidStart + CIV_MAX_CID_NUM)))
        return 0;
    if (!bs_[cid - kCiVCidStart])
        return 0;

    bs_.reset(cid - kCiVCidStart);
    return cid;
}

bool VsockCidPool::ReleaseCid(uint32_t cid) {
    if ((cid < kCiVCidStart) || (cid >= (kCiVCidStart + CIV_MAX_CID_NUM)))
        return false;
    std::lock_guard<std::mutex> lock(mutex_);
    bs_.set(cid - kCiVCidStart);
    return true;
}

VsockCidPool &VsockCidPool::Pool(void) {
    static VsockCidPool vcp_;
    return vcp_;
}

}  // namespace vm_manager
