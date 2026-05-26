/**
 * @file test_uart_rx.c
 * @brief Unity integration tests for the uart_rx component.
 *
 * Hardware setup (required for task tests):
 * -----------------------------------------
 * These tests use a physical UART loopback to simulate an external device
 * transmitting mqtt_sensor_data_t packets to the ESP32.
 *
 * Wiring:
 *   GPIO1 (UART0 TX)  ──── GPIO4 (UART1 RX = TEST_RX_PIN)
 *
 * UART0 is used as the injector: uart_write_bytes(UART_NUM_0, ...) sends
 * bytes out of GPIO1, which arrive at GPIO4 as if sent by a real sensor node.
 * UART1 is the receiver under test, driven by uart_rx_init/uart_rx_task.
 *
 * Note: GPIO1 is also the USB serial TX on most devkits. If it conflicts,
 * remap UART0 TX to a free GPIO with uart_set_pin(UART_NUM_0, GPIO_NUM_X,
 * UART_PIN_NO_CHANGE, ...) before running tests.
 *
 * Test coverage:
 * -----------------------------------------
 * init:
 *   - happy path: valid port and pin initializes without error
 *   - invalid port: UART_NUM_MAX rejected by driver
 *   - invalid pin: GPIO_NUM_NC rejected by uart_set_pin
 *   - double init: second install on same port returns error
 *
 * task:
 *   - full packet: 44 bytes arrive in one burst → one mqtt_sensor_data_t
 *     enqueued with correct field values
 *   - partial packet: 44 bytes split into two halves with a 50ms gap →
 *     task accumulates across two UART_DATA events and still enqueues
 *     exactly one valid struct (validates split-packet accumulation logic)
 *
 * deinit:
 *   - after init: driver deleted cleanly
 *   - without init: deleting uninstalled driver fails gracefully
 *   - reinit after deinit: re-initialization succeeds after clean teardown
 */

#include "unity.h"
#include "esp_err.h"
#include "uart_rx.h"
#include "sensor_data.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

#define TEST_UART_PORT  UART_NUM_1
#define TEST_RX_PIN     GPIO_NUM_4
#define QUEUE_SIZE      10

// ─── helpers ─────────────────────────────────────────────────────────────────

static QueueHandle_t create_sensor_queue(void) {
    return xQueueCreate(QUEUE_SIZE, sizeof(mqtt_sensor_data_t));
}

static mqtt_sensor_data_t make_dummy_packet(const char *node) {
    mqtt_sensor_data_t pkt = {
        .accel_x = 1.1f, .accel_y = 2.2f, .accel_z = 3.3f,
        .gyro_x  = 0.1f, .gyro_y  = 0.2f, .gyro_z  = 0.3f,
        .timestamp = 123456789,
    };
    strncpy(pkt.node_source, node, sizeof(pkt.node_source) - 1);
    return pkt;
}

// ─── uart_rx_init ─────────────────────────────────────────────────────────────

void test_uart_rx_init_happy_path(void) {
    esp_err_t ret = uart_rx_init(TEST_UART_PORT, TEST_RX_PIN);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    uart_rx_deinit(TEST_UART_PORT);
}

void test_uart_rx_init_invalid_port(void) {
    esp_err_t ret = uart_rx_init(UART_NUM_MAX, TEST_RX_PIN);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);
}

void test_uart_rx_init_invalid_pin(void) {
    esp_err_t ret = uart_rx_init(TEST_UART_PORT, GPIO_NUM_NC);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);
}

void test_uart_rx_init_double_init(void) {
    uart_rx_init(TEST_UART_PORT, TEST_RX_PIN);
    esp_err_t ret = uart_rx_init(TEST_UART_PORT, TEST_RX_PIN);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);
    uart_rx_deinit(TEST_UART_PORT);
}

// ─── uart_rx_task ─────────────────────────────────────────────────────────────

