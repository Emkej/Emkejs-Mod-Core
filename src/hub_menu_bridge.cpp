#include "hub_menu_bridge.h"

#include "hub_commit.h"
#include "hub_registry.h"
#include "hub_ui.h"

#include <Debug.h>

#include <core/Functions.h>
#include <kenshi/InputHandler.h>
#include <kenshi/TitleScreen.h>
#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_EditBox.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_TabControl.h>
#include <mygui/MyGUI_TabItem.h>
#include <mygui/MyGUI_TextBox.h>
#include <mygui/MyGUI_Widget.h>
#include <ois/OISKeyboard.h>

#include <Windows.h>

#include <cstring>
#include <sstream>
#include <string>
#include <vector>

class ToolTip;
class ForgottenGUI;
class DatapanelGUI;

class OptionsWindow : public GUIWindow, public wraps::BaseLayout
{
public:
    char _0xd0;
    lektor<std::string> _0xd8;
    int _0xf0;
    void* _0xf8;
    DatapanelGUI* datapanel;
    MyGUI::TabControl* optionsTab;
    bool _0x110;
    ToolTip* tooltip;
    bool _0x120;
};

class DatapanelGUI : public GUIWindow
{
public:
    virtual ~DatapanelGUI() {}
    virtual void vfunc0x68() {}
    virtual void vfunc0x70() {}
    virtual void vfunc0x78() {}
    virtual void vfunc0x80() {}
    virtual void vfunc0x88() {}
    virtual void vfunc0x90() {}
    virtual void vfunc0x98() {}
    virtual void vfunc0xa0() {}
    virtual void vfunc0xa8() {}
    virtual void vfunc0xb0() {}
    virtual void vfunc0xb8() {}
    virtual void vfunc0xc0(int) {}
    virtual void vfunc0xc8() {}
    virtual void vfunc0xd0() {}
    virtual void vfunc0xd8() {}
    virtual void vfunc0xe0(float) {}
    virtual void vfunc0xe8() {}
    virtual void vfunc0xf0() {}
};

typedef DatapanelGUI* (*FnCreateDatapanel)(ForgottenGUI*, const std::string&, MyGUI::Widget*, bool);
typedef void (*FnOptionsInit)(OptionsWindow*);
typedef void (*FnOptionsSave)(OptionsWindow*);

namespace
{
const char* kHubTabName = "Mod Hub";
const char* kHubPanelName = "emc_mod_hub_options";
const int kHubPanelLineId = 0x454d43;
const char* kNamespaceButtonSkin = "Kenshi_Button1";
const char* kHeaderButtonSkin = "Kenshi_Button1";
const char* kValueButtonSkin = "Kenshi_Button1";
const char* kTextSkin = "Kenshi_TextboxStandardText";
const char* kNoMatchesText = "No matches in this tab. Try checking other tabs?";

bool g_hub_enabled = true;
bool g_hooks_installed = false;

FnCreateDatapanel g_fnCreateDatapanel = 0;
FnOptionsInit g_fnOptionsInit = 0;
FnOptionsSave g_fnOptionsSave = 0;
FnOptionsInit g_fnOptionsInitOrig = 0;
FnOptionsSave g_fnOptionsSaveOrig = 0;
ForgottenGUI* g_ptrKenshiGUI = 0;

void (*InputHandler_keyDownEvent_orig)(InputHandler*, OIS::KeyCode) = 0;

OptionsWindow* g_active_options_window = 0;
DatapanelGUI* g_active_hub_panel = 0;
MyGUI::Widget* g_active_hub_panel_widget = 0;
std::string g_selected_namespace_id;
std::vector<MyGUI::Widget*> g_dynamic_widgets;

struct RenderModGroup
{
    std::string mod_id;
    std::string mod_display_name;
    bool collapsed;
    std::vector<HubUiRowView> rows;
};

struct RenderNamespaceGroup
{
    std::string namespace_id;
    std::string namespace_display_name;
    std::vector<RenderModGroup> mods;
};

bool InitPluginMenuFunctions(unsigned int platform, const std::string& version, uintptr_t base_addr)
{
    if (platform == 1u)
    {
        if (version == "1.0.65")
        {
            g_fnOptionsInit = reinterpret_cast<FnOptionsInit>(base_addr + 0x003F0120);
            g_fnOptionsSave = reinterpret_cast<FnOptionsSave>(base_addr + 0x003EC950);
            g_fnCreateDatapanel = reinterpret_cast<FnCreateDatapanel>(base_addr + 0x0073F4B0);
            g_ptrKenshiGUI = reinterpret_cast<ForgottenGUI*>(base_addr + 0x02132750);
            return true;
        }
        if (version == "1.0.68")
        {
            g_fnOptionsInit = reinterpret_cast<FnOptionsInit>(base_addr + 0x003F0260);
            g_fnOptionsSave = reinterpret_cast<FnOptionsSave>(base_addr + 0x003ECA90);
            g_fnCreateDatapanel = reinterpret_cast<FnCreateDatapanel>(base_addr + 0x0073FFE0);
            g_ptrKenshiGUI = reinterpret_cast<ForgottenGUI*>(base_addr + 0x021337B0);
            return true;
        }
    }
    else if (platform == 0u)
    {
        if (version == "1.0.65")
        {
            g_fnOptionsInit = reinterpret_cast<FnOptionsInit>(base_addr + 0x003EFD40);
            g_fnOptionsSave = reinterpret_cast<FnOptionsSave>(base_addr + 0x003EC570);
            g_fnCreateDatapanel = reinterpret_cast<FnCreateDatapanel>(base_addr + 0x0073EE10);
            g_ptrKenshiGUI = reinterpret_cast<ForgottenGUI*>(base_addr + 0x021306C0);
            return true;
        }
        if (version == "1.0.68")
        {
            g_fnOptionsInit = reinterpret_cast<FnOptionsInit>(base_addr + 0x003EFC00);
            g_fnOptionsSave = reinterpret_cast<FnOptionsSave>(base_addr + 0x003EC430);
            g_fnCreateDatapanel = reinterpret_cast<FnCreateDatapanel>(base_addr + 0x0073F980);
            g_ptrKenshiGUI = reinterpret_cast<ForgottenGUI*>(base_addr + 0x021326E0);
            return true;
        }
    }

    return false;
}

void DestroyDynamicWidgets()
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (gui == 0)
    {
        g_dynamic_widgets.clear();
        return;
    }

