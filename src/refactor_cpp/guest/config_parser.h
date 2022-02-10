/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef __CONFIG_PARSER_H__
#define __CONFIG_PARSER_H__

#define GROUP_GLOB_STR           "global"
#define GROUP_EMUL_STR           "emulator"
#define GROUP_MEM_STR            "memory"
#define GROUP_VCPU_STR           "vcpu"
#define GROUP_FIRM_STR           "firmware"
#define GROUP_DISK_STR           "disk"
#define GROUP_VGPU_STR           "graphics"
#define GROUP_VTPM_STR           "vtpm"
#define GROUP_RPMB_STR           "rpmb"
#define GROUP_AAF_STR            "aaf"
#define GROUP_PCI_PT_STR         "passthrough"
#define GROUP_MEDIATION_STR      "mediation"
#define GROUP_GUEST_SERVICE_STR  "guest_control"
#define GROUP_EXTRA_STR          "extra"

#define FIRM_OPTS_UNIFIED_STR "unified"
#define FIRM_OPTS_SPLITED_STR "splited"

#define VGPU_OPTS_NONE_STR     "headless"
#define VGPU_OPTS_VIRTIO_STR   "virtio"
#define VGPU_OPTS_RAMFB_STR    "ramfb"
#define VGPU_OPTS_GVTG_STR     "GVT-g"
#define VGPU_OPTS_GVTD_STR     "GVT-d"

#define GVTG_OPTS_V5_1_STR "i915-GVTg_V5_1"
#define GVTG_OPTS_V5_2_STR "i915-GVTg_V5_2"
#define GVTG_OPTS_V5_4_STR "i915-GVTg_V5_4"
#define GVTG_OPTS_V5_8_STR "i915-GVTg_V5_8"

#define SUSPEND_ENABLE_STR  "enable"
#define SUSPEND_DISABLE_STR "disable"

#include <string>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
using namespace boost::property_tree;


class civ_config final {
    public:
        civ_config();
        ~civ_config();
        std::string get_value(const string group, const string key);
        int set_value(const string group, const string key, const string value);
        bool read_file(const string path);
        bool write_file(string path);
    private:
        bool sanitize_opts(void);
        ptree cfg_data;
};

#endif /* __CONFIG_PARSER_H__ */
