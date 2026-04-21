#include "common/display.h"
#include "bsp/display.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "hal/lcd_types.h"
#include "pax_gfx.h"
#include "pax_types.h"
#include "sdkconfig.h"

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL) || \
    defined(CONFIG_BSP_TARGET_ESP32_P4_FUNCTION_EV_BOARD) || defined(CONFIG_BSP_TARGET_WHY2025)
#define DSI_PANEL
#endif

#ifdef DSI_PANEL
#include "esp_lcd_mipi_dsi.h"
#endif

static size_t                       display_h_res        = 0;
static size_t                       display_v_res        = 0;
static lcd_color_rgb_pixel_format_t display_color_format = LCD_COLOR_PIXEL_FORMAT_RGB565;
static lcd_rgb_data_endian_t        display_data_endian  = LCD_RGB_DATA_ENDIAN_LITTLE;
static pax_buf_t                    fb                   = {0};

#if defined(CONFIG_BSP_TARGET_KAMI)
static pax_col_t palette[] = {0xffffffff, 0xff000000, 0xffff0000};  // white, black, red
#endif

#if CONFIG_IDF_TARGET_ESP32P4
// Display framebuffer returned by `asp_disp_get_fb`.
extern uint8_t*   asp_disp_fb;
// PAX buffer returned by `asp_disp_get_pax_buf`.
extern pax_buf_t* asp_disp_pax_buf;
#endif

static const char* TAG = "display";

esp_err_t display_init(void) {
    esp_err_t res =
        bsp_display_get_parameters(&display_h_res, &display_v_res, &display_color_format, &display_data_endian);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "bsp_display_get_parameters failed: %s (0x%x)", esp_err_to_name(res), res);
        return res;
    }

    ESP_LOGI(TAG, "Display parameters: %ux%u, color_format=%d, data_endian=%d",
             (unsigned)display_h_res, (unsigned)display_v_res, (int)display_color_format,
             (int)display_data_endian);

    pax_buf_type_t format = PAX_BUF_24_888RGB;

    switch (display_color_format) {
        case LCD_COLOR_PIXEL_FORMAT_RGB565:
            format = PAX_BUF_16_565RGB;
            break;
        case LCD_COLOR_PIXEL_FORMAT_RGB888:
            format = PAX_BUF_24_888RGB;
            break;
        default:
            break;
    }

#if defined(CONFIG_BSP_TARGET_KAMI)
    format = PAX_BUF_2_PAL;
#endif

    pax_buf_init(&fb, NULL, display_h_res, display_v_res, format);
    pax_buf_reversed(&fb, display_data_endian == LCD_RGB_DATA_ENDIAN_BIG);

    if (pax_buf_get_pixels(&fb) == NULL) {
        ESP_LOGE(TAG, "PAX framebuffer allocation failed (size: %ux%u format:%d)",
                 (unsigned)display_h_res, (unsigned)display_v_res, (int)format);
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "PAX framebuffer allocated at %p (%ux%u)", pax_buf_get_pixels(&fb),
             (unsigned)display_h_res, (unsigned)display_v_res);

#if defined(CONFIG_BSP_TARGET_KAMI)
    fb.palette      = palette;
    fb.palette_size = sizeof(palette) / sizeof(pax_col_t);
#endif

    bsp_display_rotation_t display_rotation = bsp_display_get_default_rotation();
    pax_orientation_t      orientation      = PAX_O_UPRIGHT;
    switch (display_rotation) {
        case BSP_DISPLAY_ROTATION_90:
            orientation = PAX_O_ROT_CCW;
            break;
        case BSP_DISPLAY_ROTATION_180:
            orientation = PAX_O_ROT_HALF;
            break;
        case BSP_DISPLAY_ROTATION_270:
            orientation = PAX_O_ROT_CW;
            break;
        case BSP_DISPLAY_ROTATION_0:
        default:
            orientation = PAX_O_UPRIGHT;
            break;
    }
    pax_buf_set_orientation(&fb, orientation);

#if CONFIG_IDF_TARGET_ESP32P4
    asp_disp_fb      = pax_buf_get_pixels_rw(&fb);
    asp_disp_pax_buf = &fb;
#endif

    ESP_LOGI(TAG, "Display initialized: %ux%u (visual), rotation=%d",
             (unsigned)pax_buf_get_width(&fb), (unsigned)pax_buf_get_height(&fb), (int)display_rotation);
    return ESP_OK;
}

pax_buf_t* display_get_buffer(void) {
    return &fb;
}

void display_blit_buffer(pax_buf_t* fb) {
    size_t display_h_res = 0, display_v_res = 0;
    ESP_ERROR_CHECK(bsp_display_get_parameters(&display_h_res, &display_v_res, NULL, NULL));
    static bool first_blit = true;
    if (first_blit) {
        ESP_LOGI(TAG, "First display blit: %ux%u pixels at %p", (unsigned)display_h_res,
                 (unsigned)display_v_res, pax_buf_get_pixels(fb));
        first_blit = false;
    }
    ESP_ERROR_CHECK(bsp_display_blit(0, 0, display_h_res, display_v_res, pax_buf_get_pixels(fb)));
}

void display_blit(void) {
    display_blit_buffer(&fb);
}

bool display_is_initialized(void) {
    return pax_buf_get_width(&fb) > 0;
}

bool display_is_epaper(void) {
#if defined(CONFIG_BSP_TARGET_KAMI)
    return true;
#else
    return false;
#endif
}
