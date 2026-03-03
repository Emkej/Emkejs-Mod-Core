#ifndef EMC_HUB_REGISTRY_H
#define EMC_HUB_REGISTRY_H

#include "emc/mod_hub_api.h"

EMC_Result __cdecl HubRegistry_RegisterMod(const EMC_ModDescriptorV1* desc, EMC_ModHandle* out_handle);
EMC_Result __cdecl HubRegistry_RegisterBoolSetting(EMC_ModHandle mod, const EMC_BoolSettingDefV1* def);
EMC_Result __cdecl HubRegistry_RegisterKeybindSetting(EMC_ModHandle mod, const EMC_KeybindSettingDefV1* def);
EMC_Result __cdecl HubRegistry_RegisterIntSetting(EMC_ModHandle mod, const EMC_IntSettingDefV1* def);
EMC_Result __cdecl HubRegistry_RegisterFloatSetting(EMC_ModHandle mod, const EMC_FloatSettingDefV1* def);
EMC_Result __cdecl HubRegistry_RegisterActionRow(EMC_ModHandle mod, const EMC_ActionRowDefV1* def);

void HubRegistry_SetRegistrationLocked(bool is_locked);
bool HubRegistry_IsRegistrationLocked();

#endif