    for (size_t index = g_dynamic_widgets.size(); index > 0; --index)
    {
        MyGUI::Widget* widget = g_dynamic_widgets[index - 1];
        if (widget != 0)
        {
            gui->destroyWidget(widget);
        }
    }

    g_dynamic_widgets.clear();
}

template <typename TWidget>
TWidget* CreateTrackedWidget(MyGUI::Widget* parent, const char* skin, const MyGUI::IntCoord& coord)
{
    if (parent == 0)
    {
        return 0;
    }

    TWidget* widget = parent->createWidget<TWidget>(skin, coord, MyGUI::Align::Default);
    if (widget != 0)
    {
        g_dynamic_widgets.push_back(widget);
    }
    return widget;
}

MyGUI::EditBox* CreateTrackedSearchBox(MyGUI::Widget* parent, const MyGUI::IntCoord& coord)
{
    if (parent == 0)
    {
        return 0;
    }

    const char* skins[] = { "EditBox", "EditBoxEmpty", "EditBoxLE", "EditBoxLEEmpty", "Kenshi_EditBox" };
    for (size_t index = 0; index < sizeof(skins) / sizeof(skins[0]); ++index)
    {
        try
        {
            MyGUI::EditBox* widget = parent->createWidget<MyGUI::EditBox>(skins[index], coord, MyGUI::Align::Default);
            if (widget != 0)
            {
                g_dynamic_widgets.push_back(widget);
                return widget;
            }
        }
        catch (...)
        {
        }
    }

    return 0;
}

void AttachHubAction(MyGUI::Widget* widget, const char* action, const std::string& namespace_id, const std::string& mod_id, const std::string& setting_id)
{
    if (widget == 0)
    {
        return;
    }

    widget->setUserString("emc_action", action != 0 ? action : "");
    widget->setUserString("emc_ns", namespace_id);
    widget->setUserString("emc_mod", mod_id);
    widget->setUserString("emc_setting", setting_id);
}

void AttachHubIdentity(MyGUI::Widget* widget, const std::string& namespace_id, const std::string& mod_id, const std::string& setting_id)
{
    if (widget == 0)
    {
        return;
    }

    widget->setUserString("emc_ns", namespace_id);
    widget->setUserString("emc_mod", mod_id);
    widget->setUserString("emc_setting", setting_id);
}

void FindOrAddRenderNamespace(std::vector<RenderNamespaceGroup>* namespaces, const HubUiRowView& row, RenderNamespaceGroup** out_namespace)
{
    if (namespaces == 0 || out_namespace == 0)
    {
        return;
    }

    for (size_t index = 0; index < namespaces->size(); ++index)
    {
        RenderNamespaceGroup* existing = &(*namespaces)[index];
        if (existing->namespace_id == row.namespace_id)
        {
            *out_namespace = existing;
            return;
        }
    }

    RenderNamespaceGroup new_namespace;
    new_namespace.namespace_id = row.namespace_id != 0 ? row.namespace_id : "";
    new_namespace.namespace_display_name = row.namespace_display_name != 0 ? row.namespace_display_name : new_namespace.namespace_id;
    namespaces->push_back(new_namespace);
    *out_namespace = &namespaces->back();
}

