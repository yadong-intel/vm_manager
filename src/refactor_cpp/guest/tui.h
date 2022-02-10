/*
 * Copyright (c) 2022 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef __TUI_H__
#define __TUI_H__

int create_tui(void);

#include <memory>
#include <string>
#include "ftxui/component/captured_mouse.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/util/ref.hpp"
#include "ftxui/screen/terminal.hpp"

using namespace std;
using namespace ftxui;

typedef struct _checkbox_state {
    std::string name;
    bool checked;
} checkbox_state_t;


class civ_tui final {
public:
    void initialize_ui(void);
    void btn_on_save();
private:

    typedef struct _input_field {
        ftxui::Component wrap;
        ftxui::Component input;
        std::string content;
    } input_field_t;

    ftxui::ScreenInteractive _screen = ftxui::ScreenInteractive::Fullscreen();
    ftxui::Component _layout;
    ftxui::Component _form, _form_render, _form_main;
    ftxui::Component _buttons;

    /* Field: name */
    input_field_t _name;

    /* Field: flashfiles */
    input_field_t _flashfiles;

    /* Field: emulator */
    input_field_t _emulator;

    /* Field: emulator */
    input_field_t _memory;

    /* Field: vcpu */
    input_field_t _vcpu;

    /* Field: disk size/path */
    ftxui::Component _cdisk;
    input_field_t _disk_size;
    input_field_t _disk_path;

    /* Field: adb/fastboot port */
    input_field_t _adb_port;
    input_field_t _fastboot_port;

    /* Field: firmware */
    ftxui::Component _cfirm;
    ftxui::Component _cfirm_inner;
    ToggleOption _firm_toggle_option;
    int _firmware_type_selected = 0;
    std::vector<std::string> _firmware_type;
    input_field_t _firm_unified;
    input_field_t _firm_splited_code;
    input_field_t _firm_splited_data;

    /* Field: vgpu */
    std::vector<std::string> _vgpu_type;
    int _vgpu_selected = 1;
    ftxui::Component _cvgpu;
    ftxui::Component _cvgpu_type;
    ftxui::Component _cvgpu_side;

    int _gvtg_ver_selected = 1;
    std::vector<std::string> _gvtg_ver;
    input_field_t _gvtg_uuid;
    ftxui::Component _cgvtg_sub;

    /* Field: SWTPM */
    ftxui::Component _cswtpm;
    input_field_t _swtpm_bin;
    input_field_t _swtpm_data;

    /* Field: RPMB */
    ftxui::Component _crpmb;
    input_field_t _rpmb_bin;
    input_field_t _rpmb_data;

    /* Field: PCI Passthrough devices */
    std::vector<checkbox_state_t> _pci_dev;
    std::vector<string> pt_pci_disp;
    int pt_pci_tab_id = 1;
    int pt_pci_menu_selected = 0;
    ftxui::Component _cpt;
    ftxui::Component _cpt_inner, _cpt_inner_left, _cpt_inner_right, _cpt_inner_right_edit, _cpt_inner_right_disp;
    std::function<void(void)> pt_pci_click_btn;

    /* Battery/Thermal mediation */
    input_field_t _batt_med;
    input_field_t _therm_med;
    input_field_t _aaf;
    input_field_t _tkeep;
    input_field_t _pmctl;
    input_field_t _extra;

    /* Status Bar */
    string _status_bar;

    /* Buttons */
    ftxui::Component _save;
    ftxui::Component _exit;
    std::function<void(void)> save_on;

    void initialize_form(void);
    void initialize_buttons(void);

    //void init_input_field(input_field_t* f, ftxui::Component* c, string hint);
    //ftxui::Component init_input_field(input_field_t* f, string hint);
};

#endif /* __TUI_H__ */
