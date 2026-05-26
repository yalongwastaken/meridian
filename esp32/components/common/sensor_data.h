/**
 * @file sensor_data.h
 * @brief Shared sensor data type passed between uart_rx and mqtt_pub components.
 */

#pragma once
#include <stdint.h>

/**
 * @brief IMU sensor reading from a single Meridian node, queued for MQTT publish.
 */
typedef struct {
    char node_source[12];
    float accel_x, accel_y, accel_z;
    float gyro_x, gyro_y, gyro_z;
    uint32_t timestamp;
} mqtt_sensor_data_t;