/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <memory>


#include "guest/message.h"
#include "guest/client.h"
#include "guest/server.h"

namespace vm_manager {

bool Client::Connect(void) {
    boost::system::error_code ec;
    sock_.connect(boost::asio::local::stream_protocol::endpoint(server_sock), ec);
    if (ec) {
        LOG(error) << "Failed connect to server: " << ec.message();
        return false;
    }
    return true;
}

bool Client::Send(CivMsg msg) {
    boost::system::error_code ec;
    sock_.write_some(boost::asio::buffer(&msg, sizeof(msg)), ec);
    if (ec) {
        LOG(error) << "Failed send message to server: " << ec.message();
        return false;
    }
    return true;
}

bool Client::Receive(CivMsg *msg) {
    if (!msg) {
        LOG(error) << "Invalid parameter";
        return false;
    }
    boost::system::error_code ec;
    sock_.read_some(boost::asio::buffer(msg, sizeof(*msg)), ec);
    if (ec) {
        if (ec != boost::asio::error::eof)
            LOG(error) << "Failed receive message from server: " << ec.message();
        return false;
    }
    return true;
}

}  // namespace vm_manager
