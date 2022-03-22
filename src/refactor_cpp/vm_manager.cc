/*
 * Copyright (c) 2020 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <boost/bind/bind.hpp>
#include <boost/asio.hpp>

#include <iostream>
#include <filesystem>
#include <map>
#include <pwd.h>

#include "vm_manager.h"
#include "log.h"
#include "guest/vm_builder.h"
#include "guest/tui.h"
#include "guest/server.h"
#include "guest/client.h"

#define VERSION_MAJOR 0
#define VERSION_MINOR 4
#define VERSION_MICRO 3

#define VERSION xstr(VERSION_MAJOR) "." xstr(VERSION_MINOR) "." xstr(VERSION_MICRO)

namespace vm_manager {
namespace po = boost::program_options;

CivOptions::CivOptions() {
    cmdline_options.add_options()
        ("help,h",    "Show ths help message")
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
        LOG(info) << "Exception: " << e.what();
        return false;
    }
}

static int Daemonize(void) {
    if (pid_t pid = fork()) {
        if (pid > 0) {
            exit(0);
        } else {
            LOG(error) << "First fork failed %m!";
            return -1;
        }
    }

    setsid();
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    chdir("/");

    if (pid_t pid = fork()) {
        if (pid > 0) {
            return pid;
        } else {
            LOG(error) << "Second fork failed!";
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

    logger::log2file("/tmp/civ_server.log");
    LOG(info) << "\n--------------------- "
              << "CiV VM Manager Service started in background!"
              << "(PID=" << getpid() << ")"
              << " ---------------------";

    return 0;
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
    }

    Server &srv = Server::Get(suid, sgid, daemon);

    LOG(info) << "Starting Server!";
    if (!srv.Start()) {
        LOG(info) << "Server Failed to start!";
    }

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

void CivOptions::ParseOptions(int argc, char* argv[]) {
    po::store(po::command_line_parser(argc, argv).options(cmdline_options).run(), vm);
    po::notify(vm);

    if (vm.empty()) {
        PrintHelp();
        return;
    }

    if (vm.count("help")) {
        PrintHelp();
        return;
    }

    if (vm.count("import")) {
        return;
    }

    if (vm.count("delete")) {
        return;
    }

    if (vm.count("start")) {
        StartGuest(vm["start"].as<std::string>());
        return;
    }

    if (vm.count("stop")) {
        return;
    }

    if (vm.count("flash")) {
        return;
    }

    if (vm.count("update")) {
        return;
    }

    if (vm.count("list")) {
        return;
    }

    if (vm.count("start-server")) {
        bool daemon = (vm.count("daemon") == 0) ? false : true;
        StartServer(daemon);
        return;
    }

    if (vm.count("stop-server")) {
        StopServer();
        return;
    }

    if (vm.count("version")) {
        PrintVersion();
        return;
    }
}

void CivOptions::PrintHelp(void) {
    std::cout << "Usage:\n";
    std::cout << "  vm-manager"
              << " [-c] [-i config_file_path] [-d vm_name] [-b vm_name] [-q vm_name] [-f vm_name] [-u vm_name]"
              << " [-l] [-v] [-h]\n";
    std::cout << "Options:\n";

    std::cout << cmdline_options << std::endl;
}

void CivOptions::PrintVersion(void) {
    std::cout << "CiV vm-manager version: " << VERSION << std::endl;
}

}  // namespace vm_manager

static char civ_config_path[1024];
const char *GetConfigPath(void) {
    return civ_config_path;
}

static int SetupConfigPath(void) {
    int ret = 0;
    char *sudo_uid = NULL;
    char *sudo_gid = NULL;
    uid_t ruid, euid, suid;
    gid_t rgid, egid, sgid;

    getresuid(&ruid, &euid, &suid);
    getresgid(&rgid, &egid, &sgid);
    suid = geteuid();
    sgid = getegid();

    sudo_uid = std::getenv("SUDO_UID");
    sudo_gid = std::getenv("SUDO_GID");

    if (sudo_gid) {
        egid = atoi(sudo_gid);
        setresgid(rgid, egid, sgid);
    }

    if (sudo_uid) {
        euid = atoi(sudo_gid);
        setresuid(ruid, euid, suid);
    }

    snprintf(civ_config_path, sizeof(civ_config_path), "%s%s", getpwuid(euid)->pw_dir, "/.intel/.civ");
    if (!std::filesystem::exists(civ_config_path)) {
        if (!std::filesystem::create_directories(civ_config_path))
            ret = -1;
    }

    if (sudo_gid)
        setresgid(rgid, sgid, egid);

    if (sudo_uid)
        setresuid(ruid, suid, euid);

    return ret;
}

int main(int argc, char *argv[]) {
    int c;
    int ret = 0;

    logger::init();

    if (SetupConfigPath() != 0)
        return -1;

    // set_cleanup();

    vm_manager::CivOptions co;

    co.ParseOptions(argc, argv);

    return ret;
}
