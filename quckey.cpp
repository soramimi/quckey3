
#include <avr/io.h>
#include <avr/interrupt.h>

#include "queue16.h"
#include "ps2.h"
#include "ps2if.h"
#include "waitloop.h"

typedef struct Queue16 Queue;
#define qinit(Q) q16init(Q)
#define qpeek(Q) q16peek(Q)
#define qget(Q) q16get(Q)
#define qput(Q,C) q16put(Q,C)
#define qunget(Q,C) q16unget(Q,C)

extern void led(int f);
extern void press_key(uint8_t c);
extern void usb_puthex(int c);

uint8_t quckey_timerevent = 0; // 1ms interval event

PS2KB0 ps2kb0io;
PS2KB1 ps2kb1io;

struct PS2Device {
	AbstractPS2IO *io;
	uint16_t input_bits;
	uint16_t output_bits;
	uint8_t timeout;
	Queue input_queue;
	Queue output_queue;
	Queue event_queue_l;
	Queue event_queue_h;
	uint16_t reset_timer;
	uint16_t state;

	uint16_t real_device_id;
	uint16_t fake_device_id;

	uint16_t repeat_timeout;
	uint8_t decode_state;
	uint8_t shiftflags;
	uint8_t indicator;

	uint16_t typematic;
	int typematic_rate;  // ms
	int typematic_delay; // ms

	uint16_t repeat_count;
	uint16_t repeat_event;
	uint16_t _device_id;
	bool break_enable;
	bool scan_enable;
};

PS2Device ps2k0;
PS2Device ps2k1;

int countbits(uint16_t c)
{
	int i;
	for (i = 0; c; c >>= 1) {
		if (c & 1) i++;
	}
	return i;
}

bool kb_next_output(PS2Device *dev, uint8_t c)
{
	uint16_t d = c;
	if (!(countbits(d) & 1)) d |= 0x100;	// make odd parity
	d = (d | 0x600) << 1;
	//
	// d = 000011pdddddddd0
	//                    
	//                    ^ start bit
	//            ^^^^^^^^  data
	//           ^          odd parity
	//          ^           stop bit
	//         ^            reply from keyboard (ack bit)
	//
	cli();
	if (dev->input_bits) {
		sei();
		return false;
	}
	dev->output_bits = d;
	dev->io->set_clock_0();	// I/O inhibit, trigger interrupt
	dev->io->set_data_0();	// start bit
	sei();
	wait_40us();
	dev->io->set_clock_1();

	dev->timeout = 0;

	return true;
}

inline void kb_put(PS2Device *dev, uint8_t c)
{
	qput(&dev->output_queue, c & 0xff);
}

inline int kb_get(PS2Device *dev)
{
	int c;
	cli();
	c = qget(&dev->input_queue);
	sei();
	return c;
}

inline uint16_t get_event(PS2Device *dev)
{
	uint16_t l, h;
	cli();
	l = qget(&dev->event_queue_l);
	h = qget(&dev->event_queue_h);
	sei();
	if ((l | h) & 0xff00) return 0;
	return (h << 8) | l;
}

inline void put_event(PS2Device *dev, uint16_t c)
{
	cli();
	qput(&dev->event_queue_l, c);
	qput(&dev->event_queue_h, c >> 8);
	sei();
}

inline void unget_event(PS2Device *dev, uint16_t c)
{
	cli();
	qunget(&dev->event_queue_l, c);
	qunget(&dev->event_queue_h, c >> 8);
	sei();
}

// pin change interrupt

void intr(PS2Device *dev)
{
	if (dev->io->get_clock()) {
		sei();
	} else {
		int c;

		dev->timeout = 10;	// 10ms

		if (!dev->input_bits) {
			if (dev->output_bits) {			// transmit mode
				if (dev->output_bits == 1) {
					dev->output_bits = 0;		// end transmit
					dev->timeout = 0;
				} else {
					if (dev->output_bits & 1) {
						dev->io->set_data_1();
					} else {
						dev->io->set_data_0();
					}
					dev->output_bits >>= 1;
				}
			} else {
				dev->input_bits = 0x800;		// start receive
			}
		}
		if (dev->input_bits) {
			dev->input_bits >>= 1;
			if (dev->io->get_data()) {
				dev->input_bits |= 0x800;
			}
			if (dev->input_bits & 1) {
				if (dev->input_bits & 0x800) {				// stop bit ?
					if (countbits(dev->input_bits & 0x7fc) & 1) {	// odd parity ?
						c = (dev->input_bits >> 2) & 0xff;
						qput(&dev->input_queue, c);
					}
				}
				dev->input_bits = 0;
				dev->timeout = 0;
			}
		}
	}
}

