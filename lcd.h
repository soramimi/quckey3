#ifndef LCD_H
#define LCD_H

#include <stdint.h>

#ifdef __cplusplus

class lcd {
public:
	static void home();
	static void putchar(char c);
	static void print(char const *ptr);
	static void clear();
	static void init();
};

extern "C" {

#endif

void lcd_home();
void lcd_putchar(uint8_t c);
void lcd_print(char const *p);
void lcd_puthex8(uint8_t v);

#ifdef __cplusplus
}
#endif

#endif // LCD_H
