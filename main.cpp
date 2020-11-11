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


#include <stdlib.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "usb.h"
#include "avr.h"
#include <string.h>

typedef struct key {
	uint8_t value;
	uint8_t type;
} KEY;

typedef struct status {
	bool pressed;
	uint8_t release;
} STATUS;

#define CLOCK 16000000UL
#define SCALE 125
static unsigned short _scale = 0;
static volatile unsigned long _system_tick_count;
static volatile unsigned long _tick_count;
static volatile unsigned long _time_s;
static unsigned short _time_ms;
extern uint8_t quckey_timerevent;
ISR(TIMER0_OVF_vect, ISR_NOBLOCK)
{
	_system_tick_count++;
	_scale += 16;
	if (_scale >= SCALE) {
		_scale -= SCALE;
		_tick_count++;
		_time_ms++;
		if (_time_ms >= 1000) {
			quckey_timerevent = 1;
			_time_ms = 0;
			_time_s++;
		}
	}
}

void send()
{
	keyboard_modifier_keys = 0;
	usb_keyboard_send();
}

void quckey_setup();
void quckey_loop();

void led(int f)
{
	if (f) {
		PORTB |= 0x01;
	} else {
		PORTB &= ~0x01;
	}
}

void init()
{
	PORTB = 0x00;
	PORTC = 0x00;
	DDRB = 0x01;
	DDRC = 0x04;
	TCCR0B = 0x02; // 1/8 prescaling
	TIMSK0 |= 1 << TOIE0;

	// 16 MHz clock
	CLKPR = 0x80; CLKPR = 0;
	// Disable JTAG
	MCUCR |= 0x80; MCUCR |= 0x80;
	usb_init();
	while (!usb_configured()) {
		_delay_ms(100);
	}
	sei();  // Enable interrupts
}


int main()
{
	init();
	quckey_setup();

	sei();

	while (1) {
		if (1) {
#if 0
			_delay_ms(100);

			memset(keyboard_data, 0, sizeof(keyboard_data));
			keyboard_data[0] = 0x04;
			usb_keyboard_send();
			keyboard_data[0] = 0;
			usb_keyboard_send();



			mouse_data[0] = 0;
			mouse_data[1] = 1;
			mouse_data[2] = 1;
			mouse_data[3] = 0;

			usb_mouse_send();
#endif
		}
		quckey_loop();
	}
}

void press_key(uint8_t c)
{
	memset(keyboard_data, 0, sizeof(keyboard_data));
	keyboard_data[0] = c;
	usb_keyboard_send();
	keyboard_data[0] = 0;
	usb_keyboard_send();
}

void usb_puthex(int c)
{
	static const char hex[] = "\x27\x1e\x1f\x20\x21\x22\x23\x24\x25\x26\x04\x05\x06\x07\x08\x09";
	memset(keyboard_data, 0, sizeof(keyboard_data));
	keyboard_data[0] = hex[(c >> 4) & 15];
	usb_keyboard_send();
	keyboard_data[0] = 0;
	usb_keyboard_send();
	keyboard_data[0] = hex[c & 15];
	usb_keyboard_send();
	keyboard_data[0] = 0;
	usb_keyboard_send();
}

