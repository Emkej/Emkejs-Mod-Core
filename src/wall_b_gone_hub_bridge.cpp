#include "wall_b_gone_hub_bridge.h"

#include "emc/mod_hub_client.h"
#include "emc/mod_hub_api.h"

#include <Debug.h>
#include <ois/OISKeyboard.h>

#include <Windows.h>

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
const char* kPluginName = "Wall-B-Gone";
const char* kLogNone = "none";

const char* kNamespaceId = "emkej.qol";
const char* kNamespaceDisplayName = "Emkej QoL";
const char* kModId = "wall_b_gone";
const char* kModDisplayName = "Wall-B-Gone";

const char* kSettingEnabledId = "enabled";
const char* kSettingEnabledLabel = "Enabled";
const char* kSettingEnabledDescription = "Enable Wall-B-Gone features";

const char* kSettingSleepingBagEnabledId = "sleeping_bag_dismantle_enabled";
const char* kSettingSleepingBagEnabledLabel = "Allow sleeping bag dismantle";
const char* kSettingSleepingBagEnabledDescription = "Allow dismantle behavior for sleeping bags";

const char* kSettingDismantleHotkeyId = "dismantle_hotkey";
const char* kSettingDismantleHotkeyLabel = "Dismantle hotkey";
const char* kSettingDismantleHotkeyDescription = "Hotkey used to dismantle the selected wall";

const char* kActionResetHotkeyId = "reset_hotkey_default";
const char* kActionResetHotkeyLabel = "Reset hotkey default";
const char* kActionResetHotkeyDescription = "Reset dismantle hotkey to the default key";

const char* kConfigFileName = "mod-config.json";
const char* kConfigKeyEnabled = "enabled";
const char* kConfigKeySleepingBagEnabled = "sleeping_bag_dismantle_enabled";
const char* kConfigKeyDismantleHotkey = "dismantle_hotkey";
const char* kConfigKeyDismantleHotkeyModifiers = "dismantle_hotkey_modifiers";
const char* kRuntimeModuleName = "Wall-B-Gone.dll";
const char* kRuntimeGetExportName = "WallBGone_GetRuntimeStateV1";
const char* kRuntimeSetExportName = "WallBGone_SetRuntimeStateV1";

const int32_t kDefaultDismantleHotkey = static_cast<int32_t>(OIS::KC_X);

const int32_t kAttachFailureModeNone = 0;
const int32_t kAttachFailureModeStartupOnly = 1;
const int32_t kAttachFailureModeAlways = 2;

struct WallBGoneState
{
    int32_t enabled;
    int32_t sleeping_bag_dismantle_enabled;
    EMC_KeybindValueV1 dismantle_hotkey;
};

struct WallBGoneRuntimeStateV1
{
    int32_t enabled;
    int32_t sleeping_bag_dismantle_enabled;
    int32_t hotkey_keycode;
    uint32_t hotkey_modifiers;
};

typedef int(__cdecl *FnWallBGoneGetRuntimeStateV1)(WallBGoneRuntimeStateV1* out_state);
typedef int(__cdecl *FnWallBGoneSetRuntimeStateV1)(
    const WallBGoneRuntimeStateV1* state,
    char* err_buf,
    uint32_t err_buf_size);

struct ConfigProperty
{
    std::string key;
    std::string value;
};

WallBGoneState g_wall_b_gone_state = {
    1,
    0,
    { kDefaultDismantleHotkey, 0u }};

emc::ModHubClient g_mod_hub_client;
bool g_logged_register_mod_fallback = false;
int32_t g_attach_failure_mode = kAttachFailureModeNone;
std::string g_config_path;
HMODULE g_runtime_module = 0;
FnWallBGoneGetRuntimeStateV1 g_fn_get_runtime_state = 0;
FnWallBGoneSetRuntimeStateV1 g_fn_set_runtime_state = 0;

const char* SafeLogValue(const char* value)
{
    if (value == 0 || value[0] == '\0')
    {
        return kLogNone;
    }

    return value;
}

void WriteErrorText(char* err_buf, uint32_t err_buf_size, const char* text)
{
    if (err_buf == 0 || err_buf_size == 0u || text == 0)
    {
        return;
    }

    const size_t copy_len = static_cast<size_t>(err_buf_size - 1u);
    std::strncpy(err_buf, text, copy_len);
    err_buf[copy_len] = '\0';
}

