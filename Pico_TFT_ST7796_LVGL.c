#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
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

#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT * 2 / 10) // 10th of screen in 16-bit color

static uint8_t buf1[DRAW_BUF_SIZE]; // Allocate the memory array

#define COLOR_RED   0xF800
#define COLOR_GREEN 0x07E0
#define COLOR_BLUE  0x001F

void st7796_send_cmd(uint8_t cmd) {
    gpio_put(LCD_PIN_DCX, 0); // DCX Low = Command
    gpio_put(LCD_PIN_CSX, 0); // CSX Low = Active
    
    spi_write_blocking(SPI_PORT, &cmd, 1);
    
    // Wait for the hardware SPI engine to finish pushing all bits out
    while (spi_is_busy(SPI_PORT)) {
        tight_loop_contents();
    }
    
    gpio_put(LCD_PIN_CSX, 1); // CSX High = Idle
}

void st7796_send_data(const uint8_t *data, size_t len) {
    gpio_put(LCD_PIN_DCX, 1); // DCX High = Data
    gpio_put(LCD_PIN_CSX, 0); 
    
    spi_write_blocking(SPI_PORT, data, len);
    
    // Wait for the hardware SPI engine to finish pushing all bits out
    while (spi_is_busy(SPI_PORT)) {
        tight_loop_contents();
    }
    
    gpio_put(LCD_PIN_CSX, 1);
}

void st7796_init_hardware(void) {
    gpio_put(LCD_PIN_RST, 1);
    sleep_ms(10);
    gpio_put(LCD_PIN_RST, 0);
    sleep_ms(20);
    gpio_put(LCD_PIN_RST, 1);
    sleep_ms(120);

    st7796_send_cmd(0x11); // Sleep Out
    sleep_ms(120);

    st7796_send_cmd(0x36); // Orientation
    uint8_t madctl = 0xE8; 
    st7796_send_data(&madctl, 1);

    st7796_send_cmd(0x3A); // Interface Pixel Format
    uint8_t pixfmt = 0x55; 
    st7796_send_data(&pixfmt, 1);

    st7796_send_cmd(0x29); // Display On
    sleep_ms(50);
}

void st7796_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    st7796_send_cmd(0x2A); 
    uint8_t col_data[] = {(x1 >> 8) & 0xFF, x1 & 0xFF, (x2 >> 8) & 0xFF, x2 & 0xFF};
    st7796_send_data(col_data, 4);

    st7796_send_cmd(0x2B); 
    uint8_t page_data[] = {(y1 >> 8) & 0xFF, y1 & 0xFF, (y2 >> 8) & 0xFF, y2 & 0xFF};
    st7796_send_data(page_data, 4);
}

void st7796_fill_screen(uint16_t color) {
    st7796_set_window(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
    
    st7796_send_cmd(0x2C); // Memory Write
    
    gpio_put(LCD_PIN_DCX, 1);
    gpio_put(LCD_PIN_CSX, 0);

    uint8_t pixel_chunk[256];
    uint8_t high_byte = (color >> 8) & 0xFF;
    uint8_t low_byte = color & 0xFF;
    
    for (int i = 0; i < 256; i += 2) {
        pixel_chunk[i]     = high_byte;
        pixel_chunk[i + 1] = low_byte;
    }

    uint32_t total_pixels = SCREEN_WIDTH * SCREEN_HEIGHT;
    uint32_t pixels_sent = 0;

    while (pixels_sent < total_pixels) {
        uint32_t pixels_left = total_pixels - pixels_sent;
        uint32_t chunk_size = (pixels_left > 128) ? 128 : pixels_left;
        
        spi_write_blocking(SPI_PORT, pixel_chunk, chunk_size * 2);
        pixels_sent += chunk_size;
    }

    // Ensure last pixel is completely clocked out before closing CSX
    while (spi_is_busy(SPI_PORT)) {
        tight_loop_contents();
    }

    gpio_put(LCD_PIN_CSX, 1);
}

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    // 1. Tell screen where we want to draw
    st7796_set_window(area->x1, area->y1, area->x2, area->y2);
    
    // 2. Open screen memory gate
    st7796_send_cmd(0x2C); 
    
    // 3. Blast the pixel bytes over SPI
    uint32_t size = (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1) * 2;
    st7796_send_data(px_map, size);
    
    // 4. Hand execution control back to LVGL
    lv_display_flush_ready(disp);
}

// Call this exactly every 1 millisecond using a hardware timer interrupt
void system_timer_interrupt() {
    lv_tick_inc(1); // Tell LVGL 1ms has passed
}

int main() {
    stdio_init_all();

    // Reconfigured to 16 MHz for maximum reliable ST7796 data processing
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
// ==========================================
    //            HERE IS STEP B!
    // ==========================================
    // First, always initialize the core LVGL library engine
    lv_init();

    // Create the display tracking handle (setting your 480x320 boundaries)
    lv_display_t *disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    
    // Link your working SPI rendering function (my_disp_flush) to LVGL
    lv_display_set_flush_cb(disp, my_disp_flush);
    
    // Register your global 'buf1' array as the drawing canvas memory space
    lv_display_set_buffers(disp, buf1, NULL, DRAW_BUF_SIZE, LV_DISPLAY_RENDER_MODE_PARTIAL);
    // ==========================================


    // 3. INITIALIZE YOUR USER INTERFACE DESIGN LAYOUT HERE
    // Create text labels, buttons, or graphs now that LVGL is active
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "LVGL Boot Complete!");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);


    // 4. INFINITE OPERATION PROCESSOR LOOP
    while (true) {
        // Let LVGL check layout logic updates and trigger callbacks
        lv_timer_handler(); 
        sleep_ms(5); 
    }
}