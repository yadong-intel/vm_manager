/*
 * Copyright (c) 2020 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef __VM_MANAGER_H__
#define __VM_MANAGER_H__

#include <boost/program_options.hpp>

#define CIV_GUEST_QMP_SUFFIX     ".qmp.unix.socket"

#ifndef MAX_PATH
#define MAX_PATH 2048U
#endif

#define vstr(s) #s
#define xstr(s) vstr(s)

namespace vm_manager {

class CivOptions final {

public:
    CivOptions();

	CivOptions(CivOptions &) = delete;
	CivOptions& operator=(const CivOptions &) = delete;

    void ParseOptions(int argc, char* argv[]);

private:
    void PrintHelp(void);
	void PrintVersion(void);

    boost::program_options::options_description cmdline_options;
	boost::program_options::variables_map vm;
};

} //  namespace vm_manager

std::string GetConfigPath(void);

#endif /* __VM_MANAGER_H__*/
