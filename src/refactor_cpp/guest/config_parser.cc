#include <assert.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <map>
#include <vector>
using namespace std;

#include "config_parser.h"

#if 1
static map<string, vector<string>> keys_global = {
    { "name",          {}},
    { "flashfiles",    {}},
    { "adb_port",      {}},
    { "fastboot_port", {}},
    { "vsock_cid",     {}},
};

static map<string, vector<string>> keys_emulator = {
    {"type", {
        "QEMU"
    }},
    {"path", {}},
};

static map<string, vector<string>> keys_memory = {
    {"size", {}},
};

static map<string, vector<string>> keys_vcpu = {
    {"num", {}},
};

static map<string, vector<string>> keys_firmware = {
    {"type", {
        FIRM_OPTS_UNIFIED_STR,
        FIRM_OPTS_SPLITED_STR
    }},
    {"path", {}},
};

static map<string, vector<string>> keys_disk = {
    {"size", {}},
    {"path", {}},
};

static map<string, vector<string>> keys_graphics = {
    {"type", {
        VGPU_OPTS_NONE_STR,
        VGPU_OPTS_VIRTIO_STR,
        VGPU_OPTS_RAMFB_STR,
        VGPU_OPTS_GVTG_STR,
        VGPU_OPTS_GVTD_STR
    }},
    {"gvtg_version", {
        GVTG_OPTS_V5_1_STR,
        GVTG_OPTS_V5_2_STR,
        GVTG_OPTS_V5_4_STR,
        GVTG_OPTS_V5_8_STR
    }},
};

static map<string, vector<string>> keys_vtpm = {
    {"bin_path", {}},
    {"data_dir", {}},
};

static map<string, vector<string>> keys_rpmb = {
    {"bin_path", {}},
    {"data_dir", {}},
};

static map<string, vector<string>> keys_aaf = {
    {"path", {}},
    {"support_suspend", {
        SUSPEND_ENABLE_STR,
        SUSPEND_DISABLE_STR
    }},
};

static map<string, vector<string>> keys_passthrough = {
    {"passthrough_pci", {}},
};

static map<string, vector<string>> keys_mediation = {
    {"battery_med", {}},
    {"thermal_med", {}},
};

static map<string, vector<string>> keys_guest_control = {
    {"time_keep", {}},
    {"pm_control", {}},
};

static map<string, vector<string>> keys_extra = {
    {"cmd", {}},
};

static map<string, map<string, vector<string>>> civ_opts = {
    { GROUP_GLOB_STR,          keys_global, },
    { GROUP_EMUL_STR,          keys_emulator },
    { GROUP_MEM_STR,           keys_memory },
    { GROUP_VCPU_STR,          keys_vcpu },
    { GROUP_FIRM_STR,          keys_firmware },
    { GROUP_DISK_STR,          keys_disk },
    { GROUP_VGPU_STR,          keys_graphics },
    { GROUP_VTPM_STR,          keys_vtpm },
    { GROUP_RPMB_STR,          keys_rpmb },
    { GROUP_AAF_STR,           keys_aaf },
    { GROUP_PCI_PT_STR,        keys_passthrough },
    { GROUP_MEDIATION_STR,     keys_mediation },
    { GROUP_GUEST_SERVICE_STR, keys_guest_control },
    { GROUP_EXTRA_STR,         keys_extra },
};

bool civ_config::sanitize_opts(void)
{
    for (auto& section : cfg_data) {
        auto grp = civ_opts.find(section.first);
        if (grp != civ_opts.end()) {
            for (auto& subsec : section.second) {
                auto key = grp->second.find(subsec.first);
                if (key != grp->second.end()) {
                    return true;
                } else {
                    return false;
                }
            }
        } else {
            cout << "Invalid group: " << section.first <<endl;
            return false;
        }
    }
    return true;
}

bool civ_config::read_file(string path) {
    try {
        cfg_data.clear();
        read_ini(path, cfg_data);
        if (!sanitize_opts()) {
            return false;
        }
        return true;
    } catch (exception& e) {
        cout << "Exception: " << e.what() << endl;
        return false;
    }
}

bool civ_config::write_file(string path) {
    try {
        if (!sanitize_opts()) {
            return false;
        }
        write_ini(path, cfg_data);
        return true;
    } catch (exception& e) {
        cout << "Exception: " << e.what() << endl;
        return false;
    }
}

string civ_config::get_value(string group, string key)
{
    try {
        string val = cfg_data.get<string>(group + "." + key);
        return val;
    } catch (exception& e) {
        return std::string();
    }
}

#endif
