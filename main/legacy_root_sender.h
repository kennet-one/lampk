// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdbool.h>
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LEGACY_ROOT_MSG_MAX_LEN  32

// Kept for source compatibility. Shared TX broker owns physical transmission.
void legacy_root_sender_start(UBaseType_t prio);

bool legacy_send_to_root(const char *text);

#ifdef __cplusplus
}
#endif
