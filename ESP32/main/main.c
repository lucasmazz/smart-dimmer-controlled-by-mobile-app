#include <netdb.h>
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "nvs_flash.h"

/* Wifi Config */
#define WIFI_SSID "DIMMER"
#define WIFI_PASS "password"
#define WIFI_CHANNEL 1
#define MAX_STA_CONN 1

/* IP config for the WiFi AP */
#define STATIC_IP_ADDR "192.168.1.1"
#define GATEWAY_ADDR "192.168.1.1"
#define NETMASK_ADDR "255.255.255.0"

/* Interruption and GPIO */
#define ESP_INTR_FLAG_DEFAULT 0
#define INPUT_PIN GPIO_NUM_27
#define OUTPUT_PIN GPIO_NUM_33

/* Task Handle */
static TaskHandle_t task_handle = NULL;

/* Timestamps used in logic flow and trigger calculation */
static uint64_t rising_time = 0;
static uint64_t falling_time = 0;
static uint64_t zero_crossing_time = 0;
static uint64_t trigger_time = 0;

/* Powergrid sine period in us */
static uint16_t period = 0;

/* Brightness intensity in percentage */
static uint8_t brightness = 0;

/* Flags used in the logic flow */
static bool is_crossing_zero = false;
static bool is_triggering = false;

/* Timer used to trigger the TRIAC */
static esp_timer_handle_t trigger_timer;

/**
 * @brief Interrupt Service Routine (ISR) for zero-crossing detection.
 *
 * This ISR is triggered on both rising and falling edges of the input signal. 
 * It manages the timing and triggering of actions based on zero-crossing 
 * events, adjusting the timer and updating the state variables accordingly.
 *
 * @param arg Not used in this implementation.
 */
void IRAM_ATTR crossing_zero_isr_handler(void *arg)
{
    esp_err_t ret;

    const uint64_t current_time = esp_timer_get_time();
    const bool current_state = gpio_get_level(INPUT_PIN);

    /* Rising edge detected */
    if (current_state && !is_crossing_zero) {
        /* Turnoff the active trigger */
        ret = gpio_set_level(OUTPUT_PIN, 0);
        ESP_ERROR_CHECK(ret);

        /* If there is no trigger active and trigger isn't in the dead zone */
        if (!is_triggering && trigger_time < period) {
            ret = esp_timer_start_once(trigger_timer, trigger_time);
            ESP_ERROR_CHECK(ret);

            is_triggering = true;
        }

        /* Store the period in microseconds to calculate the trigger time */
        period = current_time - rising_time;
        rising_time = current_time;

    /* Falling edge detected */
    } else if (!current_state && is_crossing_zero) {
        /* Store the falling time to estimate the zero-crossing time */
        falling_time = current_time;
    }

    /* Update the zero-crossing state */
    is_crossing_zero = current_state;

    /* Resume a task from ISR */
    xTaskResumeFromISR(task_handle);
}

/**
 * @brief Callback function for the trigger timer.
 *
 * This function is called when the timer expires. It toggles the output 
 * pin to generate a trigger signal. After the sequence, the 
 * `is_triggering` flag is set to false.
 *
 * @param arg Not used in this implementation.
 */
static void trigger_timer_callback(void *arg)
{
    esp_err_t ret;

    /* Activate the trigger */
    ret = gpio_set_level(OUTPUT_PIN, 1);
    ESP_ERROR_CHECK(ret);
    is_triggering = false;
}

/**
 * @brief Controls the operation of a smart dimmer system.
 *
 * This function initializes and manages the operation of a smart dimmer 
 * system. It configures timers, GPIO pins, interrupt service routines, 
 * and tasks to controlthe brightness of a lighting system based on 
 * zero-crossing detection. The system adjusts the brightness of the lights 
 * according to the detected brightness level and zero-crossing timing.
 *
 * @param arg Not used in this implementation.
 */
void smart_dimmer_control(void *arg)
{
    esp_err_t ret;

    /* Timer configuration structure and creation */
    const esp_timer_create_args_t trigger_timer_args = {
        .callback = &trigger_timer_callback, 
        .name = "trigger"
    };

    ret = esp_timer_create(&trigger_timer_args, &trigger_timer);
    ESP_ERROR_CHECK(ret);

    /* Configure GPIO input */
    esp_rom_gpio_pad_select_gpio(INPUT_PIN);
    ret = gpio_set_direction(INPUT_PIN, GPIO_MODE_INPUT);
    ESP_ERROR_CHECK(ret);

    ret = gpio_set_intr_type(INPUT_PIN, GPIO_INTR_ANYEDGE);
    ESP_ERROR_CHECK(ret);

    ret = gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    ESP_ERROR_CHECK(ret);

    /* Add the ISR handler for the specified GPIO pin */
    ret = gpio_isr_handler_add(INPUT_PIN, crossing_zero_isr_handler, NULL);
    ESP_ERROR_CHECK(ret);

    /* Configure GPIO output */
    ret = gpio_set_direction(OUTPUT_PIN, GPIO_MODE_OUTPUT);
    ESP_ERROR_CHECK(ret);

    /* Infinity loop */
    for (;;) {
        /* Suspend the task until it is resumed externally */
        vTaskSuspend(NULL);

        if (is_crossing_zero) {
            /* Calculate trigger time based on zero-crossing detection */
            trigger_time = (uint64_t)((1 - brightness / 100.0f) * period +
                                      zero_crossing_time);

        } else if (!is_crossing_zero && rising_time != 0 && falling_time != 0) {
            /* Calculate zero-crossing time */
            zero_crossing_time = (uint64_t)((falling_time - rising_time) / 2);
        }
    }
}

