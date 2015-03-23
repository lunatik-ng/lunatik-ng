/*
 * lunatik.c
 * See Copyright Notice in lunatik.h
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/time.h>

#include "lunatik_workqueue.h"
#include "lauxlib.h"

struct loadcode_struct {
	char *code;
	size_t sz_code;
	char *result;
	size_t sz_result;
	int ret;
	char blocking;
};

static void loadcode_work_handler(struct work_struct *work)
{
	const char *result = NULL;
	struct lunatik_work_struct *loadcode_work =
		lunatik_work_container_of(work);
	struct loadcode_struct *loadcode = loadcode_work->work_data;

	pr_info("[lunatik] executing lua code: (0x%p) %s\n",
		loadcode->code, loadcode->code);

	loadcode->ret = luaL_loadbuffer(loadcode_work->L, loadcode->code,
					loadcode->sz_code - 1, "loadcode") ||
		lua_pcall(loadcode_work->L, 0, 1, 0);
	result = lua_tostring(loadcode_work->L, -1);

	if (loadcode->ret)
		pr_err("[lunatik] %s\n", result);

	if (loadcode->blocking) {
		if (result != NULL) {
			loadcode->sz_result = strlen(result) + 1;
			loadcode->result =
				kmalloc(loadcode->sz_result, GFP_KERNEL);
			if (loadcode->result == NULL)
				loadcode->ret = -ENOMEM;
			else
				strncpy(loadcode->result, result,
					loadcode->sz_result);
		}

		lua_pop(loadcode_work->L, 1);

		/* signal */
		loadcode->blocking = 0;
	} else {
		lua_pop(loadcode_work->L, 1);

		kfree(loadcode->code);
		kfree(loadcode);
		lunatik_delete_work(loadcode_work);
	}
}

int lunatik_loadcode(char *code, size_t sz_code,
		char **presult, size_t *psz_result)
{
	struct lunatik_work_struct *loadcode_work;
	struct loadcode_struct *loadcode;

	loadcode = kmalloc(sizeof(*loadcode), GFP_KERNEL);
	if (loadcode == NULL)
		return -ENOMEM;

	loadcode->code = code;
	loadcode->sz_code = sz_code;
	loadcode->result = NULL;
	loadcode->sz_result = 0;
	loadcode->ret = -1;

	loadcode->blocking = presult == NULL ? 0 : 1;

	loadcode_work = lunatik_new_work(loadcode_work_handler, loadcode);
	if (loadcode_work == NULL) {
		kfree(loadcode);
		return -ENOMEM;
	}

	pr_info("[lunatik] going to execute lua code: (0x%p) %s\n",
		loadcode->code, loadcode->code);

	lunatik_queue_work(loadcode_work);

	if (loadcode->blocking) {
		int ret;

		/* wait work */
		while(loadcode->blocking)
			schedule();

		*presult = loadcode->result;
		*psz_result = loadcode->sz_result;
		ret = loadcode->ret;

		kfree(loadcode);
		lunatik_delete_work(loadcode_work);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(lunatik_loadcode);

inline static void openlib(lua_State *L, lua_CFunction luaopen_func)
{
	lua_pushcfunction(L, luaopen_func);
	lua_pcall(L, 0, 1, 0);
}

static void openlib_work_handler(struct work_struct *work)
{
	struct lunatik_work_struct *openlib_work =
		lunatik_work_container_of(work);
	lua_CFunction luaopen_func = openlib_work->work_data;

	openlib(openlib_work->L, luaopen_func);
}

int lunatik_openlib(lua_CFunction luaopen_func)
{
	struct lunatik_work_struct *openlib_work =
		lunatik_new_work(openlib_work_handler, luaopen_func);
	if (openlib_work == NULL)
		return -ENOMEM;

	lunatik_queue_work(openlib_work);

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

static int __init lunatik_init(void)
{
	L = lua_open();
	if (L == NULL)
		return -ENOMEM;

	lunatik_workqueue_init(L);

	printk("Lunatik init done\n");
	return 0;
}

MODULE_AUTHOR("Lourival Vieira Neto <lneto@inf.puc-rio.br>, Matthias Grawinkel <grawinkel@uni-mainz.de>, Daniel Bausch <bausch@dvs.tu-darmstadt.de>");
MODULE_LICENSE("Dual MIT/GPL");

module_init(lunatik_init);
