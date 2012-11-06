/* 
 * lsyscall.c 
 * Lua System Call
 * See Copyright Notice in lunatik.h
 */

#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/hardirq.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "lunatik.h"

/* exported code */

asmlinkage long sys_lua(const char * code, size_t sz_code, char * result, size_t sz_result)
{
	int ret = 1;

	char * code_ = (char *) kmalloc(sz_code, GFP_KERNEL);
	if (code_ == NULL)
		return -ENOMEM;

	if (copy_from_user(code_, code, sz_code)) {
		kfree(code_);
		return -EFAULT;
	}

	if (result == NULL)
		ret = lunatik_loadcode(code_, sz_code, (char **) NULL, NULL);
	else {
		size_t sz_result_;
		char * result_;

		ret = lunatik_loadcode(code_, sz_code, & result_, & sz_result_);

		if (result_ != NULL){
			if (sz_result_ > sz_result)
				sz_result_ = sz_result;

			if (copy_to_user(result, result_, sz_result_)) {
				kfree(code_);
				kfree(result_);
				return -EFAULT;
			}

			kfree(result_);
		}
	}

	kfree(code_);
	return ret;
}

/* end lsyscall.c */

