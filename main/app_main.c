/* Control Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
#include "esp_mac.h"
#endif

#include "espnow.h"
#include "espnow_ctrl.h"
#include "espnow_utils.h"

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
#include "driver/rmt.h"
#endif
#include "led_strip.h"

#include "iot_button.h"

// All the default GPIOs are based on ESP32 series DevKitC boards, for other boards, please modify them accordingly.
#ifdef CONFIG_IDF_TARGET_ESP32C2
#define CONTROL_KEY_GPIO      GPIO_NUM_9
#define LED_RED_GPIO          GPIO_NUM_0
#define LED_GREEN_GPIO        GPIO_NUM_1
#define LED_BLUE_GPIO         GPIO_NUM_8
#elif CONFIG_IDF_TARGET_ESP32C3
#define CONTROL_KEY_GPIO      GPIO_NUM_9
#define LED_STRIP_GPIO        GPIO_NUM_8
#elif CONFIG_IDF_TARGET_ESP32
#define CONTROL_KEY_GPIO      GPIO_NUM_0
// There is not LED module in ESP32 DevKitC board, so you need to connect one by yourself.
#define LED_STRIP_GPIO        GPIO_NUM_18
#elif CONFIG_IDF_TARGET_ESP32S2
#define CONTROL_KEY_GPIO      GPIO_NUM_0
#define LED_STRIP_GPIO        GPIO_NUM_18
#elif CONFIG_IDF_TARGET_ESP32S3
#define CONTROL_KEY_GPIO      GPIO_NUM_0
// For old version board, the number is 48.
#define LED_STRIP_GPIO        GPIO_NUM_38
#elif CONFIG_IDF_TARGET_ESP32C6
#define CONTROL_KEY_GPIO      GPIO_NUM_9
#define LED_STRIP_GPIO        GPIO_NUM_8
#endif

static const char *TAG = "app_main";

typedef enum {
    APP_ESPNOW_CTRL_INIT,
    APP_ESPNOW_CTRL_BOUND,
    APP_ESPNOW_CTRL_MAX
} app_espnow_ctrl_status_t;

static app_espnow_ctrl_status_t s_espnow_ctrl_status = APP_ESPNOW_CTRL_INIT;


static char *bind_error_to_string(espnow_ctrl_bind_error_t bind_error)
{
    switch (bind_error) {
    case ESPNOW_BIND_ERROR_NONE: {
        return "No error";
        break;
    }

    case ESPNOW_BIND_ERROR_TIMEOUT: {
        return "bind timeout";
        break;
    }

    case ESPNOW_BIND_ERROR_RSSI: {
        return "bind packet RSSI below expected threshold";
        break;
    }

    case ESPNOW_BIND_ERROR_LIST_FULL: {
        return "bindlist is full";
        break;
    }

    default: {
        return "unknown error";
        break;
    }
    }
}

static void app_wifi_init()
{
    esp_event_loop_create_default();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void app_initiator_send_press_cb(void *arg, void *usr_data)
{
    static bool status = 0;

    ESP_ERROR_CHECK(!(BUTTON_SINGLE_CLICK == iot_button_get_event(arg)));

    if (s_espnow_ctrl_status == APP_ESPNOW_CTRL_BOUND) {
        ESP_LOGI(TAG, "initiator send press");
        espnow_ctrl_initiator_send(ESPNOW_ATTRIBUTE_KEY_1, ESPNOW_ATTRIBUTE_POWER, status);
        status = !status;
    } else {
        ESP_LOGI(TAG, "please double click to bind the devices firstly");
    }
}


static void app_driver_init(void)
{
    button_config_t button_config = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .gpio_num = CONTROL_KEY_GPIO,
            .active_level = 0,
        },
    };

    button_handle_t button_handle = iot_button_create(&button_config);

    iot_button_register_cb(button_handle, BUTTON_SINGLE_CLICK, app_initiator_send_press_cb, NULL);
}

static void app_responder_init(void)
{
    ESP_ERROR_CHECK(espnow_ctrl_responder_bind(30 * 1000, -55, NULL));
}

static void app_espnow_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    if (base != ESP_EVENT_ESPNOW) {
        return;
    }

    switch (id) {
    case ESP_EVENT_ESPNOW_CTRL_BIND: {
        espnow_ctrl_bind_info_t *info = (espnow_ctrl_bind_info_t *)event_data;
        ESP_LOGI(TAG, "bind, uuid: " MACSTR ", initiator_type: %d", MAC2STR(info->mac), info->initiator_attribute);

        //app_led_set_color(0, 255, 0);
        break;
    }

    case ESP_EVENT_ESPNOW_CTRL_BIND_ERROR: {
        espnow_ctrl_bind_error_t *bind_error = (espnow_ctrl_bind_error_t *)event_data;
        ESP_LOGW(TAG, "bind error: %s", bind_error_to_string(*bind_error));
        break;
    }

    case ESP_EVENT_ESPNOW_CTRL_UNBIND: {
        espnow_ctrl_bind_info_t *info = (espnow_ctrl_bind_info_t *)event_data;
        ESP_LOGI(TAG, "unbind, uuid: " MACSTR ", initiator_type: %d", MAC2STR(info->mac), info->initiator_attribute);

        //app_led_set_color(255, 0, 0);
        break;
    }

    default:
        break;
    }
}

void app_main(void)
{
    espnow_storage_init();

    app_wifi_init();
    app_driver_init();

    espnow_config_t espnow_config = ESPNOW_INIT_CONFIG_DEFAULT();
    espnow_init(&espnow_config);

    esp_event_handler_register(ESP_EVENT_ESPNOW, ESP_EVENT_ANY_ID, app_espnow_event_handler, NULL);

    app_responder_init();
}