
/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <boost/process.hpp>
#include <boost/asio.hpp>

#include "guest/vm_builder.h"
#include "guest/vm_interface.h"
#include "services/message.h"
#include "utils/log.h"

namespace vm_manager {

int ShutdownVm(std::string name) {
    // Shutdown VM
    return 0;
}

#if 0
static std::string GetEnvDisplay() {
    std::error_code ec;
    std::string line;
    std::size_t pos(std::string::npos);
    boost::process::ipstream pipe_stream;
    const std::string_view disp("DISPLAY=");

    boost::process::child c("systemctl --user show-environment", boost::process::std_out > pipe_stream);

    while (pipe_stream && std::getline(pipe_stream, line) && !line.empty()) {
        pos = line.find(disp);
        if (pos != std::string::npos) {
            break;
        }
        line.clear();
    }

    c.wait(ec);

    if (line.empty()) {
        LOG(info) << "Trying: DISPLAY=:0";
        return std::string(":0");
    } else {
        LOG(info) << "Detected: " << "DISPLAY=" << line.substr(pos + disp.length()).c_str();
        return line.substr(pos + disp.length());
    }
}
#endif

static boost::process::environment GetChildEnv(struct StartVmPayload vmp) {
    boost::process::environment env_ = boost::this_process::environment();
    env_["DISPLAY"] = vmp.env_disp,
    env_["XAUTHORITY"] = vmp.env_xauth;
    return env_;
}

void GuestThread(struct StartVmPayload vmp) {
    LOG(info) << "Thread-0x" << std::hex << std::this_thread::get_id() << " Starting";
    std::error_code ec;
    boost::asio::io_context ioc;
    std::future<std::string> buf_o, buf_e;

    boost::process::child c(boost::process::search_path("qemu-system-x86_64"),
                            boost::process::args = { "-device", "virtio-gpu" },
                            GetChildEnv(vmp),
                            boost::process::std_out > buf_o,
                            boost::process::std_err > buf_e,
                            ioc);

    LOG(info) << "Running=" << c.running(ec) << ", PID="<< c.id();
    LOG(info) << ec.message();


    ioc.run();

    int result = c.exit_code();

    LOG(info) << "Child-" << c.id() << " exited! code=" << result;
    LOG(info) << "Out: " << buf_o.get().c_str() << "Err:" << buf_e.get().c_str();

    LOG(info) << "Thread-0x" << std::hex << std::this_thread::get_id() << " Exiting";
}

int StartVm(struct StartVmPayload vm_payload) {
    VmBuilder vb(vm_payload);

    std::thread gt(GuestThread, vm_payload);
    std::thread::id tid = gt.get_id();
    gt.detach();

    if (vb.BuildVmArgs()) {
        vb.StartVm();
    } else {
        LOG(error) << "Failed to build args";
        return -1;
    }
    return 0;
}

}  //  namespace vm_manager
