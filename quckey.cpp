
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#include "queue16.h"
#include "ps2.h"
#include "ps2if.h"
#include "waitloop.h"
#include <stdlib.h>
#include "lcd.h"

typedef struct Queue16 Queue;
#define qinit(Q) q16init(Q)
#define qpeek(Q) q16peek(Q)
#define qget(Q) q16get(Q)
#define qput(Q,C) q16put(Q,C)
#define qunget(Q,C) q16unget(Q,C)

extern void led(uint8_t f);
extern void press_key(uint8_t key);
extern void release_key(uint8_t key);
void change_mouse(int dx, int dy, int dz, uint8_t buttons);
extern volatile uint8_t keyboard_leds;

uint8_t interval_1ms_flag = 0; // 1ms interval event

enum MouseDeviceType {
	Keyboard,
	GenericMouse,
	IntelliMouseExplorer_00000a,
	IntelliMouseExplorer_10000a,
	IntelliMouseExplorer,
	TrackManMarbleFX,
};

enum {
	I_CHECK = 0x100,
	I_SEND = 0x200,
	I_STORE = 0x300,
	I_EVENT = 0x1000,
};

#define CHECK(X) (I_CHECK | (X))
#define SEND(X) (I_SEND | (X))
#define STORE(X) (I_STORE | (X))
#define EVENT(X) (I_EVENT | (X))

PROGMEM static const uint16_t init_sequence_for_mouse[] = {
	CHECK(0xfa), SEND(0xf2), 0, // get device id
	CHECK(0xfa), 0,
	SEND(0xf3), 0,              // set sample rate
	CHECK(0xfa), SEND(10), 0,
	CHECK(0xfa), SEND(0xf2), 0, // get device id
	CHECK(0xfa), 0,
	SEND(0xe8), 0,              // set resolution
	CHECK(0xfa), SEND(0), 0,
	CHECK(0xfa), SEND(0xe6), 0, // set scaling 1:1
	CHECK(0xfa), SEND(0xe6), 0, // set scaling 1:1
	CHECK(0xfa), SEND(0xe6), 0, // set scaling 1:1
	CHECK(0xfa), SEND(0xe9), 0, // status request
	CHECK(0xfa), 0,
	STORE(0), 0,
	STORE(1), 0,
	STORE(2), EVENT(0),
	0,
};

PROGMEM static const uint16_t init_sequence_for_default_mouse[] = {
	SEND(0xe8), 0,              // set resolution
	CHECK(0xfa), SEND(3), 0,
	CHECK(0xfa), SEND(0xf3), 0, // set sample rate
	CHECK(0xfa), SEND(200), 0,
	CHECK(0xfa), SEND(0xf3), 0, // set sample rate
	CHECK(0xfa), SEND(100), 0,
	CHECK(0xfa), SEND(0xf3), 0, // set sample rate
	CHECK(0xfa), SEND(80), 0,
	CHECK(0xfa), SEND(0xf2), 0, // get device id
	CHECK(0xfa), 0,
	SEND(0xf3), 0,              // set sample rate
	CHECK(0xfa), SEND(60), 0,
	CHECK(0xfa), SEND(0xe8), 0, // set resolution
	CHECK(0xfa), SEND(3), 0,
	SEND(0xf4), 0,              // enable data reporting
	0
};

PROGMEM static const uint16_t init_sequence_for_intellimouseexplorer_mouse[] = {
	SEND(0xe8), 0,              // set resolution
	CHECK(0xfa), SEND(0), 0,
	CHECK(0xfa), SEND(0xe7), 0, // set scaling 2:1
	CHECK(0xfa), SEND(0xe7), 0, // set scaling 2:1
	CHECK(0xfa), SEND(0xe7), 0, // set scaling 2:1
	CHECK(0xfa), SEND(0xe9), 0, // status request
	CHECK(0xfa), 0,
	STORE(0), 0,
	STORE(1), 0,
	STORE(2), EVENT(1),
	SEND(0xe8), 0,              // set resolution
	CHECK(0xfa), SEND(3), 0,
	CHECK(0xfa), SEND(0xf3), 0, // set sample rate
	CHECK(0xfa), SEND(200), 0,
	CHECK(0xfa), SEND(0xf3), 0, // set sample rate
	CHECK(0xfa), SEND(200), 0,
	CHECK(0xfa), SEND(0xf3), 0, // set sample rate
	CHECK(0xfa), SEND(80), 0,
	CHECK(0xfa), SEND(0xf2), 0, // get device id
	CHECK(0xfa), 0,
	STORE(0), EVENT(2),
	SEND(0xe6), 0,              // set scaling 1:1,
	CHECK(0xfa), SEND(0xf3), 0, // set sample rate
	CHECK(0xfa), SEND(60), 0,
	CHECK(0xfa), SEND(0xe8), 0, // set resolution
	CHECK(0xfa), SEND(3), 0,
	SEND(0xf4), 0,              // enable data reporting
	0
};

