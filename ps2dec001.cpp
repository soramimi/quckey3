
#include <avr/pgmspace.h>
#include <stdint.h>
#include "ps2.h"

static uint8_t lookup_default(uint8_t c)
{
	PROGMEM static const uint8_t decodetable[] = {
		//0----1----2----3----4----5----6----7----8----9----A----B----C----D----E----F
		  0, 120,   0, 116, 114, 112, 113, 123,   0, 121, 119, 117, 115,  16,  27,   0,
		  0,  58,  44,  30,   0,  17,   2, 130, 135,   0,  46,  32,  31,  18,   3,   0,
		  0,  48,  47,  33,  19,   5,   4,   0,   0,  61,  49,  34,  21,  20,   6,   0,
		  0,  51,  50,  36,  35,  22,   7, 134,   0,   0,  52,  37,  23,   8,   9,   0,
		 86,  53,  38,  24,  25,  11,  10,   0, 110,  54,  55,  39,  40,  26,  12,   0,
		  0,  56,  41,   0,  28,  13,   0,   0,  60,  57,  43,  42,  14,  14,   0,   0,
		  0,  45,   0,   0, 132,   0,  15, 131,   0,  93,  14,  92,  91,   0,   0,   0,
		 99, 104,  98,  97, 102,  96,   1,  90, 122, 106, 103, 105,  90, 101, 125,   0,
		  0,   0,   0, 118, 124
	};
	return c < 0x85 ? pgm_read_byte(decodetable + c) : 0;
}

uint16_t ps2decode001(uint8_t *state, uint8_t c)
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
			switch (c) {
			case 0x11:	code = 133;		break;
			case 0x14:	code = 64;		break;
			case 0x41:	code = 100;		break;
			case 0x4a:	code = 95;		break;
			case 0x5a:	code = 108;		break;
			case 0x6b:	code = 79;		break;
			case 0x69:	code = 80;		break;
			case 0x70:	code = 76;		break;
			case 0x71:	code = 81;		break;
			case 0x72:	code = 84;		break;
			case 0x74:	code = 89;		break;
			case 0x75:	code = 83;		break;
			case 0x7a:	code = 85;		break;
			case 0x7d:	code = 75;		break;
			case 0x7c:	code = 124;		break;
			}
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

