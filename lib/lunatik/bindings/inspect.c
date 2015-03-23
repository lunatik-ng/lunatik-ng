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

static int __init lunatik_inspect_init(void)
{
	lua_State *L = lunatik_get_global_state();

	lua_register(L, "type", &lunatik_type);
	lua_register(L, "gc_count", &lunatik_gc_count);

	return 0;
}

MODULE_AUTHOR("Matthias Grawinkel <grawinkel@uni-mainz.de>, Daniel Bausch <bausch@dvs.tu-darmstadt.de>");
MODULE_LICENSE("Dual MIT/GPL");

module_init(lunatik_inspect_init);
