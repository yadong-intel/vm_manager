/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <signal.h>

#include <iostream>
#include <exception>
#include <utility>
#include <string>
#include <memory>
#include <filesystem>

#include <boost/asio.hpp>

#include "services/server.h"
#include "services/message.h"
#include "guest/vm_interface.h"
#include "guest/vm_builder.h"
#include "utils/log.h"

namespace vm_manager {

class session : public std::enable_shared_from_this<session> {
 public:
    explicit session(boost::asio::local::stream_protocol::socket sock)
        : socket_(std::move(sock)) {
    }

    void start() {
        do_read();
    }

 private:
    void do_read() {
        auto self(shared_from_this());
        memset(&msg_in_, 0, sizeof(msg_in_));
        socket_.async_receive(boost::asio::buffer(&msg_in_, sizeof(msg_in_)),
            [this, self](boost::system::error_code ec, std::size_t bytes) {
                if (ec) {
                    if (ec != boost::asio::error::eof)
                        LOG(error) << "vm-manager: error happened when receiving data: "
                                   << ec.message().c_str()
                                   << "(" << ec.value() << ")";
                    return;
                }
                LOG(info) << "vm-manager receive(Msg):"
                          << "type=" << msg_in_.type
                          << ", payload=" << msg_in_.payload;

                switch (msg_in_.type) {
                    case kCiVMsgStopServer:
                        stop_server_ = true;
                        break;
                    case kCivMsgStartVm:
                        StartVm(msg_in_.vm_pay_load);
                        break;
                    case kCivMsgTest:
                        break;
                    default:
                        LOG(error) << "vm-manager: received unknown message type: " << msg_in_.type;
                        return;
                }
                do_write(bytes);
        });
    }

    void do_write(std::size_t bytes) {
        auto self(shared_from_this());
        memset(&msg_out_, 0, sizeof(msg_out_));
        msg_out_.type = kCivMsgRespond;
        snprintf(msg_out_.payload, sizeof(msg_out_.payload), "Done: Server received %ld bytes!", bytes);
        socket_.async_send(boost::asio::buffer(&msg_out_, sizeof(msg_out_)),
            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                if (stop_server_) {
                    socket_.close();
                    Server &srv = Server::Get(0, 0, 0);
                    srv.Stop();
                    return;
                }
                do_read();
            }
        });
    }

    // The socket used to communicate with the client.
    boost::asio::local::stream_protocol::socket socket_;
    bool stop_server_ = false;
    CivMsg msg_in_;
    CivMsg msg_out_;
};

void Server::Accept() {
    acceptor_.async_accept([this](boost::system::error_code ec, boost::asio::local::stream_protocol::socket socket) {
        if (!ec) {
            LOG(info) << "New Session: " << ec.message().c_str();
            std::make_shared<session>(std::move(socket))->start();
        }

        Accept();
    });
}

void Server::AsyncWaitSignal(void) {
    boost::system::error_code ec;
    signals_.add(SIGINT, ec);
    if (ec)
        LOG(warning) << "Failed to add SIG(" << SIGINT << "), " << ec.message().c_str();
    signals_.add(SIGTERM, ec);
    if (ec)
        LOG(warning) << "Failed to add SIG(" << SIGTERM << "), " << ec.message().c_str();

    signals_.async_wait(
        [this](boost::system::error_code ec, int signo) {
            LOG(info) << "Signal(" << signo << ") received!";
            Stop();
    });
}

void Server::Start(void) {
    try {
        std::filesystem::remove(server_sock);

        boost::asio::local::stream_protocol::endpoint ep(server_sock);
        acceptor_.open(ep.protocol());
        acceptor_.bind(ep);
        acceptor_.listen();

        Accept();

        AsyncWaitSignal();
        LOG(info) << "Async wait signal done";

        LOG(info) << "StartServer: context run "
                  << ", pid=" << getpid()
                  << ", uid=" << getuid();
        ioc_.run();

        LOG(info) << "CiV Server exited!";
    } catch (std::exception &e) {
        LOG(error) << "CiV Server: Exception:" << e.what() << ", pid=" << getpid();
    }
}

void Server::Stop(void) {
    LOG(info) << "Stop CiV Server!";
    acceptor_.close();
    ioc_.stop();
    std::filesystem::remove(server_sock);
}

Server &Server::Get(uid_t suid, gid_t sgid, bool daemon) {
    static Server server_{ suid, sgid, daemon };
    return server_;
}

}  // namespace vm_manager
