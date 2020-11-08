
#include <avr/pgmspace.h>
#include <stdint.h>
#include "ps2.h"

static uint8_t lookup_default(uint8_t c)
{
	PROGMEM static const uint8_t decodetable[] = {
		//0----1----2----3----4----5----6----7----8----9----A----B----C----D----E----F
		  0, 120,   0, 116, 114, 112, 113, 123,   0, 121, 119, 117, 115,  16,   1,   0,
		  0,  60,  44, 133,  58,  17,   2,   0,   0,   0,  46,  32,  31,  18,   3,   0,
		  0,  48,  47,  33,  19,   5,   4,   0,   0,  61,  49,  34,  21,  20,   6,   0,
		  0,  51,  50,  36,  35,  22,   7,   0,   0,   0,  52,  37,  23,   8,   9,   0,
		  0,  53,  38,  24,  25,  11,  10,   0,   0,  54,  55,  39,  40,  26,  12,   0,
		  0,  56,  41,   0,  27,  13,   0,   0,  30,  57,  43,  28,   0,  42,   0,   0,
		  0,  45,   0,   0, 132,   0,  15, 131,   0,  93,  14,  92,  91,   0,   0,   0,
		 99, 104,  98,  97, 102,  96, 110,  90, 122, 106, 103, 105, 100, 101, 125,   0,
		  0,   0,   0, 118, 124
	};
	return c < 0x85 ? pgm_read_byte(decodetable + c) : 0;
}

static uint8_t looup_prefix_0E(uint8_t c)
{
	PROGMEM static const struct {
		uint8_t from;
		uint8_t to;
	} table[] = {
		0x11, 62,
		0x14, 64,
		0x1f, 130, // left win
		0x27, 134, // right win
		0x2f, 135, // application
		0x4a, 95,
		0x5a, 108,
		0x69, 81,
		0x6b, 79,
		0x6c, 80,
		0x70, 75,
		0x71, 76,
		0x72, 84,
		0x74, 89,
		0x75, 83,
		0x7a, 86,
		0x7c, 124,
		0x7e, 126,
		0x7d, 85,
	};
	const int N = sizeof(table) / sizeof(*table);
	for (int i = 0; i < N; i++) {
		if (c == pgm_read_byte(&table[i].from)) {
			return pgm_read_byte(&table[i].to);
		}
	}
	return 0;
}


uint16_t ps2decode(uint8_t *state, uint8_t c)
{
	enum {
		_PREFIX_E0 = 0x01,
		_PREFIX_E1 = 0x02,
		_IGNORE_90 = 0x40,
		_KEYBREAK = 0x80,
	};

	uint16_t code;

	code = 0;
	if (c == 0xf0) {
		*state |= _KEYBREAK;
	} else if (c == 0xe0) {
		*state = _PREFIX_E0;
	} else if (c == 0xe1) {
		*state = _PREFIX_E1;
	} else if (c < 0xe0) {
		switch (*state & 0x03) {
		case 0:
			code = lookup_default(c);
			break;
		case _PREFIX_E0:
			code = looup_prefix_0E(c);
			break;
		case _PREFIX_E1:
			switch (c) {
			case 0x14:	code = 126;		break;
			}
			break;
		}

		{
			uint8_t newstate = 0;
			if (code > 0) {
				if (code == 90 && (*state & _IGNORE_90)) {
					code = 0;
				} else {
					if (code == 126) newstate |= _IGNORE_90;
					if (*state & _KEYBREAK) code |= 0x8000;
				}
			}
			*state = newstate;
		}
	}

	return code;
}

uint8_t convert_scan_code_ibm_to_hid(uint8_t c)
{
	PROGMEM static const uint8_t table[] = {
		0x00,0x35,0x1e,0x1f,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x2d,0x2e,0x89,0x2a,
		0x2b,0x14,0x1a,0x08,0x15,0x17,0x1c,0x18,0x0c,0x12,0x13,0x2f,0x30,0x31,0x39,0x04,
		0x16,0x07,0x09,0x0a,0x0b,0x0d,0x0e,0x0f,0x33,0x34,0x32,0x28,0xe1,0x00,0x1d,0x1b,
		0x06,0x19,0x05,0x11,0x10,0x36,0x37,0x38,0x87,0xe5,0xe0,0x00,0xe2,0x2c,0xe6,0x00,
		0xe4,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x49,0x4c,0x00,0x00,0x50,
		0x4a,0x4d,0x00,0x52,0x51,0x4b,0x4e,0x00,0x00,0x4f,0x53,0x5f,0x5c,0x59,0x00,0x54,
		0x60,0x5d,0x5a,0x62,0x55,0x61,0x5e,0x5b,0x63,0x56,0x57,0x00,0x58,0x00,0x29,0x00,
		0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0xe3,
		0xe7,0x65,0x00,0x8b,0x8a,0x88,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	};
	return c < 0x90 ? pgm_read_byte(table + c) : 0;
}