void FindOrAddRenderMod(RenderNamespaceGroup* namespace_group, const HubUiRowView& row, RenderModGroup** out_mod)
{
    if (namespace_group == 0 || out_mod == 0)
    {
        return;
    }

    for (size_t index = 0; index < namespace_group->mods.size(); ++index)
    {
        RenderModGroup* existing = &namespace_group->mods[index];
        if (existing->mod_id == row.mod_id)
        {
            *out_mod = existing;
            return;
        }
    }

    RenderModGroup new_mod;
    new_mod.mod_id = row.mod_id != 0 ? row.mod_id : "";
    new_mod.mod_display_name = row.mod_display_name != 0 ? row.mod_display_name : new_mod.mod_id;
    new_mod.collapsed = false;
    HubUi_GetModCollapsed(row.namespace_id, row.mod_id, &new_mod.collapsed);
    namespace_group->mods.push_back(new_mod);
    *out_mod = &namespace_group->mods.back();
}

void BuildRenderNamespaces(std::vector<RenderNamespaceGroup>* out_namespaces)
{
    if (out_namespaces == 0)
    {
        return;
    }

    out_namespaces->clear();
    const uint32_t row_count = HubUi_GetRowCount();
    for (uint32_t row_index = 0; row_index < row_count; ++row_index)
    {
        HubUiRowView row;
        if (!HubUi_GetRowViewByIndex(row_index, &row))
        {
            continue;
        }

        RenderNamespaceGroup* namespace_group = 0;
        FindOrAddRenderNamespace(out_namespaces, row, &namespace_group);
        if (namespace_group == 0)
        {
            continue;
        }

        RenderModGroup* mod_group = 0;
        FindOrAddRenderMod(namespace_group, row, &mod_group);
        if (mod_group == 0)
        {
            continue;
        }

        mod_group->rows.push_back(row);
    }
}

std::string FormatBoolButtonCaption(const HubUiRowView& row)
{
    if (row.pending_bool_value == 0)
    {
        return "Off";
    }

    return "On";
}

std::string FormatKeybindButtonCaption(const HubUiRowView& row)
{
    if (row.capture_active)
    {
        return "Press key...";
    }

    if (row.pending_keybind_value.keycode == EMC_KEY_UNBOUND)
    {
        return "Unbound";
    }

    std::ostringstream caption;
    caption << "Key " << row.pending_keybind_value.keycode;
    if (row.pending_keybind_value.modifiers != 0u)
    {
        caption << " +" << row.pending_keybind_value.modifiers;
    }
    return caption.str();
}

bool TryFindRowViewById(const std::string& namespace_id, const std::string& mod_id, const std::string& setting_id, HubUiRowView* out_row)
{
    if (out_row == 0)
    {
        return false;
    }

    const uint32_t row_count = HubUi_GetRowCount();
    for (uint32_t row_index = 0; row_index < row_count; ++row_index)
    {
        HubUiRowView row;
        if (!HubUi_GetRowViewByIndex(row_index, &row))
        {
            continue;
        }

        if (namespace_id == (row.namespace_id != 0 ? row.namespace_id : "")
            && mod_id == (row.mod_id != 0 ? row.mod_id : "")
            && setting_id == (row.setting_id != 0 ? row.setting_id : ""))
        {
            *out_row = row;
            return true;
        }
    }

    return false;
}

void EnsureSelectedNamespace(const std::vector<RenderNamespaceGroup>& namespaces)
{
    if (namespaces.empty())
    {
        g_selected_namespace_id.clear();
        return;
    }

    for (size_t index = 0; index < namespaces.size(); ++index)
    {
        if (namespaces[index].namespace_id == g_selected_namespace_id)
        {
            return;
        }
    }

    g_selected_namespace_id = namespaces[0].namespace_id;
}

void RebuildHubPanelWidgets();

void OnHubSearchTextChanged(MyGUI::EditBox* sender)
{
    if (sender == 0)
    {
        return;
    }

    const std::string namespace_id = sender->getUserString("emc_ns");
    if (namespace_id.empty())
    {
        return;
    }

    const std::string query = sender->getOnlyText().asUTF8();
    const char* existing_query = "";
    if (HubUi_GetNamespaceSearchQuery(namespace_id.c_str(), &existing_query)
        && existing_query != 0
        && query == existing_query)
    {
        return;
    }

    if (HubUi_SetNamespaceSearchQuery(namespace_id.c_str(), query.c_str()) == EMC_OK)
    {
        RebuildHubPanelWidgets();
    }
}

