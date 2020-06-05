/*
 * Copyright (c) 2010, 2013-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Low-level debug output
 *
 * Low-level debugging output. Platform installs a character output routine at
 * init time. If no routine is installed, a nop routine is called.
 */

#include <kernel.h>
#include <misc/printk.h>
#include <stdarg.h>
#include <toolchain.h>
#include <linker/sections.h>
#include <syscall_handler.h>
#include <logging/log.h>

typedef int (*out_func_t)(int c, void *ctx);

enum pad_type {
	PAD_NONE,
	PAD_ZERO_BEFORE,
	PAD_SPACE_BEFORE,
	PAD_SPACE_AFTER,
};

static void _printk_dec_ulong(out_func_t out, void *ctx,
			      const unsigned long num, enum pad_type padding,
			      int min_width);
static void _printk_hex_ulong(out_func_t out, void *ctx,
			      const unsigned long long num, enum pad_type padding,
			      int min_width);

/**
 * @brief Default character output routine that does nothing
 * @param c Character to swallow
 *
 * Note this is defined as a weak symbol, allowing architecture code
 * to override it where possible to enable very early logging.
 *
 * @return 0
 */
 __attribute__((weak)) int z_arch_printk_char_out(int c)
{
	ARG_UNUSED(c);

	/* do nothing */
	return 0;
}

int (*_char_out)(int) = z_arch_printk_char_out;

/**
 * @brief Install the character output routine for printk
 *
 * To be called by the platform's console driver at init time. Installs a
 * routine that outputs one ASCII character at a time.
 * @param fn putc routine to install
 *
 * @return N/A
 */
void __printk_hook_install(int (*fn)(int))
{
	_char_out = fn;
}

/**
 * @brief Get the current character output routine for printk
 *
 * To be called by any console driver that would like to save
 * current hook - if any - for later re-installation.
 *
 * @return a function pointer or NULL if no hook is set
 */
void *__printk_get_hook(void)
{
	return _char_out;
}

static void print_err(out_func_t out, void *ctx)
{
	out('E', ctx);
	out('R', ctx);
	out('R', ctx);
}

/**
 * @brief Printk internals
 *
 * See printk() for description.
 * @param fmt Format string
 * @param ap Variable parameters
 *
 * @return N/A
 */
