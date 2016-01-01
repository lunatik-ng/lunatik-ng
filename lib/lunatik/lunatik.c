/*
 * lunatik.c
 * See Copyright Notice in lunatik.h
 */

#ifdef CONFIG_DEBUG_LUNATIK
#define DEBUG 1
#endif

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/lunatik.h>

#include "lauxlib.h"

struct lunatik_work {
	struct lunatik_context *lc;
	struct workqueue_struct *wq;
	struct work_struct work;
	void *work_data;
};
#define to_lunatik_work(work) \
	container_of(work, struct lunatik_work, work)

typedef void (*lunatik_loadcode_callback_work)(struct lunatik_work *lw);

struct loadcode_data {
	char *code;
	size_t sz_code;
	struct lunatik_result *result;
	int ret;
	bool need_result;
	void *callback_arg;
	lunatik_loadcode_callback callback;
	lunatik_loadcode_callback_nores callback_nores;
	lunatik_loadcode_callback_work callback_work;
	struct semaphore complete;
};

struct active_binding {
	struct lunatik_binding *b;
	struct list_head l;
};


static struct lunatik_result *lunatik_result_get(lua_State *L, int idx,
						int *perror)
{
	struct lunatik_result *r = kzalloc(sizeof(*r), GFP_KERNEL);
	const char *r_string;
	size_t r_string_size;
	void *r_userdata;
	size_t r_userdata_size;
	const char *r_userdata_type;
	size_t r_userdata_type_size;

	if (!r) {
		if (perror)
			*perror = -ENOMEM;
		goto out;
	}

	r->r_type = lua_type(L, idx);

	switch (r->r_type) {
	case LUA_TNIL:
		break;
	case LUA_TNUMBER:
		r->r_number = lua_tonumber(L, idx);
		break;
	case LUA_TBOOLEAN:
		r->r_boolean = lua_toboolean(L, idx);
		break;
	case LUA_TSTRING:
		r_string = lua_tolstring(L, idx, &r_string_size);
		r->r_string = kmalloc(r_string_size + 1, GFP_KERNEL);
		if (!r->r_string) {
			if (perror)
				*perror = -ENOMEM;
			break;
		}
		memcpy(r->r_string, r_string, r_string_size);
		r->r_string[r_string_size] = '\0';
		r->r_string_size = r_string_size;
		break;
	case LUA_TUSERDATA:
		r_userdata = lua_touserdata(L, idx);
		r_userdata_size = lua_objlen(L, idx);
		r->r_userdata = kmalloc(r_userdata_size, GFP_KERNEL);
		if (!r->r_userdata) {
			if (perror)
				*perror = -ENOMEM;
			break;
		}
		memcpy(r->r_userdata, r_userdata, r_userdata_size);
		r->r_userdata_size = r_userdata_size;
		if (luaL_getmetafield(L, idx, "__name") != LUA_TNIL) {
			r_userdata_type =
				lua_tolstring(L, -1, &r_userdata_type_size);
			lua_pop(L, 1);
		} else {
			r_userdata_type = "";
		}
		pr_debug("[lunatik] userdata_type = %s\n", r_userdata_type);
		r->r_userdata_type =
			kmalloc(r_userdata_type_size + 1, GFP_KERNEL);
		if (!r->r_userdata_type) {
			kfree(r->r_userdata);
			r->r_userdata = NULL;
			if (perror)
				*perror = -ENOMEM;
			break;
		}
		memcpy(r->r_userdata_type, r_userdata_type,
			r_userdata_type_size);
		r->r_userdata_type[r_userdata_type_size] = '\0';
		break;
	case LUA_TLIGHTUSERDATA:
		r->r_lightuserdata = lua_touserdata(L, idx);
		break;
	default:
		if (perror)
			*perror = -EINVAL;
	}

out:
	return r;
}

static void loadcode_internal(struct lunatik_context *lc,
			struct loadcode_data *d)
{
	lua_State *L;

	pr_debug("[lunatik] executing lua code: (0x%p) %s\n", d->code, d->code);

	lunatik_context_lock(lc);
	L = lc->L;

	d->ret = luaL_loadbuffer(L, d->code, d->sz_code - 1, "loadcode");
	if (d->ret)
		pr_err("[lunatik] luaL_loadbuffer failed\n");
	else {
		d->ret = lua_pcall(L, 0, 1, 0);
		if (d->ret)
			pr_err("[lunatik] lua_pcall failed\n");
	}