void test_uart_rx_task_full_packet(void) {
    TEST_ASSERT_EQUAL(ESP_OK, uart_rx_init(TEST_UART_PORT, TEST_RX_PIN));

    QueueHandle_t sensor_queue = create_sensor_queue();
    TEST_ASSERT_NOT_NULL(sensor_queue);

    TaskHandle_t task_handle;
    xTaskCreate(uart_rx_task, "uart_rx_task", 4096, sensor_queue, 5, &task_handle);

    // inject one complete packet via loopback wire (GPIO1 → GPIO4)
    mqtt_sensor_data_t tx_pkt = make_dummy_packet("node_A");
    uart_write_bytes(UART_NUM_0, (const char *)&tx_pkt, sizeof(mqtt_sensor_data_t));

    vTaskDelay(pdMS_TO_TICKS(200));

    mqtt_sensor_data_t rx_pkt;
    TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(sensor_queue, &rx_pkt, pdMS_TO_TICKS(100)));
    TEST_ASSERT_EQUAL_STRING("node_A", rx_pkt.node_source);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, tx_pkt.accel_x, rx_pkt.accel_x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, tx_pkt.accel_y, rx_pkt.accel_y);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, tx_pkt.accel_z, rx_pkt.accel_z);
    TEST_ASSERT_EQUAL_UINT32(tx_pkt.timestamp, rx_pkt.timestamp);

    vTaskDelete(task_handle);
    vQueueDelete(sensor_queue);
    uart_rx_deinit(TEST_UART_PORT);
}

void test_uart_rx_task_partial_packet(void) {
    TEST_ASSERT_EQUAL(ESP_OK, uart_rx_init(TEST_UART_PORT, TEST_RX_PIN));

    QueueHandle_t sensor_queue = create_sensor_queue();
    TEST_ASSERT_NOT_NULL(sensor_queue);

    TaskHandle_t task_handle;
    xTaskCreate(uart_rx_task, "uart_rx_task", 4096, sensor_queue, 5, &task_handle);

    // inject the same packet in two halves with a 50ms gap between them.
    // the gap causes the UART idle timeout to fire after the first half,
    // triggering a premature UART_DATA event. the task must accumulate
    // both halves before enqueuing.
    mqtt_sensor_data_t tx_pkt = make_dummy_packet("node_B");
    uint8_t *raw = (uint8_t *)&tx_pkt;
    size_t half = sizeof(mqtt_sensor_data_t) / 2;

    uart_write_bytes(UART_NUM_0, (const char *)raw, half);
    vTaskDelay(pdMS_TO_TICKS(50));
    uart_write_bytes(UART_NUM_0, (const char *)(raw + half), sizeof(mqtt_sensor_data_t) - half);

    vTaskDelay(pdMS_TO_TICKS(200));

    mqtt_sensor_data_t rx_pkt;
    TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(sensor_queue, &rx_pkt, pdMS_TO_TICKS(100)));
    TEST_ASSERT_EQUAL_STRING("node_B", rx_pkt.node_source);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, tx_pkt.accel_x, rx_pkt.accel_x);
    TEST_ASSERT_EQUAL_UINT32(tx_pkt.timestamp, rx_pkt.timestamp);

    // exactly one packet should have been enqueued despite the split
    TEST_ASSERT_EQUAL(0, uxQueueMessagesWaiting(sensor_queue));

    vTaskDelete(task_handle);
    vQueueDelete(sensor_queue);
    uart_rx_deinit(TEST_UART_PORT);
}

// ─── uart_rx_deinit ───────────────────────────────────────────────────────────

void test_uart_rx_deinit_after_init(void) {
    uart_rx_init(TEST_UART_PORT, TEST_RX_PIN);
    TEST_ASSERT_EQUAL(ESP_OK, uart_rx_deinit(TEST_UART_PORT));
}

void test_uart_rx_deinit_without_init(void) {
    TEST_ASSERT_NOT_EQUAL(ESP_OK, uart_rx_deinit(TEST_UART_PORT));
}

void test_uart_rx_reinit_after_deinit(void) {
    uart_rx_init(TEST_UART_PORT, TEST_RX_PIN);
    uart_rx_deinit(TEST_UART_PORT);
    TEST_ASSERT_EQUAL(ESP_OK, uart_rx_init(TEST_UART_PORT, TEST_RX_PIN));
    uart_rx_deinit(TEST_UART_PORT);
}

// ─── runner ───────────────────────────────────────────────────────────────────

void app_main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_uart_rx_init_happy_path);
    RUN_TEST(test_uart_rx_init_invalid_port);
    RUN_TEST(test_uart_rx_init_invalid_pin);
    RUN_TEST(test_uart_rx_init_double_init);

    RUN_TEST(test_uart_rx_task_full_packet);
    RUN_TEST(test_uart_rx_task_partial_packet);

    RUN_TEST(test_uart_rx_deinit_after_init);
    RUN_TEST(test_uart_rx_deinit_without_init);
    RUN_TEST(test_uart_rx_reinit_after_deinit);

    UNITY_END();
}