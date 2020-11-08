
#include "waitloop.h"

void waitloop(unsigned int n)
{
	if (n) {
		do {
			asm("nop");
			asm("nop");
			asm("nop");
			asm("nop");
			asm("nop");
			asm("nop");
			asm("nop");
			asm("nop");
			asm("nop");
			asm("nop");
			asm("nop");
		} while (--n);
	}
}

void wait_100ms()
{
	int i;
	for (i = 0; i < 100; i++) {
		wait_1ms();
	}
}

void wait_1s()
{
	int i;
	for (i = 0; i < 1000; i++) {
		wait_1ms();
	}
}
