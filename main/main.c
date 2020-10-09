#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "driver/gpio.h"

#define LED_AZUL					GPIO_NUM_12
#define LED_VERDE 					GPIO_NUM_13
#define LED_VERMELHO 				GPIO_NUM_15
#define BUTTON 						GPIO_NUM_16
#define WIFI_SSID      	            "LeonardoBagio"
#define WIFI_PASSWORD             	"monstrinho"
#define EXAMPLE_ESP_MAXIMUM_RETRY  	5

static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_CONNECTING_BIT BIT1
#define WIFI_FAIL_BIT       BIT2

void task_controle_botao();
void task_controle_led();

static const char *TAG = "STATUS_SISTEMA";
static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTING_BIT);
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        xEventGroupClearBits(s_wifi_event_group, WIFI_FAIL_BIT);
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTING_BIT);
            xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            xEventGroupClearBits(s_wifi_event_group, WIFI_FAIL_BIT);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTING_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTING_BIT);
        xEventGroupClearBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
}

void iniciar_wifi(void) {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
	        .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "iniciar_wifi finished.");

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        portMAX_DELAY
    );

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", WIFI_SSID, WIFI_PASSWORD);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", WIFI_SSID, WIFI_PASSWORD);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

void iniciar_gpio(){
    gpio_pad_select_gpio(LED_VERMELHO);
    gpio_pad_select_gpio(LED_VERDE);
    gpio_pad_select_gpio(LED_AZUL);
    gpio_set_direction(LED_VERMELHO, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_VERDE, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_AZUL, GPIO_MODE_OUTPUT);
    gpio_pad_select_gpio(BUTTON);
    gpio_set_direction(BUTTON, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON, GPIO_PULLUP_ONLY);    
}

void task_controle_botao() {
    ESP_LOGW(TAG, "task_controle_botao iniciou com sucesso.");

    while (true) {
        xEventGroupWaitBits(s_wifi_event_group, WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

        if (!gpio_get_level(BUTTON)) {
            gpio_set_level(LED_AZUL, 1);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            gpio_set_level(LED_AZUL, 0);
            s_retry_num = 0;
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTING_BIT);
            xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            xEventGroupClearBits(s_wifi_event_group, WIFI_FAIL_BIT);
            esp_wifi_connect();
        }

        vTaskDelay(250 / portTICK_PERIOD_MS);
    }
}

void task_controle_led() {
    ESP_LOGW(TAG, "task_controle_led iniciou com sucesso.");

    bool led_state = 1;
    int delay_time = 500;

    while (true) {
        EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);

        if (bits & WIFI_CONNECTING_BIT) {
            delay_time = 500;
            gpio_set_level(LED_VERDE, 0);
            gpio_set_level(LED_VERMELHO, led_state);
            led_state = !led_state;
        }
        
        if (bits & WIFI_CONNECTED_BIT) {
            gpio_set_level(LED_VERMELHO, 0);
            gpio_set_level(LED_VERDE, 1);
        }
        
        if (bits & WIFI_FAIL_BIT) {
            delay_time = 100;
            gpio_set_level(LED_VERDE, 0);
            gpio_set_level(LED_VERMELHO, led_state);
            led_state = !led_state;
        }

        vTaskDelay(delay_time / portTICK_PERIOD_MS);
    }
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    iniciar_gpio();
    iniciar_wifi();
    xTaskCreatePinnedToCore(task_controle_botao, "", 2048, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(task_controle_led, "", 2048, NULL, 1, NULL, 0);
}