ISR(INT0_vect)
{
	intr(&ps2k0);
}

ISR(INT5_vect)
{
	intr(&ps2k1);
}

//

void ps2_io_handler(PS2Device *dev)
{
	int c;

	// transmit to keyboard
	c = qget(&dev->output_queue);
	if (c >= 0) {
		if (!kb_next_output(dev, c)) {
			qunget(&dev->output_queue, c);
		}
	}
}




#if 0
enum {
	PC_INIT			= 0x0000,
	PC_NORMAL		= 0x0100,
	PC_ED			= 0x0200,
	PC_F0			= 0x0300,
	PC_F3			= 0x0400,
	PC_FB			= 0x0500,
	PC_FC			= 0x0600,
	PC_FD			= 0x0700,
	PC_FF			= 0x0800,
};
#endif

enum {
	PS2_INIT          = 0x0000,
	PS2_KEYBOARD_INIT = 0x0100,
	PS2_MOUSE_INIT    = 0x0200,
	PS2_NORMAL        = 0x0300,
	PS2_WAIT	          = 0x0400,
	PS2_ED            = 0x0500,
};

enum {
	EVENT_NULL					= 0,
	EVENT_INIT					= 0x0100,
	EVENT_KEYMAKE				= 0x0200,
	EVENT_KEYBREAK				= 0x0300,
	EVENT_SEND_KB_INDICATOR		= 0x0400,
};






#if 0

void change_typematic(uint8_t v)
{
	typematic = v;
	typematic_rate = 13;
	typematic_delay = 200;
}

#else

void change_typematic(PS2Device *kb, uint8_t v)
{
	kb->typematic = v;

	v = kb->typematic & 0x1f;
	if (v < 0x1a) {
		kb->typematic_rate = v * v * 2 + 12;
		v = (kb->typematic >> 5) & 3;
		kb->typematic_delay = (v + 1) * 200;
	} else {
		kb->typematic_rate = 0;
		kb->typematic_delay = 0;
	}
}

#endif

int get_typematic_rate(PS2Device *kb)
{
	return kb->typematic_rate;
}

int get_typematic_delay(PS2Device *kb)
{
	return kb->typematic_delay;
}


#if 0
static void _send_pc(uint8_t c, void *cookie)
{
	pc_put(c);
}
#endif

