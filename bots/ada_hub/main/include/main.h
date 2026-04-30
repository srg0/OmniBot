#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_ldo_regulator.h"
#include "lvgl.h"
#include "bsp_display.h"
#include "bsp_illuminate.h"
#include "bsp_i2c.h"

#define MAIN_TAG "main"
#define MAIN_INFO(fmt, ...) ESP_LOGI(MAIN_TAG, fmt, ##__VA_ARGS__)
#define MAIN_ERROR(fmt, ...) ESP_LOGE(MAIN_TAG, fmt, ##__VA_ARGS__)

#endif
