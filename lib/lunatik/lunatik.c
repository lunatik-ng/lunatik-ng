/* 
 * lunatik.c 
 * See Copyright Notice in lunatik.h
 */

#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lkworkqueue.h"

#include "bindings/printk.h"
#include "bindings/misc.h"
#include "bindings/crypto.h"
#include "bindings/buffer.h"

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/time.h>

/* static data */

struct loadcode_struct{
	char *	code;
	size_t	sz_code;
	char *	result;
	size_t	sz_result;
	int	ret;
	char	blocking;
};

static lua_State * L = NULL;

/* static prototypes */

static void loadcode_work_handler(struct work_struct * work);

static void openlib_work_handler(struct work_struct * work);

inline static void openlib(lua_State * L, lua_CFunction luaopen_func);

/* exported code */
	
int lunatik_loadcode(char * code, size_t sz_code, char ** presult, size_t * psz_result)
{
	struct lunatikW_work_struct * 	loadcode_work;
	struct loadcode_struct * 	loadcode;
	int ret;
	
	loadcode = (struct loadcode_struct *) kmalloc(sizeof(struct loadcode_struct), GFP_KERNEL);
	if (loadcode == NULL)
		return -ENOMEM;

	loadcode->code      	= code;
	loadcode->sz_code   	= sz_code;
	loadcode->result	= NULL;
	loadcode->sz_result 	= 0;
	loadcode->ret       	= -1;

	loadcode->blocking = 1;

	loadcode_work = lunatikW_new_work(loadcode_work_handler, loadcode);
	if (loadcode_work == NULL) {
		kfree(loadcode);
		return -ENOMEM;
	}

	lunatikW_queue_work(loadcode_work);

	/* wait work */
	while(loadcode->blocking)
		schedule();
	lunatikW_delete_work(loadcode_work);

	if (presult != NULL) {
		* presult    = loadcode->result;
		* psz_result = loadcode->sz_result;
	}
	ret = loadcode->ret;
	kfree(loadcode);

	return ret;
} /* end lunatik_loadcode */

int lunatik_openlib(lua_CFunction luaopen_func)
{
	struct lunatikW_work_struct * openlib_work = lunatikW_new_work(openlib_work_handler, luaopen_func);
	if (openlib_work == NULL)
		return -ENOMEM;

	lunatikW_queue_work(openlib_work);

	return 0;
} /* end lunatik_openlib */

/* static code */

static void loadcode_work_handler(struct work_struct * work)
{
	struct lunatikW_work_struct *	loadcode_work	= lunatikW_container_of(work);
	struct loadcode_struct * loadcode	= (struct loadcode_struct *) loadcode_work->work_data;
	const char *result = NULL;
	
	loadcode->ret = luaL_loadbuffer(loadcode_work->L, loadcode->code, loadcode->sz_code - 1, "loadcode") ||
			lua_pcall(loadcode_work->L, 0, 1, 0);

	result = lua_tostring(loadcode_work->L, -1);

	if (loadcode->ret)
		printk(KERN_ERR "[lunatik] %s\n", result);

	if (result != NULL){
		loadcode->sz_result = strlen(result) + 1;

		loadcode->result = (char *) kmalloc(loadcode->sz_result, GFP_KERNEL);
		if (loadcode->result == NULL)
			loadcode->ret = -ENOMEM;
		else
			strncpy(loadcode->result, result, loadcode->sz_result);
	}

	lua_pop(loadcode_work->L, 1);

	/* signal */
	loadcode->blocking = 0;
} /* end loadcode_work_handler */

static void openlib_work_handler(struct work_struct * work)
{
	struct lunatikW_work_struct *	openlib_work 	= lunatikW_container_of(work);
	lua_CFunction 			luaopen_func 	= (lua_CFunction) openlib_work->work_data;

	openlib(openlib_work->L, luaopen_func);

	return;
} /* end openlib_work_handler */

inline static void openlib(lua_State * L, lua_CFunction luaopen_func)
{
	lua_pushcfunction(L, luaopen_func);
	lua_pcall(L, 0, 1, 0);
} /* end openlib */

static int __init lunatik_init(void)
{
	struct luaL_Reg lib_crypto[] = {
		{ "sha1", &lunatik_sha1 },
		{ "random", &lunatik_get_random_bytes },
		{ NULL, NULL }
	};
	struct luaL_reg lib_buffer[] = {
		{ "new", &lunatik_buf_new },
		{ NULL, NULL }
	};

	L = lua_open();
	if (L == NULL)
		return -ENOMEM;

	lunatikW_init(L);

	lua_register(L, "print", &lunatik_printk);

	lua_register(L, "type", &lunatik_type);
	lua_register(L, "gc_count", &lunatik_gc_count);

	luaL_register(L, "crypto", lib_crypto);
	luaL_register(L, "buffer", lib_buffer);

	printk("Lunatik init done\n");
	return 0;
} /* end lunatik_init */

MODULE_AUTHOR("Lourival Vieira Neto <lneto@inf.puc-rio.br>");

module_init(lunatik_init);

/* end lunatik.c */