void OnHubIntTextChanged(MyGUI::EditBox* sender)
{
    if (sender == 0)
    {
        return;
    }

    const std::string namespace_id = sender->getUserString("emc_ns");
    const std::string mod_id = sender->getUserString("emc_mod");
    const std::string setting_id = sender->getUserString("emc_setting");
    if (namespace_id.empty() || mod_id.empty() || setting_id.empty())
    {
        return;
    }

    const std::string text = sender->getOnlyText().asUTF8();

    HubUiRowView row;
    if (TryFindRowViewById(namespace_id, mod_id, setting_id, &row)
        && row.pending_int_text != 0
        && text == row.pending_int_text)
    {
        return;
    }

    if (HubUi_SetPendingIntFromText(namespace_id.c_str(), mod_id.c_str(), setting_id.c_str(), text.c_str()) == EMC_OK)
    {
        RebuildHubPanelWidgets();
    }
}

void OnHubFloatTextChanged(MyGUI::EditBox* sender)
{
    if (sender == 0)
    {
        return;
    }

    const std::string namespace_id = sender->getUserString("emc_ns");
    const std::string mod_id = sender->getUserString("emc_mod");
    const std::string setting_id = sender->getUserString("emc_setting");
    if (namespace_id.empty() || mod_id.empty() || setting_id.empty())
    {
        return;
    }

    const std::string text = sender->getOnlyText().asUTF8();

    HubUiRowView row;
    if (TryFindRowViewById(namespace_id, mod_id, setting_id, &row)
        && row.pending_float_text != 0
        && text == row.pending_float_text)
    {
        return;
    }

    if (HubUi_SetPendingFloatFromText(namespace_id.c_str(), mod_id.c_str(), setting_id.c_str(), text.c_str()) == EMC_OK)
    {
        RebuildHubPanelWidgets();
    }
}

void OnHubButtonClicked(MyGUI::Widget* sender)
{
    if (sender == 0)
    {
        return;
    }

    const std::string action = sender->getUserString("emc_action");
    const std::string namespace_id = sender->getUserString("emc_ns");
    const std::string mod_id = sender->getUserString("emc_mod");
    const std::string setting_id = sender->getUserString("emc_setting");

    if (action == "namespace_select")
    {
        g_selected_namespace_id = namespace_id;
        RebuildHubPanelWidgets();
        return;
    }

    if (action == "mod_toggle")
    {
        bool collapsed = false;
        if (HubUi_GetModCollapsed(namespace_id.c_str(), mod_id.c_str(), &collapsed))
        {
            HubUi_SetModCollapsed(namespace_id.c_str(), mod_id.c_str(), !collapsed);
        }
        RebuildHubPanelWidgets();
        return;
    }

    if (action == "bool_toggle")
    {
        HubUiRowView row;
        if (TryFindRowViewById(namespace_id, mod_id, setting_id, &row))
        {
            const int32_t next_value = row.pending_bool_value == 0 ? 1 : 0;
            HubUi_SetPendingBool(namespace_id.c_str(), mod_id.c_str(), setting_id.c_str(), next_value);
        }
        RebuildHubPanelWidgets();
        return;
    }

    if (action == "keybind_bind")
    {
        HubUi_BeginKeybindCapture(namespace_id.c_str(), mod_id.c_str(), setting_id.c_str());
        RebuildHubPanelWidgets();
        return;
    }

    if (action == "keybind_clear")
    {
        HubUi_ClearPendingKeybind(namespace_id.c_str(), mod_id.c_str(), setting_id.c_str());
        RebuildHubPanelWidgets();
        return;
    }

    if (action == "int_step_dec")
    {
        HubUi_AdjustPendingIntStep(namespace_id.c_str(), mod_id.c_str(), setting_id.c_str(), -1);
        RebuildHubPanelWidgets();
        return;
    }

    if (action == "int_step_inc")
    {
        HubUi_AdjustPendingIntStep(namespace_id.c_str(), mod_id.c_str(), setting_id.c_str(), 1);
        RebuildHubPanelWidgets();
        return;
    }

    if (action == "float_step_dec")
    {
        HubUi_AdjustPendingFloatStep(namespace_id.c_str(), mod_id.c_str(), setting_id.c_str(), -1);
        RebuildHubPanelWidgets();
        return;
    }

    if (action == "float_step_inc")
    {
        HubUi_AdjustPendingFloatStep(namespace_id.c_str(), mod_id.c_str(), setting_id.c_str(), 1);
        RebuildHubPanelWidgets();
        return;
    }

    if (action == "action_invoke")
    {
        HubUi_InvokeActionRow(namespace_id.c_str(), mod_id.c_str(), setting_id.c_str());
        RebuildHubPanelWidgets();
    }
}

