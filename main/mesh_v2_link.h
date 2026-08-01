// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

esp_err_t mesh_v2_link_init(const char *tag, bool relay_eligible);
void mesh_v2_link_parent_connected(void);
void mesh_v2_link_parent_disconnected(void);
esp_err_t mesh_manual_reboot_schedule(uint16_t delay_ms, const char *reason);
