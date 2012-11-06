#include <linux/time.h>
#include "../lunatik.h"

int
lunatik_gc_count(lua_State *L) {
	lua_pushinteger(L, lua_gc(L, LUA_GCCOUNT, 0));
	return 1;
}

int
lunatik_type(lua_State *L) {
	const char *type = luaL_typename(L, -1);
	lua_pop(L, 1);
	lua_pushstring(L, type);
	return 1;
}
