#include <Debug.h>

#include "src/hub_menu_bridge.h"
#include "src/hub_registry.h"

#include "emc/mod_hub_api.h"

#include <core/Functions.h>
#include <kenshi/Kenshi.h>

#include <Windows.h>

#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>

namespace
{
const char* kPluginName = "Emkejs-Mod-Core";
const char* kTestNamespaceId = "emc.test";
const char* kTestNamespaceDisplayName = "EMC Test";
const char* kTestModId = "mod_hub_test_rows";
const char* kTestModDisplayName = "Mod Hub Test Rows";
const char* kEnvEnableTestRows = "EMC_HUB_ENABLE_TEST_ROWS";

struct HubTestState
{
    int32_t enabled;
    EMC_KeybindValueV1 keybind;
    int32_t int_value;
    float float_value;
};

HubTestState g_hub_test_state = {
    1,
    { static_cast<int32_t>('X'), 0u },
    8,
    1.5f
};

void WriteErrorText(char* err_buf, uint32_t err_buf_size, const char* text)
{
    if (err_buf == nullptr || err_buf_size == 0u || text == nullptr)
    {
        return;
    }

    const size_t copy_len = err_buf_size > 0u ? static_cast<size_t>(err_buf_size - 1u) : 0u;
    std::strncpy(err_buf, text, copy_len);
    err_buf[copy_len] = '\0';
}

bool IsEnvFalsey(const char* value)
{
    if (value == nullptr || value[0] == '\0')
    {
        return false;
    }

    return std::strcmp(value, "0") == 0
        || std::strcmp(value, "false") == 0
        || std::strcmp(value, "FALSE") == 0
        || std::strcmp(value, "no") == 0
        || std::strcmp(value, "NO") == 0
        || std::strcmp(value, "off") == 0
        || std::strcmp(value, "OFF") == 0;
}

bool ShouldRegisterHubTestRows()
{
    const char* env_value = std::getenv(kEnvEnableTestRows);
    return !IsEnvFalsey(env_value);
}

EMC_Result __cdecl TestGetBool(void* user_data, int32_t* out_value)
{
    if (user_data == nullptr || out_value == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    HubTestState* state = static_cast<HubTestState*>(user_data);
    *out_value = state->enabled;
    return EMC_OK;
}

EMC_Result __cdecl TestSetBool(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    if (user_data == nullptr)
    {
        WriteErrorText(err_buf, err_buf_size, "missing_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (value != 0 && value != 1)
    {
        WriteErrorText(err_buf, err_buf_size, "invalid_bool");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    HubTestState* state = static_cast<HubTestState*>(user_data);
    state->enabled = value;
    return EMC_OK;
}

EMC_Result __cdecl TestGetKeybind(void* user_data, EMC_KeybindValueV1* out_value)
{
    if (user_data == nullptr || out_value == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    HubTestState* state = static_cast<HubTestState*>(user_data);
    *out_value = state->keybind;
    return EMC_OK;
}

EMC_Result __cdecl TestSetKeybind(
    void* user_data,
    EMC_KeybindValueV1 value,
    char* err_buf,
    uint32_t err_buf_size)
{
    (void)err_buf;
    (void)err_buf_size;

    if (user_data == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    HubTestState* state = static_cast<HubTestState*>(user_data);
    state->keybind = value;
    return EMC_OK;
}

EMC_Result __cdecl TestGetInt(void* user_data, int32_t* out_value)
{
    if (user_data == nullptr || out_value == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    HubTestState* state = static_cast<HubTestState*>(user_data);
    *out_value = state->int_value;
    return EMC_OK;
}

EMC_Result __cdecl TestSetInt(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    if (user_data == nullptr)
    {
        WriteErrorText(err_buf, err_buf_size, "missing_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (value < 0 || value > 10)
    {
        WriteErrorText(err_buf, err_buf_size, "int_out_of_range");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    HubTestState* state = static_cast<HubTestState*>(user_data);
    state->int_value = value;
    return EMC_OK;
}

EMC_Result __cdecl TestGetFloat(void* user_data, float* out_value)
{
    if (user_data == nullptr || out_value == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    HubTestState* state = static_cast<HubTestState*>(user_data);
    *out_value = state->float_value;
    return EMC_OK;
}

EMC_Result __cdecl TestSetFloat(void* user_data, float value, char* err_buf, uint32_t err_buf_size)
{
    if (user_data == nullptr)
    {
        WriteErrorText(err_buf, err_buf_size, "missing_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (value < 0.0f || value > 2.0f)
    {
        WriteErrorText(err_buf, err_buf_size, "float_out_of_range");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    HubTestState* state = static_cast<HubTestState*>(user_data);
    state->float_value = value;
    return EMC_OK;
}

EMC_Result __cdecl TestResetAction(void* user_data, char* err_buf, uint32_t err_buf_size)
{
    (void)err_buf;
    (void)err_buf_size;

    if (user_data == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    HubTestState* state = static_cast<HubTestState*>(user_data);
    state->enabled = 1;
    state->keybind.keycode = static_cast<int32_t>('X');
    state->keybind.modifiers = 0u;
    state->int_value = 8;
    state->float_value = 1.5f;
    return EMC_OK;
}

void LogRegistrationFailure(const char* setting_id, EMC_Result result)
{
    std::stringstream line;
    line << kPluginName
         << " ERROR: failed to register test row setting=" << (setting_id != nullptr ? setting_id : "none")
         << " result=" << result;
    ErrorLog(line.str().c_str());
}

bool RegisterHubTestRows()
{
    EMC_ModDescriptorV1 mod_desc;
    mod_desc.namespace_id = kTestNamespaceId;
    mod_desc.namespace_display_name = kTestNamespaceDisplayName;
    mod_desc.mod_id = kTestModId;
    mod_desc.mod_display_name = kTestModDisplayName;
    mod_desc.mod_user_data = &g_hub_test_state;

    EMC_ModHandle mod_handle = nullptr;
    EMC_Result result = HubRegistry_RegisterMod(&mod_desc, &mod_handle);
    if (result != EMC_OK || mod_handle == nullptr)
    {
        LogRegistrationFailure("register_mod", result);
        return false;
    }

    EMC_BoolSettingDefV1 bool_def;
    bool_def.setting_id = "test_enabled";
    bool_def.label = "Test Enabled";
    bool_def.description = "Bool test row for Mod Hub";
    bool_def.user_data = &g_hub_test_state;
    bool_def.get_value = &TestGetBool;
    bool_def.set_value = &TestSetBool;
    result = HubRegistry_RegisterBoolSetting(mod_handle, &bool_def);
    if (result != EMC_OK)
    {
        LogRegistrationFailure(bool_def.setting_id, result);
        return false;
    }

    EMC_KeybindSettingDefV1 keybind_def;
    keybind_def.setting_id = "test_hotkey";
    keybind_def.label = "Test Hotkey";
    keybind_def.description = "Keybind test row for Mod Hub";
    keybind_def.user_data = &g_hub_test_state;
    keybind_def.get_value = &TestGetKeybind;
    keybind_def.set_value = &TestSetKeybind;
    result = HubRegistry_RegisterKeybindSetting(mod_handle, &keybind_def);
    if (result != EMC_OK)
    {
        LogRegistrationFailure(keybind_def.setting_id, result);
        return false;
    }

    EMC_IntSettingDefV1 int_def;
    int_def.setting_id = "test_int";
    int_def.label = "Test Int";
    int_def.description = "Int test row min=0 max=10 step=4";
    int_def.user_data = &g_hub_test_state;
    int_def.min_value = 0;
    int_def.max_value = 10;
    int_def.step = 4;
    int_def.get_value = &TestGetInt;
    int_def.set_value = &TestSetInt;
    result = HubRegistry_RegisterIntSetting(mod_handle, &int_def);
    if (result != EMC_OK)
    {
        LogRegistrationFailure(int_def.setting_id, result);
        return false;
    }

    EMC_FloatSettingDefV1 float_def;
    float_def.setting_id = "test_float";
    float_def.label = "Test Float";
    float_def.description = "Float test row min=0 max=2 step=0.3";
    float_def.user_data = &g_hub_test_state;
    float_def.min_value = 0.0f;
    float_def.max_value = 2.0f;
    float_def.step = 0.3f;
    float_def.display_decimals = 2u;
    float_def.get_value = &TestGetFloat;
    float_def.set_value = &TestSetFloat;
    result = HubRegistry_RegisterFloatSetting(mod_handle, &float_def);
    if (result != EMC_OK)
    {
        LogRegistrationFailure(float_def.setting_id, result);
        return false;
    }

    EMC_ActionRowDefV1 action_def;
    action_def.setting_id = "test_reset";
    action_def.label = "Reset Test Values";
    action_def.description = "Reset all test rows to defaults";
    action_def.user_data = &g_hub_test_state;
    action_def.action_flags = EMC_ACTION_FORCE_REFRESH;
    action_def.on_action = &TestResetAction;
    result = HubRegistry_RegisterActionRow(mod_handle, &action_def);
    if (result != EMC_OK)
    {
        LogRegistrationFailure(action_def.setting_id, result);
        return false;
    }

    return true;
}

bool IsSupportedVersion(KenshiLib::BinaryVersion& versionInfo)
{
    const unsigned int platform = versionInfo.GetPlatform();
    const std::string version = versionInfo.GetVersion();

    return platform != KenshiLib::BinaryVersion::UNKNOWN
        && (version == "1.0.65" || version == "1.0.68");
}
}

__declspec(dllexport) void startPlugin()
{
    DebugLog("Emkejs-Mod-Core: startPlugin()");

    KenshiLib::BinaryVersion versionInfo = KenshiLib::GetKenshiVersion();
    if (!IsSupportedVersion(versionInfo))
    {
        ErrorLog("Emkejs-Mod-Core: unsupported Kenshi version/platform");
        return;
    }

    const unsigned int platform = versionInfo.GetPlatform();
    const std::string version = versionInfo.GetVersion();
    if (!HubMenuBridge_InstallHooks(platform, version))
    {
        HubMenuBridge_SetHubEnabled(false);
        ErrorLog("Emkejs-Mod-Core: failed to install Mod Hub hooks; hub path disabled");
        return;
    }

    std::stringstream info;
    info << kPluginName << " INFO: base plugin initialized (hooks installed)";
    DebugLog(info.str().c_str());

    if (!ShouldRegisterHubTestRows())
    {
        DebugLog("Emkejs-Mod-Core INFO: Mod Hub test rows disabled by EMC_HUB_ENABLE_TEST_ROWS");
        return;
    }

    if (RegisterHubTestRows())
    {
        DebugLog("Emkejs-Mod-Core INFO: registered Mod Hub test rows");
    }
    else
    {
        ErrorLog("Emkejs-Mod-Core ERROR: failed to register Mod Hub test rows");
    }
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID)
{
    return TRUE;
}
