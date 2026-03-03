#ifndef EMC_HUB_UI_H
#define EMC_HUB_UI_H

#include "emc/mod_hub_api.h"

enum HubUiRowKind
{
    HUB_UI_ROW_KIND_BOOL = 0,
    HUB_UI_ROW_KIND_KEYBIND = 1,
    HUB_UI_ROW_KIND_ACTION = 2
};

struct HubUiRowView
{
    void* token;
    int32_t kind;
    bool dirty;
    bool capture_active;

    const char* namespace_id;
    const char* namespace_display_name;
    const char* mod_id;
    const char* mod_display_name;
    const char* setting_id;
    const char* label;
    const char* description;
    const char* inline_error;

    void* user_data;
    EMC_GetBoolCallback get_bool;
    EMC_SetBoolCallback set_bool;
    int32_t pending_bool_value;
    EMC_GetKeybindCallback get_keybind;
    EMC_SetKeybindCallback set_keybind;
    EMC_KeybindValueV1 pending_keybind_value;
};

void HubUi_SetOptionsWindowOpen(bool is_open);
bool HubUi_IsOptionsWindowOpen();

void HubUi_ClearSessionModel();
void HubUi_RebuildSessionModelFromRegistry();
void HubUi_PerformInitialSync();

bool HubUi_IsAnyKeybindCaptureActive();

EMC_Result HubUi_SetModCollapsed(const char* namespace_id, const char* mod_id, bool is_collapsed);
bool HubUi_GetModCollapsed(const char* namespace_id, const char* mod_id, bool* out_is_collapsed);
EMC_Result HubUi_SetNamespaceSearchQuery(const char* namespace_id, const char* search_query);
bool HubUi_GetNamespaceSearchQuery(const char* namespace_id, const char** out_search_query);
bool HubUi_DoesRowMatchNamespaceSearch(const HubUiRowView* row);
bool HubUi_DoesSettingMatchNamespaceSearch(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id,
    bool* out_matches);

EMC_Result HubUi_SetPendingBool(const char* namespace_id, const char* mod_id, const char* setting_id, int32_t value);
EMC_Result HubUi_BeginKeybindCapture(const char* namespace_id, const char* mod_id, const char* setting_id);
EMC_Result HubUi_CancelKeybindCapture(const char* namespace_id, const char* mod_id, const char* setting_id);
EMC_Result HubUi_ApplyCapturedKeycode(const char* namespace_id, const char* mod_id, const char* setting_id, int32_t keycode);
EMC_Result HubUi_ApplyCapturedKeycodeToActiveRow(int32_t keycode);
EMC_Result HubUi_ClearPendingKeybind(const char* namespace_id, const char* mod_id, const char* setting_id);
EMC_Result HubUi_InvokeActionRow(const char* namespace_id, const char* mod_id, const char* setting_id);

uint32_t HubUi_GetRowCount();
bool HubUi_GetRowViewByIndex(uint32_t index, HubUiRowView* out_view);

void HubUi_OnCommitSetFailure(void* token, const char* message);
void HubUi_OnCommitSyncBool(void* token, int32_t canonical_value);
void HubUi_OnCommitSyncKeybind(void* token, EMC_KeybindValueV1 canonical_value);

#endif
