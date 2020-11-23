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

// Quckey3 - Copyright (C) 2020 S.Fuchita (@soramimi_jp)

#include "lcd.h"
#include "usb.h"
#include <avr/interrupt.h>
#include <string.h>
#include "waitloop.h"

#define CLOCK 16000000UL
#define SCALE 125
static unsigned short _scale = 0;
static unsigned long _system_tick_count;
static unsigned long _tick_count;
static unsigned long _time_s;
static unsigned short _time_ms;
extern uint8_t interval_1ms_flag;
ISR(TIMER0_OVF_vect, ISR_NOBLOCK)
{
	_system_tick_count++;
	_scale += 16;
	if (_scale >= SCALE) {
		_scale -= SCALE;
		_tick_count++;
		_time_ms++;
		if (_time_ms >= 1000) {
			_time_ms = 0;
			_time_s++;
		}
		interval_1ms_flag = 1;
	}
}

void led(int f)
{
	if (f) {
		PORTB |= 0x01;
	} else {
		PORTB &= ~0x01;
	}
}

uint8_t keys[32]; // 256/8

struct {
	int dx = 0;
	int dy = 0;
	int dz = 0;
	int buttons = 0;
} mouse;

void change_mouse(int dx, int dy, int dz, int buttons)
{
	mouse.dx += dx;
	mouse.dy += dy;
	mouse.dz += dz;
	mouse.buttons = buttons;
}

void send_keys()
{
	memset(keyboard_data, 0, 6);
	keyboard_modifier_keys = 0;

	auto Pressed = [&](uint8_t k){
		return (keys[k >> 3] >> (k & 7)) & 1;
	};

	auto Push = [&](uint8_t k){
		memmove(keyboard_data + 1, keyboard_data, 5);
		keyboard_data[0] = k;
	};

	unsigned char k = 0xdf;
	while (k > 0) {
		if (Pressed(k)) {
			Push(k);
		}
		k--;
	}

	for (unsigned char i = 0; i < 8; i++) {
		if (Pressed(0xe0 + i) & 1) {
			keyboard_modifier_keys |= 1 << i;
		}
	}

	usb_keyboard_send();
}

void press_key(uint8_t key)
{
	keys[key >> 3] |= 1 << (key & 7);
	send_keys();
}

void release_key(uint8_t key)
{
	keys[key >> 3] &= ~(1 << (key & 7));
	send_keys();
}

static inline int clamp(int v, int min, int max)
{
	if (v > max) v = max;
	if (v < min) v = min;
	return v;
}

void mouse_loop()
{
	static int last_buttons = 0;
	int buttons = mouse.buttons;
	int dx = clamp(mouse.dx, -127, 127);
	int dy = clamp(mouse.dy, -127, 127);
	int dz = clamp(mouse.dz, -127, 127);
	if (buttons != last_buttons || dx != 0 || dy != 0 || dz != 0) {
		last_buttons = buttons;
		mouse.dx -= dx;
		mouse.dy -= dy;
		mouse.dz -= dz;
		mouse_data[0] = buttons;
		mouse_data[1] = dx;
		mouse_data[2] = dy;
		mouse_data[3] = dz;
		usb_mouse_send();
	}
}

void keyboard_setup();
void keyboard_loop();

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

	keyboard_setup();

	lcd::init();
	lcd::clear();
	lcd::home();
//	lcd::print("Quckey3");

	led(1);
}

void loop()
{
	keyboard_loop();
	mouse_loop();
}

int main()
{
	setup();
	sei();
	while (1) {
		loop();
	}
}

