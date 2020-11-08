
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


#define NULL 0

extern void led(int f);
extern void usb_put_a();
extern void usb_puthex(int c);

Queue event_queue_l;
Queue event_queue_h;
Queue kb_input_queue;
Queue kb_output_queue;
//Queue pc_input_queue;
//Queue pc_output_queue;

PS2KB0 ps2kb0io;
PS2KB1 ps2kb1io;
class PS2Keyboard {
public:
	AbstractPS2IO *io;
	unsigned short kb_input_bits;
	unsigned short kb_output_bits;
	unsigned char kb_timeout;
	unsigned short repeat_timeout;
};

PS2Keyboard ps2kb0;
PS2Keyboard ps2kb1;

PS2Keyboard *kb;




int countbits(unsigned short c)
{
	int i;
	for (i = 0; c; c >>= 1) {
		if (c & 1) i++;
	}
	return i;
}

// initializer

inline void initialize_kb_output()
{
	qinit(&kb_output_queue);
	kb->kb_output_bits = 0;
}

inline void initialize_kb_input()
{
	qinit(&kb_input_queue);
	kb->kb_input_bits = 0;
}

#if 0
inline void initialize_pc_output()
{
	qinit(&pc_output_queue);
	pc_output_bits = 0;
}

inline void initialize_pc_input()
{
	qinit(&pc_input_queue);
	pc_input_bits = 0;
}
#endif




bool kb_next_output(unsigned char c)
{
	unsigned short d = c;
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
	if (kb->kb_input_bits) {
		sei();
		return false;
	}
	kb->kb_output_bits = d;
	kb->io->set_clock_0();	// I/O inhibit, trigger interrupt
	kb->io->set_data_0();	// start bit
	sei();
	wait_40us();
	kb->io->set_clock_1();

	kb->kb_timeout = 0;

	return true;
}

#if 0
bool pc_send(unsigned char c)
{
	unsigned char i;
	unsigned short bits;

	bits = c & 0xff;
	if (!(countbits(bits) & 1)) bits |= 0x100;
	bits = (bits | 0x200) << 1;

	for (i = 0; i < 11; i++) {
		if (!pc_get_clock()) break;
		if (bits & 1)	pc_set_data_1();
		else			pc_set_data_0();
		bits >>= 1;
		wait_15us();
		if (!pc_get_clock()) break;
		pc_set_clock_0();
		wait_40us();
		pc_set_clock_1();
		wait_15us();
	}
	pc_set_data_1();

	return i == 11;
}

int pc_recv()
{
	int i;
	unsigned short bits;

	if (!pc_get_clock()) return -1;
	if (pc_get_data()) return -1;

	bits = 0;
	for (i = 0; i < 100; i++) {
		pc_set_clock_0();
		wait_40us();
		pc_set_clock_1();
		wait_15us();
		wait_15us();
		if (i < 9) {
			bits >>= 1;
			if (pc_get_data()) bits |= 0x100;
		} else {
			if (pc_get_data()) {	// stop bit ?
				break;
			}
		}
	}
	pc_set_clock_0();
	pc_set_data_0();
	wait_40us();
	pc_set_data_1();
	pc_set_clock_1();

	if (i != 9)
		return -1;							// framing error

	if (!(countbits(bits & 0x1ff) & 1))
		return -1;							// parity error

	return bits & 0xff;
}
#endif




inline void kb_put(unsigned char c)
{
	qput(&kb_output_queue, c & 0xff);
}

inline int kb_get()
{
	int c;
	cli();
	c = qget(&kb_input_queue);
	sei();
	return c;
}

#if 0
inline void pc_put(unsigned char c)
{
	qput(&pc_output_queue, c & 0xff);
}

inline int pc_get()
{
	int c = qget(&pc_input_queue);
	return c;
}
#endif

inline unsigned short get_event()
{
	unsigned short l, h;
	cli();
	l = qget(&event_queue_l);
	h = qget(&event_queue_h);
	sei();
	if ((l | h) & 0xff00) return 0;
	return (h << 8) | l;
}

inline void put_event(unsigned short c)
{
	cli();
	qput(&event_queue_l, c);
	qput(&event_queue_h, c >> 8);
	sei();
}

inline void unget_event(unsigned short c)
{
	cli();
	qunget(&event_queue_l, c);
	qunget(&event_queue_h, c >> 8);
	sei();
}

// interval timer interrupt

uint8_t quckey_timerevent = 0;

#if 0
#define CLOCK 8000000UL
#define SCALE (CLOCK / 256)
#define PRESCALE 8

static unsigned short _scale = 0;

