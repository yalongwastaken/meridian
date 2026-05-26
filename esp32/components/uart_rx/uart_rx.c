/**
 * @file uart_rx.c
 * @brief 
 */

#include <string.h>
#include "uart_rx.h"
#include "esp_err.h"
#include "esp_log.h"
#include "sensor_data.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// configuration
#define UART_RX_BUFFER_SIZE 1024
#define UART_QUEUE_SIZE 20
#define UART_BAUDRATE 115200

static const char *TAG = "uart_rx";
static QueueHandle_t uart_event_queue = NULL;

typedef struct {
    uart_port_t port_num;
} uart_rx_config_t;
static uart_rx_config_t uart_rx_config = {.port_num = -1};

esp_err_t uart_rx_init(uart_port_t port, gpio_num_t rx_pin) {
    // error management
    esp_err_t ret;

    // config
    uart_config_t config = {
        .baud_rate = UART_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    ret = uart_param_config(port, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to configure uart");
        return ret;
    }

    // install driver
    ret = uart_driver_install(port, UART_RX_BUFFER_SIZE, 0, UART_QUEUE_SIZE, &uart_event_queue, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to install uart driver");
        return ret;
    }

    // set pins
    ret = uart_set_pin(port, UART_PIN_NO_CHANGE, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to set uart pins");
        return ret;
    }

    // initialize structure
    uart_rx_config.port_num = port;

    ESP_LOGD(TAG, "initialized uart");
    return ESP_OK;
}

void uart_rx_task(void *pvParameters) {
    QueueHandle_t sensor_queue = (QueueHandle_t) pvParameters;
    uart_event_t event;
    uint8_t rx_buf[sizeof(mqtt_sensor_data_t)];

    while (true) {
        if (xQueueReceive(uart_event_queue, &event, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (event.type != UART_DATA) {
            ESP_LOGW(TAG, "unhandled uart event type: %d", event.type);
            continue;
        }

        int bytes_read = uart_read_bytes(
            uart_rx_config.port_num,
            rx_buf,
            sizeof(mqtt_sensor_data_t),
            pdMS_TO_TICKS(100)
        );

        if (bytes_read != sizeof(mqtt_sensor_data_t)) {
            ESP_LOGW(TAG, "incomplete read: got %d/%d bytes", bytes_read, sizeof(mqtt_sensor_data_t));
            continue;
        }

        mqtt_sensor_data_t sensor_data;
        memcpy(&sensor_data, rx_buf, sizeof(mqtt_sensor_data_t));

        ESP_LOGD(TAG, "rx from %s: ax=%.2f ay=%.2f az=%.2f", 
                 sensor_data.node_source,
                 sensor_data.accel_x, sensor_data.accel_y, sensor_data.accel_z);

        if (xQueueSend(sensor_queue, &sensor_data, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGW(TAG, "sensor queue full, dropping packet from %s", sensor_data.node_source);
        }
    }
}

esp_err_t uart_rx_deinit(uart_port_t port) {
    return uart_driver_delete(port);
}