#include "hub_ui.h"

#include "hub_registry.h"

#include <Debug.h>

#include <ois/OISKeyboard.h>

#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
const char* kLogNone = "none";
const char* kUnavailableMessage = "Unavailable";
const char* kActionFailedMessage = "action_callback_failed";

struct HubUiModSection;

struct HubUiSettingRow
{
    int32_t kind;
    std::string namespace_id;
    std::string namespace_display_name;
    std::string mod_id;
    std::string mod_display_name;
    std::string setting_id;
    std::string label;
    std::string description;
    void* user_data;

    EMC_GetBoolCallback get_bool;
    EMC_SetBoolCallback set_bool;
    int32_t canonical_bool_value;
    int32_t pending_bool_value;

    EMC_GetKeybindCallback get_keybind;
    EMC_SetKeybindCallback set_keybind;
    EMC_KeybindValueV1 canonical_keybind_value;
    EMC_KeybindValueV1 pending_keybind_value;

    EMC_ActionRowCallback on_action;
    uint32_t action_flags;

    bool dirty;
    bool capture_active;
    std::string inline_error;
    HubUiModSection* parent_mod;
};

struct HubUiModSection
{
    std::string namespace_id;
    std::string mod_id;
    std::string mod_display_name;
    bool collapsed;
    std::vector<HubUiSettingRow*> rows;
};

struct HubUiNamespaceTab
{
    std::string namespace_id;
    std::string namespace_display_name;
    std::string search_query;
    std::vector<HubUiModSection*> mods;
};

bool g_options_window_open = false;
std::vector<HubUiNamespaceTab*> g_tabs_in_order;
std::vector<HubUiSettingRow*> g_rows_in_order;

const char* SafeLogValue(const char* value)
{
    if (value == nullptr || value[0] == '\0')
    {
        return kLogNone;
    }

    return value;
}

bool IsBoolValueValid(int32_t value)
{
    return value == 0 || value == 1;
}

bool KeybindEquals(EMC_KeybindValueV1 lhs, EMC_KeybindValueV1 rhs)
{
    return lhs.keycode == rhs.keycode && lhs.modifiers == rhs.modifiers;
}

void LogUiGetFailure(const HubUiSettingRow* row, EMC_Result result, const char* message)
{
    std::ostringstream line;
    line << "event=hub_ui_get_failure"
         << " namespace=" << SafeLogValue(row->namespace_id.c_str())
         << " mod=" << SafeLogValue(row->mod_id.c_str())
         << " setting=" << SafeLogValue(row->setting_id.c_str())
         << " result=" << result
         << " message=" << SafeLogValue(message);
    ErrorLog(line.str().c_str());
}

void LogActionFailure(const HubUiSettingRow* row, EMC_Result result, const char* message)
{
    std::ostringstream line;
    line << "event=hub_action_failure"
         << " namespace=" << SafeLogValue(row->namespace_id.c_str())
         << " mod=" << SafeLogValue(row->mod_id.c_str())
         << " setting=" << SafeLogValue(row->setting_id.c_str())
         << " result=" << result
         << " message=" << SafeLogValue(message);
    ErrorLog(line.str().c_str());
}

void LogActionRefreshGetFailure(const HubUiSettingRow* row, EMC_Result result, const char* message)
{
    std::ostringstream line;
    line << "event=hub_action_refresh_get_failure"
         << " namespace=" << SafeLogValue(row->namespace_id.c_str())
         << " mod=" << SafeLogValue(row->mod_id.c_str())
         << " setting=" << SafeLogValue(row->setting_id.c_str())
         << " result=" << result
         << " message=" << SafeLogValue(message);
    ErrorLog(line.str().c_str());
}

const char* ResolveErrorMessage(const char* err_buf, const char* fallback)
{
    if (err_buf != nullptr && err_buf[0] != '\0')
    {
        return err_buf;
    }

    return fallback;
}

