#include "emc/mod_hub_api.h"
#include "hub_commit.h"
#include "hub_menu_bridge.h"
#include "hub_registry.h"
#include "hub_ui.h"

#include <Debug.h>

#include <cstdlib>
#include <cstring>
#include <sstream>

namespace
{
const char* kLogNone = "none";
const char* kEnvDisableRegistryAttach = "EMC_HUB_DISABLE_REGISTRY_ATTACH";
bool g_registry_attach_override_set = false;
bool g_registry_attach_override_enabled = true;

const char* SafeLogValue(const char* value)
{
    if (value == nullptr || value[0] == '\0')
    {
        return kLogNone;
    }

    return value;
}

bool IsEnvTruthy(const char* value)
{
    if (value == nullptr)
    {
        return false;
    }

    return std::strcmp(value, "1") == 0
        || std::strcmp(value, "true") == 0
        || std::strcmp(value, "TRUE") == 0
        || std::strcmp(value, "yes") == 0
        || std::strcmp(value, "YES") == 0
        || std::strcmp(value, "on") == 0
        || std::strcmp(value, "ON") == 0;
}

bool IsRegistryAttachEnabled()
{
    if (g_registry_attach_override_set)
    {
        return g_registry_attach_override_enabled;
    }

    return !IsEnvTruthy(std::getenv(kEnvDisableRegistryAttach));
}

void SetRegistryAttachOverride(bool enabled)
{
    g_registry_attach_override_set = true;
    g_registry_attach_override_enabled = enabled;
}

void ClearRegistryAttachOverride()
{
    g_registry_attach_override_set = false;
    g_registry_attach_override_enabled = true;
}

void LogRegistrationRejected(const char* api_name, const char* reason, EMC_Result result, const char* message)
{
    std::ostringstream line;
    line << "event=hub_registration_rejected"
         << " api=" << SafeLogValue(api_name)
         << " reason=" << SafeLogValue(reason)
         << " result=" << result
         << " message=" << SafeLogValue(message);
    ErrorLog(line.str().c_str());
}

EMC_Result RejectWhenRegistryAttachDisabled(const char* api_name)
{
    if (IsRegistryAttachEnabled())
    {
        return EMC_OK;
    }

    LogRegistrationRejected(
        api_name,
        "registry_attach_disabled",
        EMC_ERR_INTERNAL,
        "registry_attach_gated_off");
    return EMC_ERR_INTERNAL;
}

EMC_Result __cdecl RegisterModEntry(const EMC_ModDescriptorV1* desc, EMC_ModHandle* out_handle)
{
    if (out_handle != nullptr)
    {
        *out_handle = nullptr;
    }

    EMC_Result gate_result = RejectWhenRegistryAttachDisabled("register_mod");
    if (gate_result != EMC_OK)
    {
        return gate_result;
    }

    return HubRegistry_RegisterMod(desc, out_handle);
}

EMC_Result __cdecl RegisterBoolSettingEntry(EMC_ModHandle mod, const EMC_BoolSettingDefV1* def)
{
    EMC_Result gate_result = RejectWhenRegistryAttachDisabled("register_bool_setting");
    if (gate_result != EMC_OK)
    {
        return gate_result;
    }

    return HubRegistry_RegisterBoolSetting(mod, def);
}

EMC_Result __cdecl RegisterKeybindSettingEntry(EMC_ModHandle mod, const EMC_KeybindSettingDefV1* def)
{
    EMC_Result gate_result = RejectWhenRegistryAttachDisabled("register_keybind_setting");
    if (gate_result != EMC_OK)
    {
        return gate_result;
    }

    return HubRegistry_RegisterKeybindSetting(mod, def);
}

EMC_Result __cdecl RegisterIntSettingEntry(EMC_ModHandle mod, const EMC_IntSettingDefV1* def)
{
    EMC_Result gate_result = RejectWhenRegistryAttachDisabled("register_int_setting");
    if (gate_result != EMC_OK)
    {
        return gate_result;
    }

    return HubRegistry_RegisterIntSetting(mod, def);
}

EMC_Result __cdecl RegisterFloatSettingEntry(EMC_ModHandle mod, const EMC_FloatSettingDefV1* def)
{
    EMC_Result gate_result = RejectWhenRegistryAttachDisabled("register_float_setting");
    if (gate_result != EMC_OK)
    {
        return gate_result;
    }

    return HubRegistry_RegisterFloatSetting(mod, def);
}

EMC_Result __cdecl RegisterActionRowEntry(EMC_ModHandle mod, const EMC_ActionRowDefV1* def)
{
    EMC_Result gate_result = RejectWhenRegistryAttachDisabled("register_action_row");
    if (gate_result != EMC_OK)
    {
        return gate_result;
    }

    return HubRegistry_RegisterActionRow(mod, def);
}

const EMC_HubApiV1 kHubApiV1 = {
    EMC_HUB_API_VERSION_1,
    (uint32_t)sizeof(EMC_HubApiV1),
    &RegisterModEntry,
    &RegisterBoolSettingEntry,
    &RegisterKeybindSettingEntry,
    &RegisterIntSettingEntry,
    &RegisterFloatSettingEntry,
    &RegisterActionRowEntry};
}