bool TryResolveRuntimeApi()
{
    if (g_fn_get_runtime_state != 0 && g_fn_set_runtime_state != 0)
    {
        return true;
    }

    HMODULE module = g_runtime_module;
    if (module == 0)
    {
        module = GetModuleHandleA(kRuntimeModuleName);
        if (module == 0)
        {
            return false;
        }
        g_runtime_module = module;
    }

    FnWallBGoneGetRuntimeStateV1 get_fn =
        reinterpret_cast<FnWallBGoneGetRuntimeStateV1>(GetProcAddress(module, kRuntimeGetExportName));
    FnWallBGoneSetRuntimeStateV1 set_fn =
        reinterpret_cast<FnWallBGoneSetRuntimeStateV1>(GetProcAddress(module, kRuntimeSetExportName));
    if (get_fn == 0 || set_fn == 0)
    {
        return false;
    }

    g_fn_get_runtime_state = get_fn;
    g_fn_set_runtime_state = set_fn;
    return true;
}

bool HasRuntimeApi()
{
    return TryResolveRuntimeApi();
}

bool TryPullRuntimeState(WallBGoneState* out_state)
{
    if (out_state == 0 || !TryResolveRuntimeApi() || g_fn_get_runtime_state == 0)
    {
        return false;
    }

    WallBGoneRuntimeStateV1 runtime_state = { 0, 0, kDefaultDismantleHotkey, 0u };
    const int result = g_fn_get_runtime_state(&runtime_state);
    if (result != 0)
    {
        return false;
    }

    out_state->enabled = runtime_state.enabled != 0 ? 1 : 0;
    out_state->sleeping_bag_dismantle_enabled = runtime_state.sleeping_bag_dismantle_enabled != 0 ? 1 : 0;
    out_state->dismantle_hotkey.keycode = runtime_state.hotkey_keycode;
    out_state->dismantle_hotkey.modifiers = runtime_state.hotkey_modifiers;
    return true;
}

bool TryPushRuntimeState(const WallBGoneState& state, char* err_buf, uint32_t err_buf_size)
{
    if (!TryResolveRuntimeApi() || g_fn_set_runtime_state == 0)
    {
        return false;
    }

    WallBGoneRuntimeStateV1 runtime_state;
    runtime_state.enabled = state.enabled != 0 ? 1 : 0;
    runtime_state.sleeping_bag_dismantle_enabled = state.sleeping_bag_dismantle_enabled != 0 ? 1 : 0;
    runtime_state.hotkey_keycode = state.dismantle_hotkey.keycode;
    runtime_state.hotkey_modifiers = state.dismantle_hotkey.modifiers;
    return g_fn_set_runtime_state(&runtime_state, err_buf, err_buf_size) == 0;
}

void LogAttachFailure(const char* phase, EMC_Result result, const char* reason)
{
    std::ostringstream line;
    line << kPluginName
         << " WARN: event=wall_b_gone_hub_attach_failed"
         << " phase=" << SafeLogValue(phase)
         << " result=" << result
         << " reason=" << SafeLogValue(reason);
    ErrorLog(line.str().c_str());
}

void LogFallback(const char* reason, EMC_Result result)
{
    std::ostringstream line;
    line << kPluginName
         << " WARN: event=wall_b_gone_hub_fallback"
         << " reason=" << SafeLogValue(reason)
         << " result=" << result
         << " use_hub_ui=0";
    ErrorLog(line.str().c_str());
}

void LogConfigWarning(const char* reason, const char* detail)
{
    std::ostringstream line;
    line << kPluginName
         << " WARN: event=wall_b_gone_config_warning"
         << " reason=" << SafeLogValue(reason)
         << " detail=" << SafeLogValue(detail);
    ErrorLog(line.str().c_str());
}

size_t SkipWhitespace(const std::string& text, size_t pos)
{
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])) != 0)
    {
        ++pos;
    }

    return pos;
}

size_t SkipWhitespaceRange(const std::string& text, size_t pos, size_t end)
{
    while (pos < end && std::isspace(static_cast<unsigned char>(text[pos])) != 0)
    {
        ++pos;
    }

    return pos;
}