void CreateInlineErrorLabel(MyGUI::Widget* parent, int x, int y, int width, const char* message)
{
    if (parent == 0 || message == 0 || message[0] == '\0')
    {
        return;
    }

    MyGUI::TextBox* error_text = CreateTrackedWidget<MyGUI::TextBox>(
        parent,
        kTextSkin,
        MyGUI::IntCoord(x, y, width, 18));
    if (error_text == 0)
    {
        return;
    }

    error_text->setCaption(message);
    error_text->setTextColour(MyGUI::Colour(1.0f, 0.3f, 0.3f, 1.0f));
}

void CreateRowWidgets(MyGUI::Widget* parent, int panel_width, int y, const HubUiRowView& row, int* out_next_y)
{
    if (parent == 0 || out_next_y == 0)
    {
        return;
    }

    const int label_x = 34;
    const int label_width = panel_width - 290;
    const int value_x = panel_width - 240;
    const int row_height = 34;

    MyGUI::TextBox* label = CreateTrackedWidget<MyGUI::TextBox>(
        parent,
        kTextSkin,
        MyGUI::IntCoord(label_x, y + 6, label_width, 22));
    if (label != 0)
    {
        label->setCaption(row.label != 0 ? row.label : row.setting_id);
    }

    if (row.kind == HUB_UI_ROW_KIND_BOOL)
    {
        MyGUI::Button* button = CreateTrackedWidget<MyGUI::Button>(
            parent,
            kValueButtonSkin,
            MyGUI::IntCoord(value_x, y, 200, 30));
        if (button != 0)
        {
            button->setCaption(FormatBoolButtonCaption(row));
            button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
            AttachHubAction(button, "bool_toggle", row.namespace_id != 0 ? row.namespace_id : "", row.mod_id != 0 ? row.mod_id : "", row.setting_id != 0 ? row.setting_id : "");
        }
    }
    else if (row.kind == HUB_UI_ROW_KIND_KEYBIND)
    {
        MyGUI::Button* bind_button = CreateTrackedWidget<MyGUI::Button>(
            parent,
            kValueButtonSkin,
            MyGUI::IntCoord(value_x - 70, y, 180, 30));
        if (bind_button != 0)
        {
            bind_button->setCaption(FormatKeybindButtonCaption(row));
            bind_button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
            AttachHubAction(bind_button, "keybind_bind", row.namespace_id != 0 ? row.namespace_id : "", row.mod_id != 0 ? row.mod_id : "", row.setting_id != 0 ? row.setting_id : "");
        }

        MyGUI::Button* clear_button = CreateTrackedWidget<MyGUI::Button>(
            parent,
            kValueButtonSkin,
            MyGUI::IntCoord(value_x + 120, y, 80, 30));
        if (clear_button != 0)
        {
            clear_button->setCaption("Clear");
            clear_button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
            AttachHubAction(clear_button, "keybind_clear", row.namespace_id != 0 ? row.namespace_id : "", row.mod_id != 0 ? row.mod_id : "", row.setting_id != 0 ? row.setting_id : "");
        }
    }
    else if (row.kind == HUB_UI_ROW_KIND_INT)
    {
        MyGUI::Button* minus_button = CreateTrackedWidget<MyGUI::Button>(
            parent,
            kValueButtonSkin,
            MyGUI::IntCoord(value_x - 70, y, 40, 30));
        if (minus_button != 0)
        {
            minus_button->setCaption("-");
            minus_button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
            AttachHubAction(minus_button, "int_step_dec", row.namespace_id != 0 ? row.namespace_id : "", row.mod_id != 0 ? row.mod_id : "", row.setting_id != 0 ? row.setting_id : "");
        }

        MyGUI::EditBox* value_box = CreateTrackedSearchBox(
            parent,
            MyGUI::IntCoord(value_x - 22, y, 120, 30));
        if (value_box != 0)
        {
            value_box->setEditMultiLine(false);
            value_box->setOnlyText(row.pending_int_text != 0 ? row.pending_int_text : "");
            AttachHubIdentity(value_box, row.namespace_id != 0 ? row.namespace_id : "", row.mod_id != 0 ? row.mod_id : "", row.setting_id != 0 ? row.setting_id : "");
            value_box->eventEditTextChange += MyGUI::newDelegate(&OnHubIntTextChanged);
        }

        MyGUI::Button* plus_button = CreateTrackedWidget<MyGUI::Button>(
            parent,
            kValueButtonSkin,
            MyGUI::IntCoord(value_x + 106, y, 40, 30));
        if (plus_button != 0)
        {
            plus_button->setCaption("+");
            plus_button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
            AttachHubAction(plus_button, "int_step_inc", row.namespace_id != 0 ? row.namespace_id : "", row.mod_id != 0 ? row.mod_id : "", row.setting_id != 0 ? row.setting_id : "");
        }
    }
    else if (row.kind == HUB_UI_ROW_KIND_FLOAT)
    {
        MyGUI::Button* minus_button = CreateTrackedWidget<MyGUI::Button>(
            parent,
            kValueButtonSkin,
            MyGUI::IntCoord(value_x - 70, y, 40, 30));
        if (minus_button != 0)
        {
            minus_button->setCaption("-");
            minus_button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
            AttachHubAction(minus_button, "float_step_dec", row.namespace_id != 0 ? row.namespace_id : "", row.mod_id != 0 ? row.mod_id : "", row.setting_id != 0 ? row.setting_id : "");
        }

        MyGUI::EditBox* value_box = CreateTrackedSearchBox(
            parent,
            MyGUI::IntCoord(value_x - 22, y, 120, 30));
        if (value_box != 0)
        {
            value_box->setEditMultiLine(false);
            value_box->setOnlyText(row.pending_float_text != 0 ? row.pending_float_text : "");
            AttachHubIdentity(value_box, row.namespace_id != 0 ? row.namespace_id : "", row.mod_id != 0 ? row.mod_id : "", row.setting_id != 0 ? row.setting_id : "");
            value_box->eventEditTextChange += MyGUI::newDelegate(&OnHubFloatTextChanged);
        }

        MyGUI::Button* plus_button = CreateTrackedWidget<MyGUI::Button>(
            parent,
            kValueButtonSkin,
            MyGUI::IntCoord(value_x + 106, y, 40, 30));
        if (plus_button != 0)
        {
            plus_button->setCaption("+");
            plus_button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
            AttachHubAction(plus_button, "float_step_inc", row.namespace_id != 0 ? row.namespace_id : "", row.mod_id != 0 ? row.mod_id : "", row.setting_id != 0 ? row.setting_id : "");
        }
    }
    else if (row.kind == HUB_UI_ROW_KIND_ACTION)
    {
        MyGUI::Button* action_button = CreateTrackedWidget<MyGUI::Button>(
            parent,
            kValueButtonSkin,
            MyGUI::IntCoord(value_x + 40, y, 160, 30));
        if (action_button != 0)
        {
            action_button->setCaption("Run");
            action_button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
            AttachHubAction(action_button, "action_invoke", row.namespace_id != 0 ? row.namespace_id : "", row.mod_id != 0 ? row.mod_id : "", row.setting_id != 0 ? row.setting_id : "");
        }
    }

    int next_y = y + row_height;
    if (row.inline_error != 0 && row.inline_error[0] != '\0' && std::strcmp(row.inline_error, "none") != 0)
    {
        CreateInlineErrorLabel(parent, label_x + 12, next_y, panel_width - 80, row.inline_error);
        next_y += 20;
    }

    *out_next_y = next_y;
}

