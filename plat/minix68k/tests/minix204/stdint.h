/*
 * <stdint.h> for tests/plat/lib/test.h with the Minix 2.0.4 headers, which
 * have none: without this, ACK's own one is found, and it declares size_t
 * as 32 bits where 2.0.4 has a 16 bit unsigned int.
 */
#ifndef _MINIX204_STDINT_H
#define _MINIX204_STDINT_H

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef long int32_t;
typedef unsigned long uint32_t;
typedef long intptr_t;
typedef unsigned long uintptr_t;
typedef long intmax_t;
typedef unsigned long uintmax_t;

#define INT8_MAX   127
#define INT16_MAX  32767
#define INT32_MAX  2147483647L
#define UINT8_MAX  255
#define UINT16_MAX 65535U
#define UINT32_MAX 4294967295UL
#define SIZE_MAX   65535U

#endif