//SIGNAL(TIMER0_OVF_vect)
ISR(TIMER0_OVF_vect, ISR_NOBLOCK) // for 5576-003 problem
{
	_scale += 1000 * PRESCALE;
	if (_scale >= SCALE) {
		_scale -= SCALE;
		timerevent = true;
	}
}
#endif

// pin change interrupt

void intr(PS2Keyboard *kb)
{
	if (kb->io->get_clock()) {
		sei();
	} else {
		int c;

		kb->kb_timeout = 10;	// 10ms

		if (!kb->kb_input_bits) {
			if (kb->kb_output_bits) {			// transmit mode
				if (kb->kb_output_bits == 1) {
					kb->kb_output_bits = 0;		// end transmit
					kb->kb_timeout = 0;
				} else {
					if (kb->kb_output_bits & 1) {
						kb->io->set_data_1();
					} else {
						kb->io->set_data_0();
					}
					kb->kb_output_bits >>= 1;
				}
			} else {
				kb->kb_input_bits = 0x800;		// start receive
			}
		}
		if (kb->kb_input_bits) {
			kb->kb_input_bits >>= 1;
			if (kb->io->get_data()) {
				kb->kb_input_bits |= 0x800;
			}
			if (kb->kb_input_bits & 1) {
				if (kb->kb_input_bits & 0x800) {				// stop bit ?
					if (countbits(kb->kb_input_bits & 0x7fc) & 1) {	// odd parity ?
						c = (kb->kb_input_bits >> 2) & 0xff;
						qput(&kb_input_queue, c);
					}
				}
				kb->kb_input_bits = 0;
				kb->kb_timeout = 0;
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

void system_handler()
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
	c = qget(&kb_output_queue);
	if (c >= 0) {
		if (!kb_next_output(c)) {
			qunget(&kb_output_queue, c);
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



unsigned char key_decode_state;
//unsigned short pc_state;
unsigned short kb_state;
unsigned char shiftflags;
//unsigned short pc_init_timer;
unsigned short kb_reset_timer;
unsigned char indicator;

unsigned short typematic;
int typematic_rate;  // ms
int typematic_delay; // ms

unsigned short repeat_count;
unsigned short repeat_event;
unsigned short real_keyboard_id;
unsigned short fake_keyboard_id;
unsigned short _keyboard_id;
bool break_enable;
bool scan_enable;



#if 0

void change_typematic(unsigned char v)
{
	typematic = v;
	typematic_rate = 13;
	typematic_delay = 200;
}

#else

void change_typematic(unsigned char v)
{
	typematic = v;

	v = typematic & 0x1f;
	if (v < 0x1a) {
		typematic_rate = v * v * 2 + 12;
		v = (typematic >> 5) & 3;
		typematic_delay = (v + 1) * 200;
	} else {
		typematic_rate = 0;
		typematic_delay = 0;
	}

}

#endif

int get_typematic_rate()
{
	return typematic_rate;
}

int get_typematic_delay()
{
	return typematic_delay;
}


#if 0
static void _send_pc(unsigned char c, void *cookie)
{
	pc_put(c);
}
#endif

void handler()
{
	int c;
	long event;

	if (quckey_timerevent) {
		quckey_timerevent = false;
#if 0
		if (pc_state == PC_INIT) {
			if (pc_init_timer > 0) {
				pc_init_timer--;
			} else {
				initialize_pc_output();
				pc_put(0xaa);
				scan_enable = true;
				pc_state = PC_NORMAL;
			}
		}
#endif
		if (kb_reset_timer) {
			if (!--kb_reset_timer) {
				put_event(EVENT_INIT);
			}
		}
		if (repeat_count) {
			if (!--repeat_count) {
				if (repeat_event) {
					put_event(repeat_event);
					repeat_count = typematic_rate;
				}
			}
		}
		if (kb->repeat_timeout) {
			if (!--kb->repeat_timeout) {
				repeat_count = 0;
			}
		}
		cli();
		if (kb->kb_timeout > 0) {
			if (kb->kb_timeout > 1) {
				kb->kb_timeout--;
			} else {
				kb->kb_output_bits = 0;
				kb->kb_input_bits = 0;
				kb->io->set_data_1();
				kb->io->set_clock_1();
				kb->kb_timeout = 0;
			}
		}
		sei();
	}

	event = get_event();
	if (event > 0) {
		switch (event & 0xff00) {
		case EVENT_INIT:
			{//if ((kb_state & 0xff00) != KB_INIT) {
				kb_put(0xff);
				kb_state = KB_INIT;
				kb_reset_timer = 3000;
			}
//			scan_enable = false;
//			pc_init_timer = 400;		// wait 400ms
//			pc_state = PC_INIT;
			scan_enable = true;
			break;
		case EVENT_KEYMAKE:
			c = event & 0xff;
//			ps2encode(c, shiftflags | KEYFLAG_MAKE, _send_pc, NULL);
			switch (c) {
			case 44:	shiftflags |= KEYFLAG_SHIFT_L;		break;
			case 57:	shiftflags |= KEYFLAG_SHIFT_R;		break;
			case 58:	shiftflags |= KEYFLAG_CTRL_L;		break;
			case 64:	shiftflags |= KEYFLAG_CTRL_R;		break;
			case 60:	shiftflags |= KEYFLAG_ALT_L;		break;
			case 62:	shiftflags |= KEYFLAG_ALT_R;		break;
			}
			break;
		case EVENT_KEYBREAK:
			c = event & 0xff;
//			ps2encode(c, shiftflags, _send_pc, NULL);
			switch (c) {
			case 44:	shiftflags &= ~KEYFLAG_SHIFT_L;		break;
			case 57:	shiftflags &= ~KEYFLAG_SHIFT_R;		break;
			case 58:	shiftflags &= ~KEYFLAG_CTRL_L;		break;
			case 64:	shiftflags &= ~KEYFLAG_CTRL_R;		break;
			case 60:	shiftflags &= ~KEYFLAG_ALT_L;		break;
			case 62:	shiftflags &= ~KEYFLAG_ALT_R;		break;
			}
			break;
		case EVENT_SEND_KB_INDICATOR:
			if (kb_state != KB_NORMAL) {
				unget_event(event);
			} else {
				kb_put(0xed);
				kb_state = KB_ED;
				kb_reset_timer = 500;
			}
			break;
		}
	}

	c = kb_get();
	if (c >= 0) {
		unsigned short t;

		switch (kb_state) {
		case KB_INIT + 0:
			if (c == 0xfa) {
				c = -1;
			} if (c == 0xaa) {
				kb_put(0xf2);
				kb_state++;
				c = -1;
			}
			break;
		case KB_INIT + 1:
			if (c == 0xfa) {
				kb_state++;
				c = -1;
			}
			break;
		case KB_INIT + 2:	// receive keyboard ID 1st byte
			_keyboard_id = c;
			kb_state++;
			c = -1;
			break;
		case KB_INIT + 3:	// receive keyboard ID 2nd byte
			_keyboard_id |= c << 8;
			real_keyboard_id = _keyboard_id;
			kb_put(0xf3);
			kb_state++;
			c = -1;
			break;
		case KB_INIT + 4:
			if (c == 0xfa) {
				kb_put(0x00);
				kb_state++;
				c = -1;
			}
			break;
		case KB_INIT + 5:
			if (c == 0xfa) {
				kb_put(0xf4);
				kb_state++;
				c = -1;
			}
			break;
		case KB_INIT + 6:
			if (c == 0xfa) {
				kb_put(0xf0);
				kb_state++;
				c = -1;
			}
			break;
		case KB_INIT + 7:
			if (c == 0xfa) {
				switch (real_keyboard_id) {
				case 0x92ab:	// is IBM5576-001 ?
				case 0x90ab:	// is IBM5576-002 ?
				case 0x91ab:	// is IBM5576-003 ?
					kb_put(0x82);
					break;
				default:
					kb_put(0x02);
					break;
				}
				c = -1;
			}
			kb_state = KB_WAIT;
			break;
		case KB_ED:
			if (c == 0xfa) {
				kb_put(indicator);
				kb_state = KB_WAIT;
				c = -1;
			} else {
				kb_state = KB_NORMAL;
			}
			break;
		case KB_WAIT:
			led(1);
			kb_reset_timer = 0;
			kb_state = KB_NORMAL;
			c = -1;
			break;
		}

		switch (c) {
		case -1:
			break;
		case 0xaa:
			kb_put(0xf2);
			kb_state = KB_INIT + 1;
			break;
		case 0xfa:
		case 0xee:
		case 0xfc:
		case 0xfe:
		case 0xff:
			key_decode_state = 0;
			kb_state = KB_NORMAL;
			break;
		default:
			switch (real_keyboard_id) {
#if 0
			case 0x92ab:	// is IBM5576-001 ?
				t = ps2decode001(&key_decode_state, c);
				break;
			case 0x90ab:	// is IBM5576-002 ?
			case 0x91ab:	// is IBM5576-003 ?
				t = ps2decode002(&key_decode_state, c);
				break;
#endif
			default:
				t = ps2decode(&key_decode_state, c);
				break;
			}
			usb_puthex(t >> 8);
			usb_puthex(t);
			if (t && scan_enable) {
				unsigned short event;
				if (t & 0x8000) {
					event = (t & 0xff) | EVENT_KEYBREAK;
					if (break_enable) {
						put_event(event);
					}
					if ((event & 0xff) == (repeat_event & 0xff)) {
						repeat_event = 0;
						repeat_count = 0;
						kb->repeat_timeout = 0;
					}
				} else {
					event = (t & 0xff) | EVENT_KEYMAKE;
					if (event == repeat_event) {
						if (repeat_count) {
							kb->repeat_timeout = 600;
						}
					} else {
						put_event(event);
						repeat_event = event;
						kb->repeat_timeout = 1200;
						repeat_count = typematic_delay;

						switch (event & 0xff) {
						case 126:
							repeat_count = 0;	// inhibit repeat
							break;
						}
					}
				}
			}
			break;
		}
	}

#if 0
	c = pc_get();
	if (c >= 0) {
		switch (pc_state & 0xff00) {
		case PC_ED:
			indicator = c;
			if (c & 2) {
				shiftflags |= KEYFLAG_NUMLOCK;
			} else {
				shiftflags &= ~KEYFLAG_NUMLOCK;
			}
			pc_put(0xfa);
			pc_state = PC_NORMAL;
			put_event(EVENT_SEND_KB_INDICATOR);
			break;
		case PC_F0:
			if (!c) {
				pc_put(0x02);	// scan code set 2
			} else {
				pc_put(0xfa);
			}
			pc_state = PC_NORMAL;
			break;
		case PC_F3:
			change_typematic(c);
			pc_put(0xfa);
			pc_state = PC_NORMAL;
			break;
		case PC_INIT:
			if (c == 0xec) {
				break;
			}
			pc_state = PC_NORMAL;	// cancel initialize command
			scan_enable = true;
			// fallthru
		default:
			switch (c) {
			case 0xed:
				pc_put(0xfa);
				pc_state = PC_ED;
				break;
			case 0xee:
				pc_put(0xee);
				break;
			case 0xf0:
				pc_put(0xfa);
				pc_state = PC_F0;
				break;
			case 0xf1:
				pc_put(0xfa);
				break;
			case 0xf2:
				pc_put(0xfa);
				pc_put(fake_keyboard_id);
				pc_put(fake_keyboard_id >> 8);
				scan_enable = true;
				break;
			case 0xf3:			// set typematic rate/delay
				pc_put(0xfa);
				pc_state = PC_F3;
				break;
			case 0xf4:			// enable
				pc_put(0xfa);
				scan_enable = true;
				break;
			case 0xf5:			// default disable
				pc_put(0xfa);
				scan_enable = false;
				break;
			case 0xf6:
			case 0xf7:
			case 0xf8:
			case 0xf9:
			case 0xfa:
			case 0xfb:
			case 0xfc:
			case 0xfd:
				pc_put(0xfa);
				break;
			case 0xfe:
				break;
			case 0xff:
				pc_put(0xfa);
				put_event(EVENT_INIT);
				scan_enable = false;
				break;
			default:
				pc_put(0xfe);
				break;
			}
		}
	}
#endif
}




void quckey_setup()
{
	int c;
	unsigned short t;

	ps2kb0.io = &ps2kb0io;
	ps2kb1.io = &ps2kb1io;
	kb = &ps2kb1;

	PORTD = 0;
	DDRD = 0xaa;

//	// enable pin change interrupt
	EIMSK |= 0x21;
//	EIMSK |= 0x20;
	EICRA = 0x01;
	EICRB = 0x04;

	// enable interval timer
//	TCCR0B = 0x02; // 1/8 prescaling
//	TIMSK0 |= 1 << TOIE0;

	// start!

	kb->io->set_clock_0();
	kb->io->set_data_1();

	qinit(&event_queue_l);
	qinit(&event_queue_h);
	initialize_kb_input();
	initialize_kb_output();
//	initialize_pc_input();
//	initialize_pc_output();

//	pc_init_timer = 0;
	kb_reset_timer = 0;
//	pc_state = PC_INIT;
	kb_state = KB_INIT;
	key_decode_state = 0;
	shiftflags = 0;
	indicator = 0;
	change_typematic(0x23);
	real_keyboard_id = 0x83ab;
	fake_keyboard_id = 0x83ab;
	break_enable = true;
	scan_enable = true;

	kb->io->set_clock_1();

	put_event(EVENT_INIT);

}

void quckey_loop()
{
	handler();
	system_handler();
}