void z_vprintk(out_func_t out, void *ctx, const char *fmt, va_list ap)
{
	int might_format = 0; /* 1 if encountered a '%' */
	enum pad_type padding = PAD_NONE;
	int min_width = -1;
	char length_mod = 0;

	/* fmt has already been adjusted if needed */

	while (*fmt) {
		if (!might_format) {
			if (*fmt != '%') {
				out((int)*fmt, ctx);
			} else {
				might_format = 1;
				min_width = -1;
				padding = PAD_NONE;
				length_mod = 0;
			}
		} else {
			switch (*fmt) {
			case '-':
				padding = PAD_SPACE_AFTER;
				goto still_might_format;
			case '0':
				if (min_width < 0 && padding == PAD_NONE) {
					padding = PAD_ZERO_BEFORE;
					goto still_might_format;
				}
				/* Fall through */
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
				/* Fall through */
			case '9':
				if (min_width < 0) {
					min_width = *fmt - '0';
				} else {
					min_width = 10 * min_width + *fmt - '0';
				}

				if (padding == PAD_NONE) {
					padding = PAD_SPACE_BEFORE;
				}
				goto still_might_format;
			case 'h':
			case 'l':
			case 'z':
				if (*fmt == 'h' && length_mod == 'h') {
					length_mod = 'H';
				} else if (*fmt == 'l' && length_mod == 'l') {
					length_mod = 'L';
				} else if (length_mod == 0) {
					length_mod = *fmt;
				} else {
					out((int)'%', ctx);
					out((int)*fmt, ctx);
					break;
				}
				goto still_might_format;
			case 'd':
			case 'i': {
				long d;

				if (length_mod == 'z') {
					d = va_arg(ap, int);
				} else if (length_mod == 'l') {
					d = va_arg(ap, long);
				} else if (length_mod == 'L') {
					long long lld = va_arg(ap, long long);
					if (lld > __LONG_MAX__ ||
					    lld < ~__LONG_MAX__) {
						print_err(out, ctx);
						break;
					}
					d = lld;
				} else {
					d = va_arg(ap, int);
				}

				if (d < 0) {
					out((int)'-', ctx);
					d = -d;
					min_width--;
				}
				_printk_dec_ulong(out, ctx, d, padding,
						  min_width);
				break;
			}
			case 'u': {
				unsigned long u;

				if (length_mod == 'z') {
					u = va_arg(ap, size_t);
				} else if (length_mod == 'l') {
					u = va_arg(ap, unsigned long);
				} else if (length_mod == 'L') {
					unsigned long long llu =
						va_arg(ap, unsigned long long);
					if (llu > ~0UL) {
						print_err(out, ctx);
						break;
					}
					u = llu;
				} else {
					u = va_arg(ap, unsigned int);
				}

				_printk_dec_ulong(out, ctx, u, padding,
						  min_width);
				break;
			}
			case 'p':
				out('0', ctx);
				out('x', ctx);
				/* left-pad pointers with zeros */
				padding = PAD_ZERO_BEFORE;
				if (IS_ENABLED(CONFIG_64BIT)) {
					min_width = 16;
				} else {
					min_width = 8;
				}
				/* Fall through */
			case 'x':
			case 'X': {
				unsigned long long x;

				if (*fmt == 'p') {
					x = (uintptr_t)va_arg(ap, void *);
				} else if (length_mod == 'l') {
					x = va_arg(ap, unsigned long);
				} else if (length_mod == 'L') {
					x = va_arg(ap, unsigned long long);
				} else {
					x = va_arg(ap, unsigned int);
				}

				_printk_hex_ulong(out, ctx, x, padding,
						  min_width);
				break;
			}
			case 's': {
				char *s = va_arg(ap, char *);
				char *start = s;

				while (*s) {
					out((int)(*s++), ctx);
				}

				if (padding == PAD_SPACE_AFTER) {
					int remaining = min_width - (s - start);
					while (remaining-- > 0) {
						out(' ', ctx);
					}
				}
				break;
			}
			case 'c': {
				int c = va_arg(ap, int);

				out(c, ctx);
				break;
			}
			case 'f': {
				double d = va_arg(ap, double);

				_printk_dec_ulong(out, ctx, (long)d, padding, min_width);
				out((int)'.', ctx);
				_printk_dec_ulong(out, ctx, (long)((d - (long)d) * 1000), PAD_NONE, 0);
				break;
			}
			case '%': {
				out((int)'%', ctx);
				break;
			}
			default:
				out((int)'%', ctx);
				out((int)*fmt, ctx);
				break;
			}
			might_format = 0;
		}
still_might_format:
		++fmt;
	}
}

#ifdef CONFIG_USERSPACE
struct buf_out_context {
	int count;
	unsigned int buf_count;
	char buf[CONFIG_PRINTK_BUFFER_SIZE];
};

static void buf_flush(struct buf_out_context *ctx)
{
	k_str_out(ctx->buf, ctx->buf_count);
	ctx->buf_count = 0U;
}

static int buf_char_out(int c, void *ctx_p)
{
	struct buf_out_context *ctx = ctx_p;

	ctx->count++;
	ctx->buf[ctx->buf_count++] = c;
	if (ctx->buf_count == CONFIG_PRINTK_BUFFER_SIZE) {
		buf_flush(ctx);
	}

	return c;
}
#endif /* CONFIG_USERSPACE */

struct out_context {
	int count;
};

static int char_out(int c, void *ctx_p)
{
	struct out_context *ctx = ctx_p;

	ctx->count++;
	return _char_out(c);
}

#ifdef CONFIG_USERSPACE
void vprintk(const char *fmt, va_list ap)
{
	if (_is_user_context()) {
		struct buf_out_context ctx = { 0 };

		z_vprintk(buf_char_out, &ctx, fmt, ap);

		if (ctx.buf_count) {
			buf_flush(&ctx);
		}
	} else {
		struct out_context ctx = { 0 };

		z_vprintk(char_out, &ctx, fmt, ap);
	}
}
#else
void vprintk(const char *fmt, va_list ap)
{
	struct out_context ctx = { 0 };

	z_vprintk(char_out, &ctx, fmt, ap);
}
#endif

void z_impl_k_str_out(char *c, size_t n)
{
	int i;

	for (i = 0; i < n; i++) {
		_char_out(c[i]);
	}
}

#ifdef CONFIG_USERSPACE
Z_SYSCALL_HANDLER(k_str_out, c, n)
{
	Z_OOPS(Z_SYSCALL_MEMORY_READ(c, n));
	z_impl_k_str_out((char *)c, n);

	return 0;
}
#endif