bool TryReadBool(HubUiSettingRow* row, int32_t* out_value, EMC_Result* out_result, const char** out_message)
{
    if (row->get_bool == nullptr)
    {
        if (out_result != nullptr)
        {
            *out_result = EMC_ERR_INVALID_ARGUMENT;
        }
        if (out_message != nullptr)
        {
            *out_message = "missing_get_callback";
        }
        return false;
    }

    int32_t value = 0;
    EMC_Result result = row->get_bool(row->user_data, &value);
    if (result != EMC_OK)
    {
        if (out_result != nullptr)
        {
            *out_result = result;
        }
        if (out_message != nullptr)
        {
            *out_message = "get_callback_failed";
        }
        return false;
    }

    if (!IsBoolValueValid(value))
    {
        if (out_result != nullptr)
        {
            *out_result = EMC_ERR_CALLBACK_FAILED;
        }
        if (out_message != nullptr)
        {
            *out_message = "invalid_bool_value";
        }
        return false;
    }

    if (out_value != nullptr)
    {
        *out_value = value;
    }

    return true;
}

bool TryReadKeybind(HubUiSettingRow* row, EMC_KeybindValueV1* out_value, EMC_Result* out_result, const char** out_message)
{
    if (row->get_keybind == nullptr)
    {
        if (out_result != nullptr)
        {
            *out_result = EMC_ERR_INVALID_ARGUMENT;
        }
        if (out_message != nullptr)
        {
            *out_message = "missing_get_callback";
        }
        return false;
    }

    EMC_KeybindValueV1 value;
    value.keycode = EMC_KEY_UNBOUND;
    value.modifiers = 0;
    EMC_Result result = row->get_keybind(row->user_data, &value);
    if (result != EMC_OK)
    {
        if (out_result != nullptr)
        {
            *out_result = result;
        }
        if (out_message != nullptr)
        {
            *out_message = "get_callback_failed";
        }
        return false;
    }

    if (out_value != nullptr)
    {
        *out_value = value;
    }

    return true;
}

void ClearTabsAndRows()
{
    for (size_t tab_index = 0; tab_index < g_tabs_in_order.size(); ++tab_index)
    {
        HubUiNamespaceTab* tab = g_tabs_in_order[tab_index];
        for (size_t mod_index = 0; mod_index < tab->mods.size(); ++mod_index)
        {
            HubUiModSection* mod = tab->mods[mod_index];
            for (size_t row_index = 0; row_index < mod->rows.size(); ++row_index)
            {
                delete mod->rows[row_index];
            }
            mod->rows.clear();
            delete mod;
        }
        tab->mods.clear();
        delete tab;
    }

    g_tabs_in_order.clear();
    g_rows_in_order.clear();
}

HubUiSettingRow* FindRow(const char* namespace_id, const char* mod_id, const char* setting_id)
{
    if (namespace_id == nullptr || mod_id == nullptr || setting_id == nullptr)
    {
        return nullptr;
    }

    for (size_t index = 0; index < g_rows_in_order.size(); ++index)
    {
        HubUiSettingRow* row = g_rows_in_order[index];
        if (row->namespace_id == namespace_id && row->mod_id == mod_id && row->setting_id == setting_id)
        {
            return row;
        }
    }

    return nullptr;
}

HubUiModSection* FindModSection(const char* namespace_id, const char* mod_id)
{
    if (namespace_id == nullptr || mod_id == nullptr)
    {
        return nullptr;
    }

    for (size_t tab_index = 0; tab_index < g_tabs_in_order.size(); ++tab_index)
    {
        HubUiNamespaceTab* tab = g_tabs_in_order[tab_index];
        if (tab->namespace_id != namespace_id)
        {
            continue;
        }

        for (size_t mod_index = 0; mod_index < tab->mods.size(); ++mod_index)
        {
            HubUiModSection* mod = tab->mods[mod_index];
            if (mod->mod_id == mod_id)
            {
                return mod;
            }
        }

        return nullptr;
    }

    return nullptr;
}

