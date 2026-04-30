idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_display bsp_illuminate bsp_i2c esp_timer esp_lvgl_port
)
