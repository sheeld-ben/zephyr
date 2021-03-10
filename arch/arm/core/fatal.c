/*
 * Copyright (c) 2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Kernel fatal error handler for ARM Cortex-M
 *
 * This module provides the z_NanoFatalErrorHandler() routine for ARM Cortex-M.
 */

#include <toolchain.h>
#include <linker/sections.h>
#include <inttypes.h>

#include <kernel.h>
#include <kernel_structs.h>
#include <misc/printk.h>
#include <logging/log_ctrl.h>

/**
 *
 * @brief Kernel fatal error handler
 *
 * This routine is called when fatal error conditions are detected by software
 * and is responsible only for reporting the error. Once reported, it then
 * invokes the user provided routine z_SysFatalErrorHandler() which is
 * responsible for implementing the error handling policy.
 *
 * The caller is expected to always provide a usable ESF. In the event that the
 * fatal error does not have a hardware generated ESF, the caller should either
 * create its own or use a pointer to the global default ESF <_default_esf>.
 *
 * Unlike other arches, this function may return if z_SysFatalErrorHandler
 * determines that only the current thread should be aborted and the CPU
 * was in handler mode. PendSV will be asserted in this case and the current
 * thread taken off the run queue. Leaving the exception will immediately
 * trigger a context switch.
 *
 * @param reason the reason that the handler was called
 * @param pEsf pointer to the exception stack frame
 *
 * @return This function does not return.
 */
void z_NanoFatalErrorHandler(unsigned int reason,
					  const NANO_ESF *pEsf)
{
	LOG_PANIC();

	switch (reason) {
	case _NANO_ERR_HW_EXCEPTION:
		printk("***** Hardware exception *****\n");
		break;
	case _NANO_ERR_HW_MPU_FAULT:
	case _NANO_ERR_HW_MPU_MSTKERR:
	case _NANO_ERR_HW_MPU_MUNSTKERR:
	case _NANO_ERR_HW_MPU_DACCVIOL:
	case _NANO_ERR_HW_MPU_IACCVIOL:
	case _NANO_ERR_HW_MPU_MLSPERR:
		printk("***** Hardware exception MPU Fault *****\n");
		break;
	case _NANO_ERR_HW_BUS_FAULT:
	case _NANO_ERR_HW_BUS_STKERR:
	case _NANO_ERR_HW_BUS_UNSTKERR:
	case _NANO_ERR_HW_BUS_PRECISERR:
	case _NANO_ERR_HW_BUS_IMPRECISERR:
	case _NANO_ERR_HW_BUS_IBUSERR:
	case _NANO_ERR_HW_BUS_LSPERR:
		printk("***** Hardware exception BUS Fault *****\n");
		break;
	case _NANO_ERR_HW_USAGE_FAULT:
	case _NANO_ERR_HW_USAGE_DIVBYZERO:
	case _NANO_ERR_HW_USAGE_UNALIGNED:
	case _NANO_ERR_HW_USAGE_STKOF:
	case _NANO_ERR_HW_USAGE_NOCP:
	case _NANO_ERR_HW_USAGE_INVPC:
	case _NANO_ERR_HW_USAGE_INVSTATE:
	case _NANO_ERR_HW_USAGE_UNDEFINSTR:
		printk("***** Hardware exception USAGE Fault *****\n");
		break;
	case _NANO_ERR_HW_SECURE_FAULT:
		printk("***** Hardware exception SECURE Fault *****\n");
		break;
#if defined(CONFIG_STACK_CANARIES) || defined(CONFIG_STACK_SENTINEL) || \
		defined(CONFIG_HW_STACK_PROTECTION) || \
		defined(CONFIG_USERSPACE)
	case _NANO_ERR_STACK_CHK_FAIL:
		printk("***** Stack Check Fail! *****\n");
		break;
#endif /* CONFIG_STACK_CANARIES */

	case _NANO_ERR_ALLOCATION_FAIL:
		printk("**** Kernel Allocation Failure! ****\n");
		break;

	case _NANO_ERR_KERNEL_OOPS:
		printk("***** Kernel OOPS! *****\n");
		break;

	case _NANO_ERR_KERNEL_PANIC:
		printk("***** Kernel Panic! *****\n");
		break;

	default:
		printk("**** Unknown Fatal Error %d! ****\n", reason);
		break;
	}
	printk("Current thread ID = %p, Faulting instruction address = 0x%x\n",
	       k_current_get(), pEsf->pc);

	/*
	 * Now that the error has been reported, call the user implemented
	 * policy
	 * to respond to the error.  The decisions as to what responses are
	 * appropriate to the various errors are something the customer must
	 * decide.
	 */

	z_SysFatalErrorHandler(reason, pEsf);
}

void z_do_kernel_oops(const NANO_ESF *esf)
{
	z_NanoFatalErrorHandler(esf->r0, esf);
}

FUNC_NORETURN void z_arch_syscall_oops(void *ssf_ptr)
{
	u32_t *ssf_contents = ssf_ptr;
	NANO_ESF oops_esf = { 0 };

	LOG_PANIC();

	oops_esf.pc = ssf_contents[3];

	z_do_kernel_oops(&oops_esf);
	CODE_UNREACHABLE;
}
