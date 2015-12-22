#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/lunatik.h>

int lunatik_printk(lua_State *L) {
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
EXPORT_SYMBOL(lunatik_printk);

static int lunatik_printk_register(struct lunatik_context *lc)
{
	lunatik_context_lock(lc);
	lua_register(lc->L, "print", &lunatik_printk);
	lunatik_context_unlock(lc);

	return 0;
}

static struct lunatik_binding *lunatik_printk_binding;

static int __init lunatik_printk_init(void)
{
	lunatik_printk_binding = lunatik_bindings_register(
		THIS_MODULE, lunatik_printk_register);

	return IS_ERR(lunatik_printk_binding) ?
		PTR_ERR(lunatik_printk_binding) : 0;
}

static void __exit lunatik_printk_exit(void)
{
	if (!IS_ERR_OR_NULL(lunatik_printk_binding))
		lunatik_bindings_unregister(lunatik_printk_binding);
}

MODULE_AUTHOR("Matthias Grawinkel <grawinkel@uni-mainz.de>, Daniel Bausch <bausch@dvs.tu-darmstadt.de>");
MODULE_LICENSE("Dual MIT/GPL");

module_init(lunatik_printk_init);
module_exit(lunatik_printk_exit);
