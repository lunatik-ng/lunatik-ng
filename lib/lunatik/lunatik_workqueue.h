#ifndef _LUNATIK_WORKQUEUE_H
#define _LUNATIK_WORKQUEUE_H

/*
 * lunatik_workqueue.h
 * Lunatik Workqueue
 * See Copyright Notice in lunatik.h
 */

#include <linux/workqueue.h>
#include <linux/slab.h>

#include "lua.h"

struct lunatik_work_struct {
	lua_State *L;
	struct workqueue_struct *wq;
	struct work_struct work;
	work_func_t work_handler;
	void *work_data;
};
#define to_lunatik_work_struct(work) \
	container_of(work, struct lunatik_work_struct, work)

struct lunatik_work_struct *lunatik_new_work(work_func_t work_handler,
					void *work_data);

#define lunatik_queue_work(lunatik_work) \
	queue_work(lunatik_work->wq, &lunatik_work->work)

#define lunatik_delete_work(lunatik_work) kfree(lunatik_work)

void __init lunatik_workqueue_init(lua_State *L);

#endif /* _LUNATIK_WORKQUEUE_H */
