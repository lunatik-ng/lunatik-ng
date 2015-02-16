/*
 * arch/um/include/sysdep-i386/archsetjmp.h
 */

#ifndef _LUNATIK_SETJMP_H
#define _LUNATIK_SETJMP_H
/* FIXME: HACK ALERT! */
#include "../../../../arch/x86/um/shared/sysdep/archsetjmp.h"
#include <linux/linkage.h>

asmlinkage extern int setjmp(jmp_buf);
asmlinkage extern void longjmp(jmp_buf, int);
#endif

/*
#ifndef _KLIBC_ARCHSETJMP_H
#define _KLIBC_ARCHSETJMP_H

struct __jmp_buf {
	unsigned int __ebx;
	unsigned int __esp;
	unsigned int __ebp;
	unsigned int __esi;
	unsigned int __edi;
	unsigned int __eip;
};

typedef struct __jmp_buf jmp_buf[1];

#define JB_IP __eip
#define JB_SP __esp

#endif		*/		/* _SETJMP_H */
