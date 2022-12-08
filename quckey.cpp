
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

extern "C" void led(uint8_t f);
extern "C" void led_toggle();
void press_key(uint8_t key);
void release_key(uint8_t key);
void change_mouse(int dx, int dy, int dz, uint8_t buttons);
extern volatile uint8_t keyboard_leds;

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

PROGMEM static const uint16_t init_sequence_for_intellimouse_mouse[] = {
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
	CHECK(0xfa), SEND(100), 0,
	CHECK(0xfa), SEND(0xf3), 0, // set sample rate
	CHECK(0xfa), SEND(80), 0,
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

enum DeviceType {
	None,
	Keyboard,
	GenericMouse,
	IntelliMouse_00000a,
	IntelliMouse_10000a,
	IntelliMouse,
	TrackManMarbleFX,
};

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
	DeviceType device_type = None;
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

	bool init_request = false;
	bool kb_send_indicator_requested = false;
};

static inline bool is_keyboard(PS2Device const *k)
{
	return k->device_type == Keyboard;
}

static inline bool is_mouse(PS2Device const *k)
{
	return k->device_type != None && k->device_type != Keyboard;
}


PS2IO_A ps2_a_io;
PS2IO_B ps2_b_io;

PS2Device ps2_a;
PS2Device ps2_b;

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

inline void clear_event(PS2Device *dev)
{
	cli();
	qinit(&dev->event_queue_l);
	qinit(&dev->event_queue_h);
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
	intr(&ps2_a);
}

ISR(INT5_vect)
{
	intr(&ps2_b);
}

//

void ps2_io_handler(PS2Device *dev)
{
	// transmit to ps2 device
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
	PS2_WAIT_ACK	  = 0x0400,
	PS2_ED            = 0x0500,
};

enum {
	EVENT_KEYMAKE				= 0x0200,
	EVENT_KEYBREAK				= 0x0300,
};

