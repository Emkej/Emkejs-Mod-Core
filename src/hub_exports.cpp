#include "emc/mod_hub_api.h"
#include "hub_registry.h"

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
