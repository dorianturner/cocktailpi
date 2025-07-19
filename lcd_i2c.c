#ifndef ON_PI
#include "pigpio_emu.h"
#else
#include <pigpio.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "lcd_i2c.h"
#include "maker.h"

#define LCD_BACKLIGHT 0x08
#define ENABLE 0x04
#define CMD 0
#define DATA 1
#define ROWS 2
#define COLS 16

// only first 2 are used in a 16x2 LCD
#ifndef EMU_OUTPUT
static const int row_offsets[] = {0x00, 0x40, 0x14, 0x54};

static void lcd_write_nibble(int handle, uint8_t nibble, uint8_t mode) {
    uint8_t data = nibble | LCD_BACKLIGHT | (mode ? 0x01 : 0x00);

    i2cWriteByte(handle, data | ENABLE);
    gpioDelay(1);
    i2cWriteByte(handle, data & ~ENABLE);
    gpioDelay(50);
}

static void lcd_write_byte(int handle, uint8_t byte, uint8_t mode) {
    lcd_write_nibble(handle, (byte & 0xf0), mode);
    lcd_write_nibble(handle, ((byte << 4) & 0xf0), mode);
}
#endif

void lcd_clear(int handle) {
    #if defined(EMU_OUTPUT) || defined(DEBUG)
    print_with_timestamp(stdout, "Clearing LCD");
    #endif
    #ifndef EMU_OUTPUT
    lcd_write_byte(handle, 0x01, CMD);
    #endif
    gpioDelay(2000); // clear takes ~1.5ms
}

void lcd_set_cursor(int handle, int row, int col) {
    assert(0 <= row && row < ROWS);
    assert(0 <= col && col < COLS);
    #if defined(EMU_OUTPUT) || defined(DEBUG)
    char msg_cursor[128];
    snprintf(msg_cursor, sizeof(msg_cursor), "Cursor set to row %d, column %d", row, col);
    print_with_timestamp(stdout, msg_cursor);
    #endif
    #ifndef EMU_OUTPUT
    lcd_write_byte(handle, 0x80 | (col + row_offsets[row]), CMD);
    #endif
}


// very simple implementation, no scrolling in order to make system simple
// with long strings i have split with \n
void lcd_write_string(int handle, const char *str) {
    #if defined(EMU_OUTPUT) || defined(DEBUG)
    char msg[512];
    snprintf(msg, sizeof(msg), "Writing message to LCD: %s", str);
    print_with_timestamp(stdout, msg);
    #endif
    #ifndef EMU_OUTPUT
    while(*str) {
        if (*str == '\n') {
            lcd_set_cursor(handle, 1, 0); // move to start of second row
            str++;
            continue; // skip printing '\n', undefined
        }
        lcd_write_byte(handle, *str++, DATA);
    }
    #endif
}

int lcd_init(int bus, int addr) {
    #ifdef EMU_OUTPUT
    return 42; // FAKE
    #else
    int handle = i2cOpen(bus, addr, 0);
    if (handle < 0) return -1;
    gpioDelay(50000); // wait ~40ms 

    lcd_write_nibble(handle, 0x30, CMD);
    gpioDelay(4500);

    lcd_write_nibble(handle, 0x30, CMD);
    gpioDelay(4500);

    lcd_write_nibble(handle, 0x30, CMD);
    gpioDelay(150);

    lcd_write_nibble(handle, 0x20, CMD); // 4-bit mode
    
    lcd_write_byte(handle, 0x28, CMD); // 4-bit, 2 line, 5x8 font
    lcd_write_byte(handle, 0x0c, CMD); // display on, cursor off, blink off
    lcd_clear(handle); // clear display
    lcd_write_byte(handle, 0x06, CMD); // increment cursor and enter mode

    return handle;
    #endif
}

void lcd_close(int handle) {
    #ifdef EMU_OUTPUT
    return;
    #else
    i2cClose(handle);
    #endif
}

