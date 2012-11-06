#ifndef __LUNATIK_BINDINGS_BUFFER
#define __LUNATIK_BINDINGS_BUFFER
#define LF(name) int lunatik_##name(lua_State *L)
LF(buf_length);
LF(buf_call);
LF(buf_new);
#undef LF
#endif /* __LUNATIK_BINDINGS_BUFFER */
