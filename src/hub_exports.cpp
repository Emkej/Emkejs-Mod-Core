#include "emc/mod_hub_api.h"

namespace
{
EMC_Result __cdecl RegisterModStub(const EMC_ModDescriptorV1* desc, EMC_ModHandle* out_handle)
{
    if (out_handle != nullptr)
    {
        *out_handle = nullptr;
    }

    if (desc == nullptr || out_handle == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return EMC_ERR_INTERNAL;
}

EMC_Result __cdecl RegisterBoolSettingStub(EMC_ModHandle mod, const EMC_BoolSettingDefV1* def)
{
    if (mod == nullptr || def == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return EMC_ERR_INTERNAL;
}

EMC_Result __cdecl RegisterKeybindSettingStub(EMC_ModHandle mod, const EMC_KeybindSettingDefV1* def)
{
    if (mod == nullptr || def == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return EMC_ERR_INTERNAL;
}

EMC_Result __cdecl RegisterIntSettingStub(EMC_ModHandle mod, const EMC_IntSettingDefV1* def)
{
    if (mod == nullptr || def == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return EMC_ERR_INTERNAL;
}

EMC_Result __cdecl RegisterFloatSettingStub(EMC_ModHandle mod, const EMC_FloatSettingDefV1* def)
{
    if (mod == nullptr || def == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return EMC_ERR_INTERNAL;
}

EMC_Result __cdecl RegisterActionRowStub(EMC_ModHandle mod, const EMC_ActionRowDefV1* def)
{
    if (mod == nullptr || def == nullptr)
    {
        return EMC_ERR_INVALID_ARGUMENT;
    }

    return EMC_ERR_INTERNAL;
}

const EMC_HubApiV1 kHubApiV1 = {
    EMC_HUB_API_VERSION_1,
    (uint32_t)sizeof(EMC_HubApiV1),
    &RegisterModStub,
    &RegisterBoolSettingStub,
    &RegisterKeybindSettingStub,
    &RegisterIntSettingStub,
    &RegisterFloatSettingStub,
    &RegisterActionRowStub};
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
