/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <iostream>
#include <filesystem>
#include <map>
#include <string>

#include <boost/bind/bind.hpp>
#include <boost/asio.hpp>
#include <boost/program_options.hpp>

#include "utils/log.h"
#include "utils/utils.h"
#include "guest/vm_builder.h"
#include "guest/tui.h"
#include "services/server.h"
#include "services/client.h"

#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_MICRO 0

namespace vm_manager {

bool IsServerRunning() {
    try {
        if (!std::filesystem::exists(server_sock)) {
            return false;
        }
        if (!std::filesystem::is_socket(server_sock)) {
            return false;
        }

        Client c;
        if (!c.Connect())
            return false;
        CivMsg req = { kCivMsgTest, "Test message" };
        if (!c.Send(req)) {
            return false;
        }
        CivMsg rep;
        if (!c.Receive(&rep)) {
            return false;
        }
        return true;
    } catch (std::exception& e) {
        LOG(error) << "Exception: " << e.what();
        return false;
    }
}

static void StartGuest(std::string name) {
    if (!IsServerRunning()) {
        LOG(info) << "server is not running! Please start server first!";
        return;
    }

    LOG(info) << "Start guest " << name;

    Client c;
    c.Connect();
    CivMsg req = { kCivMsgStartVm, "" };
    if (name.length() >= req.MaxPayloadSize) {
        LOG(error) << "CiV Guest name exceed Max length:" << req.MaxPayloadSize;
        return;
    }
    strncpy(req.payload, name.c_str(), req.MaxPayloadSize);
    if (!c.Send(req)) {
        LOG(error) << "Failed Send Start Guest request to server!";
        return;
    }
    CivMsg rep;
    if (!c.Receive(&rep)) {
        LOG(error) << "Cannot get response after Start Guest request sent!";
        return;
    }

    return;
}

static void StartServer(bool daemon) {
    if (IsServerRunning()) {
        LOG(info) << "Server already running!";
        return;
    }

    int suid = getuid();
    char *sudo_uid = std::getenv("SUDO_UID");
    if (sudo_uid)
        suid = atoi(sudo_uid);

    int sgid = getgid();
    char *sudo_gid = std::getenv("SUDO_GID");
    if (sudo_gid)
        sgid = atoi(sudo_gid);

    if (daemon) {
        int ret = Daemonize();
        if (ret > 0) {
            LOG(info) << "Forked daemon service (PID=" << ret << ")";
            return;
        } else if (ret < 0) {
            LOG(error) << "vm-manager: failed to Daemonize\n";
            return;
        }
        logger::log2file("/tmp/civ_server.log");
        LOG(info) << "\n--------------------- "
                  << "CiV VM Manager Service started in background!"
                  << "(PID=" << getpid() << ")"
                  << " ---------------------";
    }

    Server &srv = Server::Get(suid, sgid, daemon);

    LOG(info) << "Starting Server!";
    srv.Start();

    return;
}

static void StopServer(void) {
    if (!IsServerRunning()) {
        LOG(info) << "Server is not running!";
        return;
    }

    LOG(info) << "Stop host server";
    Client c;
    if (!c.Connect()) {
        LOG(error) << "Failed to connect to server!";
        return;
    }
    CivMsg req = { kCiVMsgStopServer, "Stop server" };
    if (!c.Send(req))
        return;
    CivMsg rep;
    if (!c.Receive(&rep))
        return;
    LOG(info) << "Reply :type=" << rep.type << " payload: " << rep.payload;
    return;
}

namespace po = boost::program_options;
class CivOptions final {
 public:
    CivOptions() {
        cmdline_options_.add_options()
            ("help,h",    "Show ths help message")
            ("create,c",  po::value<std::string>(), "Create a CiV guest")
            ("import,i",  po::value<std::string>(), "Import a CiV guest from existing config file")
            ("delete,d",  po::value<std::string>(), "Delete a CiV guest")
            ("start,b",   po::value<std::string>(), "Start a CiV guest")
            ("stop,q",    po::value<std::string>(), "Stop a CiV guest")
            ("flash,f",   po::value<std::string>(), "Flash a CiV guest")
            ("update,u",  po::value<std::string>(), "Update an existing CiV guest")
            ("list,l",    "List existing CiV guest")
            ("version,v", "Show CiV vm-manager version")
            ("start-server",  "Start host server")
            ("stop-server",  "Stop host server")
            ("daemon", "start server as a daemon");
    }

    CivOptions(CivOptions &) = delete;
    CivOptions& operator=(const CivOptions &) = delete;

    void ParseOptions(int argc, char* argv[]) {
        po::store(po::command_line_parser(argc, argv).options(cmdline_options_).run(), vm_);
        po::notify(vm_);

        if (vm_.empty()) {
            PrintHelp();
            return;
        }

        if (vm_.count("help")) {
            PrintHelp();
            return;
        }

        if (vm_.count("create")) {
            CivTui ct;
            ct.InitializeUi();
            return;
        }

        if (vm_.count("import")) {
            return;
        }

        if (vm_.count("delete")) {
            return;
        }

        if (vm_.count("start")) {
            StartGuest(vm_["start"].as<std::string>());
            return;
        }

        if (vm_.count("stop")) {
            return;
        }

        if (vm_.count("flash")) {
            return;
        }

        if (vm_.count("update")) {
            return;
        }

        if (vm_.count("list")) {
            return;
        }

        if (vm_.count("start-server")) {
            bool daemon = (vm_.count("daemon") == 0) ? false : true;
            StartServer(daemon);
            return;
        }

        if (vm_.count("stop-server")) {
            StopServer();
            return;
        }

        if (vm_.count("version")) {
            PrintVersion();
            return;
        }
    }

 private:
    void PrintHelp(void) {
        std::cout << "Usage:\n";
        std::cout << "  vm-manager"
                  << " [-c] [-i config_file_path] [-d vm_name] [-b vm_name] [-q vm_name] [-f vm_name] [-u vm_name]"
                  << " [-l] [-v] [-h]\n";
        std::cout << "Options:\n";

        std::cout << cmdline_options_ << std::endl;
    }
    void PrintVersion(void) {
        std::cout << "CiV vm-manager version: "
                  << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_MICRO
                  << std::endl;
    }

    boost::program_options::options_description cmdline_options_;
    boost::program_options::variables_map vm_;
};

}  // namespace vm_manager

int main(int argc, char *argv[]) {
    int c;
    int ret = 0;

    logger::init();

    if (!GetConfigPath())
        return -1;

    // set_cleanup();

    vm_manager::CivOptions co;

    co.ParseOptions(argc, argv);

    return ret;
}