void BuildFilteredModsForNamespace(const RenderNamespaceGroup& namespace_group, std::vector<RenderModGroup>* out_mods)
{
    if (out_mods == 0)
    {
        return;
    }

    out_mods->clear();
    for (size_t mod_index = 0; mod_index < namespace_group.mods.size(); ++mod_index)
    {
        const RenderModGroup& mod_group = namespace_group.mods[mod_index];
        RenderModGroup filtered_mod = mod_group;
        filtered_mod.rows.clear();

        for (size_t row_index = 0; row_index < mod_group.rows.size(); ++row_index)
        {
            const HubUiRowView& row = mod_group.rows[row_index];
            if (HubUi_DoesRowMatchNamespaceSearch(&row))
            {
                filtered_mod.rows.push_back(row);
            }
        }

        if (!filtered_mod.rows.empty())
        {
            out_mods->push_back(filtered_mod);
        }
    }
}

void RebuildHubPanelWidgets()
{
    if (g_active_hub_panel_widget == 0)
    {
        return;
    }

    DestroyDynamicWidgets();

    std::vector<RenderNamespaceGroup> namespaces;
    BuildRenderNamespaces(&namespaces);
    EnsureSelectedNamespace(namespaces);

    const int panel_width = g_active_hub_panel_widget->getWidth();
    int y = 12;

    if (namespaces.empty())
    {
        MyGUI::TextBox* empty_text = CreateTrackedWidget<MyGUI::TextBox>(
            g_active_hub_panel_widget,
            kTextSkin,
            MyGUI::IntCoord(24, y, panel_width - 48, 28));
        if (empty_text != 0)
        {
            empty_text->setCaption("No registered Mod Hub settings.");
        }
        return;
    }

    int namespace_x = 20;
    for (size_t index = 0; index < namespaces.size(); ++index)
    {
        const RenderNamespaceGroup& namespace_group = namespaces[index];
        MyGUI::Button* namespace_button = CreateTrackedWidget<MyGUI::Button>(
            g_active_hub_panel_widget,
            kNamespaceButtonSkin,
            MyGUI::IntCoord(namespace_x, y, 170, 30));
        if (namespace_button != 0)
        {
            std::string caption = namespace_group.namespace_display_name;
            if (namespace_group.namespace_id == g_selected_namespace_id)
            {
                caption = "[" + caption + "]";
            }
            namespace_button->setCaption(caption);
            namespace_button->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
            AttachHubAction(namespace_button, "namespace_select", namespace_group.namespace_id, "", "");
        }

        namespace_x += 178;
    }

    y += 44;

    const RenderNamespaceGroup* selected_namespace = 0;
    for (size_t index = 0; index < namespaces.size(); ++index)
    {
        if (namespaces[index].namespace_id == g_selected_namespace_id)
        {
            selected_namespace = &namespaces[index];
            break;
        }
    }

    if (selected_namespace == 0)
    {
        selected_namespace = &namespaces[0];
    }

    const char* search_query = "";
    HubUi_GetNamespaceSearchQuery(selected_namespace->namespace_id.c_str(), &search_query);

    MyGUI::TextBox* search_label = CreateTrackedWidget<MyGUI::TextBox>(
        g_active_hub_panel_widget,
        kTextSkin,
        MyGUI::IntCoord(24, y + 6, 70, 24));
    if (search_label != 0)
    {
        search_label->setCaption("Search:");
    }

    MyGUI::EditBox* search_box = CreateTrackedSearchBox(
        g_active_hub_panel_widget,
        MyGUI::IntCoord(96, y, panel_width - 120, 30));
    if (search_box != 0)
    {
        search_box->setEditMultiLine(false);
        search_box->setOnlyText(search_query != 0 ? search_query : "");
        search_box->setUserString("emc_ns", selected_namespace->namespace_id);
        search_box->eventEditTextChange += MyGUI::newDelegate(&OnHubSearchTextChanged);
    }

    y += 42;

    std::vector<RenderModGroup> filtered_mods;
    BuildFilteredModsForNamespace(*selected_namespace, &filtered_mods);
    if (filtered_mods.empty())
    {
        MyGUI::TextBox* no_matches_text = CreateTrackedWidget<MyGUI::TextBox>(
            g_active_hub_panel_widget,
            kTextSkin,
            MyGUI::IntCoord(24, y, panel_width - 48, 28));
        if (no_matches_text != 0)
        {
            no_matches_text->setCaption(kNoMatchesText);
        }
        return;
    }

    for (size_t mod_index = 0; mod_index < filtered_mods.size(); ++mod_index)
    {
        const RenderModGroup& mod_group = filtered_mods[mod_index];

        MyGUI::Button* mod_header = CreateTrackedWidget<MyGUI::Button>(
            g_active_hub_panel_widget,
            kHeaderButtonSkin,
            MyGUI::IntCoord(20, y, panel_width - 40, 32));
        if (mod_header != 0)
        {
            const std::string prefix = mod_group.collapsed ? "[+] " : "[-] ";
            mod_header->setCaption(prefix + mod_group.mod_display_name);
            mod_header->eventMouseButtonClick += MyGUI::newDelegate(&OnHubButtonClicked);
            AttachHubAction(mod_header, "mod_toggle", selected_namespace->namespace_id, mod_group.mod_id, "");
        }

        y += 38;

        if (mod_group.collapsed)
        {
            continue;
        }

        for (size_t row_index = 0; row_index < mod_group.rows.size(); ++row_index)
        {
            int next_y = y;
            CreateRowWidgets(g_active_hub_panel_widget, panel_width, y, mod_group.rows[row_index], &next_y);
            y = next_y;
        }

        y += 8;
    }
}