void ps2_device_handler(PS2Device *dev, bool timer_event_flag)
{
	int c;

	if (timer_event_flag) {
		if (dev->reset_timer > 0) {
			dev->reset_timer--;
			if (dev->reset_timer == 0) {
				dev->init_request = true;
			}
		}

		if (dev->receive_timeout > 0) {
			dev->receive_timeout--;
			if (dev->receive_timeout == 0) {
				dev->mouse_received = 0;
			}
		}

		cli();
		if (dev->timeout > 0) {
			if (dev->timeout > 1) {
				dev->timeout--;
			} else {
				dev->output_bits = 0;
				dev->input_bits = 0;
				dev->io->set_data_1();
				dev->io->set_clock_1();
				dev->timeout = 0;
			}
		}
		sei();

		if (is_keyboard(dev)) {
			uint8_t leds = 0;
			if (keyboard_leds & 1) leds |= 2; // num lock
			if (keyboard_leds & 2) leds |= 4; // caps lock
			if (keyboard_leds & 4) leds |= 1; // scroll lock
			if (dev->indicator != leds) {
				dev->indicator = leds;
				dev->kb_send_indicator_requested = true;
				dev->reset_timer = 500;
			}
		}
	}

	if (dev->init_request) {
		dev->init_request = false;
		clear_event(dev);
		kb_put(dev, 0xff);
		dev->state = PS2_INIT;
		dev->reset_timer = 1000;
	} else {
		uint16_t event = get_event(dev);
		if (event > 0) {
			switch (event & 0xff00) {
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
			}
		} else {
			if (dev->kb_send_indicator_requested) {
				if (dev->state == PS2_NORMAL) {
					kb_put(dev, 0xed);
					dev->state = PS2_ED;
				}
			}
		}
	}

	c = kb_get(dev);
	if (c >= 0) {
		uint16_t t;

		dev->receive_timeout = 0;

		if (dev->init_sequece) {
			while (1) {
				int t = pgm_read_word(dev->init_sequece);
				if (t == 0) break;
				auto UseDefaultMouse = [&](){
					dev->init_sequece = init_sequence_for_default_mouse;
				};
				switch (t) {
				case EVENT(0):
					if (dev->mouse_buffer[0] == 0x00 && dev->mouse_buffer[1] == 0x00 && dev->mouse_buffer[2] == 0x0a) {
						dev->device_type = IntelliMouse_00000a;
						dev->init_sequece = init_sequence_for_intellimouse_mouse;
					} else if (dev->mouse_buffer[0] == 0x19 && dev->mouse_buffer[1] == 0x03 && dev->mouse_buffer[2] == 0xc8) {
						dev->device_type = TrackManMarbleFX;
						dev->init_sequece = init_sequence_for_logitech_mouse;
					} else {
						UseDefaultMouse();
					}
					break;
				case EVENT(1):
					if (dev->mouse_buffer[0] == 0x10 && dev->mouse_buffer[1] == 0x00 && dev->mouse_buffer[2] == 0x0a) {
						dev->device_type = IntelliMouse_10000a;
						dev->init_sequece++;
					} else {
						UseDefaultMouse();
					}
					break;
				case EVENT(2):
					if (dev->mouse_buffer[0] == 0x03) {
						dev->device_type = IntelliMouse;
						dev->init_sequece++;
					} else if (dev->mouse_buffer[0] == 0x04) {
						dev->device_type = IntelliMouse;
						dev->init_sequece++;
					} else {
						UseDefaultMouse();
					}
					break;
				default:
					switch (t & 0xff00) {
					case I_CHECK:
						if (c != (t & 0xff)) {
							dev->reset_timer = 1000;
							dev->init_sequece = nullptr;
							return;
						}
						break;
					case I_SEND:
						kb_put(dev, t & 0xff);
						break;
					case I_STORE:
						dev->mouse_buffer[t & 0xff] = c;
						break;
					}
					dev->init_sequece++;
					break;
				}
			}
			dev->init_sequece++;
			if (pgm_read_word(dev->init_sequece) == 0) {
				dev->init_sequece = nullptr;
				dev->state = PS2_WAIT_ACK;
			}
			c = -1;
		} else {
			switch (dev->state) {
			case PS2_INIT + 0:
				if (c == 0xfa) {
					c = -1;
				} else if (c == 0xaa) {
					kb_put(dev, 0xf2);
					dev->state++;
					c = -1;
				}
				break;
			case PS2_INIT + 1:
				if (c == 0xfa) {
					dev->state++;
					c = -1;
				}
				break;
			case PS2_INIT + 2:	// receive keyboard ID 1st byte
				if (c == 0x00) { // mouse
					dev->device_type = GenericMouse;
					dev->real_device_id = 0;
					dev->fake_device_id = 0;
					dev->mouse_buffer[0] = 0;
					dev->mouse_buffer[1] = 0;
					dev->mouse_buffer[2] = 0;
					kb_put(dev, 0xf6);
					dev->init_sequece = init_sequence_for_mouse;
				} else {
					dev->device_type = Keyboard;
					dev->_device_id = c;
					dev->state = PS2_KEYBOARD_INIT;
				}
				c = -1;
				break;
			case PS2_KEYBOARD_INIT:	// receive keyboard ID 2nd byte
				dev->_device_id |= c << 8;
				dev->real_device_id = dev->_device_id;
				kb_put(dev, 0xf5);
				dev->state++;
				c = -1;
				break;
			case PS2_KEYBOARD_INIT + 1:
				kb_put(dev, 0xf3); // set typematic rate/delay
				dev->state++;
				c = -1;
				break;
			case PS2_KEYBOARD_INIT + 2:
				if (c == 0xfa) {
					kb_put(dev, 0x00);
					dev->state++;
					c = -1;
				}
				break;
			case PS2_KEYBOARD_INIT + 3:
				kb_put(dev, 0xf4); // enable data reporting
				dev->state++;
				c = -1;
				break;
			case PS2_KEYBOARD_INIT + 4:
				if (c == 0xfa) {
					kb_put(dev, 0xf0); // select alternate scan codes
					dev->state++;
					c = -1;
				}
				break;
			case PS2_KEYBOARD_INIT + 5:
				if (c == 0xfa) {
					switch (dev->real_device_id) {
					case 0x92ab:	// is IBM5576-001 ?
					case 0x90ab:	// is IBM5576-002 ?
					case 0x91ab:	// is IBM5576-003 ?
						kb_put(dev, 0x82);
						break;
					default:
						kb_put(dev, 0x02);
						break;
					}
					c = -1;
				}
				dev->state = PS2_WAIT_ACK;
				dev->kb_send_indicator_requested = true;
				break;
			case PS2_ED:
				if (c == 0xfa) {
					kb_put(dev, dev->indicator);
					dev->state = PS2_WAIT_ACK;
					c = -1;
				} else {
					dev->state = PS2_NORMAL;
				}
				dev->kb_send_indicator_requested = false;
				break;
			case PS2_WAIT_ACK:
				dev->reset_timer = 0;
				dev->scan_enable = true;
				dev->state = PS2_NORMAL;
				c = -1;
				break;
			}
		}

		if (c >= 0) {
			if (c == 0xaa) {
				dev->reset_timer = 100;
			}
			if (is_mouse(dev)) {
				dev->receive_timeout = 10;
				uint8_t len = 3;
				if (dev->device_type == IntelliMouse) {
					len = 4;
				}
				if (dev->mouse_received < len) {
					dev->mouse_buffer[dev->mouse_received++] = c;
					if (dev->mouse_received >= len) {
						int wheel_delta = 0;
						uint8_t flags = dev->mouse_buffer[0];
						if (flags & 0xc0) {
							if (dev->device_type == TrackManMarbleFX && (flags & 0xc8) == 0xc8 && dev->mouse_buffer[1] == 0xd2) {
								dev->mouse_buttons = (dev->mouse_buttons & 0x07) | ((dev->mouse_buffer[2] >> 1) & 0x08);
							}
						} else {
							int dx = dev->mouse_buffer[1];
							int dy = dev->mouse_buffer[2];
							if (flags & 0x10) dx |= 0xff00; // make negative
							if (flags & 0x20) dy |= 0xff00;
							uint8_t buttons = 0;
							if (dev->device_type != TrackManMarbleFX) {
								dev->mouse_buttons = flags & 0x07;
								if (dev->mouse_buttons & 0x01) buttons |= 0x01; // left
								if (dev->mouse_buttons & 0x02) buttons |= 0x02; // right
								if (dev->mouse_buttons & 0x04) buttons |= 0x04; // middle
								if (dev->device_type == IntelliMouse) {
									dev->mouse_buttons |= (dev->mouse_buffer[3] >> 1) & 0x18;
									if (dev->mouse_buttons & 0x08) buttons |= 0x08; // prev
									if (dev->mouse_buttons & 0x10) buttons |= 0x10; // next
									wheel_delta = dev->mouse_buffer[3] & 0x0f;
									if (wheel_delta & 0x08) {
										wheel_delta |= 0xfff0; // make negative
									}
									wheel_delta = -wheel_delta;
								}
							}
							if (dev->device_type == TrackManMarbleFX) {
								dev->mouse_buttons = (dev->mouse_buttons & 0xf8) | (flags & 0x07);
								if (dev->mouse_buttons & 0x01) buttons |= 0x01; // left
								if (dev->mouse_buttons & 0x02) buttons |= 0x02; // right
								if (dev->mouse_buttons & 0x08) buttons |= 0x04; // middle
								bool b4th = dev->mouse_buttons & 0x04;
								if (b4th) {
									if (abs(dx) > abs(dy)) {
										dev->wheel_movement = 0;
										if (dx < 0) {
											buttons &= ~0x10;
											if (dev->side_button_movement > 0) {
												dev->side_button_movement = dx;
												buttons &= ~0x08;
											} else {
												dev->side_button_movement += dx;
												if (dev->side_button_movement < -100) {
													dev->side_button_movement = -100;
													buttons |= 0x08; // prev
												}
											}
										} else if (dx > 0) {
											buttons &= ~0x08;
											if (dev->side_button_movement < 0) {
												dev->side_button_movement = dx;
												buttons &= ~0x10;
											} else {
												dev->side_button_movement += dx;
												if (dev->side_button_movement > 100) {
													dev->side_button_movement = 100;
													buttons |= 0x10; // next
												}
											}
										} else {
											buttons &= ~0x18;
											dev->side_button_movement = 0;
										}
									} else {
										buttons &= ~0x18;
										dev->side_button_movement = 0;
										dev->wheel_movement -= dy;
										while (dev->wheel_movement >= 10) {
											dev->wheel_movement -= 10;
											wheel_delta--;
										}
										while (dev->wheel_movement <= -10) {
											dev->wheel_movement += 10;
											wheel_delta++;
										}
									}
									dx = 0;
									dy = 0;
								} else {
									buttons &= ~0x18;
									dev->side_button_movement = 0;
									dev->wheel_movement = 0;
								}
							}

							if (1) { // acceleration
								long xx = (long)dx * dx;
								long yy = (long)dy * dy;
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
						dev->mouse_received = 0;
						dev->receive_timeout = 0;
						dev->reset_timer = 0;
					}
				}
			} else if (is_keyboard(dev)) {
				switch (c) {
				case 0xaa:
					kb_put(dev, 0xf2);
					dev->state = PS2_INIT + 1;
					break;
				case 0xfa:
				case 0xee:
				case 0xfc:
				case 0xfe:
				case 0xff:
					dev->decode_state = 0;
					dev->state = PS2_NORMAL;
					break;
				default:
					switch (dev->real_device_id) {
					case 0x92ab:	// is IBM5576-001 ?
						t = ps2decode001(&dev->decode_state, c);
						break;
					case 0x90ab:	// is IBM5576-002 ?
					case 0x91ab:	// is IBM5576-003 ?
						t = ps2decode002(&dev->decode_state, c);
						break;
					default:
						t = ps2decode(&dev->decode_state, c);
						break;
					}
					if (t && dev->scan_enable) {
						uint16_t event;
						if (t & 0x8000) {
							event = (t & 0xff) | EVENT_KEYBREAK;
							if (dev->break_enable) {
								put_event(dev, event);
							}
						} else {
							event = (t & 0xff) | EVENT_KEYMAKE;
							put_event(dev, event);
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

void setup_device(PS2Device *dev)
{
	dev->io->set_clock_0();
	dev->io->set_data_1();

	init_device(dev);

	dev->receive_timeout = 0;

	dev->decode_state = 0;
	dev->indicator = 0;
	dev->real_device_id = 0x83ab;
	dev->fake_device_id = 0x83ab;
	dev->break_enable = true;
	dev->scan_enable = false;

	dev->mouse_received = 0;

	dev->io->set_clock_1();

	dev->init_request = true;
}

void ps2_setup()
{
	ps2_a.io = &ps2_a_io;
	ps2_b.io = &ps2_b_io;

	PORTD = 0;
	DDRD = 0xaa;

	EIMSK |= 0x21;
	EICRA = 0x01;
	EICRB = 0x04;

	setup_device(&ps2_a);
	setup_device(&ps2_b);
}

void ps2_loop(bool timerevent)
{
	ps2_device_handler(&ps2_a, timerevent);
	ps2_device_handler(&ps2_b, timerevent);

	ps2_io_handler(&ps2_a);
	ps2_io_handler(&ps2_b);
}





