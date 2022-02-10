
#include "tui.h"
#include "config_parser.h"

#include <boost/process.hpp>
using namespace boost::process;

#define LAYOUT_MIN_WIDTH 60
#define LAYOUT_MAX_WIDTH 120

#if 0
void civ_tui::btn_on_save()
{
    _status_bar += "called save_on!";
    _screen.ExitLoopClosure();
}

static void btn_on_exit()
{
    _screen.ExitLoopClosure();
}
#endif
#if 0
ftxui::Component civ_tui::init_input_field_win(input_field_t* f, string hint)
{
    f->input = Input(&f->content, hint);
    return Container::Horizontal({
            Renderer(f->input, [&] {
                return hbox(text(hint + ":     "), f->input->Render());
            }),
    });
}
#endif

#if 0
#define init_input_field_win(f, c, name, hint) \
{\
    f.input = Input(&f.content, hint); \
    c = Container::Horizontal({ \
            Renderer(f.input, [&] { \
                return hbox(text(name + ":") | size(WIDTH, EQUAL, 16), f.input->Render() | underlined); \
            }), \
    }); \
}
#endif

static void get_pci_dev_list(vector<checkbox_state_t>& lspci_list)
{
    ipstream pipe_stream;
    child c("lspci -D", std_out > pipe_stream);
    string line;
    while (pipe_stream && std::getline(pipe_stream, line)) {
        if (!line.empty()) {
            checkbox_state_t item = { line, false };
            lspci_list.push_back(item);
        }
        line.erase();
    }

    c.wait();
}

#define init_input_field_win(f, name, hint) \
{\
    f.input = Input(&f.content, string(hint)); \
    f.wrap = Renderer(f.input, [&] { \
                return window(text(string(name) + ":"), f.input->Render()); \
    }); \
}

#define init_input_field_hbox(f, name, hint) \
{\
    f.input = Input(&f.content, string(hint)); \
    f.wrap = Renderer(f.input, [&] { \
                return hbox(text(string(name) + ":"), f.input->Render()); \
    }); \
}