	if (d->ret && lua_type(L, -1) == LUA_TSTRING)
		pr_err("[lunatik] %s\n", lua_tostring(L, -1));

	if (d->need_result) {
		d->result = lunatik_result_get(L, -1, &d->ret);
	} else {
#ifdef DEBUG
		struct lunatik_result *r;

		pr_debug("[lunatik] async lua execution done\n");

		r = lunatik_result_get(L, -1, NULL);

		if (r && r->r_type != LUA_TNONE && r->r_type != LUA_TNIL) {
			char *r_type_name;
			switch (r->r_type) {
			case LUA_TBOOLEAN:
				r_type_name = "LUA_TBOOLEAN";
				break;
			case LUA_TLIGHTUSERDATA:
				r_type_name = "LUA_TLIGHTUSERDATA";
				break;
			case LUA_TNUMBER:
				r_type_name = "LUA_TNUMBER";
				break;
			case LUA_TSTRING:
				r_type_name = "LUA_TSTRING";
				break;
			case LUA_TUSERDATA:
				r_type_name = "LUA_TUSERDATA";
				break;
			default:
				r_type_name = "UNKNOWN";
			}
			pr_debug("[lunatik] result of type %s thrown away\n",
				r_type_name);
		}

		lunatik_result_free(r);
#endif
	}

	lua_pop(L, 1);

	lunatik_context_unlock(lc);
}

/**
 * lunatik_loadcode_direct - Lua execution in current context
 * @lc: context whos workqueue is used
 * @code: the Lua code to be executed (kernel memory)
 * @sz_code: size of the code string (including zero byte at the end)
 * @presult: pointer to the variable where the result shall be stored
 */
int lunatik_loadcode_direct(struct lunatik_context *lc, char *code,
			size_t sz_code, struct lunatik_result **presult)
{
	int ret;
	struct loadcode_data *d;

	d = kzalloc(sizeof(*d), GFP_KERNEL);
	if (!d) {
		ret = -ENOMEM;
		goto out;
	}

	d->code = code;
	d->sz_code = sz_code;
	d->ret = -1;
	d->need_result = presult != NULL;

	loadcode_internal(lc, d);

	if (presult)
		*presult = d->result;
	ret = d->ret;

	kfree(d);
out:
	return ret;
}
EXPORT_SYMBOL(lunatik_loadcode_direct);

int lunatik_workqueue_init(struct lunatik_workqueue *lwq, char *name)
{
	lwq->wq = alloc_ordered_workqueue("lunatik/%s", 0, name);

	return lwq->wq ? 0 : -ENOMEM;
}

void lunatik_workqueue_deinit(struct lunatik_workqueue *lwq)
{
	destroy_workqueue(lwq->wq);
}

struct lunatik_work *lunatik_work_create(struct lunatik_context *lc,
				work_func_t work_handler, void *work_data)
{
	struct lunatik_work *lw = kmalloc(sizeof(*lw), GFP_KERNEL);
	if (!lw) {
		lw = ERR_PTR(-ENOMEM);
		goto out;
	}

	lw->lc = lc;
	lw->wq = lc->lwq.wq;
	lw->work_data = work_data;

	INIT_WORK(&lw->work, work_handler);

out:
	return lw;
}

static inline void lunatik_work_destroy(struct lunatik_work *lw)
{
	kfree(lw);
}

static inline bool lunatik_work_queue(struct lunatik_work *lw)
{
	return queue_work(lw->wq, &lw->work);
}

struct lunatik_context *lunatik_context_create(char *name)
{
	int e;
	struct lunatik_context *lc = kzalloc(sizeof(*lc), GFP_KERNEL);

	if (!lc) {
		e = -ENOMEM;
		goto out_error;
	}

	lc->L = lua_open();
	if (!lc->L) {
		e = -ENOMEM;
		goto out_free;
	}

	mutex_init(&lc->mutex);

	INIT_LIST_HEAD(&lc->active_bindings);

	e = lunatik_workqueue_init(&lc->lwq, name);
	if (e)
		goto out_error;