void RefreshValueRowsForMod(HubUiModSection* mod, bool force_refresh)
{
    if (mod == nullptr)
    {
        return;
    }

    if (force_refresh)
    {
        for (size_t row_index = 0; row_index < mod->rows.size(); ++row_index)
        {
            HubUiSettingRow* row = mod->rows[row_index];
            if (row->kind == HUB_UI_ROW_KIND_BOOL || row->kind == HUB_UI_ROW_KIND_KEYBIND)
            {
                row->dirty = false;
                row->capture_active = false;
            }
        }
    }

    for (size_t row_index = 0; row_index < mod->rows.size(); ++row_index)
    {
        HubUiSettingRow* row = mod->rows[row_index];
        if (row->kind != HUB_UI_ROW_KIND_BOOL && row->kind != HUB_UI_ROW_KIND_KEYBIND)
        {
            continue;
        }

        if (!force_refresh && row->dirty)
        {
            continue;
        }

        if (row->kind == HUB_UI_ROW_KIND_BOOL)
        {
            int32_t value = 0;
            EMC_Result get_result = EMC_OK;
            const char* message = kLogNone;
            if (!TryReadBool(row, &value, &get_result, &message))
            {
                LogActionRefreshGetFailure(row, get_result, message);
                continue;
            }

            row->canonical_bool_value = value;
            row->pending_bool_value = value;
            row->dirty = false;
            row->inline_error.clear();
            continue;
        }

        EMC_KeybindValueV1 value;
        value.keycode = EMC_KEY_UNBOUND;
        value.modifiers = 0;
        EMC_Result get_result = EMC_OK;
        const char* message = kLogNone;
        if (!TryReadKeybind(row, &value, &get_result, &message))
        {
            LogActionRefreshGetFailure(row, get_result, message);
            continue;
        }

        row->canonical_keybind_value = value;
        row->pending_keybind_value = value;
        row->dirty = false;
        row->inline_error.clear();
    }
}

void __cdecl BuildSessionRow(
    const HubRegistryNamespaceView* namespace_view,
    const HubRegistryModView* mod_view,
    const HubRegistrySettingView* setting_view,
    void* user_data)
{
    if (namespace_view == nullptr || mod_view == nullptr || setting_view == nullptr || user_data == nullptr)
    {
        return;
    }

    std::map<std::string, HubUiNamespaceTab*>* tabs_by_id =
        static_cast<std::map<std::string, HubUiNamespaceTab*>*>(user_data);
    HubUiNamespaceTab* tab = nullptr;

    std::map<std::string, HubUiNamespaceTab*>::const_iterator tab_it = tabs_by_id->find(namespace_view->namespace_id);
    if (tab_it == tabs_by_id->end())
    {
        tab = new HubUiNamespaceTab();
        tab->namespace_id = namespace_view->namespace_id;
        tab->namespace_display_name = namespace_view->namespace_display_name;
        tab->search_query.clear();
        g_tabs_in_order.push_back(tab);
        (*tabs_by_id)[tab->namespace_id] = tab;
    }
    else
    {
        tab = tab_it->second;
    }

    HubUiModSection* mod = nullptr;
    for (size_t mod_index = 0; mod_index < tab->mods.size(); ++mod_index)
    {
        HubUiModSection* existing_mod = tab->mods[mod_index];
        if (existing_mod->namespace_id == mod_view->namespace_id && existing_mod->mod_id == mod_view->mod_id)
        {
            mod = existing_mod;
            break;
        }
    }

    if (mod == nullptr)
    {
        mod = new HubUiModSection();
        mod->namespace_id = mod_view->namespace_id;
        mod->mod_id = mod_view->mod_id;
        mod->mod_display_name = mod_view->mod_display_name;
        mod->collapsed = false;
        tab->mods.push_back(mod);
    }

    if (setting_view->kind != HUB_REGISTRY_SETTING_KIND_BOOL
        && setting_view->kind != HUB_REGISTRY_SETTING_KIND_KEYBIND
        && setting_view->kind != HUB_REGISTRY_SETTING_KIND_ACTION)
    {
        return;
    }

    HubUiSettingRow* row = new HubUiSettingRow();
    row->kind = setting_view->kind == HUB_REGISTRY_SETTING_KIND_BOOL
        ? HUB_UI_ROW_KIND_BOOL
        : (setting_view->kind == HUB_REGISTRY_SETTING_KIND_KEYBIND ? HUB_UI_ROW_KIND_KEYBIND : HUB_UI_ROW_KIND_ACTION);
    row->namespace_id = namespace_view->namespace_id;
    row->namespace_display_name = namespace_view->namespace_display_name;
    row->mod_id = mod_view->mod_id;
    row->mod_display_name = mod_view->mod_display_name;
    row->setting_id = setting_view->setting_id;
    row->label = setting_view->label;
    row->description = setting_view->description;
    row->user_data = setting_view->user_data;
    row->get_bool = setting_view->get_bool;
    row->set_bool = setting_view->set_bool;
    row->canonical_bool_value = 0;
    row->pending_bool_value = 0;
    row->get_keybind = setting_view->get_keybind;
    row->set_keybind = setting_view->set_keybind;
    row->canonical_keybind_value.keycode = EMC_KEY_UNBOUND;
    row->canonical_keybind_value.modifiers = 0;
    row->pending_keybind_value.keycode = EMC_KEY_UNBOUND;
    row->pending_keybind_value.modifiers = 0;
    row->on_action = setting_view->on_action;
    row->action_flags = setting_view->action_flags;
    row->dirty = false;
    row->capture_active = false;
    row->inline_error.clear();
    row->parent_mod = mod;

    mod->rows.push_back(row);
    g_rows_in_order.push_back(row);
}
}

