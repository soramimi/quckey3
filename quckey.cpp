
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

class PS2Keyboard {
public:
	AbstractPS2IO *io;
	uint16_t input_bits;
	uint16_t output_bits;
	uint8_t timeout;
	uint16_t repeat_timeout;
	Queue input_queue;
	Queue output_queue;

	uint8_t decode_state;
	uint16_t state;
	uint8_t shiftflags;
	uint16_t reset_timer;
	uint8_t indicator;

	uint16_t typematic;
	int typematic_rate;  // ms
	int typematic_delay; // ms

	uint16_t repeat_count;
	uint16_t repeat_event;
	uint16_t _keyboard_id;
	uint16_t real_keyboard_id;
	uint16_t fake_keyboard_id;
	bool break_enable;
	bool scan_enable;

	Queue event_queue_l;
	Queue event_queue_h;

};

PS2Keyboard ps2kb0;
PS2Keyboard ps2kb1;

int countbits(uint16_t c)
{
	int i;
	for (i = 0; c; c >>= 1) {
		if (c & 1) i++;
	}
	return i;
}

bool kb_next_output(PS2Keyboard *kb, uint8_t c)
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
	if (kb->input_bits) {
		sei();
		return false;
	}
	kb->output_bits = d;
	kb->io->set_clock_0();	// I/O inhibit, trigger interrupt
	kb->io->set_data_0();	// start bit
	sei();
	wait_40us();
	kb->io->set_clock_1();

	kb->timeout = 0;

	return true;
}

inline void kb_put(PS2Keyboard *kb, uint8_t c)
{
	qput(&kb->output_queue, c & 0xff);
}

inline int kb_get(PS2Keyboard *kb)
{
	int c;
	cli();
	c = qget(&kb->input_queue);
	sei();
	return c;
}

inline uint16_t get_event(PS2Keyboard *kb)
{
	uint16_t l, h;
	cli();
	l = qget(&kb->event_queue_l);
	h = qget(&kb->event_queue_h);
	sei();
	if ((l | h) & 0xff00) return 0;
	return (h << 8) | l;
}

inline void put_event(PS2Keyboard *kb, uint16_t c)
{
	cli();
	qput(&kb->event_queue_l, c);
	qput(&kb->event_queue_h, c >> 8);
	sei();
}

inline void unget_event(PS2Keyboard *kb, uint16_t c)
{
	cli();
	qunget(&kb->event_queue_l, c);
	qunget(&kb->event_queue_h, c >> 8);
	sei();
}

// pin change interrupt

void intr(PS2Keyboard *kb)
{
	if (kb->io->get_clock()) {
		sei();
	} else {
		int c;

		kb->timeout = 10;	// 10ms

		if (!kb->input_bits) {
			if (kb->output_bits) {			// transmit mode
				if (kb->output_bits == 1) {
					kb->output_bits = 0;		// end transmit
					kb->timeout = 0;
				} else {
					if (kb->output_bits & 1) {
						kb->io->set_data_1();
					} else {
						kb->io->set_data_0();
					}
					kb->output_bits >>= 1;
				}
			} else {
				kb->input_bits = 0x800;		// start receive
			}
		}
		if (kb->input_bits) {
			kb->input_bits >>= 1;
			if (kb->io->get_data()) {
				kb->input_bits |= 0x800;
			}
			if (kb->input_bits & 1) {
				if (kb->input_bits & 0x800) {				// stop bit ?
					if (countbits(kb->input_bits & 0x7fc) & 1) {	// odd parity ?
						c = (kb->input_bits >> 2) & 0xff;
						qput(&kb->input_queue, c);
					}
				}
				kb->input_bits = 0;
				kb->timeout = 0;
			}
		}
	}
}

ISR(INT0_vect)
{
	intr(&ps2kb0);
}

ISR(INT5_vect)
{
	intr(&ps2kb1);
}

//

