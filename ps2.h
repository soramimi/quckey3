
#ifndef PS2_H
#define PS2_H

#include <stdint.h>

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

#ifdef __cplusplus
extern "C" {
#endif

uint16_t ps2decode(uint8_t *state, uint8_t c);
uint8_t convert_scan_code_ibm_to_hid(uint8_t c);

#ifdef __cplusplus
}
#endif

int ps2encode(uint8_t number, uint8_t keyflag, void (*output)(uint8_t c, void *cookie), void *cookie);

uint16_t ps2decode001(uint8_t *state, uint8_t c);
uint16_t ps2decode002(uint8_t *state, uint8_t c);

#endif // PS2_H
