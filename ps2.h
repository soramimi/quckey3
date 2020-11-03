
#ifndef PS2_H
#define PS2_H

enum {
	KEYFLAG_SHIFT_L = 0x01,
	KEYFLAG_SHIFT_R = 0x02,
	KEYFLAG_SHIFT = 0x03,
	KEYFLAG_CTRL_L = 0x04,
	KEYFLAG_CTRL_R = 0x08,
	KEYFLAG_CTRL = 0x0c,
	KEYFLAG_ALT_L = 0x10,
	KEYFLAG_ALT_R = 0x20,
	KEYFLAG_ALT = 0x30,
	KEYFLAG_NUMLOCK = 0x40,
	KEYFLAG_MAKE = 0x80,
};

unsigned short ps2decode(unsigned char *state, unsigned char c);
int ps2encode(unsigned char number, unsigned char keyflag, void (*output)(unsigned char c, void *cookie), void *cookie);

unsigned short ps2decode001(unsigned char *state, unsigned char c);
unsigned short ps2decode002(unsigned char *state, unsigned char c);

#endif // PS2_H
