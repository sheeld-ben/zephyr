/*
 * Copyright (c) 2013-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Cortex-M public error handling
 *
 * ARM-specific kernel error handling interface. Included by arm/arch.h.
 */

#ifndef ZEPHYR_INCLUDE_ARCH_ARM_CORTEX_M_ERROR_H_
#define ZEPHYR_INCLUDE_ARCH_ARM_CORTEX_M_ERROR_H_

#include <arch/arm/syscall.h>
#include <arch/arm/cortex_m/exc.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _ASMLANGUAGE
extern void z_NanoFatalErrorHandler(unsigned int reason, const NANO_ESF *esf);
extern void z_SysFatalErrorHandler(unsigned int reason, const NANO_ESF *esf);
#endif

#define _NANO_ERR_HW_EXCEPTION (0)      /* MPU/Bus/Usage fault */
#define _NANO_ERR_STACK_CHK_FAIL (2)    /* Stack corruption detected */
#define _NANO_ERR_ALLOCATION_FAIL (3)   /* Kernel Allocation Failure */
#define _NANO_ERR_KERNEL_OOPS (4)       /* Kernel oops (fatal to thread) */
#define _NANO_ERR_KERNEL_PANIC (5)      /* Kernel panic (fatal to system) */
#define _NANO_ERR_RECOVERABLE (6)       /* Recoverable error */

#define _NANO_ERR_HW_MPU_FAULT (10)     /* MPU fault */
#define _NANO_ERR_HW_MPU_MSTKERR (11)   /* MPU fault: MemManage fault on stacking for exception entry. */
#define _NANO_ERR_HW_MPU_MUNSTKERR (12) /* MPU fault: MemManage fault on unstacking for a return from exception. */
#define _NANO_ERR_HW_MPU_DACCVIOL (13)  /* MPU fault: Data access violation flag. */
#define _NANO_ERR_HW_MPU_IACCVIOL (14)  /* MPU fault: Instruction access violation flag. */
#define _NANO_ERR_HW_MPU_MLSPERR (15)   /* MPU fault: Floating-point lazy state preservation error. */

#define _NANO_ERR_HW_BUS_FAULT (20)     /* BUS fault */
#define _NANO_ERR_HW_BUS_STKERR (21)    /* BUS fault: BusFault on stacking for exception entry. */
#define _NANO_ERR_HW_BUS_UNSTKERR (22)  /* BUS fault: BusFault on unstacking for a return from exception. */
#define _NANO_ERR_HW_BUS_PRECISERR (23) /* BUS fault: Precise data bus error. */
#define _NANO_ERR_HW_BUS_IBUSERR (24)   /* BUS fault: Instruction bus error. */
#define _NANO_ERR_HW_BUS_LSPERR (25)    /* BUS fault: Floating-point lazy state preservation error. */

#define _NANO_ERR_HW_USAGE_FAULT (30)      /* Usage fault */
#define _NANO_ERR_HW_USAGE_DIVBYZERO (31)  /* Usage fault: Divide by zero flag. */
#define _NANO_ERR_HW_USAGE_UNALIGNED (32)  /* Usage fault: Unaligned access flag. */
#define _NANO_ERR_HW_USAGE_STKOF (33)      /* Usage fault: Stack overflow flag. */
#define _NANO_ERR_HW_USAGE_NOCP (34)       /* Usage fault: No coprocessor flag. */
#define _NANO_ERR_HW_USAGE_INVPC (35)      /* Usage fault: Invalid PC flag. */
#define _NANO_ERR_HW_USAGE_INVSTATE (36)   /* Usage fault: Invalid state flag. */
#define _NANO_ERR_HW_USAGE_UNDEFINSTR (37) /* Usage fault: Undefined instruction flag. */

#define _NANO_ERR_HW_SECURE_FAULT (40)     /* Secure fault */

#if defined(CONFIG_ARMV6_M_ARMV8_M_BASELINE)
/* ARMv6 will hard-fault if SVC is called with interrupts locked. Just
 * force them unlocked, the thread is in an undefined state anyway
 *
 * On ARMv7m we won't get a HardFault, but if interrupts were locked the
 * thread will continue executing after the exception and forbid PendSV to
 * schedule a new thread until they are unlocked which is not what we want.
 * Force them unlocked as well.
 */
#define Z_ARCH_EXCEPT(reason_p) do { \
	__asm__ volatile ( \
		"cpsie i\n\t" \
		"movs r0, %[reason]\n\t" \
		"svc %[id]\n\t" \
		: \
		: [reason] "i" (reason_p), [id] "i" (_SVC_CALL_RUNTIME_EXCEPT) \
		: "memory"); \
	CODE_UNREACHABLE; \
} while (false)
#elif defined(CONFIG_ARMV7_M_ARMV8_M_MAINLINE)
#define Z_ARCH_EXCEPT(reason_p) do { \
	__asm__ volatile ( \
		"eors.n r0, r0\n\t" \
		"msr BASEPRI, r0\n\t" \
		"mov r0, %[reason]\n\t" \
		"svc %[id]\n\t" \
		: \
		: [reason] "i" (reason_p), [id] "i" (_SVC_CALL_RUNTIME_EXCEPT) \
		: "memory"); \
	CODE_UNREACHABLE; \
} while (false)
#else
#error Unknown ARM architecture
#endif /* CONFIG_ARMV6_M_ARMV8_M_BASELINE */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_ARCH_ARM_CORTEX_M_ERROR_H_ */
