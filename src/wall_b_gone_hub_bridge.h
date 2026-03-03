#ifndef EMC_WALL_B_GONE_HUB_BRIDGE_H
#define EMC_WALL_B_GONE_HUB_BRIDGE_H

#include <stdint.h>

void WallBGoneHubBridge_OnPluginStart();
void WallBGoneHubBridge_OnOptionsWindowInit();

bool WallBGoneHubBridge_UseHubUi();
bool WallBGoneHubBridge_IsAttachRetryPending();
bool WallBGoneHubBridge_HasAttachRetryAttempted();

void WallBGoneHubBridge_Test_SetAttachFailureMode(int32_t mode);
void WallBGoneHubBridge_Test_ResetRuntimeState();

#endif
