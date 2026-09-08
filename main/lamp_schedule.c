#include "lamp_schedule.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "keemash_mesh_node.h"
#include "lamp_node.h"

#define LAMP_SCHEDULE_BLOB_MAGIC 0x31534c4bUL
#define LAMP_SCHEDULE_ACTION_OFF 0U
#define LAMP_SCHEDULE_ACTION_ON 1U
#define STATUS_POLL_MS 500U
#define STATUS_HEARTBEAT_MS 30000U
#define STATUS_RETRY_MS 5000U
#define TIME_SYNC_STALE_MS 60000U

static const char *TAG = "lamp_sched";
static keemash_weekly_schedule_t *s_schedule;
static TaskHandle_t s_publisher_task;

static uint64_t now_ms(void)
{
	return (uint64_t)esp_timer_get_time() / 1000ULL;
}

static bool parse_hex_field(const char *text, size_t offset, size_t digits,
			    uint32_t *value)
{
	if (!text || !value || digits == 0 || digits > 8) return false;
	uint32_t parsed = 0;
	for (size_t i = 0; i < digits; i++) {
		char c = text[offset + i];
		uint8_t nibble;
		if (c >= '0' && c <= '9') nibble = (uint8_t)(c - '0');
		else if (c >= 'a' && c <= 'f') nibble = (uint8_t)(c - 'a' + 10);
		else if (c >= 'A' && c <= 'F') nibble = (uint8_t)(c - 'A' + 10);
		else return false;
		parsed = (parsed << 4) | nibble;
	}
	*value = parsed;
	return true;
}

static esp_err_t apply_point(void *user,
			     const keemash_weekly_schedule_point_t *point,
			     uint8_t index, bool catch_up)
{
	(void)user;
	if (!point || point->action > LAMP_SCHEDULE_ACTION_ON) {
		return ESP_ERR_INVALID_ARG;
	}
	esp_err_t err = lamp_node_set_state(point->action == LAMP_SCHEDULE_ACTION_ON);
	if (err == ESP_OK) {
		ESP_LOGI(TAG, "%s point %u applied state=%s",
			 catch_up ? "catch-up" : "scheduled", (unsigned)index,
			 point->action == LAMP_SCHEDULE_ACTION_ON ? "ON" : "OFF");
	}
	return err;
}

static void format_meta(char *out, size_t out_size,
			const keemash_weekly_schedule_status_t *status)
{
	uint8_t active = status->active_index == KEEMASH_WEEKLY_SCHEDULE_NO_INDEX
		? 0x0fU : status->active_index;
	uint8_t next = status->next_index == KEEMASH_WEEKLY_SCHEDULE_NO_INDEX
		? 0x0fU : status->next_index;
	snprintf(out, out_size, "LSM%08" PRIX32 "%X%u%u%u%X%X%u",
		 status->config.generation, (unsigned)status->config.count,
		 status->config.enabled ? 1U : 0U,
		 status->config.persistence_enabled ? 1U : 0U,
		 status->clock_valid ? 1U : 0U, (unsigned)active, (unsigned)next,
		 lamp_node_state() ? 1U : 0U);
}

static void format_point(char *out, size_t out_size, uint32_t generation,
			 uint8_t index,
			 const keemash_weekly_schedule_point_t *point)
{
	snprintf(out, out_size, "LSP%08" PRIX32 "%X%u%03X%u%02X",
		 generation, (unsigned)index, point->enabled ? 1U : 0U,
		 (unsigned)point->minute_of_day, (unsigned)point->action,
		 (unsigned)point->days_mask);
}