/**
 * @brief Output a string
 *
 * Output a string on output installed by platform at init time. Some
 * printf-like formatting is available.
 *
 * Available formatting:
 * - %x/%X:  outputs a 32-bit number in ABCDWXYZ format. All eight digits
 *	    are printed: if less than 8 characters are needed, leading zeroes
 *	    are displayed.
 * - %s:	    output a null-terminated string
 * - %p:     pointer, same as %x
 * - %d/%i/%u: outputs a 32-bit number in unsigned decimal format.
 *
 * @param fmt formatted string to output
 *
 * @return N/A
 */
void printk(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	if (IS_ENABLED(CONFIG_LOG_PRINTK)) {
		log_printk(fmt, ap);
	} else {
		vprintk(fmt, ap);
	}
	va_end(ap);
}

/**
 * @brief Output an unsigned long long in hex format
 *
 * Output an unsigned long long on output installed by platform at init time.
 * Able to print full 64-bit values.
 * @param num Number to output
 *
 * @return N/A
 */
static void _printk_hex_ulong(out_func_t out, void *ctx,
			      const unsigned long long num,
			      enum pad_type padding,
			      int min_width)
{
	int size = sizeof(num) * 2;
	int found_largest_digit = 0;
	int remaining = 16; /* 16 digits max */
	int digits = 0;

	for (; size != 0; size--) {
		char nibble = (num >> ((size - 1) << 2) & 0xf);

		if (nibble != 0 || found_largest_digit != 0 || size == 1) {
			found_largest_digit = 1;
			nibble += nibble > 9 ? 87 : 48;
			out((int)nibble, ctx);
			digits++;
			continue;
		}

		if (remaining-- <= min_width) {
			if (padding == PAD_ZERO_BEFORE) {
				out('0', ctx);
			} else if (padding == PAD_SPACE_BEFORE) {
				out(' ', ctx);
			}
		}
	}

	if (padding == PAD_SPACE_AFTER) {
		remaining = min_width * 2 - digits;
		while (remaining-- > 0) {
			out(' ', ctx);
		}
	}
}

/**
 * @brief Output an unsigned long (32-bit) in decimal format
 *
 * Output an unsigned long on output installed by platform at init time. Only
 * works with 32-bit values.
 * @param num Number to output
 *
 * @return N/A
 */
static void _printk_dec_ulong(out_func_t out, void *ctx,
			      const unsigned long num, enum pad_type padding,
			      int min_width)
{
	unsigned long pos = 999999999;
	unsigned long remainder = num;
	int found_largest_digit = 0;
	int remaining = 10; /* 10 digits max */
	int digits = 1;

	/* make sure we don't skip if value is zero */
	if (min_width <= 0) {
		min_width = 1;
	}

	while (pos >= 9) {
		if (found_largest_digit != 0 || remainder > pos) {
			found_largest_digit = 1;
			out((int)((remainder / (pos + 1)) + 48), ctx);
			digits++;
		} else if (remaining <= min_width
				&& padding < PAD_SPACE_AFTER) {
			out((int)(padding == PAD_ZERO_BEFORE ? '0' : ' '), ctx);
			digits++;
		}
		remaining--;
		remainder %= (pos + 1);
		pos /= 10;
	}
	out((int)(remainder + 48), ctx);

	if (padding == PAD_SPACE_AFTER) {
		remaining = min_width - digits;
		while (remaining-- > 0) {
			out(' ', ctx);
		}
	}
}

struct str_context {
	char *str;
	int max;
	int count;
};

static int str_out(int c, struct str_context *ctx)
{
	if (ctx->str == NULL || ctx->count >= ctx->max) {
		ctx->count++;
		return c;
	}

	if (ctx->count == ctx->max - 1) {
		ctx->str[ctx->count++] = '\0';
	} else {
		ctx->str[ctx->count++] = c;
	}

	return c;
}

int snprintk(char *str, size_t size, const char *fmt, ...)
{
	struct str_context ctx = { str, size, 0 };
	va_list ap;

	va_start(ap, fmt);
	z_vprintk((out_func_t)str_out, &ctx, fmt, ap);
	va_end(ap);

	if (ctx.count < ctx.max) {
		str[ctx.count] = '\0';
	}

	return ctx.count;
}

int vsnprintk(char *str, size_t size, const char *fmt, va_list ap)
{
	struct str_context ctx = { str, size, 0 };

	z_vprintk((out_func_t)str_out, &ctx, fmt, ap);

	if (ctx.count < ctx.max) {
		str[ctx.count] = '\0';
	}

	return ctx.count;
}