void ps2_device_handler(PS2Device *k, bool timer_event_flag)
{
	int c;
	long event;

	if (timer_event_flag) {
		if (k->reset_timer > 0) {
			k->reset_timer--;
			if (k->reset_timer == 0) {
				put_event(k, EVENT_INIT);
			}
		}
		if (k->repeat_count > 0) {
			k->repeat_count--;
			if (k->repeat_count == 0) {
				if (k->repeat_event != 0) {
					put_event(k, k->repeat_event);
					k->repeat_count = k->typematic_rate;
				}
			}
		}
		if (k->repeat_timeout > 0) {
			k->repeat_timeout--;
			if (k->repeat_timeout == 0) {
				k->repeat_count = 0;
			}
		}
		cli();
		if (k->timeout > 0) {
			if (k->timeout > 1) {
				k->timeout--;
			} else {
				k->output_bits = 0;
				k->input_bits = 0;
				k->io->set_data_1();
				k->io->set_clock_1();
				k->timeout = 0;
			}
		}
		sei();
	}

	event = get_event(k);
	if (event > 0) {
		switch (event & 0xff00) {
		case EVENT_INIT:
			kb_put(k, 0xff);
			k->state = PS2_INIT;
			k->reset_timer = 3000;
			break;
		case EVENT_KEYMAKE:
			c = event & 0xff;
			c = convert_scan_code_ibm_to_hid(c);
			press_key(c);
			switch (c) {
			case 44:	k->shiftflags |= KEYFLAG_SHIFT_L;		break;
			case 57:	k->shiftflags |= KEYFLAG_SHIFT_R;		break;
			case 58:	k->shiftflags |= KEYFLAG_CTRL_L;		break;
			case 64:	k->shiftflags |= KEYFLAG_CTRL_R;		break;
			case 60:	k->shiftflags |= KEYFLAG_ALT_L;		break;
			case 62:	k->shiftflags |= KEYFLAG_ALT_R;		break;
			}
			break;
		case EVENT_KEYBREAK:
			c = event & 0xff;
			press_key(0);
			switch (c) {
			case 44:	k->shiftflags &= ~KEYFLAG_SHIFT_L;		break;
			case 57:	k->shiftflags &= ~KEYFLAG_SHIFT_R;		break;
			case 58:	k->shiftflags &= ~KEYFLAG_CTRL_L;		break;
			case 64:	k->shiftflags &= ~KEYFLAG_CTRL_R;		break;
			case 60:	k->shiftflags &= ~KEYFLAG_ALT_L;		break;
			case 62:	k->shiftflags &= ~KEYFLAG_ALT_R;		break;
			}
			break;
		case EVENT_SEND_KB_INDICATOR:
			if (k->state != PS2_NORMAL) {
				unget_event(k, event);
			} else {
				kb_put(k, 0xed);
				k->state = PS2_ED;
				k->reset_timer = 500;
			}
			break;
		}
	}

	c = kb_get(k);
	if (c >= 0) {
		uint16_t t;

		switch (k->state) {
		case PS2_INIT + 0:
			if (c == 0xfa) {
				c = -1;
			} else if (c == 0xaa) {
				kb_put(k, 0xf2);
				k->state++;
				c = -1;
			}
			break;
		case PS2_INIT + 1:
			if (c == 0xfa) {
				k->state++;
				c = -1;
			}
			break;
		case PS2_INIT + 2:	// receive keyboard ID 1st byte
			if (c == 0x00) { // mouse
				k->real_device_id = 0;
				k->fake_device_id = 0;
				kb_put(k, 0xf6);
				k->state = PS2_MOUSE_INIT;
			} else {
				k->_device_id = c;
				k->state = PS2_KEYBOARD_INIT;
			}
			c = -1;
			break;
		case PS2_KEYBOARD_INIT + 0:	// receive keyboard ID 2nd byte
			k->_device_id |= c << 8;
			k->real_device_id = k->_device_id;
			kb_put(k, 0xf3);
			k->state++;
			c = -1;
			break;
		case PS2_KEYBOARD_INIT + 1:
			if (c == 0xfa) {
				kb_put(k, 0x00);
				k->state++;
				c = -1;
			}
			break;
		case PS2_KEYBOARD_INIT + 2:
			if (c == 0xfa) {
				kb_put(k, 0xf4);
				k->state++;
				c = -1;
			}
			break;
		case PS2_KEYBOARD_INIT + 3:
			if (c == 0xfa) {
				kb_put(k, 0xf0);
				k->state++;
				c = -1;
			}
			break;
		case PS2_KEYBOARD_INIT + 4:
			if (c == 0xfa) {
				switch (k->real_device_id) {
				case 0x92ab:	// is IBM5576-001 ?
				case 0x90ab:	// is IBM5576-002 ?
				case 0x91ab:	// is IBM5576-003 ?
					kb_put(k, 0x82);
					break;
				default:
					kb_put(k, 0x02);
					break;
				}
				c = -1;
			}
			k->state = PS2_WAIT;
			break;
		case PS2_MOUSE_INIT + 0:
			if (c == 0xfa) {
				kb_put(k, 0xf3); // set sample rate
				k->state++;
				c = -1;
			}
			break;
		case PS2_MOUSE_INIT + 1:
			if (c == 0xfa) {
				kb_put(k, 100); // 100samples/sec
				k->state++;
				c = -1;
			}
			break;
		case PS2_MOUSE_INIT + 2:
			if (c == 0xfa) {
				kb_put(k, 0xe8); // set resolution
				k->state++;
				c = -1;
			}
			break;
		case PS2_MOUSE_INIT + 3:
			if (c == 0xfa) {
				kb_put(k, 3); // 8count/mm
				k->state++;
				c = -1;
			}
			break;
		case PS2_MOUSE_INIT + 4:
			if (c == 0xfa) {
				kb_put(k, 0xf4); // enable data reporting
				k->state = PS2_WAIT;
				c = -1;
			}
			break;
		case PS2_ED:
			if (c == 0xfa) {
				kb_put(k, k->indicator);
				k->state = PS2_WAIT;
				c = -1;
			} else {
				k->state = PS2_NORMAL;
			}
			break;
		case PS2_WAIT:
			led(1);
			k->reset_timer = 0;
			k->scan_enable = true;
			k->state = PS2_NORMAL;
			c = -1;
			break;
		}

		switch (c) {
		case -1:
			break;
		case 0xaa:
			kb_put(k, 0xf2);
			k->state = PS2_INIT + 1;
			break;
		case 0xfa:
		case 0xee:
		case 0xfc:
		case 0xfe:
		case 0xff:
			k->decode_state = 0;
			k->state = PS2_NORMAL;
			break;
		default:
			switch (k->real_device_id) {
			case 0x92ab:	// is IBM5576-001 ?
				t = ps2decode001(&k->decode_state, c);
				break;
			case 0x90ab:	// is IBM5576-002 ?
			case 0x91ab:	// is IBM5576-003 ?
				t = ps2decode002(&k->decode_state, c);
				break;
			default:
				t = ps2decode(&k->decode_state, c);
				break;
			}
			if (t && k->scan_enable) {
				uint16_t event;
				if (t & 0x8000) {
					event = (t & 0xff) | EVENT_KEYBREAK;
					if (k->break_enable) {
						put_event(k, event);
					}
					if ((event & 0xff) == (k->repeat_event & 0xff)) {
						k->repeat_event = 0;
						k->repeat_count = 0;
						k->repeat_timeout = 0;
					}
				} else {
					event = (t & 0xff) | EVENT_KEYMAKE;
					if (event == k->repeat_event) {
						if (k->repeat_count) {
							k->repeat_timeout = 600;
						}
					} else {
						put_event(k, event);
						k->repeat_event = event;
						k->repeat_timeout = 1200;
						k->repeat_count = k->typematic_delay;

						switch (event & 0xff) {
						case 126:
							k->repeat_count = 0;	// inhibit repeat
							break;
						}
					}
				}
			}
			break;
		}
	}
}





