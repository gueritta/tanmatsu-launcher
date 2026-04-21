// WHY2025 badge display initialization using the ST7703 panel driver.
//
// The badge-bsp uses the esp32-p4-function-ev-board target for WHY2025 builds,
// which hard-codes the EK79007 panel driver. The WHY2025 badge uses an ST7703
// panel instead. This file overrides the ek79007_* symbols from the
// mipi_dsi_abstraction component so the BSP transparently drives the ST7703.

#include "dsi_panel_espressif_ek79007.h"
#include "esp_check.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7703.h"
#include "esp_log.h"
#include "hal/lcd_types.h"

static const char *TAG = "WHY2025_display";

// DSI bus configuration
#define WHY2025_DSI_LANE_NUM        2
#define WHY2025_DSI_LANE_MBPS       1000

// Display timing for the WHY2025 720x720 ST7703 panel
#define WHY2025_LCD_H_RES           720
#define WHY2025_LCD_V_RES           720
#define WHY2025_DPI_CLK_MHZ         58

static esp_lcd_panel_handle_t st7703_panel = NULL;

// Panel-specific initialisation sequence for the WHY2025 ST7703 display.
static const st7703_lcd_init_cmd_t why2025_init_cmds[] = {
    {0xB9, (uint8_t[]){0xF1, 0x12, 0x83}, 3, 0},
    {0xBA, (uint8_t[]){0x31, 0x81, 0x05, 0xF9, 0x0E, 0x0E, 0x20, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x44, 0x25, 0x00, 0x90, 0x0A, 0x00,
                       0x00, 0x01, 0x4F, 0x01, 0x00, 0x00, 0x37}, 27, 0},
    {0xB8, (uint8_t[]){0x25, 0x22, 0xF0, 0x63}, 4, 0},
    {0xBF, (uint8_t[]){0x02, 0x11, 0x00}, 3, 0},
    {0xB3, (uint8_t[]){0x10, 0x10, 0x28, 0x28, 0x03, 0xFF, 0x00, 0x00, 0x00, 0x00}, 10, 0},
    {0xC0, (uint8_t[]){0x73, 0x73, 0x50, 0x50, 0x00, 0x00, 0x12, 0x70, 0x00}, 9, 0},
    {0xBC, (uint8_t[]){0x46}, 1, 0},
    {0xCC, (uint8_t[]){0x0B}, 1, 0},
    {0xB4, (uint8_t[]){0x80}, 1, 0},
    {0xB2, (uint8_t[]){0x3C, 0x02, 0x30}, 3, 0},
    {0xE3, (uint8_t[]){0x07, 0x07, 0x0B, 0x0B, 0x03, 0x0B, 0x00, 0x00, 0x00, 0x00,
                       0xFF, 0x00, 0xC0, 0x10}, 14, 0},
    {0xC1, (uint8_t[]){0x36, 0x00, 0x32, 0x32, 0x77, 0xF1, 0xCC, 0xCC, 0x77, 0x77,
                       0x33, 0x33}, 12, 0},
    {0xB5, (uint8_t[]){0x0A, 0x0A}, 2, 0},
    {0xB6, (uint8_t[]){0xB2, 0xB2}, 2, 0},
    {0xE9, (uint8_t[]){0xC8, 0x10, 0x0A, 0x10, 0x0F, 0xA1, 0x80, 0x12, 0x31, 0x23,
                       0x47, 0x86, 0xA1, 0x80, 0x47, 0x08, 0x00, 0x00, 0x0D, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x48, 0x02,
                       0x8B, 0xAF, 0x46, 0x02, 0x88, 0x88, 0x88, 0x88, 0x88, 0x48,
                       0x13, 0x8B, 0xAF, 0x57, 0x13, 0x88, 0x88, 0x88, 0x88, 0x88,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00}, 63, 0},
    {0xEA, (uint8_t[]){0x96, 0x12, 0x01, 0x01, 0x01, 0x78, 0x02, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x4F, 0x31, 0x8B, 0xA8, 0x31, 0x75, 0x88, 0x88,
                       0x88, 0x88, 0x88, 0x4F, 0x20, 0x8B, 0xA8, 0x20, 0x64, 0x88,
                       0x88, 0x88, 0x88, 0x88, 0x23, 0x00, 0x00, 0x01, 0x02, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x40, 0xA1, 0x80, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00}, 63, 0},
    {0xE0, (uint8_t[]){0x00, 0x0A, 0x0F, 0x29, 0x3B, 0x3F, 0x42, 0x39, 0x06, 0x0D,
                       0x10, 0x13, 0x15, 0x14, 0x15, 0x10, 0x17, 0x00, 0x0A, 0x0F,
                       0x29, 0x3B, 0x3F, 0x42, 0x39, 0x06, 0x0D, 0x10, 0x13, 0x15,
                       0x14, 0x15, 0x10, 0x17}, 34, 0},
    {0x11, NULL, 0, 250},
    {0x29, NULL, 0, 50},
};