	return lc;

out_free:
	kfree(lc);
out_error:
	return ERR_PTR(e);
}
EXPORT_SYMBOL(lunatik_context_create);

void lunatik_context_destroy(struct lunatik_context *lc)
{
	struct active_binding *ab, *tmp;

	if (!lc || IS_ERR(lc))
		return;

	lunatik_context_lock(lc);

	list_for_each_entry_safe(ab, tmp, &lc->active_bindings, l) {
		module_put(ab->b->owner);
		list_del(&ab->l);
	}

	lua_close(lc->L);
	lc->L = NULL;

	lunatik_context_unlock(lc);

	mutex_destroy(&lc->mutex);

	kfree(lc);
}
EXPORT_SYMBOL(lunatik_context_destroy);

static void loadcode_work_handler(struct work_struct *work)
{
	struct lunatik_work *lw = to_lunatik_work(work);
	struct loadcode_data *d = lw->work_data;

	loadcode_internal(lw->lc, d);

	up(&d->complete);

	if (d->callback)
		d->callback(d->callback_arg, d->ret, d->result);

	if (d->callback_nores)
		d->callback_nores(d->callback_arg, d->ret);

	if (d->callback_work)
		d->callback_work(lw);
}

static void loadcode_work_cleanup(struct lunatik_work *lw)
{
	struct loadcode_data *d = lw->work_data;

	kfree(d->code);
	kfree(d);
	lunatik_work_destroy(lw);
}

/* DEPRECATED - use direct, sync, or async variant instead */
/* will execute and free code asynchronously iff no result is requested */
int lunatik_loadcode(struct lunatik_context *lc, char *code, size_t sz_code,
		struct lunatik_result **presult)
{
	int ret;
	struct lunatik_work *lw;
	struct loadcode_data *d;

	d = kzalloc(sizeof(*d), GFP_KERNEL);
	if (!d) {
		ret = -ENOMEM;
		goto out;
	}

	d->code = code;
	d->sz_code = sz_code;
	d->ret = -1;
	if (presult)
		d->need_result = 1;
	else
		d->callback_work = loadcode_work_cleanup;
	sema_init(&d->complete, 0);

	lw = lunatik_work_create(lc, loadcode_work_handler, d);
	if (!lw) {
		ret = -ENOMEM;
		goto out_free;
	}

	pr_debug("[lunatik_loadcode] going to execute lua code: (0x%p) %s\n",
		d->code, d->code);

	lunatik_work_queue(lw);

	/*
	 * Further processing and cleanup is delegated to job if running
	 * asynchronously.
	 */
	if (!presult) {
		ret = 0;
		goto out;
	}

	/* wait work */
	ret = down_interruptible(&d->complete);
	if (ret)
		goto out_free_work;

	*presult = d->result;
	ret = d->ret;

out_free_work:
	lunatik_work_destroy(lw);
out_free:
	/*
	 * In the synchronous case the caller is expected to free code.
	 * We only free what we have allocated.
	 */
	kfree(d);
out:
	return ret;
}
EXPORT_SYMBOL(lunatik_loadcode);

/**
 * lunatik_loadcode_sync - Lua execution on workqueue with waiting until done
 * @lc: context whos workqueue is used
 * @code: the Lua code to be executed (kernel memory)
 * @sz_code: size of the code string (including zero byte at the end)
 * @presult: pointer to the variable where the result shall be stored
 */
int lunatik_loadcode_sync(struct lunatik_context *lc, char *code,
			size_t sz_code, struct lunatik_result **presult)
{
	int ret;
	struct lunatik_work *lw;
	struct loadcode_data *d;

	d = kzalloc(sizeof(*d), GFP_KERNEL);
	if (!d) {
		ret = -ENOMEM;
		goto out;
	}

	d->code = code;
	d->sz_code = sz_code;
	d->ret = -1;
	if (presult)
		d->need_result = 1;
	sema_init(&d->complete, 0);

	lw = lunatik_work_create(lc, loadcode_work_handler, d);
	if (!lw) {
		ret = -ENOMEM;
		goto out_free;
	}

	pr_debug("[lunatik_loadcode_sync] going to execute lua code: (0x%p) %s\n",
		d->code, d->code);

	lunatik_work_queue(lw);

	/* wait work */
	ret = down_interruptible(&d->complete);
	if (ret)
		goto out_free_work;

	if (presult)
		*presult = d->result;
	ret = d->ret;

out_free_work:
	lunatik_work_destroy(lw);
out_free:
	kfree(d);
out:
	return ret;
}
EXPORT_SYMBOL(lunatik_loadcode_sync);

