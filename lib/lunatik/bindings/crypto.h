#ifndef __LUNATIK_BINDINGS_CRYPTO
#define __LUNATIK_BINDINGS_CRYPTO

#define LF(name) int lunatik_##name(lua_State *L)
LF(sha1);
LF(get_random_bytes);
#undef LF

#endif /* __LUNATIK_BINDINGS_CRYPTO */
