#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"

// Hardware Pin Configuration matching your setup
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

// 16-bit RGB565 Color Definitions
#define COLOR_RED   0xF800
#define COLOR_GREEN 0x07E0
#define COLOR_BLUE  0x001F

void st7796_send_cmd(uint8_t cmd) {
    gpio_put(LCD_PIN_DCX, 0); // DCX Low = Command
    gpio_put(LCD_PIN_CSX, 0); // CSX Low = Active
    spi_write_blocking(SPI_PORT, &cmd, 1);
    gpio_put(LCD_PIN_CSX, 1); // CSX High = Idle
}

void st7796_send_data(const uint8_t *data, size_t len) {
    gpio_put(LCD_PIN_DCX, 1); // DCX High = Data
    gpio_put(LCD_PIN_CSX, 0); 
    spi_write_blocking(SPI_PORT, data, len);
    gpio_put(LCD_PIN_CSX, 1);
}

// Hardware Initialization sequence native to the ST7796 driver matrix
void st7796_init_hardware(void) {
    // Hardware Reset
    gpio_put(LCD_PIN_RST, 1);
    sleep_ms(10);
    gpio_put(LCD_PIN_RST, 0);
    sleep_ms(20);
    gpio_put(LCD_PIN_RST, 1);
    sleep_ms(120);

    st7796_send_cmd(0x11); // Sleep Out
    sleep_ms(120);

    st7796_send_cmd(0x36); // Memory Data Access Control (Orientation)
    uint8_t madctl = 0xE8; // Landscape orientation, normal color order
    st7796_send_data(&madctl, 1);

    st7796_send_cmd(0x3A); // Interface Pixel Format
    uint8_t pixfmt = 0x55; // 16-bit RGB565
    st7796_send_data(&pixfmt, 1);

    st7796_send_cmd(0x29); // Display On
    sleep_ms(50);
}

// Sets the target drawing window coordinates
void st7796_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    st7796_send_cmd(0x2A); // Column Address Set
    uint8_t col_data[] = {(x1 >> 8) & 0xFF, x1 & 0xFF, (x2 >> 8) & 0xFF, x2 & 0xFF};
    st7796_send_data(col_data, 4);

    st7796_send_cmd(0x2B); // Row Address Set
    uint8_t page_data[] = {(y1 >> 8) & 0xFF, y1 & 0xFF, (y2 >> 8) & 0xFF, y2 & 0xFF};
    st7796_send_data(page_data, 4);
}

// Blasts a single solid color color to the entire display frame
void st7796_fill_screen(uint16_t color) {
    st7796_set_window(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
    
    st7796_send_cmd(0x2C); // Memory Write
    
    gpio_put(LCD_PIN_DCX, 1);
    gpio_put(LCD_PIN_CSX, 0);

    // Prepare a small block chunk of uniform pixels to stream rapidly
    uint8_t pixel_chunk[256];
    uint8_t high_byte = (color >> 8) & 0xFF;
    uint8_t low_byte = color & 0xFF;
    
    for (int i = 0; i < 256; i += 2) {
        pixel_chunk[i]     = high_byte;
        pixel_chunk[i + 1] = low_byte;
    }

    uint32_t total_pixels = SCREEN_WIDTH * SCREEN_HEIGHT;
    uint32_t pixels_sent = 0;

    // Stream chunks until the whole screen buffer is populated
    while (pixels_sent < total_pixels) {
        uint32_t pixels_left = total_pixels - pixels_sent;
        uint32_t chunk_size = (pixels_left > 128) ? 128 : pixels_left;
        
        spi_write_blocking(SPI_PORT, pixel_chunk, chunk_size * 2);
        pixels_sent += chunk_size;
    }

    gpio_put(LCD_PIN_CSX, 1);
}

int main() {
    stdio_init_all();

    // Initialize SPI1 at a stable, hardware-safe speed of 16 MHz
    spi_init(SPI_PORT, 2 * 1000 * 1000); 
    gpio_set_function(LCD_PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(LCD_PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(LCD_PIN_SCK, GPIO_FUNC_SPI);

    // Initialize Control Pins
    gpio_init(LCD_PIN_CSX);       gpio_set_dir(LCD_PIN_CSX, GPIO_OUT);
    gpio_init(LCD_PIN_DCX);       gpio_set_dir(LCD_PIN_DCX, GPIO_OUT);
    gpio_init(LCD_PIN_RST);       gpio_set_dir(LCD_PIN_RST, GPIO_OUT);
    gpio_init(LCD_PIN_BACKLIGHT); gpio_set_dir(LCD_PIN_BACKLIGHT, GPIO_OUT);

    gpio_put(LCD_PIN_CSX, 1);
    gpio_put(LCD_PIN_BACKLIGHT, 1); // Enable Backlight

    // Fire driver configuration commands
    st7796_init_hardware();

    // Infinite Loop cycling between solid color fills
    while (true) {
        st7796_fill_screen(COLOR_RED);
        sleep_ms(2000);

        st7796_fill_screen(COLOR_GREEN);
        sleep_ms(2000);

        st7796_fill_screen(COLOR_BLUE);
        sleep_ms(2000);
    }
}