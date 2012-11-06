#include <linux/kernel.h>
#include "../lunatik.h"

int
lunatik_printk(lua_State *L) {
	int n = lua_gettop(L);
	int i;
	for (i = n; i >= 1; i--) {
		const char *s = lua_tostring(L, -i);
		if (s)
			printk("%s", s);
		else
			printk("(null)");
	}
	if (n != 0)
		printk("\n");
	lua_pop(L, n);
	return 0;
}
