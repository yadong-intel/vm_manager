/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <map>

#include <grpcpp/grpcpp.h>

#include "utils/log.h"
#include "guest/startup_listener.h"
#include "services/protos/gens/vm_host.grpc.pb.h"

namespace vm_manager {

class StartupListenerImpl final : public vm_manager::StartupListener::Service {
 public:
    StartupListenerImpl() = default;
    StartupListenerImpl(const StartupListenerImpl &) = delete;
    StartupListenerImpl& operator=(const StartupListenerImpl&) = delete;

    ~StartupListenerImpl() override = default;

    grpc::Status VmReady(grpc::ServerContext* ctx, const EmptyMessage* request, EmptyMessage* respond) override;

    void AddPendingVM(uint32_t cid, std::function<void(void)> event);  //base::WaitableEvent *event);
    void RemovePendingVM(uint32_t cid);

 private:
    std::map<uint32_t, std::function<void(void)>> pending_vms_;
    std::mutex vm_lock_;
};

grpc::Status StartupListenerImpl::VmReady(grpc::ServerContext* ctx, const EmptyMessage* request, EmptyMessage* respond) {
    uint32_t cid = 0;
    if (sscanf(ctx->peer().c_str(), "vsock:%u", &cid) != 1) {
        LOG(warning) << "Failed to parse peer vsock address " << ctx->peer();
        return grpc::Status(grpc::FAILED_PRECONDITION, "Invalid peer for StartupListener");
    }

    std::scoped_lock lock(vm_lock_);
    auto iter = pending_vms_.find(cid);
    if (iter == pending_vms_.end()) {
        LOG(error) << "Received VmReady from VM with unknown cid: " << cid;
        return grpc::Status(grpc::FAILED_PRECONDITION, "VM is unknown");
    }

    //iter->second->Signal();
    iter->second();
    pending_vms_[cid]();

    pending_vms_.erase(iter);

    return grpc::Status::OK;
}

void StartupListenerImpl::AddPendingVM(uint32_t cid, std::function<void(void)> event) {
  std::scoped_lock lock(vm_lock_);

  pending_vms_[cid] = event;
}

void StartupListenerImpl::RemovePendingVM(uint32_t cid) {
  std::scoped_lock lock(vm_lock_);

  pending_vms_.erase(cid);
}

}  // namespace vm_manager