/**
 * @brief Handles HTTP GET requests, extracts a "brightness" query parameter, 
 * and responds with the brightness value.
 *
 * This function processes incoming HTTP GET requests to extract the 
 * "brightness" parameter from the URL query string.
 *
 * @param req Pointer to the HTTP request.
 * 
 * @return ESP_OK on success.
 */
static esp_err_t http_request_handler(httpd_req_t *req)
{
    esp_err_t ret;

    char buffer[16];
    size_t buffer_length;

    buffer_length = httpd_req_get_url_query_len(req) + 1;

    /* Extracts the query string into a buffer */
    if ((buffer_length > 1) && (buffer_length <= sizeof(buffer))) {

        ret = httpd_req_get_url_query_str(req, buffer, buffer_length);
        
        if (ret == ESP_OK) {
            char param[5] = { 0 };

            ret = httpd_query_key_value(buffer, "brightness", 
                                        param, sizeof(param));

            /* Converts the "brightness" parameter value to an integer */
            if (ret == ESP_OK) {
                int value = atoi(param);

                /* Ensure value is within range 0-100 */
                if (value > 100) {
                    brightness = 100;
                } else if (value < 0) {
                    brightness = 0;
                } else {
                    brightness = (uint8_t)value;
                }
            }
        }
    }

    /* Buffer to hold brightness value (up to 3 digits + null terminator) */
    char response_buffer[5];

    snprintf(response_buffer, sizeof(response_buffer), "%d", brightness);

    /* Send current brightness value as response */
    ret = httpd_resp_send(req, response_buffer, strlen(response_buffer));
    ESP_ERROR_CHECK(ret);

    return ESP_OK;
}

/**
 * @brief Initializes and starts the HTTP server.
 *
 * @return void
 */
static void http_server_init(void)
{
    esp_err_t ret;

    /* Creates an HTTP server handle and default server configuration */
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root_uri = { 
            .uri = "/",
            .method = HTTP_GET,
            .handler = http_request_handler,
            .user_ctx = NULL 
        };

        /* Registers a URI handler for the root ("/") endpoint */
        ret = httpd_register_uri_handler(server, &root_uri);
        ESP_ERROR_CHECK(ret);

    } else {
        ESP_LOGE("HTTP_SERVER", "Failed to start server");
    }
}

/**
 * @brief Initializes the Wi-Fi access point (AP) mode with static IP 
 * configuration.
 * 
 * @return void
 */
static void wifi_ap_init(void)
{
    esp_err_t ret;

    /* Initialize the network interface */
    ret = esp_netif_init();
    ESP_ERROR_CHECK(ret);

    /* Create the default event loop */
    ret = esp_event_loop_create_default();
    ESP_ERROR_CHECK(ret);

    /* Create the default Wi-Fi AP network interface */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    ESP_ERROR_CHECK(ret);

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = WIFI_CHANNEL,
            .password = WIFI_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };

    /* Initializes the Wi-Fi driver with the default configuration */
    ret = esp_wifi_set_mode(WIFI_MODE_AP);
    ESP_ERROR_CHECK(ret);

    ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    ESP_ERROR_CHECK(ret);

    /* Set static IP information */
    esp_netif_ip_info_t ip_info;

    memset(&ip_info, 0, sizeof(esp_netif_ip_info_t));

    ip_info.ip.addr = ipaddr_addr(STATIC_IP_ADDR);
    ip_info.gw.addr = ipaddr_addr(GATEWAY_ADDR);
    ip_info.netmask.addr = ipaddr_addr(NETMASK_ADDR);

    /* Keep the netif instance for setting IP info */
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();

    /* Stop DHCP server before setting static IP info */
    ret = esp_netif_dhcps_stop(ap_netif);
    ESP_ERROR_CHECK(ret);

    ret = esp_netif_set_ip_info(ap_netif, &ip_info);
    ESP_ERROR_CHECK(ret);

    /* Start DHCP server to assign IPs to other connected devices */
    ret = esp_netif_dhcps_start(ap_netif);
    ESP_ERROR_CHECK(ret);

    ret = esp_wifi_start();
    ESP_ERROR_CHECK(ret);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {

        ret = nvs_flash_erase();
        ESP_ERROR_CHECK(ret);
        
        ret = nvs_flash_init();
        ESP_ERROR_CHECK(ret);
    }

    /* Initialize WiFi in AP mode */
    wifi_ap_init();

    /* Initialize the HTTP server */
    http_server_init();

    /* Run the trigger configuration and calculations in a dedicated core */
    xTaskCreatePinnedToCore(smart_dimmer_control, "smart_dimmer_control",
                            configMINIMAL_STACK_SIZE, NULL,
                            configMAX_PRIORITIES - 1, &task_handle, 1);
}
