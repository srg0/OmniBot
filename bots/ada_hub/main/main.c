#include <stdio.h>

#include "main.h"
#include "esp_lvgl_port.h"

static esp_ldo_channel_handle_t s_ldo3;
static esp_ldo_channel_handle_t s_ldo4;

typedef struct {
    lv_obj_t *coord_label;
    lv_obj_t *dot;
} touch_demo_ui_t;

static touch_demo_ui_t s_touch_ui;

static void init_fail_loop(const char *module, esp_err_t err)
{
    while (1) {
        MAIN_ERROR("[%s] init failed: %s", module, esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void screen_touch_event(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING && code != LV_EVENT_RELEASED) {
        return;
    }

    lv_indev_t *indev = lv_indev_get_act();
    if (indev == NULL) {
        return;
    }

    lv_point_t pt;
    lv_indev_get_point(indev, &pt);

    char buf[56];
    if (code == LV_EVENT_RELEASED) {
        snprintf(buf, sizeof(buf), "released — last x=%d y=%d", (int)pt.x, (int)pt.y);
    } else {
        snprintf(buf, sizeof(buf), "x=%d  y=%d", (int)pt.x, (int)pt.y);
    }
    lv_label_set_text(s_touch_ui.coord_label, buf);

    /* Keep marker on-screen */
    lv_coord_t dx = pt.x - lv_obj_get_width(s_touch_ui.dot) / 2;
    lv_coord_t dy = pt.y - lv_obj_get_height(s_touch_ui.dot) / 2;
    lv_obj_set_pos(s_touch_ui.dot, dx, dy);
}

static void create_touch_demo_ui(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Touch test — tap or drag");
    lv_obj_set_style_text_color(title, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);

    s_touch_ui.coord_label = lv_label_create(scr);
    lv_label_set_text(s_touch_ui.coord_label, "x=---  y=---");
    lv_obj_set_style_text_color(s_touch_ui.coord_label, lv_color_hex(0xEEEEEE), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_touch_ui.coord_label, &lv_font_montserrat_24, 0);
    lv_obj_align(s_touch_ui.coord_label, LV_ALIGN_CENTER, 0, -40);

    s_touch_ui.dot = lv_obj_create(scr);
    lv_obj_set_size(s_touch_ui.dot, 28, 28);
    lv_obj_set_style_radius(s_touch_ui.dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_touch_ui.dot, lv_color_hex(0xFF4444), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_touch_ui.dot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_touch_ui.dot, 0, LV_PART_MAIN);
    lv_obj_clear_flag(s_touch_ui.dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_touch_ui.dot, H_size / 2 - 14, V_size / 2 - 14);

    /*
     * Full-screen invisible layer on top so touches are not eaten by labels/dot.
     * Must be above other widgets but fully transparent.
     */
    lv_obj_t *touch_layer = lv_obj_create(scr);
    lv_obj_set_size(touch_layer, H_size, V_size);
    lv_obj_align(touch_layer, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(touch_layer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(touch_layer, 0, LV_PART_MAIN);
    lv_obj_clear_flag(touch_layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(touch_layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(touch_layer, screen_touch_event, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(touch_layer, screen_touch_event, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(touch_layer, screen_touch_event, LV_EVENT_RELEASED, NULL);
    lv_obj_move_foreground(touch_layer);
}

static void system_init(void)
{
    esp_err_t err;

    esp_ldo_channel_config_t ldo3_cfg = {
        .chan_id = 3,
        .voltage_mv = 2500,
    };
    err = esp_ldo_acquire_channel(&ldo3_cfg, &s_ldo3);
    if (err != ESP_OK) {
        init_fail_loop("ldo3", err);
    }

    esp_ldo_channel_config_t ldo4_cfg = {
        .chan_id = 4,
        .voltage_mv = 3300,
    };
    err = esp_ldo_acquire_channel(&ldo4_cfg, &s_ldo4);
    if (err != ESP_OK) {
        init_fail_loop("ldo4", err);
    }
    MAIN_INFO("LDO3/LDO4 OK");

    err = i2c_init();
    if (err != ESP_OK) {
        init_fail_loop("I2C", err);
    }

    vTaskDelay(pdMS_TO_TICKS(150));

    vTaskDelay(pdMS_TO_TICKS(80));

    err = touch_init();
    if (err != ESP_OK) {
        init_fail_loop("touch", err);
    }
    MAIN_INFO("touch OK (GT911)");

    err = display_init();
    if (err != ESP_OK) {
        init_fail_loop("display", err);
    }
    MAIN_INFO("display OK");

    err = set_lcd_blight(100);
    if (err != ESP_OK) {
        init_fail_loop("backlight", err);
    }

    if (!lvgl_port_lock(0)) {
        MAIN_ERROR("lvgl_port_lock failed");
        return;
    }
    create_touch_demo_ui();
    lvgl_port_unlock();

    MAIN_INFO("Touch demo UI ready");
}

void app_main(void)
{
    MAIN_INFO("ada_hub touch demo");
    system_init();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
