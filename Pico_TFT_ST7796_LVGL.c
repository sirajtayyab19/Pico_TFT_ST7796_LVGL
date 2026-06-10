#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/timer.h" // Required for the repeating timer
#include "lvgl.h"

#define LCD_PIN_BACKLIGHT 10
#define LCD_PIN_MOSI      11
#define LCD_PIN_MISO      12
#define LCD_PIN_CSX       13
#define LCD_PIN_SCK       14
#define LCD_PIN_RST       15
#define LCD_PIN_DCX       17

#define SPI_PORT          spi1
#define SCREEN_WIDTH      480
#define SCREEN_HEIGHT     320

#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT * 2) 

static uint8_t buf1[DRAW_BUF_SIZE]; 

#define COLOR_RED   0xF800
#define COLOR_GREEN 0x07E0
#define COLOR_BLUE  0x001F

// System Tick Heartbeat Callback
bool system_timer_interrupt(struct repeating_timer *t) {
    lv_tick_inc(1); // Tell LVGL 1ms has passed
    return true;
}

void st7796_send_cmd(uint8_t cmd) {
    gpio_put(LCD_PIN_DCX, 0); 
    gpio_put(LCD_PIN_CSX, 0); 
    spi_write_blocking(SPI_PORT, &cmd, 1);
    while (spi_is_busy(SPI_PORT)) { tight_loop_contents(); }
    gpio_put(LCD_PIN_CSX, 1); 
}

void st7796_send_data(const uint8_t *data, size_t len) {
    gpio_put(LCD_PIN_DCX, 1); 
    gpio_put(LCD_PIN_CSX, 0); 
    spi_write_blocking(SPI_PORT, data, len);
    while (spi_is_busy(SPI_PORT)) { tight_loop_contents(); }
    gpio_put(LCD_PIN_CSX, 1);
}

void st7796_init_hardware(void) {
    gpio_put(LCD_PIN_RST, 1); sleep_ms(10);
    gpio_put(LCD_PIN_RST, 0); sleep_ms(20);
    gpio_put(LCD_PIN_RST, 1); sleep_ms(120);

    st7796_send_cmd(0x11); sleep_ms(120);

    st7796_send_cmd(0x36); 
    uint8_t madctl = 0xE8; 
    st7796_send_data(&madctl, 1);

    st7796_send_cmd(0x3A); 
    uint8_t pixfmt = 0x55; 
    st7796_send_data(&pixfmt, 1);

    st7796_send_cmd(0x29); sleep_ms(50);
}

void st7796_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    st7796_send_cmd(0x2A); 
    uint8_t col_data[] = {(x1 >> 8) & 0xFF, x1 & 0xFF, (x2 >> 8) & 0xFF, x2 & 0xFF};
    st7796_send_data(col_data, 4);

    st7796_send_cmd(0x2B); 
    uint8_t page_data[] = {(y1 >> 8) & 0xFF, y1 & 0xFF, (y2 >> 8) & 0xFF, y2 & 0xFF};
    st7796_send_data(page_data, 4);
}

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    st7796_set_window(area->x1, area->y1, area->x2, area->y2);
    st7796_send_cmd(0x2C); 
    uint32_t size = (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1) * 2;
    st7796_send_data(px_map, size);
    lv_display_flush_ready(disp);
}

int main() {
    stdio_init_all();

    spi_init(SPI_PORT, 16 * 1000 * 1000); 
    gpio_set_function(LCD_PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(LCD_PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(LCD_PIN_SCK, GPIO_FUNC_SPI);

    gpio_init(LCD_PIN_CSX);       gpio_set_dir(LCD_PIN_CSX, GPIO_OUT);
    gpio_init(LCD_PIN_DCX);       gpio_set_dir(LCD_PIN_DCX, GPIO_OUT);
    gpio_init(LCD_PIN_RST);       gpio_set_dir(LCD_PIN_RST, GPIO_OUT);
    gpio_init(LCD_PIN_BACKLIGHT); gpio_set_dir(LCD_PIN_BACKLIGHT, GPIO_OUT);

    gpio_put(LCD_PIN_CSX, 1);
    gpio_put(LCD_PIN_BACKLIGHT, 1); 

    st7796_init_hardware();

    // 1. Initialize core LVGL engine
    lv_init();

    // Attach hardware repeating timer interrupt loop for LVGL system ticks
    struct repeating_timer timer;
    add_repeating_timer_ms(1, system_timer_interrupt, NULL, &timer);

    // Create the display tracking handle
    lv_display_t *disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, buf1, NULL, DRAW_BUF_SIZE, LV_DISPLAY_RENDER_MODE_PARTIAL);

    // ==========================================
    //      1. MAKE WHOLE SCREEN BLACK
    // ==========================================
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);
      sleep_ms(500);
    // ==========================================
    //      2. DRAW A SQUARE ON IT
    // ==========================================
    lv_obj_t *square = lv_obj_create(lv_screen_active());
    
    // Set explicit square dimensions (e.g., 100 x 100 pixels)
    lv_obj_set_size(square, 100, 100);
    lv_obj_center(square);
    
    // Style configuration: Solid Green, no borders, remove default container padding
    lv_obj_set_style_bg_color(square, lv_color_make(0, 255, 0), 0);
    lv_obj_set_style_border_width(square, 0, 0);
    lv_obj_set_style_radius(square, 0, 0); // Pure sharp square corners

    int32_t current_angle = 0;

    // ==========================================
    //      3. INFINITE PROCESSOR / ROTATE LOOP
    // ==========================================
    while (true) {
        // Increment rotation angle step (0.5 degree steps -> 5 tenths)
        current_angle += 5;
        if (current_angle >= 3600) {
            current_angle = 0;
        }

        // Apply rotation angle transformation style (values are in 0.1 degree units)
        lv_obj_set_style_transform_rotation(square, current_angle, 0);

        // Process layout engine invalidation pipelines
        lv_timer_handler(); 
        sleep_ms(10); 
    }
}