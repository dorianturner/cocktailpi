#ifndef LCD_I2C_H
#define LCD_I2C_H

int lcd_init(int i2c_bus, int addr);
void lcd_close(int handle);
void lcd_clear(int handle);
void lcd_set_cursor(int handle, int row, int col);
void lcd_write_string(int handle, const char *str);

#endif

