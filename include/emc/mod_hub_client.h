#ifndef EMC_MOD_HUB_CLIENT_H
#define EMC_MOD_HUB_CLIENT_H

#include "emc/mod_hub_api.h"

namespace emc
{
typedef EMC_Result(__cdecl* ModHubClientGetApiFn)(
    uint32_t requested_version,
    uint32_t caller_api_size,
    const EMC_HubApiV1** out_api,
    uint32_t* out_api_size);

typedef EMC_Result(__cdecl* ModHubClientRegisterFn)(const EMC_HubApiV1* api, void* user_data);

typedef bool(__cdecl* ModHubClientForceAttachFailureFn)(
    void* user_data,
    bool is_retry,
    EMC_Result* out_result);

class ModHubClient
{
public:
    struct Config
    {
        ModHubClientGetApiFn get_api_fn;
        ModHubClientRegisterFn register_fn;
        void* register_user_data;
        ModHubClientForceAttachFailureFn should_force_attach_failure_fn;
        void* attach_failure_user_data;

        Config();
    };

    enum AttemptResult
    {
        ATTACH_SUCCESS = 0,
        ATTACH_FAILED = 1,
        REGISTRATION_FAILED = 2,
        INVALID_CONFIGURATION = 3
    };

    ModHubClient();
    explicit ModHubClient(const Config& config);

    void SetConfig(const Config& config);
    const Config& GetConfig() const;

    void Reset();

    AttemptResult OnStartup();
    AttemptResult OnOptionsWindowInit();
    bool UseHubUi() const;
    bool IsAttachRetryPending() const;
    bool HasAttachRetryAttempted() const;
    EMC_Result LastAttemptFailureResult() const;

private:
    AttemptResult AttemptAttachAndRegister(bool is_retry);

    Config config_;
    bool use_hub_ui_;
    bool attach_retry_pending_;
    bool attach_retry_attempted_;
    EMC_Result last_attempt_failure_result_;
};
}

#endif