void HubUi_SetOptionsWindowOpen(bool is_open)
{
    g_options_window_open = is_open;
}

bool HubUi_IsOptionsWindowOpen()
{
    return g_options_window_open;
}

void HubUi_ClearSessionModel()
{
    ClearTabsAndRows();
}

void HubUi_RebuildSessionModelFromRegistry()
{
    ClearTabsAndRows();

    std::map<std::string, HubUiNamespaceTab*> tabs_by_id;
    HubRegistry_ForEachSettingInOrder(&BuildSessionRow, &tabs_by_id);
}

void HubUi_PerformInitialSync()
{
    for (size_t row_index = 0; row_index < g_rows_in_order.size(); ++row_index)
    {
        HubUiSettingRow* row = g_rows_in_order[row_index];
        row->capture_active = false;

        if (row->kind == HUB_UI_ROW_KIND_BOOL)
        {
            int32_t value = 0;
            EMC_Result get_result = EMC_OK;
            const char* message = kLogNone;
            if (!TryReadBool(row, &value, &get_result, &message))
            {
                row->dirty = false;
                row->inline_error = kUnavailableMessage;
                LogUiGetFailure(row, get_result, message);
                continue;
            }

            row->canonical_bool_value = value;
            row->pending_bool_value = value;
            row->dirty = false;
            row->inline_error.clear();
            continue;
        }

        if (row->kind == HUB_UI_ROW_KIND_KEYBIND)
        {
            EMC_KeybindValueV1 value;
            value.keycode = EMC_KEY_UNBOUND;
            value.modifiers = 0;
            EMC_Result get_result = EMC_OK;
            const char* message = kLogNone;
            if (!TryReadKeybind(row, &value, &get_result, &message))
            {
                row->dirty = false;
                row->inline_error = kUnavailableMessage;
                LogUiGetFailure(row, get_result, message);
                continue;
            }

            row->canonical_keybind_value = value;
            row->pending_keybind_value = value;
            row->dirty = false;
            row->inline_error.clear();
        }
    }
}

bool HubUi_IsAnyKeybindCaptureActive()
{
    for (size_t row_index = 0; row_index < g_rows_in_order.size(); ++row_index)
    {
        HubUiSettingRow* row = g_rows_in_order[row_index];
        if (row->kind == HUB_UI_ROW_KIND_KEYBIND && row->capture_active)
        {
            return true;
        }
    }

    return false;
}

EMC_Result HubUi_SetModCollapsed(const char* namespace_id, const char* mod_id, bool is_collapsed)
{
    HubUiModSection* mod = FindModSection(namespace_id, mod_id);
    if (mod == nullptr)
    {
        return EMC_ERR_NOT_FOUND;
    }

    mod->collapsed = is_collapsed;
    return EMC_OK;
}

bool HubUi_GetModCollapsed(const char* namespace_id, const char* mod_id, bool* out_is_collapsed)
{
    if (out_is_collapsed == nullptr)
    {
        return false;
    }

    HubUiModSection* mod = FindModSection(namespace_id, mod_id);
    if (mod == nullptr)
    {
        return false;
    }

    *out_is_collapsed = mod->collapsed;
    return true;
}

