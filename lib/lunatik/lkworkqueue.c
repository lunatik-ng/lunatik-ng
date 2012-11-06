/* 
 * lkworkqueue.c 
 * Lunatik Workqueue
 * See Copyright Notice in lunatik.h
 */

#include "lkworkqueue.h"
#include <linux/kernel.h>

/* static data */

struct lunatikW_workqueue_struct{
	lua_State *                     L;
	struct workqueue_struct *       wq;
};

static struct lunatikW_workqueue_struct lunatikd;

/* exported code */

struct lunatikW_work_struct * lunatikW_new_work(work_func_t work_handler, void * work_data)
{
	struct lunatikW_work_struct * lunatik_work =
		(struct lunatikW_work_struct *) kmalloc(sizeof(struct lunatikW_work_struct), GFP_KERNEL);
	if (lunatik_work == NULL)
		return NULL;

	lunatik_work->L			= lunatikd.L;
	lunatik_work->wq		= lunatikd.wq;
	lunatik_work->work_handler	= work_handler;
	lunatik_work->work_data		= work_data;

	INIT_WORK(& lunatik_work->work, lunatik_work->work_handler);

	return lunatik_work;
} /* end lunatikW_new_work */

void __init lunatikW_init(lua_State * L)
{
	lunatikd.wq	= create_workqueue("lunatikd");
	lunatikd.L	= L;

	return;
} /* end lunatikW_init */

/* end lkworkqueue.c */