void ClearActiveUiState()
{
    DestroyDynamicWidgets();
    g_selected_namespace_id.clear();
    g_active_hub_panel_widget = 0;
    g_active_hub_panel = 0;
    g_active_options_window = 0;
}

bool EnsureHubPanel(OptionsWindow* self)
{
    if (self == 0 || self->optionsTab == 0 || g_ptrKenshiGUI == 0 || g_fnCreateDatapanel == 0)
    {
        return false;
    }

    if (self->optionsTab->findItemWith(kHubTabName))
    {
        return g_active_hub_panel_widget != 0;
    }

    MyGUI::TabItem* hub_tab = self->optionsTab->addItem(kHubTabName);
    if (hub_tab == 0)
    {
        ErrorLog("Emkejs-Mod-Core: failed to add Mod Hub tab item");
        return false;
    }

    DatapanelGUI* hub_panel = g_fnCreateDatapanel(g_ptrKenshiGUI, kHubPanelName, hub_tab, false);
    if (hub_panel == 0)
    {
        ErrorLog("Emkejs-Mod-Core: failed to create Mod Hub datapanel");
        return false;
    }

    hub_panel->vfunc0xc0(kHubPanelLineId);
    hub_panel->vfunc0xe0(25.0f);

    g_active_options_window = self;
    g_active_hub_panel = hub_panel;
    g_active_hub_panel_widget = hub_panel->getWidget();
    if (g_active_hub_panel_widget == 0)
    {
        ErrorLog("Emkejs-Mod-Core: Mod Hub panel widget is null");
        return false;
    }

    hub_tab->setVisible(false);
    self->optionsTab->setItemData(hub_tab, hub_panel);
    return true;
}