static void format_diagnostic(char *out, size_t out_size,
			      const keemash_weekly_schedule_status_t *status)
{
	uint8_t flags = (status->clock_valid ? 1U : 0U) |
		(status->config.enabled ? 2U : 0U) |
		(status->config.persistence_enabled ? 4U : 0U) |
		(status->catch_up_pending ? 8U : 0U) |
		(status->last_apply_valid ? 16U : 0U) |
		(status->time_sync_age_ms > TIME_SYNC_STALE_MS ? 32U : 0U);
	uint8_t weekday = status->local_weekday <= 6U ? status->local_weekday : 0x0fU;
	uint16_t minute = status->local_minute < 1440U ? status->local_minute : 0x0fffU;
	uint8_t last = status->last_apply_valid && status->last_apply_index < 8U ?
		status->last_apply_index : 0x0fU;
	uint8_t kind = status->last_apply_valid ? status->last_apply_kind : 0U;
	uint32_t age_s = status->last_apply_valid ? status->last_apply_age_ms / 1000U :
		0xffffU;
	if (age_s > 0xffffU) age_s = 0xffffU;
	snprintf(out, out_size, "LSD%08" PRIX32 "%02X%X%03X%X%X%04X%04X",
		 status->config.generation, (unsigned)flags, (unsigned)weekday,
		 (unsigned)minute, (unsigned)last, (unsigned)kind,
		 (unsigned)((uint32_t)status->last_error & 0xffffU), (unsigned)age_s);
}

static bool publish_token(const char *token)
{
	return mesh_v2_node_send_event(0, token) == ESP_OK;
}

static void publisher_task(void *arg)
{
	(void)arg;
	char last_meta[32] = {0};
	char last_diag_key[32] = {0};
	bool last_output = false;
	bool have_output = false;
	uint64_t last_sent_ms = 0;
	uint64_t last_attempt_ms = 0;

	for (;;) {
		keemash_weekly_schedule_status_t status;
		lamp_schedule_get_status(&status);
		char meta[32] = {0};
		char diagnostic[32] = {0};
		char diagnostic_key[32] = {0};
		char output[16] = {0};
		format_meta(meta, sizeof(meta), &status);
		format_diagnostic(diagnostic, sizeof(diagnostic), &status);
		snprintf(diagnostic_key, sizeof(diagnostic_key), "%08" PRIX32 ":%u:%u:%u:%u:%u:%d",
			 status.config.generation, status.clock_valid ? 1U : 0U,
			 status.catch_up_pending ? 1U : 0U,
			 (unsigned)status.local_minute, (unsigned)status.last_apply_index,
			 (unsigned)status.last_apply_kind, (int)status.last_error);
		bool output_on = lamp_node_state();
		snprintf(output, sizeof(output), "La%u", output_on ? 1U : 0U);
		uint64_t now = now_ms();
		bool changed = strcmp(meta, last_meta) != 0 ||
			strcmp(diagnostic_key, last_diag_key) != 0 ||
			!have_output || output_on != last_output;
		bool heartbeat_due = now - last_sent_ms >= STATUS_HEARTBEAT_MS;
		bool retry_ready = now - last_attempt_ms >= STATUS_RETRY_MS;
		if ((changed || heartbeat_due) && retry_ready) {
			last_attempt_ms = now;
			bool ok = publish_token(output) && publish_token(meta) &&
				publish_token(diagnostic);
			if (ok) {
				snprintf(last_meta, sizeof(last_meta), "%s", meta);
				snprintf(last_diag_key, sizeof(last_diag_key), "%s", diagnostic_key);
				last_output = output_on;
				have_output = true;
				last_sent_ms = now;
			} else {
				ESP_LOGD(TAG, "status submit deferred");
			}
		}
		vTaskDelay(pdMS_TO_TICKS(STATUS_POLL_MS));
	}
}

esp_err_t lamp_schedule_start(void)
{
	if (s_schedule) return ESP_OK;
	keemash_weekly_schedule_options_t options = {
		.nvs_namespace = "lamp",
		.nvs_key = "schedule_v1",
		.blob_magic = LAMP_SCHEDULE_BLOB_MAGIC,
		.max_action = LAMP_SCHEDULE_ACTION_ON,
		.catch_up_on_clock_ready = true,
		.stage_timeout_ms = 30000,
		.task_period_ms = 1000,
		.task_stack_words = 3584,
		.task_priority = 7,
		.task_name = "lamp_sched",
		.apply = apply_point,
	};
	return keemash_weekly_schedule_start(&s_schedule, &options);
}

esp_err_t lamp_schedule_publisher_start(void)
{
	if (s_publisher_task) return ESP_OK;
	if (!s_schedule) return ESP_ERR_INVALID_STATE;
	if (xTaskCreate(publisher_task, "lamp_status", 3072, NULL, 5,
			&s_publisher_task) != pdPASS) {
		s_publisher_task = NULL;
		return ESP_ERR_NO_MEM;
	}
	return ESP_OK;
}