extern "C" EMC_MOD_HUB_API EMC_Result __cdecl EMC_ModHub_GetApi(
    uint32_t requested_version,
    uint32_t caller_api_size,
    const EMC_HubApiV1** out_api,
    uint32_t* out_api_size)
{
    if (out_api == nullptr || out_api_size == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    *out_api = nullptr;
    *out_api_size = 0;

    if (requested_version != EMC_HUB_API_VERSION_1)
    {
        return EMC_ERR_UNSUPPORTED_VERSION;
    }

    if (caller_api_size < EMC_HUB_API_V1_MIN_SIZE)
    {
        return EMC_ERR_API_SIZE_MISMATCH;
    }

    *out_api = &kHubApiV1;
    *out_api_size = (uint32_t)sizeof(EMC_HubApiV1);
    return EMC_OK;
}

extern "C" EMC_MOD_HUB_API void __cdecl EMC_ModHub_Test_SetRegistrationLocked(int32_t is_locked)
{
    HubRegistry_SetRegistrationLocked(is_locked != 0);
}

extern "C" EMC_MOD_HUB_API void __cdecl EMC_ModHub_Test_SetRegistryAttachEnabled(int32_t is_enabled)
{
    if (is_enabled < 0)
    {
        ClearRegistryAttachOverride();
        return;
    }

    SetRegistryAttachOverride(is_enabled != 0);
}

extern "C" EMC_MOD_HUB_API void __cdecl EMC_ModHub_Test_Menu_SetHubEnabled(int32_t is_enabled)
{
    HubMenuBridge_SetHubEnabled(is_enabled != 0);
}

extern "C" EMC_MOD_HUB_API void __cdecl EMC_ModHub_Test_Menu_OpenOptionsWindow()
{
    HubMenuBridge_OnOptionsWindowInit();
}

extern "C" EMC_MOD_HUB_API void __cdecl EMC_ModHub_Test_Menu_SaveOptionsWindow()
{
    HubMenuBridge_OnOptionsWindowSave();
}

extern "C" EMC_MOD_HUB_API void __cdecl EMC_ModHub_Test_Menu_CloseOptionsWindow()
{
    HubMenuBridge_OnOptionsWindowClose();
}

extern "C" EMC_MOD_HUB_API EMC_Result __cdecl EMC_ModHub_Test_UI_SetPendingBool(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id,
    int32_t value)
{
    return HubUi_SetPendingBool(namespace_id, mod_id, setting_id, value);
}

extern "C" EMC_MOD_HUB_API EMC_Result __cdecl EMC_ModHub_Test_UI_BeginKeybindCapture(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id)
{
    return HubUi_BeginKeybindCapture(namespace_id, mod_id, setting_id);
}

extern "C" EMC_MOD_HUB_API EMC_Result __cdecl EMC_ModHub_Test_UI_CancelKeybindCapture(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id)
{
    return HubUi_CancelKeybindCapture(namespace_id, mod_id, setting_id);
}

extern "C" EMC_MOD_HUB_API EMC_Result __cdecl EMC_ModHub_Test_UI_ApplyCapturedKeycode(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id,
    int32_t keycode)
{
    return HubUi_ApplyCapturedKeycode(namespace_id, mod_id, setting_id, keycode);
}

extern "C" EMC_MOD_HUB_API EMC_Result __cdecl EMC_ModHub_Test_UI_ClearPendingKeybind(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id)
{
    return HubUi_ClearPendingKeybind(namespace_id, mod_id, setting_id);
}

extern "C" EMC_MOD_HUB_API EMC_Result __cdecl EMC_ModHub_Test_UI_InvokeActionRow(
    const char* namespace_id,
    const char* mod_id,
    const char* setting_id)
{
    return HubUi_InvokeActionRow(namespace_id, mod_id, setting_id);
}

extern "C" EMC_MOD_HUB_API void __cdecl EMC_ModHub_Test_Commit_GetLastSummary(
    uint32_t* out_attempted,
    uint32_t* out_succeeded,
    uint32_t* out_failed,
    uint32_t* out_skipped,
    int32_t* out_skip_reason)
{
    if (out_attempted == nullptr
        || out_succeeded == nullptr
        || out_failed == nullptr
        || out_skipped == nullptr
        || out_skip_reason == nullptr)
    {
        return;
    }

    HubCommitSummary summary;
    HubCommit_GetLastSummary(&summary);
    *out_attempted = summary.attempted;
    *out_succeeded = summary.succeeded;
    *out_failed = summary.failed;
    *out_skipped = summary.skipped;
    *out_skip_reason = summary.skip_reason;
}
