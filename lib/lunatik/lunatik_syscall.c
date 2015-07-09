/*
 * lunatik_syscall.c
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
#include <linux/lunatik.h>

static int copy_lunatik_result_to_user(struct lunatik_result *r,
				int __user *r_type, void __user *r_data,
				size_t __user *r_size)
{
	int ret;
	int ret2;
	long n;
	size_t copy_size;
	size_t userdata_type_len;
	size_t r_size_kern;

	ret = get_user(r_size_kern, r_size);
	if (ret)
		goto out;

	ret = put_user(r->r_type, r_type);
	if (ret)
		goto out;

	switch (r->r_type) {
	case LUA_TNUMBER:
		if (r_size_kern >= sizeof(s64)) {
			ret = put_user((s64) r->r_number,
				(s64 __user *) r_data);
			r_size_kern = sizeof(s64);
		} else {
			r_size_kern = 0;
		}
		break;
	case LUA_TBOOLEAN:
		if (r_size_kern >= sizeof(int)) {
			ret = put_user((int) r->r_boolean,
				(int __user *) r_data);
			r_size_kern = sizeof(int);
		} else {
			r_size_kern = 0;
		}
		break;
	case LUA_TSTRING:
		copy_size = min(r_size_kern, r->r_string_size + 1);
		n = copy_to_user(r_data, r->r_string, copy_size);
		if (n)
			ret = -EFAULT;
		r_size_kern = copy_size - n;
		break;
	case LUA_TUSERDATA:
		userdata_type_len = strlen(r->r_userdata_type) + 1;

		copy_size = min(r_size_kern, userdata_type_len);
		n = copy_to_user(r_data, r->r_userdata_type, copy_size);
		/* terminate userdata_type with 0 if truncated */
		if (copy_size - n < userdata_type_len)
			(void) put_user(
				(char) 0,
				(char __user *) r_data + copy_size - n - 1);
		if (n) {
			r_size_kern = copy_size - n;
			ret = -EFAULT;
			break;
		}

		if (r_size_kern <= userdata_type_len) {
			r_size_kern = copy_size;
			break;
		}

		copy_size = min(r_size_kern - userdata_type_len,
				r->r_userdata_size);
		n = copy_to_user((char __user *) r_data + userdata_type_len,
				r->r_userdata, copy_size);
		if (n)
			ret = -EFAULT;
		r_size_kern = userdata_type_len + copy_size - n;
		break;
	case LUA_TLIGHTUSERDATA:
		if (r_size_kern >= sizeof(void *)) {
			ret = put_user(r->r_lightuserdata,
				(void * __user *) r_data);
			r_size_kern = sizeof(void *);
		} else {
			r_size_kern = 0;
		}
		break;
	default:
		r_size_kern = 0;
	}

	ret2 = put_user(r_size_kern, r_size);
	if (!ret && ret2)
		ret = ret2;

out:
	return ret;
}

asmlinkage long sys_lua(const char __user *code, size_t code_size,
			int __user *r_type, void __user *r_data,
			size_t __user *r_size)
{
	int ret, ret2;
	char *code_kern;

	code_kern = kmalloc(code_size, GFP_KERNEL);
	if (code_kern == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	if (copy_from_user(code_kern, code, code_size)) {
		ret = -EFAULT;
		goto out_code;
	}

	if (!r_type && !r_data && !r_size) {
		/* will execute asynchronously and will free code_kern */
		ret = lunatik_loadcode(code_kern, code_size, NULL);
		goto out;
	} else if (r_type && r_data && r_size) {
		struct lunatik_result *lunatik_result = NULL;

		/* will wait for result and will not free code_kern */
		ret = lunatik_loadcode(code_kern, code_size, &lunatik_result);

		if (lunatik_result) {
			ret2 = copy_lunatik_result_to_user(lunatik_result,
							r_type, r_data, r_size);
			if (!ret && ret2)
				ret = ret2;

			lunatik_result_free(lunatik_result);
		}
	} else {
		ret = -EINVAL;
	}

out_code:
	kfree(code_kern);
out:
	return ret;
}