void ps2io_handler(PS2Keyboard *kb)
{
	int c;

#if 0
	// receive from host
	c = pc_recv();
	if (c >= 0) {
		qput(&pc_input_queue, c);
	}

	// transmit to host
	c = qget(&pc_output_queue);
	if (c >= 0) {
		if (!pc_send(c)) {
			qunget(&pc_output_queue, c);	// failed
		}
	}
#endif

	// transmit to keyboard
	c = qget(&kb->output_queue);
	if (c >= 0) {
		if (!kb_next_output(kb, c)) {
			qunget(&kb->output_queue, c);
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
	KB_INIT			= 0x0000,
	KB_NORMAL		= 0x0100,
	KB_WAIT			= 0x0200,
	KB_ED			= 0x0300,
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

void change_typematic(PS2Keyboard *kb, uint8_t v)
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

int get_typematic_rate(PS2Keyboard *kb)
{
	return kb->typematic_rate;
}

int get_typematic_delay(PS2Keyboard *kb)
{
	return kb->typematic_delay;
}


#if 0
static void _send_pc(uint8_t c, void *cookie)
{
	pc_put(c);
}
#endif

void keyboard_handler(PS2Keyboard *kb, bool timer_event_flag)
{
	int c;
	long event;

	if (timer_event_flag) {
		if (kb->reset_timer) {
			if (!--kb->reset_timer) {
				put_event(kb, EVENT_INIT);
			}
		}
		if (kb->repeat_count) {
			if (!--kb->repeat_count) {
				if (kb->repeat_event) {
					put_event(kb, kb->repeat_event);
					kb->repeat_count = kb->typematic_rate;
				}
			}
		}
		if (kb->repeat_timeout) {
			if (!--kb->repeat_timeout) {
				kb->repeat_count = 0;
			}
		}
		cli();
		if (kb->timeout > 0) {
			if (kb->timeout > 1) {
				kb->timeout--;
			} else {
				kb->output_bits = 0;
				kb->input_bits = 0;
				kb->io->set_data_1();
				kb->io->set_clock_1();
				kb->timeout = 0;
			}
		}
		sei();
	}

	event = get_event(kb);
	if (event > 0) {
		switch (event & 0xff00) {
		case EVENT_INIT:
			kb_put(kb, 0xff);
			kb->state = KB_INIT;
			kb->reset_timer = 3000;
			break;
		case EVENT_KEYMAKE:
			c = event & 0xff;
			c = convert_scan_code_ibm_to_hid(c);
			press_key(c);
			switch (c) {
			case 44:	kb->shiftflags |= KEYFLAG_SHIFT_L;		break;
			case 57:	kb->shiftflags |= KEYFLAG_SHIFT_R;		break;
			case 58:	kb->shiftflags |= KEYFLAG_CTRL_L;		break;
			case 64:	kb->shiftflags |= KEYFLAG_CTRL_R;		break;
			case 60:	kb->shiftflags |= KEYFLAG_ALT_L;		break;
			case 62:	kb->shiftflags |= KEYFLAG_ALT_R;		break;
			}
			break;
		case EVENT_KEYBREAK:
			c = event & 0xff;
			press_key(0);
			switch (c) {
			case 44:	kb->shiftflags &= ~KEYFLAG_SHIFT_L;		break;
			case 57:	kb->shiftflags &= ~KEYFLAG_SHIFT_R;		break;
			case 58:	kb->shiftflags &= ~KEYFLAG_CTRL_L;		break;
			case 64:	kb->shiftflags &= ~KEYFLAG_CTRL_R;		break;
			case 60:	kb->shiftflags &= ~KEYFLAG_ALT_L;		break;
			case 62:	kb->shiftflags &= ~KEYFLAG_ALT_R;		break;
			}
			break;
		case EVENT_SEND_KB_INDICATOR:
			if (kb->state != KB_NORMAL) {
				unget_event(kb, event);
			} else {
				kb_put(kb, 0xed);
				kb->state = KB_ED;
				kb->reset_timer = 500;
			}
			break;
		}
	}

	c = kb_get(kb);
	if (c >= 0) {
		uint16_t t;

		switch (kb->state) {
		case KB_INIT + 0:
			if (c == 0xfa) {
				c = -1;
			} if (c == 0xaa) {
				kb_put(kb, 0xf2);
				kb->state++;
				c = -1;
			}
			break;
		case KB_INIT + 1:
			if (c == 0xfa) {
				kb->state++;
				c = -1;
			}
			break;
		case KB_INIT + 2:	// receive keyboard ID 1st byte
			kb->_keyboard_id = c;
			kb->state++;
			c = -1;
			break;
		case KB_INIT + 3:	// receive keyboard ID 2nd byte
			kb->_keyboard_id |= c << 8;
			kb->real_keyboard_id = kb->_keyboard_id;
			kb_put(kb, 0xf3);
			kb->state++;
			c = -1;
			break;
		case KB_INIT + 4:
			if (c == 0xfa) {
				kb_put(kb, 0x00);
				kb->state++;
				c = -1;
			}
			break;
		case KB_INIT + 5:
			if (c == 0xfa) {
				kb_put(kb, 0xf4);
				kb->state++;
				c = -1;
			}
			break;
		case KB_INIT + 6:
			if (c == 0xfa) {
				kb_put(kb, 0xf0);
				kb->state++;
				c = -1;
			}
			break;
		case KB_INIT + 7:
			if (c == 0xfa) {
				switch (kb->real_keyboard_id) {
				case 0x92ab:	// is IBM5576-001 ?
				case 0x90ab:	// is IBM5576-002 ?
				case 0x91ab:	// is IBM5576-003 ?
					kb_put(kb, 0x82);
					break;
				default:
					kb_put(kb, 0x02);
					break;
				}
				c = -1;
			}
			kb->state = KB_WAIT;
			break;
		case KB_ED:
			if (c == 0xfa) {
				kb_put(kb, kb->indicator);
				kb->state = KB_WAIT;
				c = -1;
			} else {
				kb->state = KB_NORMAL;
			}
			break;
		case KB_WAIT:
			led(1);
			kb->reset_timer = 0;
			kb->scan_enable = true;
			kb->state = KB_NORMAL;
			c = -1;
			break;
		}

		switch (c) {
		case -1:
			break;
		case 0xaa:
			kb_put(kb, 0xf2);
			kb->state = KB_INIT + 1;
			break;
		case 0xfa:
		case 0xee:
		case 0xfc:
		case 0xfe:
		case 0xff:
			kb->decode_state = 0;
			kb->state = KB_NORMAL;
			break;
		default:
			switch (kb->real_keyboard_id) {
			case 0x92ab:	// is IBM5576-001 ?
				t = ps2decode001(&kb->decode_state, c);
				break;
			case 0x90ab:	// is IBM5576-002 ?
			case 0x91ab:	// is IBM5576-003 ?
				t = ps2decode002(&kb->decode_state, c);
				break;
			default:
				t = ps2decode(&kb->decode_state, c);
				break;
			}
			if (t && kb->scan_enable) {
				uint16_t event;
				if (t & 0x8000) {
					event = (t & 0xff) | EVENT_KEYBREAK;
					if (kb->break_enable) {
						put_event(kb, event);
					}
					if ((event & 0xff) == (kb->repeat_event & 0xff)) {
						kb->repeat_event = 0;
						kb->repeat_count = 0;
						kb->repeat_timeout = 0;
					}
				} else {
					event = (t & 0xff) | EVENT_KEYMAKE;
					if (event == kb->repeat_event) {
						if (kb->repeat_count) {
							kb->repeat_timeout = 600;
						}
					} else {
						put_event(kb, event);
						kb->repeat_event = event;
						kb->repeat_timeout = 1200;
						kb->repeat_count = kb->typematic_delay;

						switch (event & 0xff) {
						case 126:
							kb->repeat_count = 0;	// inhibit repeat
							break;
						}
					}
				}
			}
			break;
		}
	}
}

void init_keyboard(PS2Keyboard *kb)
{
	kb->io->set_clock_0();
	kb->io->set_data_1();

	qinit(&kb->event_queue_l);
	qinit(&kb->event_queue_h);
	qinit(&kb->output_queue);
	qinit(&kb->input_queue);
	kb->output_bits = 0;
	kb->input_bits = 0;

	kb->reset_timer = 0;
	kb->state = KB_INIT;
	kb->decode_state = 0;
	kb->shiftflags = 0;
	kb->indicator = 0;
	change_typematic(kb, 0x23);
	kb->real_keyboard_id = 0x83ab;
	kb->fake_keyboard_id = 0x83ab;
	kb->break_enable = true;
	kb->scan_enable = false;

	kb->io->set_clock_1();

	put_event(kb, EVENT_INIT);
}

void quckey_setup()
{
	ps2kb0.io = &ps2kb0io;
	ps2kb1.io = &ps2kb1io;

	PORTD = 0;
	DDRD = 0xaa;

	EIMSK |= 0x21;
	EICRA = 0x01;
	EICRB = 0x04;

	init_keyboard(&ps2kb0);
	init_keyboard(&ps2kb1);
}

void quckey_loop()
{
	cli();
	bool timerevent = quckey_timerevent;
	quckey_timerevent = false;
	sei();
	keyboard_handler(&ps2kb0, timerevent);
	keyboard_handler(&ps2kb1, timerevent);
	ps2io_handler(&ps2kb0);
	ps2io_handler(&ps2kb1);
}





