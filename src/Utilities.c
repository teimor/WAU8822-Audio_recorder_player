
#include <stdio.h>
#include <stdint.h>
#include <NUC1xx.h>
#include "ff.h"

#pragma diag_suppress 177
static void RoughDelay(uint32_t t)
{
    volatile int32_t delay;

    delay = t;

    while(delay-- >= 0);
}

void Delay(uint32_t delayCnt)
{
    while(delayCnt--)
    {
        __NOP();
        __NOP();
    }
}

void HexConv(int num, int8_t bytes[4])
{
	bytes[0] = (num >> 24) & 0xFF;
	bytes[1] = (num >> 16) & 0xFF;
	bytes[2] = (num >> 8) & 0xFF;
	bytes[3] =	num & 0xFF;
}
