#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"
#include "keemash_weekly_schedule.h"

esp_err_t lamp_schedule_start(void);
esp_err_t lamp_schedule_publisher_start(void);
bool lamp_schedule_execute_command(const char *text, esp_err_t *error,
				     char *reply, size_t reply_size);
void lamp_schedule_get_status(keemash_weekly_schedule_status_t *status);
