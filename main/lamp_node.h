// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

typedef void (*lamp_node_event_cb_t)(const char *reply, void *user);

esp_err_t lamp_node_init(void);
void lamp_node_set_event_callback(lamp_node_event_cb_t callback, void *user);
bool lamp_node_handle_command(const char *text, char *reply, size_t reply_size);
bool lamp_node_state(void);
