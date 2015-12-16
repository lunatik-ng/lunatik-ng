/*
 * lunatik.c
 * See Copyright Notice in lunatik.h
 */

#ifdef CONFIG_DEBUG_LUNATIK
#define DEBUG 1
#endif

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/lunatik.h>

#include "lunatik_workqueue.h"
#include "lauxlib.h"

struct loadcode_data {
	char *code;
	size_t sz_code;
	struct lunatik_result *result;
	int ret;
	char blocking;
};

static struct lunatik_result *lunatik_result_get(lua_State *L, int idx,
						int *perror)
{
	struct lunatik_result *r = kzalloc(sizeof(*r), GFP_KERNEL);
	const char *r_string;
	size_t r_string_size;
	void *r_userdata;
	size_t r_userdata_size;
	const char *r_userdata_type;
	size_t r_userdata_type_size;

	if (!r) {
		if (perror)
			*perror = -ENOMEM;
		goto out;
	}

	r->r_type = lua_type(L, idx);

	switch (r->r_type) {
	case LUA_TNIL:
		break;
	case LUA_TNUMBER:
		r->r_number = lua_tonumber(L, idx);
		break;
	case LUA_TBOOLEAN:
		r->r_boolean = lua_toboolean(L, idx);
		break;
	case LUA_TSTRING:
		r_string = lua_tolstring(L, idx, &r_string_size);
		r->r_string = kmalloc(r_string_size + 1, GFP_KERNEL);
		if (!r->r_string) {
			if (perror)
				*perror = -ENOMEM;
			break;
		}
		memcpy(r->r_string, r_string, r_string_size);
		r->r_string[r_string_size] = '\0';
		r->r_string_size = r_string_size;
		break;
	case LUA_TUSERDATA:
		r_userdata = lua_touserdata(L, idx);
		r_userdata_size = lua_objlen(L, idx);
		r->r_userdata = kmalloc(r_userdata_size, GFP_KERNEL);
		if (!r->r_userdata) {
			if (perror)
				*perror = -ENOMEM;
			break;
		}
		memcpy(r->r_userdata, r_userdata, r_userdata_size);
		r->r_userdata_size = r_userdata_size;
		if (luaL_getmetafield(L, idx, "__name") != LUA_TNIL) {
			r_userdata_type =
				lua_tolstring(L, -1, &r_userdata_type_size);
			lua_pop(L, 1);
		} else {
			r_userdata_type = "";
		}
		pr_debug("[lunatik] userdata_type = %s\n", r_userdata_type);
		r->r_userdata_type =
			kmalloc(r_userdata_type_size + 1, GFP_KERNEL);
		if (!r->r_userdata_type) {
			kfree(r->r_userdata);
			r->r_userdata = NULL;
			if (perror)
				*perror = -ENOMEM;
			break;
		}
		memcpy(r->r_userdata_type, r_userdata_type,
			r_userdata_type_size);
		r->r_userdata_type[r_userdata_type_size] = '\0';
		break;
	case LUA_TLIGHTUSERDATA:
		r->r_lightuserdata = lua_touserdata(L, idx);
		break;
	default:
		if (perror)
			*perror = -EINVAL;
	}

out:
	return r;
}

static void loadcode_internal(lua_State *L, struct loadcode_data *d,
			bool need_result)
{

	lunatik_lock_global_state();
	pr_debug("[lunatik] executing lua code: (0x%p) %s\n", d->code, d->code);

	d->ret = luaL_loadbuffer(L, d->code, d->sz_code - 1, "loadcode");
	if (d->ret)
		pr_err("[lunatik] luaL_loadbuffer failed\n");
	else {
		d->ret = lua_pcall(L, 0, 1, 0);
		if (d->ret)
			pr_err("[lunatik] lua_pcall failed\n");
	}

	if (d->ret && lua_type(L, -1) == LUA_TSTRING)
		pr_err("[lunatik] %s\n", lua_tostring(L, -1));

	if (need_result) {
		d->result = lunatik_result_get(L, -1, &d->ret);
	} else {
#ifdef DEBUG
		struct lunatik_result *r;

		pr_debug("[lunatik] async lua execution done\n");

		r = lunatik_result_get(L, -1, NULL);

		if (r && r->r_type != LUA_TNONE && r->r_type != LUA_TNIL) {
			char *r_type_name;
			switch (r->r_type) {
			case LUA_TBOOLEAN:
				r_type_name = "LUA_TBOOLEAN";
				break;
			case LUA_TLIGHTUSERDATA:
				r_type_name = "LUA_TLIGHTUSERDATA";
				break;
			case LUA_TNUMBER:
				r_type_name = "LUA_TNUMBER";
				break;
			case LUA_TSTRING:
				r_type_name = "LUA_TSTRING";
				break;
			case LUA_TUSERDATA:
				r_type_name = "LUA_TUSERDATA";
				break;
			default:
				r_type_name = "UNKNOWN";
			}
			pr_debug("[lunatik] result of type %s thrown away\n",
				r_type_name);
		}

		lunatik_result_free(r);
#endif
	}

	lua_pop(L, 1);

	lunatik_unlock_global_state();
}