void init_device(PS2Device *dev)
{
	qinit(&dev->event_queue_l);
	qinit(&dev->event_queue_h);
	qinit(&dev->output_queue);
	qinit(&dev->input_queue);
	dev->output_bits = 0;
	dev->input_bits = 0;

	dev->reset_timer = 0;
	dev->state = PS2_INIT;
}

void init_keyboard(PS2Device *k)
{
	k->io->set_clock_0();
	k->io->set_data_1();

	init_device(k);

	k->decode_state = 0;
	k->shiftflags = 0;
	k->indicator = 0;
	change_typematic(k, 0x23);
	k->real_device_id = 0x83ab;
	k->fake_device_id = 0x83ab;
	k->break_enable = true;
	k->scan_enable = false;

	k->io->set_clock_1();

	put_event(k, EVENT_INIT);
}

void quckey_setup()
{
	ps2k0.io = &ps2kb0io;
	ps2k1.io = &ps2kb1io;

	PORTD = 0;
	DDRD = 0xaa;

	EIMSK |= 0x21;
	EICRA = 0x01;
	EICRB = 0x04;

	init_keyboard(&ps2k0);
	init_keyboard(&ps2k1);
}

void quckey_loop()
{
	cli();
	bool timerevent = quckey_timerevent;
	quckey_timerevent = false;
	sei();

	ps2_device_handler(&ps2k0, timerevent);
	ps2_device_handler(&ps2k1, timerevent);

	ps2_io_handler(&ps2k0);
	ps2_io_handler(&ps2k1);
}