EMC_Result HubUi_SetPendingBool(const char* namespace_id, const char* mod_id, const char* setting_id, int32_t value)
{
    if (!IsBoolValueValid(value))
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    HubUiSettingRow* row = FindRow(namespace_id, mod_id, setting_id);
    if (row == nullptr || row->kind != HUB_UI_ROW_KIND_BOOL)
    {
        return EMC_ERR_NOT_FOUND;
    }

    row->pending_bool_value = value;
    row->dirty = row->pending_bool_value != row->canonical_bool_value;
    return EMC_OK;
}

EMC_Result HubUi_BeginKeybindCapture(const char* namespace_id, const char* mod_id, const char* setting_id)
{
    HubUiSettingRow* row = FindRow(namespace_id, mod_id, setting_id);
    if (row == nullptr || row->kind != HUB_UI_ROW_KIND_KEYBIND)
    {
        return EMC_ERR_NOT_FOUND;
    }

    for (size_t row_index = 0; row_index < g_rows_in_order.size(); ++row_index)
    {
        HubUiSettingRow* candidate = g_rows_in_order[row_index];
        if (candidate->kind == HUB_UI_ROW_KIND_KEYBIND)
        {
            candidate->capture_active = false;
        }
    }

    row->capture_active = true;
    return EMC_OK;
}

EMC_Result HubUi_CancelKeybindCapture(const char* namespace_id, const char* mod_id, const char* setting_id)
{
    HubUiSettingRow* row = FindRow(namespace_id, mod_id, setting_id);
    if (row == nullptr || row->kind != HUB_UI_ROW_KIND_KEYBIND)
    {
        return EMC_ERR_NOT_FOUND;
    }

    row->capture_active = false;
    return EMC_OK;
}

EMC_Result HubUi_ApplyCapturedKeycode(const char* namespace_id, const char* mod_id, const char* setting_id, int32_t keycode)
{
    HubUiSettingRow* row = FindRow(namespace_id, mod_id, setting_id);
    if (row == nullptr || row->kind != HUB_UI_ROW_KIND_KEYBIND)
    {
        return EMC_ERR_NOT_FOUND;
    }

    const int32_t escape_keycode = static_cast<int32_t>(OIS::KC_ESCAPE);
    const int32_t backspace_keycode = static_cast<int32_t>(OIS::KC_BACK);

    if (keycode == 27 || keycode == escape_keycode)
    {
        row->capture_active = false;
        return EMC_OK;
    }

    if (keycode == 8 || keycode == backspace_keycode)
    {
        row->pending_keybind_value.keycode = EMC_KEY_UNBOUND;
        row->pending_keybind_value.modifiers = 0;
        row->capture_active = false;
        row->dirty = !KeybindEquals(row->pending_keybind_value, row->canonical_keybind_value);
        return EMC_OK;
    }

    row->pending_keybind_value.keycode = keycode;
    row->pending_keybind_value.modifiers = 0;
    row->capture_active = false;
    row->dirty = !KeybindEquals(row->pending_keybind_value, row->canonical_keybind_value);
    return EMC_OK;
}

EMC_Result HubUi_ApplyCapturedKeycodeToActiveRow(int32_t keycode)
{
    for (size_t row_index = 0; row_index < g_rows_in_order.size(); ++row_index)
    {
        HubUiSettingRow* row = g_rows_in_order[row_index];
        if (row->kind != HUB_UI_ROW_KIND_KEYBIND || !row->capture_active)
        {
            continue;
        }

        return HubUi_ApplyCapturedKeycode(row->namespace_id.c_str(), row->mod_id.c_str(), row->setting_id.c_str(), keycode);
    }

    return EMC_ERR_NOT_FOUND;
}