/* this does not free code - must be done in user callback if need be */
static void loadcode_async_work_cleanup(struct lunatik_work *lw)
{
	struct loadcode_data *d = lw->work_data;

	kfree(d);
	lunatik_work_destroy(lw);
}

/**
 * lunatik_loadcode_async - asynchronous Lua execution
 * @lc: context whos workqueue is used
 * @code: the Lua code to be executed (kernel memory)
 * @sz_code: size of the code string (including zero byte at the end)
 * @callb: address of callback function that will be called when done
 * @callb_arg: general purpose argument supplied to the callback
 *
 * The result is supplied to the callback function.
 *
 * Compared to lunatik_loadcode this does not free code.  Therefore this must
 * be done in the callback function if need be.
 */
int lunatik_loadcode_async(struct lunatik_context *lc, char *code,
			size_t sz_code,	lunatik_loadcode_callback callb,
			void *callb_arg)
{
	int ret;
	struct lunatik_work *lw;
	struct loadcode_data *d;

	d = kzalloc(sizeof(*d), GFP_KERNEL);
	if (!d) {
		ret = -ENOMEM;
		goto out;
	}

	d->code = code;
	d->sz_code = sz_code;
	d->ret = -1;
	d->need_result = 1;
	d->callback = callb;
	d->callback_arg = callb_arg;
	d->callback_work = loadcode_async_work_cleanup;
	sema_init(&d->complete, 0);

	lw = lunatik_work_create(lc, loadcode_work_handler, d);
	if (!lw) {
		ret = -ENOMEM;
		goto out_free;
	}

	pr_debug("[lunatik_loadcode_async] going to execute lua code: (0x%p) %s\n",
		d->code, d->code);

	lunatik_work_queue(lw);

	/*
	 * Further processing and cleanup is delegated to callbacks.
	 */
	ret = 0;
	goto out;

out_free:
	kfree(d);
out:
	return ret;
}
EXPORT_SYMBOL(lunatik_loadcode_async);

/**
 * lunatik_loadcode_async_nores - asynchronous Lua execution (ignore result)
 * @lc: context whos workqueue is used
 * @code: the Lua code to be executed (kernel memory)
 * @sz_code: size of the code string (including zero byte at the end)
 * @callb: address of callback function that will be called when done
 * @callb_arg: general purpose argument supplied to the callback
 *
 * The result of the Lua execution will be silently and efficiently ignored.
 *
 * Compared to lunatik_loadcode this does not free code.  Therefore this must
 * be done in the callback function if need be.
 */
int lunatik_loadcode_async_nores(struct lunatik_context *lc, char *code,
				size_t sz_code,	lunatik_loadcode_callback_nores callb,
				void *callb_arg)
{
	int ret;
	struct lunatik_work *lw;
	struct loadcode_data *d;

	d = kzalloc(sizeof(*d), GFP_KERNEL);
	if (!d) {
		ret = -ENOMEM;
		goto out;
	}

	d->code = code;
	d->sz_code = sz_code;
	d->ret = -1;
	d->need_result = 0;
	d->callback_nores = callb;
	d->callback_arg = callb_arg;
	d->callback_work = loadcode_async_work_cleanup;
	sema_init(&d->complete, 0);

	lw = lunatik_work_create(lc, loadcode_work_handler, d);
	if (!lw) {
		ret = -ENOMEM;
		goto out_free;
	}

	pr_debug("[lunatik_loadcode_async_nores] going to execute lua code: (0x%p) %s\n",
		d->code, d->code);

	lunatik_work_queue(lw);

	/*
	 * Further processing and cleanup is delegated to callbacks.
	 */
	ret = 0;
	goto out;

out_free:
	kfree(d);
out:
	return ret;
}
EXPORT_SYMBOL(lunatik_loadcode_async_nores);

void lunatik_result_free(const struct lunatik_result *result)
{
	if (result) {
		pr_debug("[lunatik] free result\n");
		switch (result->r_type) {
		case LUA_TSTRING:
			kfree(result->r_string);
			break;
		case LUA_TUSERDATA:
			kfree(result->r_userdata);
			kfree(result->r_userdata_type);
		}
		kfree(result);
	}
}
EXPORT_SYMBOL(lunatik_result_free);

