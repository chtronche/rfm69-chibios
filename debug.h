#ifndef _DEBUG_H_
#define _DEBUG_H_

#include "rfm69.h"

void print(const char *s);

void led(int n, unsigned mask);

void dumpReg(RFM69Driver *);

#endif