PROGMEM static const uint16_t init_sequence_for_logitech_mouse[] = {
	SEND(0xe6), 0,              // set defaults
	CHECK(0xfa), SEND(0xe8), 0, // set resolution
	CHECK(0xfa), SEND(0), 0,
	CHECK(0xfa), SEND(0xe8), 0, // set resolution
	CHECK(0xfa), SEND(3), 0,
	CHECK(0xfa), SEND(0xe8), 0, // set resolution
	CHECK(0xfa), SEND(2), 0,
	CHECK(0xfa), SEND(0xe8), 0, // set resolution
	CHECK(0xfa), SEND(1), 0,
	CHECK(0xfa), SEND(0xe6), 0, // set scaling 1:1
	CHECK(0xfa), SEND(0xe8), 0, // set resolution
	CHECK(0xfa), SEND(3), 0,
	CHECK(0xfa), SEND(0xe8), 0, // set resolution
	CHECK(0xfa), SEND(1), 0,
	CHECK(0xfa), SEND(0xe8), 0, // set resolution
	CHECK(0xfa), SEND(2), 0,
	CHECK(0xfa), SEND(0xe8), 0, // set resolution
	CHECK(0xfa), SEND(3), 0,
	CHECK(0xfa), SEND(0xe8), 0, // set resolution
	CHECK(0xfa), SEND(3), 0,
	CHECK(0xfa), SEND(0xf2), 0, // get device id
	CHECK(0xfa), 0,
	SEND(0xf3), 0,              // set sample rate
	CHECK(0xfa), SEND(60), 0,
	CHECK(0xfa), SEND(0xe8), 0, // set resolution
	CHECK(0xfa), SEND(3), 0,
	SEND(0xf4), 0,              // enable data reporting
	0
};

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
	MouseDeviceType mouse_device_type = Keyboard;
	uint16_t const *init_sequece = nullptr;

	uint16_t receive_timeout;
	uint8_t mouse_received;
	int wheel_movement = 0;
	int side_button_movement = 0;
	uint8_t mouse_buffer[4];
	uint8_t mouse_buttons = 0;

	uint16_t repeat_timeout = 0;
	uint8_t decode_state = 0;
	uint8_t indicator = 0;

	uint16_t _device_id;
	bool break_enable;
	bool scan_enable;
};

PS2Device ps2k0;
PS2Device ps2k1;