EMC_Result HubUi_ClearPendingKeybind(const char* namespace_id, const char* mod_id, const char* setting_id)
{
    HubUiSettingRow* row = FindRow(namespace_id, mod_id, setting_id);
    if (row == nullptr || row->kind != HUB_UI_ROW_KIND_KEYBIND)
    {
        return EMC_ERR_NOT_FOUND;
    }

    row->pending_keybind_value.keycode = EMC_KEY_UNBOUND;
    row->pending_keybind_value.modifiers = 0;
    row->capture_active = false;
    row->dirty = !KeybindEquals(row->pending_keybind_value, row->canonical_keybind_value);
    return EMC_OK;
}

EMC_Result HubUi_InvokeActionRow(const char* namespace_id, const char* mod_id, const char* setting_id)
{
    HubUiSettingRow* row = FindRow(namespace_id, mod_id, setting_id);
    if (row == nullptr || row->kind != HUB_UI_ROW_KIND_ACTION)
    {
        return EMC_ERR_NOT_FOUND;
    }

    if (row->on_action == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    char err_buf[256];
    std::memset(err_buf, 0, sizeof(err_buf));
    const EMC_Result callback_result = row->on_action(row->user_data, err_buf, (uint32_t)sizeof(err_buf));
    if (callback_result != EMC_OK)
    {
        const char* message = ResolveErrorMessage(err_buf, kActionFailedMessage);
        row->inline_error = message;
        LogActionFailure(row, callback_result, message);
        return callback_result;
    }

    row->inline_error.clear();
    const bool force_refresh = (row->action_flags & EMC_ACTION_FORCE_REFRESH) != 0u;
    RefreshValueRowsForMod(row->parent_mod, force_refresh);
    return EMC_OK;
}

uint32_t HubUi_GetRowCount()
{
    return static_cast<uint32_t>(g_rows_in_order.size());
}

bool HubUi_GetRowViewByIndex(uint32_t index, HubUiRowView* out_view)
{
    if (out_view == nullptr)
    {
        return false;
    }

    if (index >= g_rows_in_order.size())
    {
        return false;
    }

    HubUiSettingRow* row = g_rows_in_order[index];
    out_view->token = row;
    out_view->kind = row->kind;
    out_view->dirty = row->dirty;
    out_view->capture_active = row->capture_active;
    out_view->namespace_id = row->namespace_id.c_str();
    out_view->namespace_display_name = row->namespace_display_name.c_str();
    out_view->mod_id = row->mod_id.c_str();
    out_view->mod_display_name = row->mod_display_name.c_str();
    out_view->setting_id = row->setting_id.c_str();
    out_view->label = row->label.c_str();
    out_view->description = row->description.c_str();
    out_view->inline_error = row->inline_error.c_str();
    out_view->user_data = row->user_data;
    out_view->get_bool = row->get_bool;
    out_view->set_bool = row->set_bool;
    out_view->pending_bool_value = row->pending_bool_value;
    out_view->get_keybind = row->get_keybind;
    out_view->set_keybind = row->set_keybind;
    out_view->pending_keybind_value = row->pending_keybind_value;
    return true;
}

void HubUi_OnCommitSetFailure(void* token, const char* message)
{
    if (token == nullptr)
    {
        return;
    }

    HubUiSettingRow* row = static_cast<HubUiSettingRow*>(token);
    row->inline_error = SafeLogValue(message);
}

void HubUi_OnCommitSyncBool(void* token, int32_t canonical_value)
{
    if (token == nullptr)
    {
        return;
    }

    HubUiSettingRow* row = static_cast<HubUiSettingRow*>(token);
    if (row->kind != HUB_UI_ROW_KIND_BOOL)
    {
        return;
    }

    row->canonical_bool_value = IsBoolValueValid(canonical_value) ? canonical_value : 0;
    row->pending_bool_value = row->canonical_bool_value;
    row->dirty = false;
    row->inline_error.clear();
}

void HubUi_OnCommitSyncKeybind(void* token, EMC_KeybindValueV1 canonical_value)
{
    if (token == nullptr)
    {
        return;
    }

    HubUiSettingRow* row = static_cast<HubUiSettingRow*>(token);
    if (row->kind != HUB_UI_ROW_KIND_KEYBIND)
    {
        return;
    }

    row->canonical_keybind_value = canonical_value;
    row->pending_keybind_value = canonical_value;
    row->dirty = false;
    row->capture_active = false;
    row->inline_error.clear();
}
