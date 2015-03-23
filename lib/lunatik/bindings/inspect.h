#ifndef __LUNATIK_BINDINGS_INSPECT
#define __LUNATIK_BINDINGS_INSPECT

#include <linux/lunatik.h>

#define LF(name) int lunatik_##name(lua_State *L)
LF(gc_count);
LF(type);
#undef LF

#endif /* __LUNATIK_BINDINGS_INSPECT */
