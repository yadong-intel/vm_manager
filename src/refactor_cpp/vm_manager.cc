/*
 * Copyright (c) 2020 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <pwd.h>
#include <iostream>
#include <filesystem>
#include <map>

#include "vm_manager.h"
#include "log.h"
#include "guest/vm_builder.h"
#include "guest/tui.h"
#include "guest/vm_interface.h"

#define VERSION_MAJOR 0
#define VERSION_MINOR 4
#define VERSION_MICRO 3

#define VERSION xstr(VERSION_MAJOR) "." xstr(VERSION_MINOR) "." xstr(VERSION_MICRO)

namespace vm_manager {
namespace po = boost::program_options;

CivOptions::CivOptions() {
    cmdline_options.add_options()
		("help,h",    "Show ths help message")
		("create,c",  "Create a new CiV guest")
		("import,i",  po::value<std::string>(), "Import a CiV guest from existing config file")
		("delete,d",  po::value<std::string>(), "Delete a CiV guest")
		("start,b",   po::value<std::string>(), "Start a CiV guest")
		("stop,q",    po::value<std::string>(), "Stop a CiV guest")
		("flash,f",   po::value<std::string>(), "Flash a CiV guest")
		("update,u",  po::value<std::string>(), "Update an existing CiV guest")
		("list,l",    "List existing CiV guest")
		("version,v", "Show CiV vm-manager version")
	;
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

	if (vm.count("create")) {
		return;
	}

	if (vm.count("import")) {
		return;
	}

	if (vm.count("delete")) {
		return;
	}

	if (vm.count("start")) {
		LOG(info) << "Start guest " << vm["start"].as<std::string>();
		StartVm(vm["start"].as<std::string>());
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

	if (vm.count("version")) {
		PrintVersion();
		return;
	}
}

void CivOptions::PrintHelp(void) {
	std::cout << "Usage:\n";
	std::cout << "  vm-manager [-c] [-i config_file_path] [-d vm_name] [-b vm_name] [-q vm_name] [-f vm_name] [-u vm_name] [-l] [-v] [-h]\n";
	std::cout << "Options:\n";
	std::cout << cmdline_options << std::endl;
}

void CivOptions::PrintVersion(void) {
	std::cout << "CiV vm-manager version: " << VERSION << std::endl;
}

}  // namespace vm_manager

static string civ_config_path;
string GetConfigPath(void) {
	return civ_config_path;
}

static int SetupConfigPath(void)
{
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

    civ_config_path = string(getpwuid(euid)->pw_dir) + "/.intel/.civ";
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

int main(int argc, char *argv[])
{
	int c;
	int ret = 0;

    logger::init();

	if (SetupConfigPath() != 0)
		return -1;

	//set_cleanup();
	vm_manager::CivOptions co;
	co.ParseOptions(argc, argv);

	return ret;
}