inline static void openlib(lua_State *L, lua_CFunction luaopen_func)
{
	lua_pushcfunction(L, luaopen_func);
	lua_pcall(L, 0, 1, 0);
}

static void openlib_work_handler(struct work_struct *work)
{
	struct lunatik_work *lw = to_lunatik_work(work);
	lua_CFunction luaopen_func = lw->work_data;

	lunatik_context_lock(lw->lc);
	openlib(lw->lc->L, luaopen_func);
	lunatik_context_unlock(lw->lc);
}

int lunatik_openlib(struct lunatik_context *lc, lua_CFunction luaopen_func)
{
	struct lunatik_work *lw =
		lunatik_work_create(lc, openlib_work_handler, luaopen_func);

	if (!lw)
		return -ENOMEM;

	lunatik_work_queue(lw);

	return 0;
}
EXPORT_SYMBOL(lunatik_openlib);

LIST_HEAD(lunatik_bindings);

struct lunatik_binding *lunatik_bindings_register(
	struct module *owner, lunatik_binding_func regfunc)
{
	struct lunatik_binding *b = kmalloc(sizeof(*b), GFP_KERNEL);
	if (!b)
		return ERR_PTR(-ENOMEM);

	b->owner = owner;
	b->regfunc = regfunc;

	list_add_tail(&b->link, &lunatik_bindings);

	return b;
}
EXPORT_SYMBOL(lunatik_bindings_register);

void lunatik_bindings_unregister(struct lunatik_binding *b)
{
	list_del(&b->link);
	kfree(b);
}
EXPORT_SYMBOL(lunatik_bindings_unregister);

int lunatik_bindings_load(struct lunatik_context *lc)
{
	int ret = 0;
	struct lunatik_binding *b;

	list_for_each_entry(b, &lunatik_bindings, link) {
		struct active_binding *ab;

		/* skip bindings which are already active */
		list_for_each_entry(ab, &lc->active_bindings, l) {
			if (ab->b == b)
				goto next_binding;
		}

		if (try_module_get(b->owner)) {
			ret = b->regfunc(lc);
			if (ret)
				goto out;

			ab = kmalloc(sizeof(*ab), GFP_KERNEL);
			if (!ab) {
				ret = -ENOMEM;
				goto out;
			}

			ab->b = b;

			list_add_tail(&ab->l, &lc->active_bindings);
		}

	next_binding:
		;
	}

out:
	return ret;
}
EXPORT_SYMBOL(lunatik_bindings_load);

static struct lunatik_context *lunatik_default_context;

struct lunatik_context *lunatik_default_context_get(void)
{
	int ret;

	/*
	 * At module init time (of lunatik_core.ko) no bindings will be known.
	 * So we are forced to load them into the default context at a later
	 * point in time.  Also we want bindings from modules loaded much
	 * later to be available, too.  ('load' only loads bindings which are
	 * not active already.)
	 */
	ret = lunatik_bindings_load(lunatik_default_context);

	/* TODO implement reference counting (incl. module ref count) */

	return ret ? ERR_PTR(ret) : lunatik_default_context;
}
EXPORT_SYMBOL(lunatik_default_context_get);

static int __init lunatik_default_context_init(void)
{
	lunatik_default_context = lunatik_context_create("default");

	if (IS_ERR(lunatik_default_context))
		return PTR_ERR(lunatik_default_context);
	else
		return 0;
}

static int __init lunatik_init(void)
{
	int ret;

	ret = lunatik_default_context_init();
	if (ret)
		goto out;

	pr_info("Lunatik init done\n");
out:
	return ret;
}

static void __exit lunatik_exit(void)
{
	lunatik_context_destroy(lunatik_default_context);
}

MODULE_AUTHOR("Lourival Vieira Neto <lneto@inf.puc-rio.br>, Matthias Grawinkel <grawinkel@uni-mainz.de>, Daniel Bausch <bausch@dvs.tu-darmstadt.de>");
MODULE_LICENSE("Dual MIT/GPL");

module_init(lunatik_init);
module_exit(lunatik_exit);