std::string TrimCopy(const std::string& text)
{
    size_t start = 0u;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0)
    {
        ++start;
    }

    size_t end = text.size();
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1u])) != 0)
    {
        --end;
    }

    return text.substr(start, end - start);
}

bool ParseJsonStringToken(
    const std::string& text,
    size_t start,
    size_t end,
    size_t* out_next,
    std::string* out_value)
{
    if (out_next == 0 || out_value == 0 || start >= end || text[start] != '"')
    {
        return false;
    }

    size_t pos = start + 1u;
    while (pos < end)
    {
        const char ch = text[pos];
        if (ch == '\\')
        {
            pos += 2u;
            continue;
        }
        if (ch == '"')
        {
            *out_value = text.substr(start + 1u, pos - (start + 1u));
            *out_next = pos + 1u;
            return true;
        }
        ++pos;
    }

    return false;
}

size_t FindJsonValueEnd(const std::string& text, size_t start, size_t end, bool* out_ok)
{
    if (out_ok == 0)
    {
        return start;
    }

    *out_ok = false;
    size_t pos = SkipWhitespaceRange(text, start, end);
    if (pos >= end)
    {
        return pos;
    }

    const char first = text[pos];
    if (first == '"')
    {
        std::string ignored;
        size_t next = 0u;
        if (!ParseJsonStringToken(text, pos, end, &next, &ignored))
        {
            return pos;
        }
        *out_ok = true;
        return next;
    }

    if (first == '{' || first == '[')
    {
        int depth_curly = 0;
        int depth_square = 0;
        bool in_string = false;
        bool escaped = false;

        for (size_t index = pos; index < end; ++index)
        {
            const char ch = text[index];
            if (in_string)
            {
                if (escaped)
                {
                    escaped = false;
                    continue;
                }
                if (ch == '\\')
                {
                    escaped = true;
                    continue;
                }
                if (ch == '"')
                {
                    in_string = false;
                }
                continue;
            }

            if (ch == '"')
            {
                in_string = true;
                continue;
            }
            if (ch == '{')
            {
                ++depth_curly;
                continue;
            }
            if (ch == '}')
            {
                --depth_curly;
                if (depth_curly < 0)
                {
                    return pos;
                }
                if (depth_curly == 0 && depth_square == 0 && first == '{')
                {
                    *out_ok = true;
                    return index + 1u;
                }
                continue;
            }
            if (ch == '[')
            {
                ++depth_square;
                continue;
            }
            if (ch == ']')
            {
                --depth_square;
                if (depth_square < 0)
                {
                    return pos;
                }
                if (depth_curly == 0 && depth_square == 0 && first == '[')
                {
                    *out_ok = true;
                    return index + 1u;
                }
                continue;
            }
        }

        return pos;
    }

    size_t index = pos;
    while (index < end && text[index] != ',')
    {
        ++index;
    }

    *out_ok = true;
    return index;
}

bool ParseJsonTopLevelProperties(const std::string& text, std::vector<ConfigProperty>* out_props)
{
    if (out_props == 0)
    {
        return false;
    }

    out_props->clear();

    const size_t object_start = text.find('{');
    const size_t object_end = text.rfind('}');
    if (object_start == std::string::npos || object_end == std::string::npos || object_end <= object_start)
    {
        return false;
    }

    size_t pos = object_start + 1u;
    while (true)
    {
        pos = SkipWhitespaceRange(text, pos, object_end);
        if (pos >= object_end)
        {
            break;
        }

        if (text[pos] == ',')
        {
            ++pos;
            continue;
        }

        std::string key;
        size_t next = 0u;
        if (!ParseJsonStringToken(text, pos, object_end, &next, &key))
        {
            return false;
        }

        pos = SkipWhitespaceRange(text, next, object_end);
        if (pos >= object_end || text[pos] != ':')
        {
            return false;
        }
        ++pos;

        const size_t value_start = SkipWhitespaceRange(text, pos, object_end);
        bool ok = false;
        const size_t value_end = FindJsonValueEnd(text, value_start, object_end, &ok);
        if (!ok || value_end <= value_start)
        {
            return false;
        }

        ConfigProperty prop;
        prop.key = key;
        prop.value = TrimCopy(text.substr(value_start, value_end - value_start));
        out_props->push_back(prop);

        pos = SkipWhitespaceRange(text, value_end, object_end);
        if (pos < object_end && text[pos] == ',')
        {
            ++pos;
        }
    }

    return true;
}