void lamp_schedule_get_status(keemash_weekly_schedule_status_t *status)
{
	keemash_weekly_schedule_get_status(s_schedule, status);
}

bool lamp_schedule_execute_command(const char *text, esp_err_t *error,
				     char *reply, size_t reply_size)
{
	if (!text || !error || !reply || reply_size == 0 || !s_schedule) return false;
	*error = ESP_OK;
	reply[0] = '\0';
	size_t length = strlen(text);
	uint32_t generation = 0;
	if (length == 14 && strncmp(text, "LSB", 3) == 0) {
		uint32_t count = 0, enabled = 0, persist = 0;
		if (!parse_hex_field(text, 3, 8, &generation) || generation == 0 ||
		    !parse_hex_field(text, 11, 1, &count) ||
		    !parse_hex_field(text, 12, 1, &enabled) ||
		    !parse_hex_field(text, 13, 1, &persist) ||
		    count > KEEMASH_WEEKLY_SCHEDULE_MAX_POINTS ||
		    enabled > 1 || persist > 1) {
			*error = ESP_ERR_INVALID_ARG;
		} else {
			*error = keemash_weekly_schedule_stage_begin(
				s_schedule, generation, (uint8_t)count,
				enabled != 0, persist != 0);
		}
	} else if (length == 19 && strncmp(text, "LSP", 3) == 0) {
		uint32_t index = 0, enabled = 0, minute = 0, action = 0, days = 0;
		if (!parse_hex_field(text, 3, 8, &generation) || generation == 0 ||
		    !parse_hex_field(text, 11, 1, &index) ||
		    !parse_hex_field(text, 12, 1, &enabled) ||
		    !parse_hex_field(text, 13, 3, &minute) ||
		    !parse_hex_field(text, 16, 1, &action) ||
		    !parse_hex_field(text, 17, 2, &days) || enabled > 1 ||
		    index >= KEEMASH_WEEKLY_SCHEDULE_MAX_POINTS ||
		    minute >= 24U * 60U || action > LAMP_SCHEDULE_ACTION_ON ||
		    days == 0 || days > KEEMASH_WEEKLY_SCHEDULE_ALL_DAYS) {
			*error = ESP_ERR_INVALID_ARG;
		} else {
			keemash_weekly_schedule_point_t point = {
				.enabled = enabled != 0,
				.minute_of_day = (uint16_t)minute,
				.action = (uint8_t)action,
				.days_mask = (uint8_t)days,
			};
			*error = keemash_weekly_schedule_stage_point(
				s_schedule, generation, (uint8_t)index, &point);
		}
	} else if (length == 11 && strncmp(text, "LSC", 3) == 0) {
		if (!parse_hex_field(text, 3, 8, &generation) || generation == 0) {
			*error = ESP_ERR_INVALID_ARG;
		} else {
			*error = keemash_weekly_schedule_stage_commit(s_schedule, generation);
			if (*error == ESP_OK) {
				keemash_weekly_schedule_status_t status;
				lamp_schedule_get_status(&status);
				format_meta(reply, reply_size, &status);
			}
		}
	} else if (length == 3 && strcmp(text, "LSD") == 0) {
		keemash_weekly_schedule_status_t status;
		lamp_schedule_get_status(&status);
		format_diagnostic(reply, reply_size, &status);
	} else if (length == 3 && strcmp(text, "LSQ") == 0) {
		keemash_weekly_schedule_status_t status;
		lamp_schedule_get_status(&status);
		format_meta(reply, reply_size, &status);
	} else if (length == 4 && strncmp(text, "LSQ", 3) == 0) {
		uint32_t index = 0;
		keemash_weekly_schedule_status_t status;
		lamp_schedule_get_status(&status);
		if (!parse_hex_field(text, 3, 1, &index) || index >= status.config.count) {
			*error = ESP_ERR_NOT_FOUND;
		} else {
			format_point(reply, reply_size, status.config.generation,
				     (uint8_t)index, &status.config.points[index]);
		}
	} else {
		return false;
	}

	if (*error != ESP_OK && reply[0] == '\0') {
		snprintf(reply, reply_size, "ERR:LS:%d", (int)*error);
	}
	reply[reply_size - 1] = '\0';
	return true;
}
