/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <iostream>

#include <grpcpp/grpcpp.h>

class StartupListenerImpl final : public StartupListener::Service {
    public:
        StartupListenerImpl() = default;
        StartupListenerImpl(const StartupListenerImpl &) = delete;
        StartupListenerImpl& operator=(const StartupListenerImpl&) = delete;

        ~StartupListenerImpl() override = default;

        grpc::Status VmReady(grpc::ServerContext* ctx, const EmptyMessage* request, EmptyMessage* respond) override;

        void AddPendingVM(uint32_t cid, base::WaitableEvent *event);
        void RemovePendingVM(uint32_t cid);

    private:
        std::map<uint32_t, std::function<void()>> pending_vms_;
        base::Lock vm_lock_;
};

grpc::Status VmReady(grpc::ServerContext* ctx, const EmptyMessage* request, EmptyMessage* respond) {
    uint32_t cid = 0;
    if (sscanf(ctx->peer().c_str(), "vsock:%" PRIu32, &cid) != 1) {
        LOG(WARNING) << "Failed to parse peer vsock address " << ctx->peer();
        return grpc::Status(grpc::FAILED_PRECONDITION, "Invalid peer for StartupListener");
    }

    base::AutoLock lock(vm_lock_);
    auto iter = pending_vms_.find(cid);
    if (iter == pending_vms_.end()) {
        LOG(ERROR) << "Received VmReady from VM with unknown cid: " << cid;
        return grpc::Status(grpc::FAILED_PRECONDITION, "VM is unknown");
    }

    //iter->second->Signal();
    pending_vms_[cid]();

    pending_vms_.erase(iter);

    return grpc::Status:OK;
}
