/*
 * lunatik_workqueue.c
 * Lunatik Workqueue
 * See Copyright Notice in lunatik.h
 */

#include <linux/kernel.h>

#include "lunatik_workqueue.h"

struct lunatik_workqueue_struct {
	lua_State *L;
	struct workqueue_struct *wq;
};

static struct lunatik_workqueue_struct lunatik_workqueue;

struct lunatik_work_struct *lunatik_new_work(work_func_t work_handler,
					void *work_data)
{
	struct lunatik_work_struct *lunatik_work =
		kmalloc(sizeof(*lunatik_work), GFP_KERNEL);
	if (lunatik_work == NULL)
		return NULL;

	lunatik_work->L = lunatik_workqueue.L;
	lunatik_work->wq = lunatik_workqueue.wq;
	lunatik_work->work_handler = work_handler;
	lunatik_work->work_data = work_data;

	INIT_WORK(&lunatik_work->work, lunatik_work->work_handler);

	return lunatik_work;
}

void __init lunatik_workqueue_init(lua_State *L)
{
	lunatik_workqueue.wq = create_workqueue("lunatik");
	lunatik_workqueue.L = L;

	return;
}