void civ_tui::initialize_form(void)
{
    _form = Container::Vertical({});
    
    /* Field: name */
    init_input_field_win(_name, string("name"), string("instance name"));

    /* Field: flashfiles */
    init_input_field_win(_flashfiles, string("flashfiles"), string("flashfiles path"));
    _form->Add(_flashfiles.wrap);

    /* Field: emulator */
    init_input_field_win(_emulator, string("emulator"), string("emulator path"));
    _form->Add(_emulator.wrap);

    /* Field: memory */
    init_input_field_win(_memory, string("memory"), string("memory size"));
    _form->Add(_memory.wrap);

    /* Field: vcpu */
    init_input_field_win(_vcpu, string("vcpu"), string("vcpu number(1~1024)"));
    _form->Add(_vcpu.wrap);

    /* Field: DISK */
    init_input_field_hbox(_disk_size, string("disk size"), string("virtual disk size"));
    init_input_field_hbox(_disk_path, string("disk path"), string("virtual disk path"));
    _cdisk = Renderer(Container::Vertical({_disk_size.wrap, _disk_path.wrap}), [&] {
        return window(
            text(string("disk") + ":"),
            vbox(_disk_size.wrap->Render(), _disk_path.wrap->Render()));
    });
    _form->Add(_cdisk);

    /* Field: adb port */
    init_input_field_win(_adb_port, string("adb port"), string("ADB port(TCP-Localhost), e.g.:5555"));
    _form->Add(_adb_port.wrap);

    /* Field: fastboot path */
    init_input_field_win(_fastboot_port, string("fastboot port"), string("Fastboot port(TCP-Localhost), e.g.:5554"));
    _form->Add(_fastboot_port.wrap);

    /* Field: firmware */
    _firmware_type = { FIRM_OPTS_UNIFIED_STR, FIRM_OPTS_SPLITED_STR };
    _firm_toggle_option = ToggleOption();
    _firm_toggle_option.style_selected = Decorator(bold) | inverted;
    _firm_unified.input = Input(&_firm_unified.content, "Unified firmware binary path");
    _firm_splited_code.input = Input(&_firm_splited_code.content, "Splited firmware binary code path");
    _firm_splited_data.input = Input(&_firm_splited_data.content, "Splited firmware binary data path");
    _cfirm_inner = Container::Vertical({
        Toggle(&_firmware_type, &_firmware_type_selected, &_firm_toggle_option),
        Container::Tab(
            {
                Renderer(_firm_unified.input, [&] {
                    return hbox({
                        text("    ->"), _firm_unified.input->Render()
                    });
                }),
                Renderer(Container::Vertical({ _firm_splited_code.input, _firm_splited_data.input }), [&] {
                    return vbox({
                        hbox({ text("    ->"), _firm_splited_code.input->Render(), }),
                        hbox({ text("    ->"), _firm_splited_data.input->Render(), }),
                    });
                }),
            },
            &_firmware_type_selected
        )
        });
    _cfirm = Renderer(_cfirm_inner, [&] {
        return window(text("firmware: "), _cfirm_inner->Render());
    });
    _form->Add(_cfirm);

    /* Field: virtual GPU */
    _vgpu_type = { VGPU_OPTS_NONE_STR, VGPU_OPTS_VIRTIO_STR, VGPU_OPTS_RAMFB_STR, VGPU_OPTS_GVTG_STR, VGPU_OPTS_GVTD_STR };
    _cvgpu_type = Dropdown(&_vgpu_type, &_vgpu_selected);

    _gvtg_ver = {GVTG_OPTS_V5_1_STR, GVTG_OPTS_V5_2_STR, GVTG_OPTS_V5_4_STR,GVTG_OPTS_V5_8_STR};
    init_input_field_hbox(_gvtg_uuid, "uuid", "UUID for a GVTg-VGPU");
    _cgvtg_sub = Container::Vertical({Dropdown(&_gvtg_ver, &_gvtg_ver_selected), _gvtg_uuid.wrap});

    _cvgpu_side = Container::Tab({
        Renderer(emptyElement), // NONE
        Renderer(emptyElement), // VIRTIO
        Renderer(emptyElement), // RAMFB
        _cgvtg_sub,             // GVT-g
        Renderer(emptyElement), // GVT-d
    }, &_vgpu_selected);

    _cvgpu = Renderer(Container::Horizontal({ _cvgpu_type, _cvgpu_side }), [&]{
        return window(
            text("graphics: "),
            hbox(_cvgpu_type->Render(), _cvgpu_side->Render()));
    });
    _form->Add(_cvgpu);

    /* Field: SWTPM */
    init_input_field_hbox(_swtpm_bin, "bin path", "binary path");
    init_input_field_hbox(_swtpm_data, "data dir", "data folder");
    _cswtpm = Renderer(Container::Vertical({_swtpm_bin.wrap, _swtpm_data.wrap}), [&] {
        return window(
            text(string("swtpm") + ":"),
            vbox(_swtpm_bin.wrap->Render(), _swtpm_data.wrap->Render()));
    });
    _form->Add(_cswtpm);

    /* Field: RPMB */
    init_input_field_hbox(_rpmb_bin, "bin path", "binary path");
    init_input_field_hbox(_rpmb_data, "data dir", "data folder");
    _crpmb = Renderer(Container::Vertical({_rpmb_bin.wrap, _rpmb_data.wrap}), [&] {
        return window(
            text(string("rpmb") + ":"),
            vbox(_rpmb_bin.wrap->Render(), _rpmb_data.wrap->Render())
        );
    });
    _form->Add(_crpmb);

    /* Field: PCI Passthrough */
    get_pci_dev_list(_pci_dev);

    pt_pci_click_btn = [&]() {
        pt_pci_disp.clear();
        for (vector<checkbox_state_t>::iterator it = _pci_dev.begin(); it != _pci_dev.end(); ++it) {
            if (it->checked) {
                pt_pci_disp.push_back(it->name);
            }
        }
        pt_pci_tab_id = !pt_pci_tab_id;
        if (pt_pci_tab_id == 1) {
            _cpt_inner_right->TakeFocus();
            _cpt_inner_left = Button("Done", pt_pci_click_btn);
        } else
            _cpt_inner_left = Button("Edit", pt_pci_click_btn);
    };

    _cpt_inner_left = Button("Edit", pt_pci_click_btn);
    _cpt_inner_right_edit = Container::Vertical({});
    for (vector<checkbox_state_t>::iterator t = _pci_dev.begin(); t != _pci_dev.end(); ++t) {
        _cpt_inner_right_edit->Add(Checkbox(t->name, &t->checked));
    }
    _cpt_inner_right_disp = Menu(&pt_pci_disp, &pt_pci_menu_selected);

    _cpt_inner_right = Container::Tab({_cpt_inner_right_disp, _cpt_inner_right_edit}, &pt_pci_tab_id);
    _cpt_inner = Renderer(Container::Horizontal({_cpt_inner_left, _cpt_inner_right}), [&]{
        return hbox({
            _cpt_inner_left->Render(),
            _cpt_inner_right->Render() | frame | vscroll_indicator
        });
    });
    _cpt = Renderer(_cpt_inner, [&]{
        return window(
            text("PCI Devices to Passthrough"),
            _cpt_inner->Render()
        ) | size(HEIGHT, LESS_THAN, 6);
    }); 
     _form->Add(_cpt);

    /* Field: Battery */
    init_input_field_win(_batt_med, string("Battery Mediation"), string("Battery Mediation host side executable binary"));
    _form->Add(_batt_med.wrap);

    /* Field: Thermal */
    init_input_field_win(_therm_med, string("Thermal Mediation"), string("Thermal Mediation host side executable binary"));
    _form->Add(_therm_med.wrap);

    /* Field: AAF */
    init_input_field_win(_aaf, string("AAF"), string("AAF configuration folder path"));
    _form->Add(_aaf.wrap);

     /* Field: Timekeep */
    init_input_field_win(_tkeep, string("Guest Time Keep"), string("Time Keep host side executable binary"));
    _form->Add(_tkeep.wrap);

     /* Field: PM Control */
    init_input_field_win(_pmctl, string("Guest PM Control"), string("PM Control host side executable binary"));
    _form->Add(_pmctl.wrap);

     /* Field: Extra */
    init_input_field_win(_extra, string("Extra"), string("Extra command that emulator can acccept"));
    _form->Add(_extra.wrap);

     _form_render = Renderer(_form, [&]{
        Element _eform = vbox(_form->Render()) | frame | vscroll_indicator;
        return _eform;
    });   
#if 0
    /* Form components */
    _form = Container::Vertical({
        _name.wrap,
        _flashfiles.wrap,
        _emulator.wrap,
        _memory.wrap,
        _vcpu.wrap,
        _cdisk,
        _cfirm,
        _cvgpu,
        _adb_port.wrap,
        _fastboot_port.wrap,
        _cswtpm,
        _crpmb,
    });
#endif
}