void UpsertConfigProperty(std::vector<ConfigProperty>* props, const char* key, const std::string& value)
{
    if (props == 0 || key == 0)
    {
        return;
    }

    for (size_t i = 0u; i < props->size(); ++i)
    {
        if ((*props)[i].key == key)
        {
            (*props)[i].value = value;
            return;
        }
    }

    ConfigProperty added;
    added.key = key;
    added.value = value;
    props->push_back(added);
}

bool LocateJsonValue(const std::string& text, const char* key, size_t* out_pos)
{
    if (key == 0 || out_pos == 0)
    {
        return false;
    }

    const std::string needle = std::string("\"") + key + "\"";
    const size_t key_pos = text.find(needle);
    if (key_pos == std::string::npos)
    {
        return false;
    }

    const size_t colon_pos = text.find(':', key_pos + needle.size());
    if (colon_pos == std::string::npos)
    {
        return false;
    }

    *out_pos = SkipWhitespace(text, colon_pos + 1u);
    return true;
}

bool ParseJsonBool(const std::string& text, const char* key, bool* out_found, int32_t* out_value)
{
    if (out_found == 0 || out_value == 0)
    {
        return false;
    }

    *out_found = false;

    size_t pos = 0u;
    if (!LocateJsonValue(text, key, &pos))
    {
        return true;
    }

    *out_found = true;
    if (text.compare(pos, 4u, "true") == 0)
    {
        *out_value = 1;
        return true;
    }
    if (text.compare(pos, 5u, "false") == 0)
    {
        *out_value = 0;
        return true;
    }
    if (pos < text.size() && text[pos] == '1')
    {
        *out_value = 1;
        return true;
    }
    if (pos < text.size() && text[pos] == '0')
    {
        *out_value = 0;
        return true;
    }

    return false;
}

bool ParseJsonInt32(const std::string& text, const char* key, bool* out_found, int32_t* out_value)
{
    if (out_found == 0 || out_value == 0)
    {
        return false;
    }

    *out_found = false;

    size_t pos = 0u;
    if (!LocateJsonValue(text, key, &pos))
    {
        return true;
    }

    *out_found = true;
    const char* start = text.c_str() + pos;
    char* end = 0;
    const long parsed = std::strtol(start, &end, 10);
    if (end == start)
    {
        return false;
    }
    if (parsed < static_cast<long>(INT32_MIN)
        || parsed > static_cast<long>(INT32_MAX))
    {
        return false;
    }

    *out_value = static_cast<int32_t>(parsed);
    return true;
}

bool ParseJsonUint32(const std::string& text, const char* key, bool* out_found, uint32_t* out_value)
{
    if (out_found == 0 || out_value == 0)
    {
        return false;
    }

    *out_found = false;

    size_t pos = 0u;
    if (!LocateJsonValue(text, key, &pos))
    {
        return true;
    }

    *out_found = true;
    const char* start = text.c_str() + pos;
    char* end = 0;
    const unsigned long parsed = std::strtoul(start, &end, 10);
    if (end == start || parsed > static_cast<unsigned long>(UINT32_MAX))
    {
        return false;
    }

    *out_value = static_cast<uint32_t>(parsed);
    return true;
}

bool ResolveConfigPath(std::string* out_path)
{
    if (out_path == 0)
    {
        return false;
    }

    HMODULE module = 0;
    if (GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&ResolveConfigPath),
            &module) == 0)
    {
        return false;
    }

    char module_path[MAX_PATH];
    module_path[0] = '\0';
    const DWORD path_len = GetModuleFileNameA(module, module_path, MAX_PATH);
    if (path_len == 0u || path_len >= MAX_PATH)
    {
        return false;
    }

    std::string path(module_path, path_len);
    const size_t sep_pos = path.find_last_of("\\/");
    if (sep_pos == std::string::npos)
    {
        return false;
    }

    path.resize(sep_pos + 1u);
    path += kConfigFileName;
    *out_path = path;
    return true;
}

