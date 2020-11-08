
#include <avr/pgmspace.h>
#include <stdint.h>
#include "ps2.h"


/*PROGMEM*/ static const uint8_t decodetable[] = {
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

static int tabledecode(unsigned char c)
{
	return c >= 0 && c < 0x85 ? decodetable[c] : 0;
}

unsigned short ps2decode(unsigned char *state, unsigned char c)
{
	enum {
		_PREFIX_E0 = 0x01,
		_PREFIX_E1 = 0x02,
		_IGNORE_90 = 0x40,
		_KEYBREAK = 0x80,
	};

	unsigned short code;

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
			code = tabledecode(c);
			break;
		case _PREFIX_E0:
			switch (c) {
			case 0x11:	code = 62;		break;
			case 0x14:	code = 64;		break;
			case 0x1f:	code = 130;		break; // left win
			case 0x27:	code = 134;		break; // right win
			case 0x2f:	code = 135;		break; // application
			case 0x4a:	code = 95;		break;
			case 0x5a:	code = 108;		break;
			case 0x69:	code = 81;		break;
			case 0x6b:	code = 79;		break;
			case 0x6c:	code = 80;		break;
			case 0x70:	code = 75;		break;
			case 0x71:	code = 76;		break;
			case 0x72:	code = 84;		break;
			case 0x74:	code = 89;		break;
			case 0x75:	code = 83;		break;
			case 0x7a:	code = 86;		break;
			case 0x7c:	code = 124;		break;
			case 0x7e:	code = 126;		break;
			case 0x7d:	code = 85;		break;
			}
			break;
		case _PREFIX_E1:
			switch (c) {
			case 0x14:	code = 126;		break;
			}
			break;
		}

		{
			unsigned char newstate = 0;
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

