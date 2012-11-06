/* 
 * lkworkqueue.h 
 * Lunatik Workqueue
 * See Copyright Notice in lunatik.h
 */

#include "lua/lua.h"
#include <linux/workqueue.h>
#include <linux/slab.h>

#define lunatikW_queue_work(lunatik_work)	queue_work(lunatik_work->wq, & lunatik_work->work)

#define lunatikW_container_of(work)		container_of(work, struct lunatikW_work_struct, work)

#define lunatikW_delete_work(lunatik_work)	kfree(lunatik_work)

struct lunatikW_work_struct{
	lua_State *                     L;
	struct workqueue_struct *       wq;
	struct work_struct 		work;
	work_func_t 			work_handler;
	void *				work_data;
};


struct lunatikW_work_struct * lunatikW_new_work(work_func_t work_handler, void * work_data);

void __init lunatikW_init(lua_State * L);

/* end lkworkqueue.h */