const std::string& GetConfigPath()
{
    if (!g_config_path.empty())
    {
        return g_config_path;
    }

    if (!ResolveConfigPath(&g_config_path))
    {
        g_config_path = kConfigFileName;
    }

    return g_config_path;
}

bool SaveStateToConfig()
{
    const std::string& path = GetConfigPath();

    std::vector<ConfigProperty> props;
    {
        std::ifstream in(path.c_str(), std::ios::binary);
        if (in.is_open())
        {
            std::stringstream existing_buffer;
            existing_buffer << in.rdbuf();
            if (in.good() || in.eof())
            {
                const std::string existing_text = existing_buffer.str();
                if (!existing_text.empty() && !ParseJsonTopLevelProperties(existing_text, &props))
                {
                    LogConfigWarning("parse_for_preserve_failed", path.c_str());
                    props.clear();
                }
            }
            else
            {
                LogConfigWarning("read_for_preserve_failed", path.c_str());
            }
        }
    }

    UpsertConfigProperty(&props, kConfigKeyEnabled, g_wall_b_gone_state.enabled != 0 ? "true" : "false");
    UpsertConfigProperty(
        &props,
        kConfigKeySleepingBagEnabled,
        g_wall_b_gone_state.sleeping_bag_dismantle_enabled != 0 ? "true" : "false");

    {
        std::ostringstream value;
        value << g_wall_b_gone_state.dismantle_hotkey.keycode;
        UpsertConfigProperty(&props, kConfigKeyDismantleHotkey, value.str());
    }
    {
        std::ostringstream value;
        value << g_wall_b_gone_state.dismantle_hotkey.modifiers;
        UpsertConfigProperty(&props, kConfigKeyDismantleHotkeyModifiers, value.str());
    }

    std::ofstream out(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!out.is_open())
    {
        LogConfigWarning("open_failed", path.c_str());
        return false;
    }

    out << "{\n";
    for (size_t i = 0u; i < props.size(); ++i)
    {
        out << "  \"" << props[i].key << "\": " << props[i].value;
        if (i + 1u < props.size())
        {
            out << ",";
        }
        out << "\n";
    }
    out << "}\n";

    if (!out.good())
    {
        LogConfigWarning("write_failed", path.c_str());
        return false;
    }

    return true;
}

void LoadStateFromConfig()
{
    if (TryPullRuntimeState(&g_wall_b_gone_state))
    {
        return;
    }

    const std::string& path = GetConfigPath();
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in.is_open())
    {
        return;
    }

    std::stringstream buffer;
    buffer << in.rdbuf();
    if (!in.good() && !in.eof())
    {
        LogConfigWarning("read_failed", path.c_str());
        return;
    }

    const std::string text = buffer.str();

    bool found = false;
    int32_t bool_value = 0;
    if (ParseJsonBool(text, kConfigKeyEnabled, &found, &bool_value))
    {
        if (found)
        {
            g_wall_b_gone_state.enabled = bool_value;
        }
    }
    else
    {
        LogConfigWarning("parse_bool_failed", kConfigKeyEnabled);
    }

    found = false;
    bool_value = 0;
    if (ParseJsonBool(text, kConfigKeySleepingBagEnabled, &found, &bool_value))
    {
        if (found)
        {
            g_wall_b_gone_state.sleeping_bag_dismantle_enabled = bool_value;
        }
    }
    else
    {
        LogConfigWarning("parse_bool_failed", kConfigKeySleepingBagEnabled);
    }

    found = false;
    int32_t keycode = 0;
    if (ParseJsonInt32(text, kConfigKeyDismantleHotkey, &found, &keycode))
    {
        if (found)
        {
            g_wall_b_gone_state.dismantle_hotkey.keycode = keycode;
        }
    }
    else
    {
        LogConfigWarning("parse_int_failed", kConfigKeyDismantleHotkey);
    }

    bool found_modifiers = false;
    uint32_t modifiers = 0u;
    if (ParseJsonUint32(text, kConfigKeyDismantleHotkeyModifiers, &found_modifiers, &modifiers))
    {
        if (found_modifiers)
        {
            g_wall_b_gone_state.dismantle_hotkey.modifiers = modifiers;
        }
    }
    else
    {
        LogConfigWarning("parse_uint_failed", kConfigKeyDismantleHotkeyModifiers);
    }
}