esp_lcd_panel_handle_t ek79007_get_panel(void) {
    return st7703_panel;
}

esp_err_t ek79007_get_parameters(size_t *h_res, size_t *v_res, lcd_color_rgb_pixel_format_t *color_fmt) {
    if (h_res) {
        *h_res = WHY2025_LCD_H_RES;
    }
    if (v_res) {
        *v_res = WHY2025_LCD_V_RES;
    }
    if (color_fmt) {
        *color_fmt = LCD_COLOR_PIXEL_FORMAT_RGB888;
    }
    return ESP_OK;
}

esp_err_t ek79007_initialize(const ek79007_configuration_t *config) {
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "Display configuration is NULL");
    ESP_LOGI(TAG, "Initializing WHY2025 ST7703 display (%dx%d)", WHY2025_LCD_H_RES, WHY2025_LCD_V_RES);

    esp_lcd_dsi_bus_handle_t dsi_bus;
    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id             = 0,
        .num_data_lanes     = WHY2025_DSI_LANE_NUM,
        .phy_clk_src        = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = WHY2025_DSI_LANE_MBPS,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_dsi_bus(&bus_config, &dsi_bus), TAG, "Failed to create DSI bus");

    esp_lcd_panel_io_handle_t dbi_io;
    esp_lcd_dbi_io_config_t dbi_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits    = 8,
        .lcd_param_bits  = 8,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_dbi(dsi_bus, &dbi_config, &dbi_io), TAG, "Failed to create DBI IO");

    esp_lcd_dpi_panel_config_t dpi_config = {
        .dpi_clk_src        = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = WHY2025_DPI_CLK_MHZ,
        .virtual_channel    = 0,
        .pixel_format       = LCD_COLOR_PIXEL_FORMAT_RGB888,
        .num_fbs            = config->num_fbs,
        .video_timing =
            {
                .h_size            = WHY2025_LCD_H_RES,
                .v_size            = WHY2025_LCD_V_RES,
                .hsync_back_porch  = 80,
                .hsync_pulse_width = 20,
                .hsync_front_porch = 80,
                .vsync_back_porch  = 12,
                .vsync_pulse_width = 4,
                .vsync_front_porch = 30,
            },
        .flags.use_dma2d = true,
    };

    st7703_vendor_config_t vendor_config = {
        .init_cmds           = why2025_init_cmds,
        .init_cmds_size      = sizeof(why2025_init_cmds) / sizeof(why2025_init_cmds[0]),
        // Send the panel init commands (including Sleep Out and Display On) while
        // the MIPI DPI video stream is still off.  Without this flag the driver
        // starts the DPI stream first and then attempts a blocking panel ID read
        // via DBI; the ST7703 on the WHY2025 does not respond to reads while the
        // HS video clock is already running, causing the initialisation to hang.
        .init_in_command_mode = true,
        .mipi_config =
            {
                .dsi_bus    = dsi_bus,
                .dpi_config = &dpi_config,
            },
    };

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = config->reset_pin,
        // The WHY2025 badge displays RGB data in BGR order
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 24,
        .vendor_config  = &vendor_config,
    };

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7703(dbi_io, &panel_config, &st7703_panel), TAG,
                        "Failed to create ST7703 panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(st7703_panel), TAG, "Failed to reset ST7703 panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(st7703_panel), TAG, "Failed to initialize ST7703 panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(st7703_panel, true), TAG, "Failed to enable ST7703 panel output");

    ESP_LOGI(TAG, "ST7703 display ready");
    return ESP_OK;
}
