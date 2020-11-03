
#ifndef __QUEUE16_H
#define __QUEUE16_H

struct Queue16 {
	unsigned char pos;
	unsigned char len;
	unsigned char buf[16];
};

extern void q16init(struct Queue16 *q);
extern int q16peek(struct Queue16 *q);
extern int q16get(struct Queue16 *q);
extern void q16put(struct Queue16 *q, unsigned char c);
extern void q16unget(struct Queue16 *q, unsigned char c);

#endif

