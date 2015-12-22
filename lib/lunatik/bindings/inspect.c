#include <linux/module.h>
#include <linux/time.h>
#include <linux/lunatik.h>

int lunatik_gc_count(lua_State *L) {
	lua_pushinteger(L, lua_gc(L, LUA_GCCOUNT, 0));
	return 1;
}
EXPORT_SYMBOL(lunatik_gc_count);

int lunatik_type(lua_State *L) {
	const char *type = luaL_typename(L, -1);
	lua_pop(L, 1);
	lua_pushstring(L, type);
	return 1;
}
EXPORT_SYMBOL(lunatik_type);

static int lunatik_inspect_register(struct lunatik_context *lc)
{
	lunatik_context_lock(lc);
	lua_register(lc->L, "type", &lunatik_type);
	lua_register(lc->L, "gc_count", &lunatik_gc_count);
	lunatik_context_unlock(lc);

	return 0;
}

static struct lunatik_binding *lunatik_inspect_binding;

static int __init lunatik_inspect_init(void)
{
	lunatik_inspect_binding = lunatik_bindings_register(
		THIS_MODULE, lunatik_inspect_register);

	return IS_ERR(lunatik_inspect_binding) ?
		PTR_ERR(lunatik_inspect_binding) : 0;
}

static void __exit lunatik_inspect_exit(void)
{
	if (!IS_ERR_OR_NULL(lunatik_inspect_binding))
		lunatik_bindings_unregister(lunatik_inspect_binding);
}

MODULE_AUTHOR("Matthias Grawinkel <grawinkel@uni-mainz.de>, Daniel Bausch <bausch@dvs.tu-darmstadt.de>");
MODULE_LICENSE("Dual MIT/GPL");

module_init(lunatik_inspect_init);
module_exit(lunatik_inspect_exit);