uint8_t countbits(uint16_t c)
{
	uint8_t i;
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
						uint8_t c = (dev->input_bits >> 2) & 0xff;
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
	// transmit to keyboard
	int c = qget(&dev->output_queue);
	if (c >= 0) {
		if (!kb_next_output(dev, c)) {
			qunget(&dev->output_queue, c);
		}
	}
}

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
		if (k->receive_timeout > 0) {
			k->receive_timeout--;
			if (k->receive_timeout == 0) {
				k->mouse_received = 0;
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

		if (k->mouse_device_type == Keyboard) {
			uint8_t leds = 0;
			if (keyboard_leds & 1) leds |= 2; // num lock
			if (keyboard_leds & 2) leds |= 4; // caps lock
			if (keyboard_leds & 4) leds |= 1; // scroll lock
			if (k->indicator != leds) {
				k->indicator = leds;
				put_event(k, EVENT_SEND_KB_INDICATOR);
			}
		}
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
			break;
		case EVENT_KEYBREAK:
			c = event & 0xff;
			c = convert_scan_code_ibm_to_hid(c);
			release_key(c);
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

		k->receive_timeout = 0;

		if (k->init_sequece) {
			while (1) {
				int t = pgm_read_word(k->init_sequece);
				if (t == 0) break;
				auto UseDefaultMouse = [&](){
					k->init_sequece = init_sequence_for_default_mouse;
				};
				switch (t) {
				case EVENT(0):
					if (k->mouse_buffer[0] == 0x00 && k->mouse_buffer[1] == 0x00 && k->mouse_buffer[2] == 0x0a) {
						k->mouse_device_type = IntelliMouseExplorer_00000a;
						k->init_sequece = init_sequence_for_intellimouseexplorer_mouse;
					} else if (k->mouse_buffer[0] == 0x19 && k->mouse_buffer[1] == 0x03 && k->mouse_buffer[2] == 0xc8) {
						k->mouse_device_type = TrackManMarbleFX;
						k->init_sequece = init_sequence_for_logitech_mouse;
					} else {
						UseDefaultMouse();
					}
					break;
				case EVENT(1):
					if (k->mouse_buffer[0] == 0x10 && k->mouse_buffer[1] == 0x00 && k->mouse_buffer[2] == 0x0a) {
						k->mouse_device_type = IntelliMouseExplorer_10000a;
						k->init_sequece++;
					} else {
						UseDefaultMouse();
					}
					break;
				case EVENT(2):
					if (k->mouse_buffer[0] == 0x04) {
						k->mouse_device_type = IntelliMouseExplorer;
						k->init_sequece++;
					} else {
						UseDefaultMouse();
					}
					break;
				default:
					switch (t & 0xff00) {
					case I_CHECK:
						if (c != (t & 0xff)) {
							k->reset_timer = 1000;
							k->init_sequece = nullptr;
							return;
						}
						break;
					case I_SEND:
						kb_put(k, t & 0xff);
						break;
					case I_STORE:
						k->mouse_buffer[t & 0xff] = c;
						break;
					}
					k->init_sequece++;
					break;
				}
			}
			k->init_sequece++;
			if (pgm_read_word(k->init_sequece) == 0) {
				k->init_sequece = nullptr;
				k->state = PS2_WAIT;
			}
			c = -1;
		} else {
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
					k->mouse_device_type = GenericMouse;
					k->mouse_buffer[0] = 0;
					k->mouse_buffer[1] = 0;
					k->mouse_buffer[2] = 0;
					kb_put(k, 0xf6);
					k->init_sequece = init_sequence_for_mouse;
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
					kb_put(k, 0xf4); // enable data reporting
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
				k->reset_timer = 0;
				k->scan_enable = true;
				k->state = PS2_NORMAL;
				c = -1;
				break;
			}
		}

		if (c >= 0) {
			if (c == 0xaa) {
				k->reset_timer = 100;
			}
			if (k->real_device_id == 0) { // mouse
				k->receive_timeout = 10;
				uint8_t len = 3;
				if (k->mouse_device_type == IntelliMouseExplorer) {
					len = 4;
				}
				if (k->mouse_received < len) {
					k->mouse_buffer[k->mouse_received++] = c;
					if (k->mouse_received >= len) {
						int wheel_delta = 0;
						uint8_t flags = k->mouse_buffer[0];
						if (flags & 0xc0) {
							if (k->mouse_device_type == TrackManMarbleFX && (flags & 0xc8) == 0xc8 && k->mouse_buffer[1] == 0xd2) {
								k->mouse_buttons = (k->mouse_buttons & 0x07) | ((k->mouse_buffer[2] >> 1) & 0x08);
							}
						} else {
							int dx = k->mouse_buffer[1];
							int dy = k->mouse_buffer[2];
							if (flags & 0x10) dx |= -1 << 8;
							if (flags & 0x20) dy |= -1 << 8;
							uint8_t buttons = 0;
							if (k->mouse_device_type != TrackManMarbleFX) {
								k->mouse_buttons = flags & 0x07;
								if (k->mouse_buttons & 0x01) buttons |= 0x01; // left
								if (k->mouse_buttons & 0x02) buttons |= 0x02; // right
								if (k->mouse_buttons & 0x04) buttons |= 0x04; // middle
								if (k->mouse_device_type == IntelliMouseExplorer) {
									k->mouse_buttons |= (k->mouse_buffer[3] >> 1) & 0x18;
									if (k->mouse_buttons & 0x08) buttons |= 0x08; // middle
									if (k->mouse_buttons & 0x10) buttons |= 0x10; // middle
									wheel_delta = k->mouse_buffer[3] & 0x0f;
									if (wheel_delta & 0x08) {
										wheel_delta |= -1 << 4;
									}
									wheel_delta = -wheel_delta;
								}
							}
							if (k->mouse_device_type == TrackManMarbleFX) {
								k->mouse_buttons = (k->mouse_buttons & 0xf8) | (flags & 0x07);
								if (k->mouse_buttons & 0x01) buttons |= 0x01; // left
								if (k->mouse_buttons & 0x02) buttons |= 0x02; // right
								if (k->mouse_buttons & 0x08) buttons |= 0x04; // middle
								bool b4th = k->mouse_buttons & 0x04;
								if (b4th) {
									if (abs(dx) > abs(dy)) {
										k->wheel_movement = 0;
										if (dx < 0) {
											buttons &= ~0x10;
											if (k->side_button_movement > 0) {
												k->side_button_movement = dx;
												buttons &= ~0x08;
											} else {
												k->side_button_movement += dx;
												if (k->side_button_movement < -100) {
													k->side_button_movement = -100;
													buttons |= 0x08;
												}
											}
										} else if (dx > 0) {
											buttons &= ~0x08;
											if (k->side_button_movement < 0) {
												k->side_button_movement = dx;
												buttons &= ~0x10;
											} else {
												k->side_button_movement += dx;
												if (k->side_button_movement > 100) {
													k->side_button_movement = 100;
													buttons |= 0x10;
												}
											}
										} else {
											buttons &= ~0x18;
											k->side_button_movement = 0;
										}
									} else {
										buttons &= ~0x18;
										k->side_button_movement = 0;
										k->wheel_movement -= dy;
										while (k->wheel_movement >= 10) {
											k->wheel_movement -= 10;
											wheel_delta--;
										}
										while (k->wheel_movement <= -10) {
											k->wheel_movement += 10;
											wheel_delta++;
										}
									}
									dx = 0;
									dy = 0;
								} else {
									buttons &= ~0x18;
									k->side_button_movement = 0;
									k->wheel_movement = 0;
								}
							}

							if (1) { // acceleration
								int xx = dx * dx;
								int yy = dy * dy;
								if (xx + yy >= 9) {
									if (xx + yy < 300) {
										if (xx > yy) {
											if (dx < 0) {
												dx++;
											} else {
												dx--;
											}
										}
										if (xx < yy) {
											if (dy < 0) {
												dy++;
											} else {
												dy--;
											}
										}
									}
									dx *= 2;
									dy *= 2;
								}
							}

							change_mouse(dx, -dy, wheel_delta, buttons);
						}
						k->mouse_received = 0;
						k->receive_timeout = 0;
						k->reset_timer = 0;
					}
				}
			} else { // keyboard
				switch (c) {
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
						} else {
							event = (t & 0xff) | EVENT_KEYMAKE;
							put_event(k, event);
						}
					}
					break;
				}
			}
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

	k->receive_timeout = 0;

	k->decode_state = 0;
	k->indicator = 0;
	k->real_device_id = 0x83ab;
	k->fake_device_id = 0x83ab;
	k->break_enable = true;
	k->scan_enable = false;

	k->mouse_received = 0;

	k->io->set_clock_1();

	put_event(k, EVENT_INIT);
}

void keyboard_setup()
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

void keyboard_loop()
{
	cli();
	bool timerevent = interval_1ms_flag;
	interval_1ms_flag = false;
	sei();

	ps2_device_handler(&ps2k0, timerevent);
	ps2_device_handler(&ps2k1, timerevent);

	ps2_io_handler(&ps2k0);
	ps2_io_handler(&ps2k1);
}





