/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <syslog.h>
#include <signal.h>

#include <iostream>
#include <cstring>
#include <exception>
#include <utility>

#include <boost/serialization/vector.hpp>
#include <boost/bind/bind.hpp>
#include <boost/asio.hpp>

#include "guest/vm_builder.h"
#include "log.h"
#include "guest/service.h"
#include "guest/message.h"

#if 0
namespace vm_manager {

static int Damonize(void) {
    if (pid_t pid = fork()) {
        if (pid > 0) {
            return pid;
        } else {
            syslog(LOG_ERR | LOG_USER, "First fork failed: %m");
            return -1;
        }
    }

    setsid();
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    chdir("/");

    if (pid_t pid = fork()) {
        if (pid > 0) {
            exit(0);
        } else {
            syslog(LOG_ERR | LOG_USER, "Second fork failed: %m");
            exit(-1);
        }
    }

    umask(0);
    for (int t = sysconf(_SC_OPEN_MAX); t >= 0; t--) {
        close(t);
    }

    /* Close the standard streams. */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    int fd = open("/dev/null", O_RDWR);
    if (fd != STDIN_FILENO)
      return -1;
    if (dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO)
      return -1;
    if (dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)
      return -1;

    return 0;
}


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
                syslog(LOG_INFO, "vm-manager receive(MMsg): type=%d, pid=%d, pl=%s, bytes=%ld\n",
                                  msg_in_.type, getpid(), msg_in_.payload, bytes);
                if (ec) {
                    if (ec != boost::asio::error::eof)
                        syslog(LOG_ERR, "vm-manager: error happened when receiving data: %s(%d)",
                                     ec.message().c_str(), ec.value());
                    return;
                }
                Service *srv = Service::Get();
                switch (msg_in_.type) {
                    case kCiVMsgStopServer:
                        socket_.close();
                        srv->Stop();
                        break;
                    case kCivMsgStartVm:
                    case kCivMsgRespond:
                    case kCivMsgTest:
                        break;
                    default:
                        syslog(LOG_ERR, "vm-manager: received unknown message type: %d", msg_in_.type);
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
            if (!ec)
                do_read();
        });
    }

    // The socket used to communicate with the client.
    boost::asio::local::stream_protocol::socket socket_;
    CivMsg msg_in_;
    CivMsg msg_out_;
};

class server {
 public:
    server(boost::asio::io_context& io_context, const std::string& file)
      : acceptor_(io_context, boost::asio::local::stream_protocol::endpoint(file)) {
        do_accept();
    }

 private:
    void do_accept() {
        acceptor_.async_accept([this](boost::system::error_code ec, boost::asio::local::stream_protocol::socket socket) {
            if (!ec) {
                syslog(LOG_INFO, "New Session: ec=%s, pid=%d\n", ec.message().c_str(), getpid());
                std::make_shared<session>(std::move(socket))->start();
                syslog(LOG_INFO, "Session started: ec=%s, pid=%d\n", ec.message().c_str(), getpid());
            }

            do_accept();
        });
    }

    boost::asio::local::stream_protocol::acceptor acceptor_;
};

void Service::DoAccept() {
    acceptor_.async_accept([this](boost::system::error_code ec, boost::asio::local::stream_protocol::socket socket) {
        if (!ec) {
            syslog(LOG_INFO, "New Session: ec=%s, pid=%d\n", ec.message().c_str(), getpid());
            std::make_shared<session>(std::move(socket))->start();
            syslog(LOG_INFO, "Session started: ec=%s, pid=%d\n", ec.message().c_str(), getpid());
        }

        DoAccept();
    });
}

void Service::StartServer() {
    try {
        const char civ_hs_sock[] = "/tmp/civ_hs.socket";
        std::remove(civ_hs_sock);
        server s(io_context, civ_hs_sock);
        acceptor_ = boost::asio::local::stream_protocol::acceptor(io_context, civ_hs_sock);
        DoAccept();
        syslog(LOG_INFO, "StartServer: context run pid=%d, uid=%d, %d, %d, %d, %s\n",
                getpid(), getuid(), geteuid(), getgid(), getegid(), std::getenv("SUDO_UID"));
        io_context.run();
        syslog(LOG_INFO, "Server stoped: context run pid=%d\n", getpid());
        std::remove("/tmp/civ_hs.socket");
    } catch (std::exception &e) {
        syslog(LOG_ERR, "StartServer: context run pid=%d, %s\n", getpid(), e.what());
    }
}

bool Service::Run(void) {
    syslog(LOG_INFO, "dae=%d, pid=%d", daemon_, getpid());
    if (daemon_) {
        int ret = Damonize();
        syslog(LOG_ERR, "StartServer: context run pid=%d, ret=%d\n", getpid(), ret);
        if (ret > 0) {
            return true;
        } else if (ret < 0) {
            syslog(LOG_INFO, "vm-manager: failed to Daemonize\n");
            return false;
        }
    }

    StartServer();
    return true;
}

void Service::StopServer(void) {
    io_context.stop();
}

void Service::Stop(void) {
    StopServer();
}

Service *Service::Get(uid_t suid, gid_t sgid, bool daemon) {
    //static Service service_(suid, sgid, daemon);
    std::lock_guard<std::mutex> lock(mutex_);
    if (s_ == nullptr)
        s_ = new Service(suid, sgid, daemon);
    return s_;
}

bool IsServerRunning() {
    try {
        boost::system::error_code ec;
        boost::asio::io_context io_context;
        boost::asio::local::stream_protocol::socket s(io_context);
        s.connect(boost::asio::local::stream_protocol::endpoint(service_sock), ec);
        if (ec) {
            LOG(info) << "Failed to connect server: " << ec.message();
            return false;
        }
        CivMsg req = { kCivMsgTest, "Test message" };
        size_t request_length = sizeof(req);
        s.write_some(boost::asio::buffer(&req, request_length), ec);
        if (ec) {
            LOG(info) << "Failed to send to server: " << ec.message();
            return false;
        }
        CivMsg reply;
        size_t reply_length = s.receive(boost::asio::buffer(&reply, sizeof(reply)));
        LOG(info) << "Reply is:t=" << reply.type << "len=" << reply_length << " payload:" << reply.payload;

        s.close();
        return true;
    } catch(std::exception& e) {
        LOG(info) << "Exception: " << e.what();
        return false;
    }
}

bool StopServer(void) {
    try {
        boost::asio::io_context io_context;
        boost::asio::local::stream_protocol::socket s(io_context);
        s.connect(boost::asio::local::stream_protocol::endpoint(service_sock));
        CivMsg request;
        request.type = kCiVMsgStopServer;
        s.send(boost::asio::buffer(&request, sizeof(request)));
        char reply[1024];
        size_t reply_length = s.receive(boost::asio::buffer(reply, sizeof(reply)));
        return true;
    } catch(std::exception &e) {
        LOG(info) << "Server not running!";
        return false;
    }
}

}  // namespace vm_manager
#endif