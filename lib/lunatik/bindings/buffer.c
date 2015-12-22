#include <linux/slab.h>
#include <linux/module.h>
#include <linux/lunatik.h>

int lunatik_buf_length(lua_State *L) {
	lua_pushinteger(L, lua_objlen(L, -1));
	return 1;
}
EXPORT_SYMBOL(lunatik_buf_length);

int lunatik_buf_call(lua_State *L) {
	size_t buffer_len;
	off_t idx;
	char *buf;

	buf = (char *) luaL_checkudata(L, -1, "buffer");
	buffer_len = lua_objlen(L, -1);

	lua_pop(L, 1);

	lua_createtable(L, 0, buffer_len);
	for (idx = 0; idx < buffer_len; idx++) {
		lua_pushinteger(L, buf[idx]);
		lua_rawseti(L, -2, idx + 1);
	}

	return 1;
}
EXPORT_SYMBOL(lunatik_buf_call);

int lunatik_buf_newindex(lua_State *L) {
	char *buf = (char *) luaL_checkudata(L, -3, "buffer");
	lua_Number idx = luaL_checkinteger(L, -2);
	lua_Number val = luaL_checkinteger(L, -1);

	if ((idx < 0) || (idx >= lua_objlen(L, -3))) {
		lua_pushstring(L, "index out of range");
		lua_error(L);
	}
	if ((val < 0) || (val > 255)) {
		lua_pushstring(L, "value must be between 0 and 255");
		lua_error(L);
	}

	buf[idx] = val;

	return 0;
}

int lunatik_buf_index(lua_State *L) {
	char *buf = (char *) luaL_checkudata(L, -2, "buffer");
	lua_Number idx = luaL_checkinteger(L, -1);

	if ((idx < 0) || (idx >= lua_objlen(L, -2))) {
		lua_pushstring(L, "index out of  range");
		lua_error(L);
	}

	lua_pushinteger(L, (unsigned char) buf[idx]);
	return 1;
}

int lunatik_buf_new(lua_State *L) {
	/*  TODO: create buffer from table */
	lua_Integer size = luaL_checkinteger(L, -1);
	lua_pop(L, 1);

	(void) lua_newuserdata(L, (size_t) size);

	luaL_newmetatable(L, "buffer");

	lua_pushcfunction(L, &lunatik_buf_call);
	lua_setfield(L, -2, "__call");
	lua_pushcfunction(L, &lunatik_buf_length);
	lua_setfield(L, -2, "__len");
	lua_pushcfunction(L, &lunatik_buf_newindex);
	lua_setfield(L, -2, "__newindex");
	lua_pushcfunction(L, &lunatik_buf_index);
	lua_setfield(L, -2, "__index");

	lua_setmetatable(L, -2);

	return 1;
}
EXPORT_SYMBOL(lunatik_buf_new);

static int lunatik_buf_register(struct lunatik_context *lc)
{
	struct luaL_reg lib_buffer[] = {
		{ "new", &lunatik_buf_new },
		{ NULL, NULL }
	};

	lunatik_context_lock(lc);
	luaL_register(lc->L, "buffer", lib_buffer);
	lunatik_context_unlock(lc);

	return 0;
}

static struct lunatik_binding *lunatik_buf_binding;

static int __init lunatik_buf_init(void)
{
	lunatik_buf_binding = lunatik_bindings_register(
		THIS_MODULE, lunatik_buf_register);

	return IS_ERR(lunatik_buf_binding) ?
		PTR_ERR(lunatik_buf_binding) : 0;
}

static void __exit lunatik_buf_exit(void)
{
	if (!IS_ERR_OR_NULL(lunatik_buf_binding))
		lunatik_bindings_unregister(lunatik_buf_binding);
}

MODULE_AUTHOR("Matthias Grawinkel <grawinkel@uni-mainz.de>, Daniel Bausch <bausch@dvs.tu-darmstadt.de>");
MODULE_LICENSE("Dual MIT/GPL");

module_init(lunatik_buf_init);
module_exit(lunatik_buf_exit);
