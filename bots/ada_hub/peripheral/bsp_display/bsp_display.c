/*————————————————————————————————————————Header file declaration————————————————————————————————————————*/
#include "bsp_display.h"
#include "esp_lcd_panel_io.h"
/*——————————————————————————————————————Header file declaration end——————————————————————————————————————*/

/*——————————————————————————————————————————Variable declaration—————————————————————————————————————————*/

esp_lcd_touch_handle_t tp = NULL;

static uint16_t touch_x = 0xffff;
static uint16_t touch_y = 0xffff;
static bool is_pressed = false;
/*————————————————————————————————————————Variable declaration end———————————————————————————————————————*/

/*—————————————————————————————————————————Functional function———————————————————————————————————————————*/

void set_coor(uint16_t x, uint16_t y, bool press)
{
    touch_x = x;
    touch_y = y;
    is_pressed = press;
}

void get_coor(uint16_t *x, uint16_t *y, bool *press)
{
    *x = touch_x;
    *y = touch_y;
    *press = is_pressed;
}

esp_err_t touch_init(void)
{
    esp_err_t err;
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;

    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    io_config.scl_speed_hz = 400000;

    esp_lcd_touch_io_gt911_config_t gt911_drv_cfg = {
        .dev_addr = io_config.dev_addr,
    };

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = H_size,
        .y_max = V_size,
        .rst_gpio_num = Touch_GPIO_RST,
        .int_gpio_num = Touch_GPIO_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .driver_data = &gt911_drv_cfg,
    };

    err = esp_lcd_new_panel_io_i2c((i2c_master_bus_handle_t)i2c_bus_handle, &io_config, &tp_io_handle);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &tp);
    if (err != ESP_OK) {
        /* Retry with alternate GT911 address */
        esp_lcd_panel_io_del(tp_io_handle);
        io_config.dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP;
        gt911_drv_cfg.dev_addr = io_config.dev_addr;

        err = esp_lcd_new_panel_io_i2c((i2c_master_bus_handle_t)i2c_bus_handle, &io_config, &tp_io_handle);
        if (err != ESP_OK) {
            return err;
        }
        err = esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &tp);
        if (err != ESP_OK) {
            return err;
        }
    }

    return ESP_OK;
}

esp_err_t touch_read(void)
{
    if (tp == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint16_t tx[1];
    uint16_t ty[1];
    uint16_t touch_strength[1];
    uint8_t touch_cnt = 0;

    esp_err_t err = esp_lcd_touch_read_data(tp);
    if (err != ESP_OK) {
        return err;
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);

    if (esp_lcd_touch_get_coordinates(tp, tx, ty, touch_strength, &touch_cnt, 1)) {
        set_coor(tx[0], ty[0], true);
    } else {
        set_coor(0xffff, 0xffff, false);
    }

    return ESP_OK;
}

/*———————————————————————————————————————Functional function end—————————————————————————————————————————*/