int lunatik_loadcode_direct(char *code, size_t sz_code,
			struct lunatik_result **presult)
{
	int ret;
	struct loadcode_data *d;

	d = kmalloc(sizeof(*d), GFP_KERNEL);
	if (!d) {
		ret = -ENOMEM;
		goto out;
	}

	d->code = code;
	d->sz_code = sz_code;
	d->result = NULL;
	d->ret = -1;
	d->blocking = 1;

	loadcode_internal(lunatik_get_global_state(), d, presult != NULL);

	if (presult)
		*presult = d->result;
	ret = d->ret;

	kfree(d);
out:
	return ret;
}
EXPORT_SYMBOL(lunatik_loadcode_direct);

static void loadcode_work_handler(struct work_struct *work)
{
	struct lunatik_work *lw = to_lunatik_work(work);
	struct loadcode_data *d = lw->work_data;
	lua_State *L = lw->L;

	loadcode_internal(L, d, d->blocking);

	if (d->blocking) {
		/* signal */
		d->blocking = 0;
	} else {
		kfree(d->code);
		kfree(d);
		lunatik_work_destroy(lw);
	}
}

int lunatik_loadcode(char *code, size_t sz_code,
		struct lunatik_result **presult)
{
	int ret;
	struct lunatik_work *lw;
	struct loadcode_data *d;

	d = kmalloc(sizeof(*d), GFP_KERNEL);
	if (!d) {
		ret = -ENOMEM;
		goto out;
	}

	d->code = code;
	d->sz_code = sz_code;
	d->result = NULL;
	d->ret = -1;
	d->blocking = presult == NULL ? 0 : 1;

	lw = lunatik_new_work(loadcode_work_handler, d);
	if (!lw) {
		ret = -ENOMEM;
		goto out_free;
	}

	pr_debug("[lunatik] going to execute lua code: (0x%p) %s\n",
		d->code, d->code);

	lunatik_queue_work(lw);

	/*
	 * Further processing and cleanup is delegated to job if running
	 * asynchronously.
	 */
	if (!d->blocking) {
		ret = 0;
		goto out;
	}

	/* wait work */
	while (d->blocking)
		schedule();

	*presult = d->result;
	ret = d->ret;

	lunatik_delete_work(lw);

out_free:
	/*
	 * In the synchronous case the caller is expected to free code.
	 * We only free what we have allocated.
	 */

	kfree(d);
out:
	return ret;
}
EXPORT_SYMBOL(lunatik_loadcode);

void lunatik_result_free(const struct lunatik_result *result)
{
	if (result) {
		pr_debug("[lunatik] free result\n");
		switch (result->r_type) {
		case LUA_TSTRING:
			kfree(result->r_string);
			break;
		case LUA_TUSERDATA:
			kfree(result->r_userdata);
			kfree(result->r_userdata_type);
		}
		kfree(result);
	}
}
EXPORT_SYMBOL(lunatik_result_free);

inline static void openlib(lua_State *L, lua_CFunction luaopen_func)
{
	lua_pushcfunction(L, luaopen_func);
	lua_pcall(L, 0, 1, 0);
}

static void openlib_work_handler(struct work_struct *work)
{
	struct lunatik_work *lw = to_lunatik_work(work);
	lua_CFunction luaopen_func = lw->work_data;

	openlib(lw->L, luaopen_func);
}

int lunatik_openlib(lua_CFunction luaopen_func)
{
	struct lunatik_work *lw =
		lunatik_new_work(openlib_work_handler, luaopen_func);

	if (!lw)
		return -ENOMEM;

	lunatik_queue_work(lw);

	return 0;
}
EXPORT_SYMBOL(lunatik_openlib);

static lua_State *L = NULL;

lua_State *lunatik_get_global_state(void)
{
	/* TODO implement reference counting (incl. module ref count) */
	return L;
}
EXPORT_SYMBOL(lunatik_get_global_state);

static DEFINE_MUTEX(lunatik_global_state_mutex);

void lunatik_lock_global_state(void)
{
	mutex_lock(&lunatik_global_state_mutex);
}
EXPORT_SYMBOL(lunatik_lock_global_state);

void lunatik_unlock_global_state(void)
{
	mutex_unlock(&lunatik_global_state_mutex);
}
EXPORT_SYMBOL(lunatik_unlock_global_state);

static int __init lunatik_init(void)
{
	L = lua_open();
	if (L == NULL)
		return -ENOMEM;

	lunatik_workqueue_init(L);

	pr_info("Lunatik init done\n");
	return 0;
}

MODULE_AUTHOR("Lourival Vieira Neto <lneto@inf.puc-rio.br>, Matthias Grawinkel <grawinkel@uni-mainz.de>, Daniel Bausch <bausch@dvs.tu-darmstadt.de>");
MODULE_LICENSE("Dual MIT/GPL");

module_init(lunatik_init);
