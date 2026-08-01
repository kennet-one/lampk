// SPDX-License-Identifier: Apache-2.0

#include "lamp_node.h"

#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define RELAY_GPIO ((gpio_num_t)CONFIG_LAMPK_RELAY_GPIO)
#define BUTTON_GPIO ((gpio_num_t)CONFIG_LAMPK_BUTTON_GPIO)

#ifdef CONFIG_LAMPK_RELAY_DRIVE_ACTIVE_HIGH
#define RELAY_ACTIVE_LEVEL 1
#else
#define RELAY_ACTIVE_LEVEL 0
#endif

static const char *TAG = "lamp";
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_initialized;
static bool s_state;
static lamp_node_event_cb_t s_event_callback;
static void *s_event_user;

static int relay_level(bool enabled)
{
	return enabled ? RELAY_ACTIVE_LEVEL : !RELAY_ACTIVE_LEVEL;
}

static void format_state_reply(char *reply, size_t reply_size)
{
	if (!reply || reply_size == 0) return;
	snprintf(reply, reply_size, "La%u", lamp_node_state() ? 1U : 0U);
	reply[reply_size - 1] = '\0';
}

static esp_err_t set_state(bool enabled)
{
	esp_err_t err = gpio_set_level(RELAY_GPIO, relay_level(enabled));
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "relay GPIO%d write failed: %s", (int)RELAY_GPIO,
			 esp_err_to_name(err));
		return err;
	}
	portENTER_CRITICAL(&s_lock);
	s_state = enabled;
	portEXIT_CRITICAL(&s_lock);
	ESP_LOGI(TAG, "relay GPIO%d=%u state=%s", (int)RELAY_GPIO,
		 (unsigned)relay_level(enabled), enabled ? "ON" : "OFF");
	return ESP_OK;
}

static esp_err_t toggle_state(void)
{
	return set_state(!lamp_node_state());
}

static void publish_button_event(void)
{
	char reply[8];
	format_state_reply(reply, sizeof(reply));

	portENTER_CRITICAL(&s_lock);
	lamp_node_event_cb_t callback = s_event_callback;
	void *user = s_event_user;
	portEXIT_CRITICAL(&s_lock);
	if (callback) callback(reply, user);
}

static void button_task(void *arg)
{
	(void)arg;
	int raw = gpio_get_level(BUTTON_GPIO);
	int stable = raw;
	TickType_t changed_at = xTaskGetTickCount();

	for (;;) {
		vTaskDelay(pdMS_TO_TICKS(CONFIG_LAMPK_BUTTON_POLL_MS));
		int sample = gpio_get_level(BUTTON_GPIO);
		TickType_t now = xTaskGetTickCount();
		if (sample != raw) {
			raw = sample;
			changed_at = now;
			continue;
		}
		if (sample == stable ||
		    (now - changed_at) < pdMS_TO_TICKS(CONFIG_LAMPK_BUTTON_DEBOUNCE_MS)) {
			continue;
		}

		stable = sample;
		if (stable == 0) {
			if (toggle_state() == ESP_OK) publish_button_event();
		}
	}
}

esp_err_t lamp_node_init(void)
{
	if (s_initialized) return ESP_OK;

	/* External pulldown is the hardware fail-safe; set the inactive level first. */
	esp_err_t err = gpio_set_level(RELAY_GPIO, relay_level(false));
	if (err != ESP_OK) return err;
	gpio_config_t relay_config = {
		.pin_bit_mask = 1ULL << RELAY_GPIO,
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE,
	};
	if ((err = gpio_config(&relay_config)) != ESP_OK) return err;
	if ((err = gpio_set_level(RELAY_GPIO, relay_level(false))) != ESP_OK) return err;

	gpio_config_t button_config = {
		.pin_bit_mask = 1ULL << BUTTON_GPIO,
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_ENABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE,
	};
	if ((err = gpio_config(&button_config)) != ESP_OK) return err;

	portENTER_CRITICAL(&s_lock);
	s_state = false;
	s_initialized = true;
	portEXIT_CRITICAL(&s_lock);

	if (xTaskCreate(button_task, "lamp_button", 2048, NULL, 5, NULL) != pdPASS) {
		portENTER_CRITICAL(&s_lock);
		s_initialized = false;
		portEXIT_CRITICAL(&s_lock);
		return ESP_ERR_NO_MEM;
	}

	ESP_LOGI(TAG, "relay GPIO%d initialized OFF, button GPIO%d active low",
		 (int)RELAY_GPIO, (int)BUTTON_GPIO);
	return ESP_OK;
}

void lamp_node_set_event_callback(lamp_node_event_cb_t callback, void *user)
{
	portENTER_CRITICAL(&s_lock);
	s_event_callback = callback;
	s_event_user = user;
	portEXIT_CRITICAL(&s_lock);
}

bool lamp_node_state(void)
{
	portENTER_CRITICAL(&s_lock);
	bool state = s_state;
	portEXIT_CRITICAL(&s_lock);
	return state;
}

bool lamp_node_handle_command(const char *text, char *reply, size_t reply_size)
{
	if (!text || !reply || reply_size == 0 || !s_initialized) return false;

	if (strcmp(text, "lam") == 0) {
		(void)toggle_state();
	} else if (strcmp(text, "lamech") == 0) {
		/* Read-only state query. */
	} else if (strcmp(text, "lampk.status") == 0) {
		snprintf(reply, reply_size, "lamp=%u button=%u",
			 lamp_node_state() ? 1U : 0U,
			 gpio_get_level(BUTTON_GPIO) == 0 ? 1U : 0U);
		reply[reply_size - 1] = '\0';
		return true;
	} else {
		return false;
	}

	format_state_reply(reply, reply_size);
	return true;
}
