#ifndef LCD_H
#define LCD_H

#include <stdint.h>

class lcd {
public:
	static void home();
	static void putchar(char c);
	static void print(char const *ptr);
	static void clear();
	static void init();
	static void puthex8(uint8_t c);
	static void puthex16(uint16_t v);
};

#endif // LCD_H
