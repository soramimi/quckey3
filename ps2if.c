
#include <avr/io.h>
#include "typedef.h"

void pc_set_clock_0()
{
	PORTD |= 0x02;
}

void pc_set_clock_1()
{
	PORTD &= ~0x02;
}

void pc_set_data_0()
{
	PORTD |= 0x08;
}

void pc_set_data_1()
{
	PORTD &= ~0x08;
}

bool pc_get_clock()
{
	return (PIND >> 0) & 1;
}

bool pc_get_data()
{
	return (PIND >> 2) & 1;
}


void kb_set_clock_0()
{
	PORTD |= 0x20;
}

void kb_set_clock_1()
{
	PORTD &= ~0x20;
}

void kb_set_data_0()
{
	PORTD |= 0x80;
}

void kb_set_data_1()
{
	PORTD &= ~0x80;
}

bool kb_get_clock()
{
	return (PIND >> 4) & 1;
}

bool kb_get_data()
{
	return (PIND >> 6) & 1;
}