void OptionsWindowInitHook(OptionsWindow* self)
{
    if (g_fnOptionsInitOrig != 0)
    {
        g_fnOptionsInitOrig(self);
    }

    HubMenuBridge_OnOptionsWindowInit();
    if (!g_hub_enabled)
    {
        return;
    }

    if (!EnsureHubPanel(self))
    {
        return;
    }

    RebuildHubPanelWidgets();
}

void OptionsWindowSaveHook(OptionsWindow* self)
{
    if (g_fnOptionsSaveOrig != 0)
    {
        g_fnOptionsSaveOrig(self);
    }

    HubMenuBridge_OnOptionsWindowSave();
    HubMenuBridge_OnOptionsWindowClose();
    ClearActiveUiState();
}

void InputHandler_keyDownEvent_hook(InputHandler* thisptr, OIS::KeyCode key_code)
{
    if (g_hub_enabled && HubUi_IsOptionsWindowOpen() && HubUi_IsAnyKeybindCaptureActive())
    {
        HubUi_ApplyCapturedKeycodeToActiveRow(static_cast<int32_t>(key_code));
        RebuildHubPanelWidgets();
        return;
    }

    if (InputHandler_keyDownEvent_orig != 0)
    {
        InputHandler_keyDownEvent_orig(thisptr, key_code);
    }
}
}

void HubMenuBridge_SetHubEnabled(bool is_enabled)
{
    g_hub_enabled = is_enabled;
    if (!g_hub_enabled)
    {
        HubUi_SetOptionsWindowOpen(false);
        HubUi_ClearSessionModel();
        HubRegistry_SetRegistrationLocked(false);
        ClearActiveUiState();
    }
}

bool HubMenuBridge_IsHubEnabled()
{
    return g_hub_enabled;
}

bool HubMenuBridge_InstallHooks(unsigned int platform, const std::string& version)
{
    if (g_hooks_installed)
    {
        return true;
    }

    const uintptr_t base_addr = reinterpret_cast<uintptr_t>(GetModuleHandleA(0));
    if (!InitPluginMenuFunctions(platform, version, base_addr))
    {
        ErrorLog("Emkejs-Mod-Core: failed to initialize menu function pointers");
        return false;
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(g_fnOptionsInit, OptionsWindowInitHook, &g_fnOptionsInitOrig))
    {
        ErrorLog("Emkejs-Mod-Core: Could not hook options init");
        return false;
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(g_fnOptionsSave, OptionsWindowSaveHook, &g_fnOptionsSaveOrig))
    {
        ErrorLog("Emkejs-Mod-Core: Could not hook options save");
        return false;
    }

    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
        KenshiLib::GetRealAddress(&InputHandler::keyDownEvent),
        InputHandler_keyDownEvent_hook,
        &InputHandler_keyDownEvent_orig))
    {
        ErrorLog("Emkejs-Mod-Core: Could not hook InputHandler::keyDownEvent");
        return false;
    }

    g_hooks_installed = true;
    return true;
}

void HubMenuBridge_OnOptionsWindowInit()
{
    if (!g_hub_enabled)
    {
        HubUi_SetOptionsWindowOpen(false);
        HubUi_ClearSessionModel();
        HubRegistry_SetRegistrationLocked(false);
        return;
    }

    HubUi_SetOptionsWindowOpen(true);
    HubRegistry_SetRegistrationLocked(true);
    HubUi_RebuildSessionModelFromRegistry();
    HubUi_PerformInitialSync();
}

void HubMenuBridge_OnOptionsWindowSave()
{
    if (!g_hub_enabled || !HubUi_IsOptionsWindowOpen())
    {
        return;
    }

    HubCommit_RunOptionsSave();
}

void HubMenuBridge_OnOptionsWindowClose()
{
    HubUi_SetOptionsWindowOpen(false);
    HubUi_ClearSessionModel();
    HubRegistry_SetRegistrationLocked(false);
}
