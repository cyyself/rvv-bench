#ifndef NOLIBC_H
#define NOLIBC_H

#include <limits.h>
#include <stdint.h>
#include <stddef.h>
#include <float.h>


static void flush(void);

#if __STDC_HOSTED__
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define IFHOSTED(...) __VA_ARGS__

int nolibc_main(void);

static void
memwrite(void const *ptr, size_t len)
{
	fwrite(ptr, 1, len, stdout);
}

int main(void) {
	int x = nolibc_main();
	flush();
	exit(x);
}
#define main nolibc_main

#else

#define IFHOSTED(...)
#define NDEBUG

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0
static void
exit(int x)
{
	__asm volatile(
		"mv a0, %0\n"
		"li a7, 93\n"
		"ecall\n"
	:
	: "r"(x)
	: "a0", "a7");
}

volatile unsigned int *const ttyS0 = (unsigned int *const)0x91400000;

#define UART8250_RX  0
#define UART8250_TX  0
#define UART8250_LCR 3
#define UART8250_LSR 5

#define UART8250_LCR_DLAB 0x80

#define UART8250_LSR_DATA 0x01
#define UART8250_LSR_TXRDY 0x20

void uart_init() {
    // set DLAB = 0
    ttyS0[UART8250_LCR] &= ~UART8250_LCR_DLAB;
}

void uart_put_c(char c) {
    while ((ttyS0[UART8250_LSR] & UART8250_LSR_TXRDY) == 0);
    ttyS0[UART8250_TX] = c;
}

static void
memwrite(void const *ptr, size_t len)
{
	char const *c = ptr;
	for (size_t i = 0; i < len; ++i)
		uart_put_c(c[i]);
}

int main(void);

void c_start(void) {
	uart_init();
	memwrite("uart debug!\r\n", 13);
	int x = main();
	flush();
	exit(x);
}

#endif

/* utils */

static void
memswap(void *a, void *b, size_t size)
{
	unsigned char *A = (unsigned char*)a, *B = (unsigned char*)b;
	unsigned char *aEnd = A + size;
	while (A < aEnd) {
		unsigned char temp = *A;
		*A++ = *B;
		*B++ = temp;
	}
}

#if !__STDC_HOSTED__
static void *
memcpy(void *restrict dest, void const *restrict src, size_t n)
{
	unsigned char *d = dest;
	unsigned char const *s = src;
	while (n--) *d++ = *s++;
	return dest;
}

static void *
memset(void *dest, int c, size_t n)
{
	unsigned char *d = dest;
	while (n--) *d++ = c;
	return dest;
}

static size_t
strlen(char const *str)
{
	size_t len = 0;
	while (*str++) ++len;
	return len;
}

static void
qsort(void *base, size_t len, size_t size, int (*cmp)(const void *, const void *))
{
	if (len < 2) return;
	unsigned char *b = base;
	size_t pivot = len >> 1, l = 0, h = len - 1;
	for (; 1; ++l, --h) {
		while (cmp(b + l*size, b + pivot*size) < 0) ++l;
		while (cmp(b + h*size, b + pivot*size) > 0) --h;
		if (l >= h) break;
		memswap( b + l*size, b + h*size, size);
		pivot = pivot == l ? h : pivot == h ? l : pivot;
	}
	qsort(b, l, size, cmp);
	qsort(b + h * size + size, len - h - 1, size, cmp);
}
#endif


static uint64_t
u64sqrt(uint64_t y)
{
	uint64_t L = 0, R = y + 1;
	while (L != R - 1) {
		uint64_t M = (L + R) / 2;
		if (M * M <= y) L = M;
		else            R = M;
	}
	return L;
}

static uint64_t
hash64(uint64_t x)
{
	x ^= x >> 32;
	x *= 0xd6e8feb86659fd93U;
	x ^= x >> 32;
	x *= 0xd6e8feb86659fd93U;
	x ^= x >> 32;
	return x;
}


/* string conversions */

#define U64TOA_MAX 20

static size_t
u64toa(char *str, uint64_t val)
{
	char buf[U64TOA_MAX], *end = buf + sizeof buf, *it = end;
	do *--it = (val % 10) + '0'; while ((val /= 10));
	val = end - it;
	memcpy(str, it, val);
	return val;
}


#define FTOA_MAX (20+9+2)

static inline size_t
ftoa(char *str, double val, size_t prec)
{
	static const double pow10[] = {
		1, 10, 100, 1000, 10000, 100000, 1000000,
		10000000, 100000000, 1000000000
	};
	char buf[FTOA_MAX], *end = buf + sizeof buf, *it = end;
	prec = prec > 9 ? 9 : prec;

	if (val != val) return memcpy(buf, "NaN", 3), 3;
	if (val < 0) val = -val, *--it = '-';

	uint64_t ival = val, frac = (val-ival) * pow10[prec];
	do *--it = frac % 10 + '0'; while ((frac /= 10, --prec));
	*--it = '.';
	do *--it = ival % 10 + '0'; while ((ival /= 10));

	prec = end - it;
	memcpy(str, it, prec);
	return prec;
}

/* print API */

static char printBuffer[1024];
static char *printIt = printBuffer;
static char *const printEnd = printBuffer + sizeof printBuffer;

#define print_lit(s) print_raw(s, (sizeof s) - 1)

static void
flush(void)
{
	memwrite(printBuffer, printIt - printBuffer);
	printIt = printBuffer;
}

static void
print_raw(const char *s, size_t len)
{
	if ((uintptr_t)(printEnd - printIt) > len) {
		memcpy(printIt, s, len);
		printIt += len;
	} else {
		flush();
		memwrite(s, len);
	}
}

static void print_s(const char *s) { print_raw(s, strlen(s)); }

static void
print_u(uint64_t val)
{
	if (printEnd - printIt < U64TOA_MAX)
		flush();
	printIt += u64toa(printIt, val);
}

static void
print_fn(size_t prec, double val)
{
	if (printEnd - printIt < FTOA_MAX)
		flush();
	printIt += ftoa(printIt, val, prec);
}

static void print_f(double val) { print_fn(7, val); }


#define ARR_LEN(x) (sizeof x / sizeof *(x))



#define PRINT_AT_1(a,b,...) b
#define PRINT_SELECT(a,b,...) PRINT_AT_1(b,PRINT_FUNC,)
#define PRINT_LIT ,print_lit
#define PRINT_FUNC(f,...) print_##f(__VA_ARGS__)
#define PRINT_A(...) PRINT_SELECT(__VA_ARGS__,PRINT_LIT,)(__VA_ARGS__), (void)PRINT_B
#define PRINT_B(...) PRINT_SELECT(__VA_ARGS__,PRINT_LIT,)(__VA_ARGS__), (void)PRINT_A
#define print PRINT_A
static const char PRINT_A = 0, PRINT_B = 0;


#define PRINT_TEST_A(_) 1+PRINT_TEST_B
#define PRINT_TEST_B(_) 1+PRINT_TEST_A
#if PRINT_TEST_A(0)(0)(0) != 3 // if you get an error here, see below
// Your preprocessor implementation doesn't support sequence iteration,
// you can make the code compile by continuing the following pattern
# undef PRINT_B
# define PRINT_B(...)  PRINT_NEXT(__VA_ARGS__) PRINT_B1
# define PRINT_B1(...) PRINT_NEXT(__VA_ARGS__) PRINT_B2
#endif

#endif

