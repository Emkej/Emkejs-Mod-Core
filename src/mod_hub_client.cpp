#include "emc/mod_hub_client.h"

namespace
{
EMC_Result DefaultGetApi(
    uint32_t requested_version,
    uint32_t caller_api_size,
    const EMC_HubApiV1** out_api,
    uint32_t* out_api_size)
{
    return EMC_ModHub_GetApi(requested_version, caller_api_size, out_api, out_api_size);
}
}

namespace emc
{
ModHubClient::Config::Config()
    : get_api_fn(0)
    , register_fn(0)
    , register_user_data(0)
    , should_force_attach_failure_fn(0)
    , attach_failure_user_data(0)
{
}

ModHubClient::ModHubClient()
{
    Reset();
}

ModHubClient::ModHubClient(const Config& config)
    : config_(config)
{
    Reset();
}

void ModHubClient::SetConfig(const Config& config)
{
    config_ = config;
}

const ModHubClient::Config& ModHubClient::GetConfig() const
{
    return config_;
}

void ModHubClient::Reset()
{
    use_hub_ui_ = false;
    attach_retry_pending_ = false;
    attach_retry_attempted_ = false;
    last_attempt_failure_result_ = EMC_OK;
}

ModHubClient::AttemptResult ModHubClient::OnStartup()
{
    Reset();

    const AttemptResult result = AttemptAttachAndRegister(false);
    if (result == ATTACH_FAILED)
    {
        attach_retry_pending_ = true;
    }

    return result;
}

ModHubClient::AttemptResult ModHubClient::OnOptionsWindowInit()
{
    if (!attach_retry_pending_ || attach_retry_attempted_)
    {
        return use_hub_ui_ ? ATTACH_SUCCESS : ATTACH_FAILED;
    }

    attach_retry_attempted_ = true;
    attach_retry_pending_ = false;
    return AttemptAttachAndRegister(true);
}

bool ModHubClient::UseHubUi() const
{
    return use_hub_ui_;
}

bool ModHubClient::IsAttachRetryPending() const
{
    return attach_retry_pending_;
}

bool ModHubClient::HasAttachRetryAttempted() const
{
    return attach_retry_attempted_;
}

EMC_Result ModHubClient::LastAttemptFailureResult() const
{
    return last_attempt_failure_result_;
}

ModHubClient::AttemptResult ModHubClient::AttemptAttachAndRegister(bool is_retry)
{
    if (config_.register_fn == 0)
    {
        use_hub_ui_ = false;
        last_attempt_failure_result_ = EMC_ERR_INVALID_ARGUMENT;
        return INVALID_CONFIGURATION;
    }

    if (config_.should_force_attach_failure_fn != 0)
    {
        EMC_Result forced_result = EMC_ERR_INTERNAL;
        if (config_.should_force_attach_failure_fn(config_.attach_failure_user_data, is_retry, &forced_result))
        {
            use_hub_ui_ = false;
            last_attempt_failure_result_ = forced_result;
            return ATTACH_FAILED;
        }
    }

    const ModHubClientGetApiFn get_api_fn = config_.get_api_fn != 0 ? config_.get_api_fn : &DefaultGetApi;
    const EMC_HubApiV1* api = 0;
    uint32_t api_size = 0u;
    EMC_Result get_api_result = get_api_fn(
        EMC_HUB_API_VERSION_1,
        EMC_HUB_API_V1_MIN_SIZE,
        &api,
        &api_size);
    if (get_api_result != EMC_OK)
    {
        use_hub_ui_ = false;
        last_attempt_failure_result_ = get_api_result;
        return ATTACH_FAILED;
    }

    if (api == 0 || api_size < EMC_HUB_API_V1_MIN_SIZE)
    {
        use_hub_ui_ = false;
        last_attempt_failure_result_ = EMC_ERR_INTERNAL;
        return ATTACH_FAILED;
    }

    EMC_Result register_result = config_.register_fn(api, config_.register_user_data);
    if (register_result != EMC_OK)
    {
        use_hub_ui_ = false;
        last_attempt_failure_result_ = register_result;
        return REGISTRATION_FAILED;
    }

    use_hub_ui_ = true;
    last_attempt_failure_result_ = EMC_OK;
    return ATTACH_SUCCESS;
}
}
