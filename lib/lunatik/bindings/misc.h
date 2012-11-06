#ifndef __LUNATIK_BINDINGS_MISC
#define __LUNATIK_BINDINGS_MISC

#define LF(name) int lunatik_##name(lua_State *L)

LF(gc_count);
LF(type);

#undef LF
#endif
