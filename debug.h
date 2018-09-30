#ifndef _DEBUG_H_
#define _DEBUG_H_

#include "rfm69.h"

void print(const char *s);

void init_led(void);
void led(int n, unsigned mask);
void chenillard(void);

void dumpReg(RFM69Driver *);

#endif
