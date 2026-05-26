/**
 * @file uart_rx.h
 * @brief UART receive component interface for Meridian ESP32 node.
 *        Provides initialization, task entry point, and cleanup
 *        for event-driven UART data reception via ESP-IDF driver.
 */

#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"

/**
 * @brief Initialize the UART peripheral and install the ESP-IDF driver.
 *
 * @param port   UART port number (e.g. UART_NUM_1)
 * @param rx_pin GPIO pin assigned to UART RX
 * @return ESP_OK on success, ESP_FAIL or relevant esp_err_t on failure
 */
esp_err_t uart_rx_init(uart_port_t port, gpio_num_t rx_pin);

/**
 * @brief FreeRTOS task that blocks on UART events and forwards
 *        received data onto the sensor queue.
 *
 * @param pvParameters QueueHandle_t for the sensor queue
 */
void uart_rx_task(void *pvParameters);

/**
 * @brief Uninstall the UART driver and release resources.
 *
 * @param port UART port number to deinitialize
 * @return ESP_OK on success, ESP_FAIL or relevant esp_err_t on failure
 */
esp_err_t uart_rx_deinit(uart_port_t port);