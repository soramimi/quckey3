
// Quckey3 - Copyright (C) 2020 S.Fuchita (@soramimi_jp)

/* USB Keyboard Firmware code for generic Teensy Keyboards
 * Copyright (c) 2012 Fredrik Atmer, Bathroom Epiphanies Inc
 * http://bathroomepiphanies.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "lcd.h"
#include "usb.h"
#include <avr/interrupt.h>
#include <string.h>
#include "waitloop.h"

#define CLOCK 16000000UL
#define SCALE 125
static unsigned short _scale = 0;
//static unsigned long _system_tick_count;
//static unsigned long _tick_count;
//static unsigned long _time_s;
//static unsigned short _time_ms = 0;
uint8_t interval_1ms_flag = 0;
ISR(TIMER0_OVF_vect, ISR_NOBLOCK)
{
//	_system_tick_count++;
	_scale += 16;
	if (_scale >= SCALE) {
		_scale -= SCALE;
//		_tick_count++;
//		_time_ms++;
//		if (_time_ms >= 1000) {
//			_time_ms = 0;
//			_time_s++;
//		}
		interval_1ms_flag = 1;
	}
}

static bool led_status = false;

extern "C" void led(uint8_t f)
{
	if (f) {
		PORTB |= 0x01;
	} else {
		PORTB &= ~0x01;
	}
	led_status = f;
}

extern "C" void led_toggle()
{
	led(!led_status);
}

static int8_t clamp(int v, int8_t min, int8_t max)
{
	if (v > max) v = max;
	if (v < min) v = min;
	return v;
}

void change_mouse(int dx, int dy, int dz, uint8_t buttons)
{
	if (buttons != mouse_data[0] || dx != 0 || dy != 0 || dz != 0) {
		usb_remote_wakeup();
		mouse_data[0] = buttons;
		mouse_data[1] = clamp(dx, -127, 127);
		mouse_data[2] = clamp(dy, -127, 127);
		mouse_data[3] = clamp(dz, -127, 127);
		usb_mouse_send();
	}
}

void clear_key(uint8_t key)
{
	if (key > 0) {
		if (key >= 0xe0 && key < 0xe8) {
			keyboard_data[0] &= ~(1 << (key - 0xe0));
		}
		uint8_t i = 8;
		while (i > 2) {
			i--;
			if (keyboard_data[i] == key) {
				if (i < 7) {
					memmove(keyboard_data + i, keyboard_data + i + 1, 7 - i);
				}
				keyboard_data[7] = 0;
			}
		}
	}
}

bool caps_flag = false;
int caps_timeouts[] = { 0, 0 };
uint8_t caps_instead = 0;
uint8_t muhenkan_flag = 0;

enum HID_Modifire_Mask {
	L_CTRL = 0x01,
	L_SHIFT = 0x02,
	L_ALT = 0x04,
	L_GUI = 0x08,
	R_CTRL = 0x10,
	R_SHIFT = 0x20,
	R_ALT = 0x40,
	R_GUI = 0x80,
};

void release_key(uint8_t key)
{
	if (key == 0x39) { // caps lock
		caps_flag = false;
		clear_key(caps_instead);
		caps_instead = 0;
	}

	if (key == 0x39 || key == 0x51) { // down arrow
		clear_key(0x65); // app
	}

	if (muhenkan_flag) {
		clear_key(0x28); // enter
		clear_key(0x8b); // muhenkan
	}

	clear_key(key);
	keyboard_data[1] = 0;
	usb_keyboard_send();

	muhenkan_flag = 0;
}

void push_key(uint8_t key)
{
	clear_key(key);
	memmove(keyboard_data + 3, keyboard_data + 2, 5);
	keyboard_data[2] = key;
}

void press_key(uint8_t key)
{
	if (key == 0x39 && !(keyboard_data[0] & (L_SHIFT | R_SHIFT))) { // caps lock && not shift
		if (caps_timeouts[1] > 0) {
			clear_key(key);
			key = 0xe3; // left win
			caps_instead = key;
		}
		if (!caps_flag && caps_timeouts[0] < 990) {
			auto t = caps_timeouts[0];
			if (t > 0 && t < 10) {
				t = 10;
			}
			caps_timeouts[1] = t;
		}
		caps_timeouts[0] = 1000;
	} else {
		caps_timeouts[0] = 0;
		caps_timeouts[1] = 0;
	}

	if (key == 0x39) {
		caps_flag = true;
	} else if (caps_flag) {
		if (key == 0x51) { // down arrow
			key = 0x65; // app
		}
	}

	if (key >= 0xe0 && key < 0xe8) {
		keyboard_data[0] |= 1 << (key - 0xe0);
	} else if (key == 0x8b && keyboard_data[0] == 0) { // muhenkan && no modifiers
		muhenkan_flag = 1;
		push_key(0x28); // enter
	} else {
		if (muhenkan_flag) {
			clear_key(0x28); // enter
			clear_key(0x8b); // muhenkan
			muhenkan_flag = 0;
		}
		if (key > 0) {
			push_key(key);
		}
	}
	keyboard_data[1] = 0;
	usb_keyboard_send();
}

void ps2_setup();
void ps2_loop(bool timerevent);

#ifdef LCD_ENABLED

enum {
	LCD_NONE = 0,
	LCD_HOME = 0x0c,
};

uint8_t lcd_queue[128];
uint8_t lcd_head = 0;
uint8_t lcd_size = 0;

extern "C" void lcd_putchar(uint8_t c)
{
	cli();
	if (lcd_size < 128 && c != 0) {
		lcd_queue[(lcd_head + lcd_size) & 0x7f] = c;
		lcd_size++;
	}
	sei();
}

extern "C" void lcd_print(char const *p)
{
	while (*p) {
		lcd_putchar(*p++);
	}
}

extern "C" void lcd_puthex8(uint8_t v)
{
	static char hex[] = "0123456789ABCDEF";
	lcd_putchar(hex[(v >> 4) & 0x0f]);
	lcd_putchar(hex[v & 0x0f]);
}

extern "C" void lcd_home()
{
	lcd_putchar(LCD_HOME);
}

static uint8_t lcd_popfront()
{
	uint8_t c = 0;
	cli();
	if (lcd_size > 0) {
		c = lcd_queue[lcd_head];
		lcd_head = (lcd_head + 1) & 0x7f;
		lcd_size--;
	}
	sei();
	return c;
}

#endif //  LCD_ENABLED

void setup()
{
	// 16 MHz clock
	CLKPR = 0x80; CLKPR = 0;
	// Disable JTAG
	MCUCR |= 0x80; MCUCR |= 0x80;

	PORTB = 0x00;
	PORTC = 0x00;
	DDRB = 0x01;
	DDRC = 0x04;

	TCCR0B = 0x02; // 1/8 prescaling
	TIMSK0 |= 1 << TOIE0;

	usb_init();
	while (!usb_configured()) {
		msleep(10);
	}

	ps2_setup();

#ifdef LCD_ENABLED
	lcd::init();
	lcd::clear();
	lcd::home();
	lcd_print("Quckey3");
#endif

	led(0);
}

void loop()
{
	cli();
	bool timerevent = interval_1ms_flag;
	interval_1ms_flag = false;
	sei();

	if (timerevent) {
		if (caps_timeouts[0] > 0) caps_timeouts[0]--;
		if (caps_timeouts[1] > 0) caps_timeouts[1]--;
	}

	ps2_loop(timerevent);
}

int main()
{
	setup();
	sei();
	while (1) {
#ifdef LCD_ENABLED
		uint8_t c = lcd_popfront();
		if (c == LCD_NONE) {
			loop();
		} else if (c < ' ') {
			if (c == LCD_HOME) {
				lcd::home();
			}
		} else {
			lcd::putchar(c);
		}
#else
		loop();
#endif
	}
}
