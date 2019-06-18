#include <stdio.h>
#include <stdint.h>
#include <NUC1xx.h>
#include "ff.h"

extern void RoughDelay(uint32_t t);
	
extern void Delay(uint32_t delayCnt);

extern void HexConv(int num, int8_t bytes[4]);