void civ_tui::initialize_buttons(void)
{
#if 1
    save_on = [&]() {
        _screen.ExitLoopClosure();
        _status_bar += "called save_on!";
        _screen.Clear();
    };
#endif

    _save = Button("SAVE", [&]{save_on();});
    _exit = Button("EXIT", _screen.ExitLoopClosure());

    _buttons = Container::Horizontal({
        _save, _exit
    });
}

void civ_tui::initialize_ui(void)
{
    initialize_form();
    initialize_buttons();

    _layout = Container::Vertical({
        _form_render,
        _buttons,
    });

    auto layout_render = Renderer(_layout, [&] {
        return vbox({
            text("Celadon in VM Configuration"),
            vbox({_form_render->Render() | vscroll_indicator }) | border | vscroll_indicator ,
            text(_status_bar),
            _buttons->Render() | center,
        }) | border | size(WIDTH, GREATER_THAN, LAYOUT_MIN_WIDTH) | size(HEIGHT, LESS_THAN, 80) | center;
    });

    _screen.Loop(layout_render);
}
#if 0
civ_tui::civ_tui() : _screen(ftxui::ScreenInteractive::TerminalOutput()) {
#if 0
    save_on = [&]() {
        _screen.ExitLoopClosure();
        _status_bar += "called save_on!";
        _screen.Clear();
    };
#endif
}
#endif