bool PersistStateAndReportError(char* err_buf, uint32_t err_buf_size)
{
    if (SaveStateToConfig())
    {
        return true;
    }

    WriteErrorText(err_buf, err_buf_size, "persist_failed");
    return false;
}

void ResetValuesToDefaults()
{
    g_wall_b_gone_state.enabled = 1;
    g_wall_b_gone_state.sleeping_bag_dismantle_enabled = 0;
    g_wall_b_gone_state.dismantle_hotkey.keycode = kDefaultDismantleHotkey;
    g_wall_b_gone_state.dismantle_hotkey.modifiers = 0u;
}

bool ShouldForceAttachFailure(bool is_retry)
{
    if (g_attach_failure_mode == kAttachFailureModeAlways)
    {
        return true;
    }

    return g_attach_failure_mode == kAttachFailureModeStartupOnly && !is_retry;
}

EMC_Result __cdecl GetEnabled(void* user_data, int32_t* out_value)
{
    if (user_data == 0 || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    WallBGoneState* state = static_cast<WallBGoneState*>(user_data);
    TryPullRuntimeState(state);
    *out_value = state->enabled;
    return EMC_OK;
}

EMC_Result __cdecl SetEnabled(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    if (user_data == 0)
    {
        WriteErrorText(err_buf, err_buf_size, "missing_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (value != 0 && value != 1)
    {
        WriteErrorText(err_buf, err_buf_size, "invalid_bool");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    WallBGoneState* state = static_cast<WallBGoneState*>(user_data);
    const int32_t previous_value = state->enabled;
    state->enabled = value;
    const bool runtime_api_available = HasRuntimeApi();
    if (TryPushRuntimeState(*state, err_buf, err_buf_size))
    {
        return EMC_OK;
    }

    if (runtime_api_available)
    {
        state->enabled = previous_value;
        if (err_buf != 0 && err_buf_size > 0u && err_buf[0] == '\0')
        {
            WriteErrorText(err_buf, err_buf_size, "runtime_set_failed");
        }
        return EMC_ERR_INTERNAL;
    }

    if (!PersistStateAndReportError(err_buf, err_buf_size))
    {
        state->enabled = previous_value;
        return EMC_ERR_INTERNAL;
    }

    return EMC_OK;
}

EMC_Result __cdecl GetSleepingBagEnabled(void* user_data, int32_t* out_value)
{
    if (user_data == 0 || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    WallBGoneState* state = static_cast<WallBGoneState*>(user_data);
    TryPullRuntimeState(state);
    *out_value = state->sleeping_bag_dismantle_enabled;
    return EMC_OK;
}

EMC_Result __cdecl SetSleepingBagEnabled(void* user_data, int32_t value, char* err_buf, uint32_t err_buf_size)
{
    if (user_data == 0)
    {
        WriteErrorText(err_buf, err_buf_size, "missing_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    if (value != 0 && value != 1)
    {
        WriteErrorText(err_buf, err_buf_size, "invalid_bool");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    WallBGoneState* state = static_cast<WallBGoneState*>(user_data);
    const int32_t previous_value = state->sleeping_bag_dismantle_enabled;
    state->sleeping_bag_dismantle_enabled = value;
    const bool runtime_api_available = HasRuntimeApi();
    if (TryPushRuntimeState(*state, err_buf, err_buf_size))
    {
        return EMC_OK;
    }

    if (runtime_api_available)
    {
        state->sleeping_bag_dismantle_enabled = previous_value;
        if (err_buf != 0 && err_buf_size > 0u && err_buf[0] == '\0')
        {
            WriteErrorText(err_buf, err_buf_size, "runtime_set_failed");
        }
        return EMC_ERR_INTERNAL;
    }

    if (!PersistStateAndReportError(err_buf, err_buf_size))
    {
        state->sleeping_bag_dismantle_enabled = previous_value;
        return EMC_ERR_INTERNAL;
    }

    return EMC_OK;
}

EMC_Result __cdecl GetDismantleHotkey(void* user_data, EMC_KeybindValueV1* out_value)
{
    if (user_data == 0 || out_value == 0)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    WallBGoneState* state = static_cast<WallBGoneState*>(user_data);
    TryPullRuntimeState(state);
    *out_value = state->dismantle_hotkey;
    return EMC_OK;
}

EMC_Result __cdecl SetDismantleHotkey(
    void* user_data,
    EMC_KeybindValueV1 value,
    char* err_buf,
    uint32_t err_buf_size)
{
    if (user_data == 0)
    {
        WriteErrorText(err_buf, err_buf_size, "missing_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    WallBGoneState* state = static_cast<WallBGoneState*>(user_data);
    const EMC_KeybindValueV1 previous_value = state->dismantle_hotkey;
    state->dismantle_hotkey = value;
    const bool runtime_api_available = HasRuntimeApi();
    if (TryPushRuntimeState(*state, err_buf, err_buf_size))
    {
        return EMC_OK;
    }

    if (runtime_api_available)
    {
        state->dismantle_hotkey = previous_value;
        if (err_buf != 0 && err_buf_size > 0u && err_buf[0] == '\0')
        {
            WriteErrorText(err_buf, err_buf_size, "runtime_set_failed");
        }
        return EMC_ERR_INTERNAL;
    }

    if (!PersistStateAndReportError(err_buf, err_buf_size))
    {
        state->dismantle_hotkey = previous_value;
        return EMC_ERR_INTERNAL;
    }

    return EMC_OK;
}

EMC_Result __cdecl ResetHotkeyDefault(void* user_data, char* err_buf, uint32_t err_buf_size)
{
    if (user_data == 0)
    {
        WriteErrorText(err_buf, err_buf_size, "missing_user_data");
        return EMC_ERR_INVALID_ARGUMENT;
    }

    WallBGoneState* state = static_cast<WallBGoneState*>(user_data);
    const EMC_KeybindValueV1 previous_value = state->dismantle_hotkey;
    state->dismantle_hotkey.keycode = kDefaultDismantleHotkey;
    state->dismantle_hotkey.modifiers = 0u;
    const bool runtime_api_available = HasRuntimeApi();
    if (TryPushRuntimeState(*state, err_buf, err_buf_size))
    {
        return EMC_OK;
    }

    if (runtime_api_available)
    {
        state->dismantle_hotkey = previous_value;
        if (err_buf != 0 && err_buf_size > 0u && err_buf[0] == '\0')
        {
            WriteErrorText(err_buf, err_buf_size, "runtime_set_failed");
        }
        return EMC_ERR_INTERNAL;
    }

    if (!PersistStateAndReportError(err_buf, err_buf_size))
    {
        state->dismantle_hotkey = previous_value;
        return EMC_ERR_INTERNAL;
    }

    return EMC_OK;
}

const emc::ModHubClientTableRegistrationV1* GetWallBGoneTableRegistration()
{
    static const EMC_ModDescriptorV1 kModDescriptor = {
        kNamespaceId,
        kNamespaceDisplayName,
        kModId,
        kModDisplayName,
        &g_wall_b_gone_state};

    static const EMC_BoolSettingDefV1 kEnabledSettingDef = {
        kSettingEnabledId,
        kSettingEnabledLabel,
        kSettingEnabledDescription,
        &g_wall_b_gone_state,
        &GetEnabled,
        &SetEnabled};

    static const EMC_BoolSettingDefV1 kSleepingBagSettingDef = {
        kSettingSleepingBagEnabledId,
        kSettingSleepingBagEnabledLabel,
        kSettingSleepingBagEnabledDescription,
        &g_wall_b_gone_state,
        &GetSleepingBagEnabled,
        &SetSleepingBagEnabled};

    static const EMC_KeybindSettingDefV1 kHotkeySettingDef = {
        kSettingDismantleHotkeyId,
        kSettingDismantleHotkeyLabel,
        kSettingDismantleHotkeyDescription,
        &g_wall_b_gone_state,
        &GetDismantleHotkey,
        &SetDismantleHotkey};

    static const EMC_ActionRowDefV1 kResetHotkeyActionDef = {
        kActionResetHotkeyId,
        kActionResetHotkeyLabel,
        kActionResetHotkeyDescription,
        &g_wall_b_gone_state,
        EMC_ACTION_FORCE_REFRESH,
        &ResetHotkeyDefault};

    static const emc::ModHubClientSettingRowV1 kSettingRows[] = {
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kEnabledSettingDef },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_BOOL, &kSleepingBagSettingDef },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_KEYBIND, &kHotkeySettingDef },
        { emc::MOD_HUB_CLIENT_SETTING_KIND_ACTION, &kResetHotkeyActionDef }
    };

    static const emc::ModHubClientTableRegistrationV1 kTableRegistration = {
        &kModDescriptor,
        kSettingRows,
        (uint32_t)(sizeof(kSettingRows) / sizeof(kSettingRows[0]))};

    return &kTableRegistration;
}

bool __cdecl ShouldForceAttachFailureForClient(void* user_data, bool is_retry, EMC_Result* out_result)
{
    (void)user_data;
    if (!ShouldForceAttachFailure(is_retry))
    {
        return false;
    }

    if (out_result != 0)
    {
        *out_result = EMC_ERR_INTERNAL;
    }

    return true;
}

void ConfigureHubClient()
{
    emc::ModHubClient::Config config;
    config.table_registration = GetWallBGoneTableRegistration();
    config.should_force_attach_failure_fn = &ShouldForceAttachFailureForClient;
    config.attach_failure_user_data = 0;
    g_mod_hub_client.SetConfig(config);
}
}

void WallBGoneHubBridge_OnPluginStart()
{
    ResetValuesToDefaults();
    LoadStateFromConfig();
    g_logged_register_mod_fallback = false;
    ConfigureHubClient();

    const emc::ModHubClient::AttemptResult result = g_mod_hub_client.OnStartup();
    if (result == emc::ModHubClient::ATTACH_FAILED)
    {
        LogAttachFailure("startup", g_mod_hub_client.LastAttemptFailureResult(), "get_api_failed");
    }
    else if (result == emc::ModHubClient::REGISTRATION_FAILED)
    {
        if (!g_logged_register_mod_fallback)
        {
            g_logged_register_mod_fallback = true;
            LogFallback("register_mod_or_setting_failed", g_mod_hub_client.LastAttemptFailureResult());
        }
    }
    else
    {
        LogFallback("invalid_client_configuration", g_mod_hub_client.LastAttemptFailureResult());
    }
}

void WallBGoneHubBridge_OnOptionsWindowInit()
{
    if (!g_mod_hub_client.IsAttachRetryPending() || g_mod_hub_client.HasAttachRetryAttempted())
    {
        return;
    }

    const emc::ModHubClient::AttemptResult result = g_mod_hub_client.OnOptionsWindowInit();
    if (result == emc::ModHubClient::ATTACH_FAILED)
    {
        LogAttachFailure("options_init_retry", g_mod_hub_client.LastAttemptFailureResult(), "get_api_failed");
        LogFallback("attach_retry_failed", g_mod_hub_client.LastAttemptFailureResult());
    }
    else if (result == emc::ModHubClient::REGISTRATION_FAILED)
    {
        if (!g_logged_register_mod_fallback)
        {
            g_logged_register_mod_fallback = true;
            LogFallback("register_mod_or_setting_failed", g_mod_hub_client.LastAttemptFailureResult());
        }
    }
    else
    {
        LogFallback("invalid_client_configuration", g_mod_hub_client.LastAttemptFailureResult());
    }
}

bool WallBGoneHubBridge_UseHubUi()
{
    return g_mod_hub_client.UseHubUi();
}

bool WallBGoneHubBridge_IsAttachRetryPending()
{
    return g_mod_hub_client.IsAttachRetryPending();
}

bool WallBGoneHubBridge_HasAttachRetryAttempted()
{
    return g_mod_hub_client.HasAttachRetryAttempted();
}

void WallBGoneHubBridge_Test_SetAttachFailureMode(int32_t mode)
{
    if (mode == kAttachFailureModeStartupOnly || mode == kAttachFailureModeAlways)
    {
        g_attach_failure_mode = mode;
        return;
    }

    g_attach_failure_mode = kAttachFailureModeNone;
}

void WallBGoneHubBridge_Test_ResetRuntimeState()
{
    ResetValuesToDefaults();
    g_mod_hub_client.Reset();
    g_logged_register_mod_fallback = false;
    g_attach_failure_mode = kAttachFailureModeNone;
}
