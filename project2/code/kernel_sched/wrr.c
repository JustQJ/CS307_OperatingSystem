
/*********************************************************************

This program is the main file we realise the wrr scheduler and defined all the needed functions.

the main function we defined as follow


init_wrr_rq();  //used to initialize the wrr runqueue, including all members , especially the wrr queue 


update_curr_wrr(); //used to updata the running time of current process when it is the wrr policy , if it exceeds the limit time, resched it.


__dequeue_wrr_entity(); //used to remove the task from the the queue

__enqueue_wrr_entity(); //add  the task to the wrr_queue

requeue_wrr_entity(); //add the task to the wrr_queue when it give up the cup initiatively.

pick_next_task_wrr();  //used to pick the task from the queue

put_prev_task_wrr();   //put the preview task

set_curr_task_wrr(); //updata the start time of the process

task_tick_wrr();     //used to updata the state of current task and set the timeslice for the process when it used up its time  

**********************************************************************/



#include "sched.h"
#include <linux/slab.h>

void init_wrr_rq(struct wrr_rq *wrr_rq, struct rq *rq)
{
	struct head_list *queue=&wrr_rq->queue_wrr;
	

	INIT_LIST_HEAD(queue);

#if defined CONFIG_SMP
	wrr_rq->wrr_nr_migratory = 0;
	wrr_rq->overloaded = 0;
	plist_head_init(&wrr_rq->pushable_tasks);
#endif
	wrr_rq->wrr_nr_running = 0;
	wrr_rq->wrr_time = 0;
	wrr_rq->wrr_throttled = 0;
	wrr_rq->wrr_runtime = 0;
	raw_spin_lock_init(&wrr_rq->wrr_runtime_lock);
}


#ifdef CONFIG_WRR_GROUP_SCHED

#define wrr_entity_is_task(wrr_se) (!(wrr_se)->my_q)   // in function *wrr_task_of

static inline struct task_struct *wrr_task_of(struct sched_wrr_entity *wrr_se)
{
#ifdef CONFIG_SCHED_DEBUG
	WARN_ON_ONCE(!wrr_entity_is_task(wrr_se));
#endif
	return container_of(wrr_se, struct task_struct, wrr);
}

static inline struct rq *rq_of_wrr_rq(struct wrr_rq *wrr_rq)
{
	return wrr_rq->rq;
}

static inline struct wrr_rq *wrr_rq_of_se(struct sched_wrr_entity *wrr_se)
{
	return wrr_se->wrr_rq;
}


void free_wrr_sched_group(struct task_group *tg)  //
{


	int i;

	

	for_each_possible_cpu(i) {
		if (tg->wrr_rq)
			kfree(tg->wrr_rq[i]);
		if (tg->wrr_se)
			kfree(tg->wrr_se[i]);
	}

	kfree(tg->wrr_rq);
	kfree(tg->wrr_se);
}


void init_tg_wrr_entry(struct task_group *tg, struct wrr_rq *wrr_rq,
		struct sched_wrr_entity *wrr_se, int cpu,
		struct sched_wrr_entity *parent)
{
	struct rq *rq = cpu_rq(cpu);

	
	wrr_rq->wrr_nr_boosted = 0;
	wrr_rq->rq = rq;
	wrr_rq->tg = tg;

	tg->wrr_rq[cpu] = wrr_rq;
	tg->wrr_se[cpu] = wrr_se;

	if (!wrr_se)
		return;

	if (!parent)
		wrr_se->wrr_rq = &rq->wrr;
	else
		wrr_se->wrr_rq = parent->my_q;

	wrr_se->my_q = wrr_rq;
	wrr_se->parent = parent;
	INIT_LIST_HEAD(&wrr_se->run_list);
}


int alloc_wrr_sched_group(struct task_group *tg, struct task_group *parent) // 

{
	struct wrr_rq *wrr_rq;
	struct sched_wrr_entity *wrr_se;
	int i;

	tg->wrr_rq = kzalloc(sizeof(wrr_rq) * nr_cpu_ids, GFP_KERNEL);
	if (!tg->wrr_rq)
		goto err;
	tg->wrr_se = kzalloc(sizeof(wrr_se) * nr_cpu_ids, GFP_KERNEL);
	if (!tg->wrr_se)
		goto err;

	//init_wrr_bandwidth(&tg->wrr_bandwidth,
			//ktime_to_ns(def_wrr_bandwidth.wrr_period), 0);

	for_each_possible_cpu(i) {
		wrr_rq = kzalloc_node(sizeof(struct wrr_rq),
				     GFP_KERNEL, cpu_to_node(i));
		if (!wrr_rq)
			goto err;

		wrr_se = kzalloc_node(sizeof(struct sched_wrr_entity),
				     GFP_KERNEL, cpu_to_node(i));
		if (!wrr_se)
			goto err_free_rq;

		init_wrr_rq(wrr_rq, cpu_rq(i));
		wrr_rq->wrr_runtime = tg->wrr_bandwidth.wrr_runtime;
		init_tg_wrr_entry(tg, wrr_rq, wrr_se, i, parent->wrr_se[i]);
	}

	return 1;

err_free_rq:
	kfree(wrr_rq);
err:
	return 0;
}


#else /* CONFIG_WRR_GROUP_SCHED */
#define rt_entity_is_task(rt_se) (1)

static inline struct task_struct *wrr_task_of(struct sched_wrr_entity *wrr_se)
{
	return container_of(wrr_se, struct task_struct, wrr);
}

static inline struct rq *rq_of_wrr_rq(struct wrr_rq *wrr_rq)
{
	return container_of(wrr_rq, struct rq, wrr);
}

static inline struct wrr_rq *wrr_rq_of_se(struct sched_wrr_entity *wrr_se)
{
	struct task_struct *p = wrr_task_of(wrr_se);
	struct rq *rq = task_rq(p);

	return &rq->wrr;
}

void free_wrr_sched_group(struct task_group *tg) { }

int alloc_wrr_sched_group(struct task_group *tg, struct task_group *parent)
{
	return 1;
}

#endif /* CONFIG_WRR_GROUP_SCHED */


#ifdef CONFIG_SMP

static inline int wrr_overloaded(struct rq *rq)
{
	return atomic_read(&rq->rd->wrro_count);
}



static inline void wrr_set_overload(struct rq *rq)
{
	if (!rq->online)
		return;

	cpumask_set_cpu(rq->cpu, rq->rd->wrro_mask);
	/*
	 * Make sure the mask is visible before we set
	 * the overload count. That is checked to determine
	 * if we should look at the mask. It would be a shame
	 * if we looked at the mask, but the mask was not
	 * updated yet.
	 */
	wmb();
	atomic_inc(&rq->rd->wrro_count);
}




static inline void wrr_clear_overload(struct rq *rq)
{
	if (!rq->online)
		return;

	/* the order here really doesn't matter */
	atomic_dec(&rq->rd->wrro_count);  //  wrro ?
	cpumask_clear_cpu(rq->cpu, rq->rd->wrro_mask);
}

static void update_wrr_migration(struct wrr_rq *wrr_rq)
{
	if (wrr_rq->wrr_nr_migratory && wrr_rq->wrr_nr_total > 1) {
		if (!wrr_rq->overloaded) {
			wrr_set_overload(rq_of_wrr_rq(wrr_rq));  //return the rq of the task
			wrr_rq->overloaded = 1;
		}
	} else if (wrr_rq->overloaded) {
		wrr_clear_overload(rq_of_wrr_rq(wrr_rq));
		wrr_rq->overloaded = 0;
	}
}



static void inc_wrr_migration(struct sched_wrr_entity *wrr_se, struct wrr_rq *wrr_rq)
{
	if (!wrr_entity_is_task(wrr_se)) //wrr_entity_is_task(wrr_se) -> !(wrr_se)->my_q
		return;

	wrr_rq = &rq_of_wrr_rq(wrr_rq)->wrr;

	wrr_rq->wrr_nr_total++;
	if (wrr_se->nr_cpus_allowed > 1)
		wrr_rq->wrr_nr_migratory++;

	update_wrr_migration(wrr_rq);
}


static void dec_wrr_migration(struct sched_wrr_entity *wrr_se, struct wrr_rq *wrr_rq)
{
	if (!wrr_entity_is_task(wrr_se))  //wrr_entity_is_task(wrr_se) -> !(wrr_se)->my_q
		return;

	wrr_rq = &rq_of_wrr_rq(wrr_rq)->wrr;  //get the pointer

	wrr_rq->wrr_nr_total--;                   
	if (wrr_se->nr_cpus_allowed > 1)   //the number of cpu this task can run
		wrr_rq->wrr_nr_migratory--;   //-1

	update_wrr_migration(wrr_rq);
}

static inline int has_pushable_tasks(struct rq *rq)
{
	return !plist_head_empty(&rq->wrr.pushable_tasks);
}

static void enqueue_pushable_task(struct rq *rq, struct task_struct *p)
{
	plist_del(&p->pushable_tasks, &rq->wrr.pushable_tasks);
	plist_add(&p->pushable_tasks, &rq->wrr.pushable_tasks);


}

static void dequeue_pushable_task(struct rq *rq, struct task_struct *p)
{
	plist_del(&p->pushable_tasks, &rq->wrr.pushable_tasks);

}

#else  /* NO CONFIG_SMP*/


static inline void enqueue_pushable_task(struct rq *rq, struct task_struct *p)
{
}

static inline void dequeue_pushable_task(struct rq *rq, struct task_struct *p)
{
}

static inline
void inc_wrr_migration(struct sched_wrr_entity *wrr_se, struct wrr_rq *wrr_rq)
{
}

static inline
void dec_wrr_migration(struct sched_wrr_entity *wrr_se, struct wrr_rq *wrr_rq)
{
}

#endif /* CONFIG_SMP */

static inline int on_wrr_rq(struct sched_wrr_entity *wrr_se)  //judge the list is empty or not?
{
	return !list_empty(&wrr_se->run_list);
}

#ifdef CONFIG_WRR_GROUP_SCHED


static inline u64 sched_wrr_runtime(struct wrr_rq *wrr_rq)
{
	if (!wrr_rq->tg)
		return RUNTIME_INF;

	return wrr_rq->wrr_runtime;
}

typedef struct task_group *wrr_rq_iter_t;

static inline struct task_group *next_task_group(struct task_group *tg)
{
	do {
		tg = list_entry_rcu(tg->list.next,
			typeof(struct task_group), list);
	} while (&tg->list != &task_groups && task_group_is_autogroup(tg));

	if (&tg->list == &task_groups)
		tg = NULL;

	return tg;
}

#define for_each_wrr_rq(wrr_rq, iter, rq)					\
	for (iter = container_of(&task_groups, typeof(*iter), list);	\
		(iter = next_task_group(iter)) &&			\
		(wrr_rq = iter->wrr_rq[cpu_of(rq)]);)


static inline void list_add_leaf_wrr_rq(struct wrr_rq *wrr_rq)
{
	list_add_rcu(&wrr_rq->leaf_wrr_rq_list,
			&rq_of_wrr_rq(wrr_rq)->leaf_wrr_rq_list);
}


static inline void list_del_leaf_wrr_rq(struct wrr_rq *wrr_rq)
{
	list_del_rcu(&wrr_rq->leaf_wrr_rq_list);
}

#define for_each_leaf_wrr_rq(wrr_rq, rq) \
	list_for_each_entry_rcu(wrr_rq, &rq->leaf_wrr_rq_list, leaf_wrr_rq_list)

#define for_each_sched_wrr_entity(wrr_se) \
	for (; wrr_se; wrr_se = wrr_se->parent)   //the group entity 


static inline struct wrr_rq *group_wrr_rq(struct sched_wrr_entity *wrr_se)
{
	return wrr_se->my_q;
}


static void enqueue_wrr_entity(struct sched_wrr_entity *wrr_se, bool head);

static void dequeue_wrr_entity(struct sched_wrr_entity *wrr_se);



static void sched_wrr_rq_enqueue(struct wrr_rq *wrr_rq)
{
	struct task_struct *curr = rq_of_wrr_rq(wrr_rq)->curr;
	struct sched_wrr_entity *wrr_se;

	int cpu = cpu_of(rq_of_wrr_rq(wrr_rq));

	wrr_se = wrr_rq->tg->wrr_se[cpu];

	if (wrr_rq->wrr_nr_running) {
		if (wrr_se && !on_wrr_rq(wrr_se))
			enqueue_wrr_entity(wrr_se, false);
	}
}


static void sched_wrr_rq_dequeue(struct wrr_rq *wrr_rq)
{
	struct sched_wrr_entity *wrr_se;
	int cpu = cpu_of(rq_of_wrr_rq(wrr_rq));

	wrr_se = wrr_rq->tg->wrr_se[cpu];

	if (wrr_se && on_wrr_rq(wrr_se))
		dequeue_wrr_entity(wrr_se);
}



static inline int wrr_rq_throttled(struct wrr_rq *wrr_rq)  
{
	return wrr_rq->wrr_throttled && !wrr_rq->wrr_nr_boosted;
}



static int wrr_se_boosted(struct sched_wrr_entity *wrr_se)
{
	struct wrr_rq *wrr_rq = group_wrr_rq(wrr_se);
	struct task_struct *p;

	if (wrr_rq)
		return !!wrr_rq->wrr_nr_boosted;

	p =wrr_task_of(wrr_se);
	return p->prio != p->normal_prio;
}






#else /* !CONFIG_WRR_GROUP_SCHED */



static inline u64 sched_wrr_runtime(struct wrr_rq *wrr_rq)
{
	return wrr_rq->wrr_runtime;
}


typedef struct wrr_rq *wrr_rq_iter_t;


#define for_each_wrr_rq(wrr_rq, iter, rq) \
	for ((void) iter, wrr_rq = &rq->wrr; wrr_rq; wrr_rq = NULL)

static inline void list_add_leaf_wrr_rq(struct wrr_rq *wrr_rq)
{
}

static inline void list_del_leaf_wrr_rq(struct wrr_rq *wrr_rq)
{
}

#define for_each_leaf_wrr_rq(wrr_rq, rq) \
	for (wrr_rq = &rq->wrr; wrr_rq; wrr_rq = NULL)

#define for_each_sched_wrr_entity(wrr_se) \
	for (; wrr_se; wrr_se = NULL)

static inline struct wrr_rq *group_wrr_rq(struct sched_wrr_entity *wrr_se)
{
	return NULL;
}

static inline void sched_wrr_rq_enqueue(struct wrr_rq *wrr_rq)
{
	if (wrr_rq->wrr_nr_running)
		resched_task(rq_of_wrr_rq(wrr_rq)->curr);
}

static inline void sched_wrr_rq_dequeue(struct wrr_rq *wrr_rq)
{
}

static inline int wrr_rq_throttled(struct wrr_rq *wrr_rq)
{
	return wrr_rq->wrr_throttled;
}
/*  
static inline const struct cpumask *sched_wrr_period_mask(void)
{
	return cpu_online_mask;
}

static inline
struct wrr_rq *sched_wrr_period_wrr_rq(struct wrr_bandwidth *wrr_b, int cpu)
{
	return &cpu_rq(cpu)->wrr;
}

static inline struct wrr_bandwidth *sched_wrr_bandwidth(struct wrr_rq *wrr_rq)
{
	return &def_wrr_bandwidth;
}
*/
#endif /* CONFIG_WRR_GROUP_SCHED */





static int sched_wrr_runtime_exceeded(struct wrr_rq *wrr_rq)
{
	u64 runtime = sched_wrr_runtime(wrr_rq);

	if (wrr_rq->wrr_throttled)
		return wrr_rq_throttled(wrr_rq);

	//if (runtime >= sched_wrr_period(wrr_rq))
		//return 0;

	//balance_runtime(wrr_rq);
	//runtime = sched_wrr_runtime(wrr_rq);
	if (runtime == RUNTIME_INF)
		return 0;

	if (wrr_rq->wrr_time > runtime) {
	//	struct wrr_bandwidth *wrr_b = sched_wrr_bandwidth(wrr_rq);

		/*
		 * Don't actually throttle groups that have no runtime assigned
		 * but accrue some time due to boosting.
		 */
		/*if (likely(wrr_b->wrr_runtime)) {
			static bool once = false;

			wrr_rq->wrr_throttled = 1;

			if (!once) {
				once = true;
				printk_sched("sched: wrr throttling activated\n");
			}
		} else {
			/*
			 * In case we did anyway, make it go away,
			 * replenishment is a joke, since it will replenish us
			 * with exactly 0 ns.
			 */
			//wrr_rq->wrr_time = 0;
		//}

		if (wrr_rq_throttled(wrr_rq)) {
			sched_wrr_rq_dequeue(wrr_rq);
			return 1;
		}
	}

	return 0;
}


static void update_curr_wrr(struct rq *rq)
{
	struct task_struct *curr = rq->curr;  //current task
	struct sched_wrr_entity *wrr_se = &curr->wrr;  //get the sentity
	struct wrr_rq *wrr_rq = wrr_rq_of_se(wrr_se);
	u64 delta_exec;

	if (curr->sched_class != &wrr_sched_class)  //if it is our class
		return;  

	delta_exec = rq->clock_task - curr->se.exec_start;   //get the runtime difference
	if (unlikely((s64)delta_exec < 0))             
		delta_exec = 0;

	schedstat_set(curr->se.statistics.exec_max,
		      max(curr->se.statistics.exec_max, delta_exec));

	curr->se.sum_exec_runtime += delta_exec;       //update the runtime
	account_group_exec_runtime(curr, delta_exec);   

	curr->se.exec_start = rq->clock_task;   //set the start time
	cpuacct_charge(curr, delta_exec);

	sched_wrr_avg_update(rq, delta_exec);  //update the average time 

	

	for_each_sched_wrr_entity(wrr_se) {   
		wrr_rq = wrr_rq_of_se(wrr_se);

		if (sched_wrr_runtime(wrr_rq) != RUNTIME_INF) {  //judge whether 
			raw_spin_lock(&wrr_rq->wrr_runtime_lock);
			wrr_rq->wrr_time += delta_exec;
			if (sched_wrr_runtime_exceeded(wrr_rq))
				resched_task(curr);
			raw_spin_unlock(&wrr_rq->wrr_runtime_lock);
			
		}
	}
}

#ifdef CONFIG_WRR_GROUP_SCHED

static void
inc_wrr_group(struct sched_wrr_entity *wrr_se, struct wrr_rq *wrr_rq)
{
	if (wrr_se_boosted(wrr_se))
		wrr_rq->wrr_nr_boosted++;

	//if (wrr_rq->tg)
	//	stawrr_wrr_bandwidth(&wrr_rq->tg->wrr_bandwidth);
}


static void
dec_wrr_group(struct sched_wrr_entity *wrr_se, struct wrr_rq *wrr_rq)
{
	if (wrr_se_boosted(wrr_se))
		wrr_rq->wrr_nr_boosted--;

	WARN_ON(!wrr_rq->wrr_nr_running && wrr_rq->wrr_nr_boosted);
}


#else /* CONFIG_RT_GROUP_SCHED */



static void
inc_wrr_group(struct sched_wrr_entity *wrr_se, struct wrr_rq *wrr_rq)
{
}


static void
dec_wrr_group(struct sched_wrr_entity *wrr_se, struct wrr_rq *wrr_rq)
{
}

#endif /* CONFIG_RT_GROUP_SCHED */



static inline
void inc_wrr_tasks(struct sched_wrr_entity *wrr_se, struct wrr_rq *wrr_rq)
{
	

	
	wrr_rq->wrr_nr_running++;  //the task ++

	inc_wrr_migration(wrr_se, wrr_rq);  //  
	inc_wrr_group(wrr_se, wrr_rq);   //                  now need doing 
}


static inline
void dec_wrr_tasks(struct sched_wrr_entity *wrr_se, struct wrr_rq *wrr_rq)
{

	WARN_ON(!wrr_rq->wrr_nr_running);
	wrr_rq->wrr_nr_running--;             //the number of the entity minus 1

	dec_wrr_migration(wrr_se, wrr_rq);    //the number of the migration -1
	dec_wrr_group(wrr_se, wrr_rq);       //
}


static void __dequeue_wrr_entity(struct sched_wrr_entity *wrr_se)
{
	struct wrr_rq *wrr_rq = wrr_rq_of_se(wrr_se);  //get the wrr_rq pointer from the wrr_se
	struct list_head *head  = &wrr_rq->queue_wrr;  //get the head of the list queue 

	list_del_init(&wrr_se->run_list);  //get the pointer of the task

	dec_wrr_tasks(wrr_se, wrr_rq);
	if (!wrr_rq->wrr_nr_running)
		list_del_leaf_wrr_rq(wrr_rq);  //delete from the group
}


static void __enqueue_wrr_entity(struct sched_wrr_entity *wrr_se, bool head)
{
	struct wrr_rq *wrr_rq = wrr_rq_of_se(wrr_se);    //get the rq pointer
	struct list_head *queue=&wrr_rq->queue_wrr;
	struct wrr_rq *group_rq = group_wrr_rq(wrr_se);
	struct task_struct *p=wrr_task_of(wrr_se);

	/*
	 * Don't enqueue the group if its throttled, or when empty.
	 * The latter is a consequence of the former when a child group
	 * get throttled and the current group doesn't have any other
	 * active members.
	 */
	if (group_rq && (wrr_rq_throttled(group_rq) || !group_rq->wrr_nr_running))
		return;

	if (!wrr_rq->wrr_nr_running) //add the wrr_rq to the rq 
		list_add_leaf_wrr_rq(wrr_rq);
		
	//give the new running time	
	struct task_group *taskgroup = p->sched_task_group;
      char group_path[1024];
     if (!(autogroup_path(taskgroup, group_path, 1024)))
    {
        if (!taskgroup->css.cgroup) {
            group_path[0] = '\0';
        }
        cgroup_path(taskgroup->css.cgroup, group_path, 1024);
    }

    if(group_path[1]=='b')
    {
        p->wrr.time_slice = WRRB_TIMESLICE;
    
        printk("Set the timeslice for backgroup\n");
    }
    else if(group_path[1]!='b')
    {
        p->wrr.time_slice = WRRF_TIMESLICE;
        printk("Set the timeslice for foregroup\n");
    }
	


	if (head)
		list_add(&wrr_se->run_list, queue);     //add the head
	else
		list_add_tail(&wrr_se->run_list, queue);   //add the tail 

	inc_wrr_tasks(wrr_se, wrr_rq);
}


static void dequeue_wrr_stack(struct sched_wrr_entity *wrr_se)
{
	struct sched_wrr_entity *back = NULL;

	for_each_sched_wrr_entity(wrr_se) {  //reverse the pointer 
		wrr_se->back = back;
		back = wrr_se;
	}

	for (wrr_se = back; wrr_se; wrr_se = wrr_se->back) {  //along the list 
		if (on_wrr_rq(wrr_se))   //if is not empty
			__dequeue_wrr_entity(wrr_se);
	}
}



static void enqueue_wrr_entity(struct sched_wrr_entity *wrr_se, bool head)   //enqueue the task
{
	dequeue_wrr_stack(wrr_se);       //remove the task from the group that 
	for_each_sched_wrr_entity(wrr_se)
		__enqueue_wrr_entity(wrr_se, head);  //head is used to judge the position ,tail or head
}


static void dequeue_wrr_entity(struct sched_wrr_entity *wrr_se)
{
	dequeue_wrr_stack(wrr_se);

	for_each_sched_wrr_entity(wrr_se) {  
		struct wrr_rq *wrr_rq = group_wrr_rq(wrr_se);

		if (wrr_rq && wrr_rq->wrr_nr_running)   //add the group to the wrr_rq
			__enqueue_wrr_entity(wrr_se, false);
	}
}


/*
 * Put task to the head or the end of the run list without the overhead of
 * dequeue followed by enqueue.
 */
static void
requeue_wrr_entity(struct wrr_rq *wrr_rq, struct sched_wrr_entity *wrr_se, int head)
{
	if (on_wrr_rq(wrr_se)) {
		
		struct list_head *queue = &wrr_rq->queue_wrr;

		if (head)
			list_move(&wrr_se->run_list, queue);
		else
			list_move_tail(&wrr_se->run_list, queue);
	}
}


static void requeue_task_wrr(struct rq *rq, struct task_struct *p, int head)
{
	struct sched_wrr_entity *wrr_se = &p->wrr;
	struct wrr_rq *wrr_rq;

	for_each_sched_wrr_entity(wrr_se) {  //deal the group , when there is a group of the entity
		wrr_rq =wrr_rq_of_se(wrr_se);
		requeue_wrr_entity(wrr_rq, wrr_se, head);
	}
}






//Adding/removing a task to /from a priority arry

static void enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_wrr_entity *wrr_se = &p->wrr;   //get the entity
	if(flags & ENQUEUE_WAKEUP)
		wrr_se->timeout=0;
	
	enqueue_wrr_entity(wrr_se, flags & ENQUEUE_HEAD);    //need to judge the position of  the task need to be added , tail or head ? 

	if(!task_current(rq,p)&&p->wrr.nr_cpus_allowed>1)
		enqueue_pushable_task(rq,p);
	
	inc_nr_running(rq);  //increase the number of the tasks

}

static void dequeue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_wrr_entity *wrr_se = &p->wrr;  //get the wrr_se

	update_curr_wrr(rq);        //update the static      
	dequeue_wrr_entity(wrr_se);    //dequeue

	dequeue_pushable_task(rq, p);  //dequeue from the pushable task
 
	dec_nr_running(rq);   //the number in the rq --
}

static void yield_task_wrr(struct rq *rq)  //put the task to the head or the end of the wrr_rq  
{
	requeue_task_wrr(rq, rq->curr, 0);
}


static void check_preempt_curr_wrr(struct rq *rq, struct task_struct *p, int flags)
{
	if (p->prio < rq->curr->prio) {
		resched_task(rq->curr);
		return;
	}

#ifdef CONFIG_SMP
	/*
	 * If:
	 *
	 * - the newly woken task is of equal priority to the current task
	 * - the newly woken task is non-migratable while current is migratable
	 * - current will be preempted on the next reschedule
	 *
	 * we should check to see if current can readily move to a different
	 * cpu.  If so, we will reschedule to allow the push logic to try
	 * to move current somewhere else, making room for our non-miwrrgratable
	 * task.
	 */
	if (p->prio == rq->curr->prio && !test_tsk_need_resched(rq->curr))
		check_preempt_equal_prio(rq, p);
#endif
}


static struct sched_wrr_entity *pick_next_wrr_entity(struct rq *rq,
						   struct wrr_rq *wrr_rq)
{
	
	struct sched_wrr_entity *next = NULL;
	struct list_head *queue=&wrr_rq->queue_wrr;  //get the head of the group

	

	next = list_entry(queue->next, struct sched_wrr_entity, run_list);

	return next;
}

static struct task_struct *_pick_next_task_wrr(struct rq *rq)
{
	struct sched_wrr_entity *wrr_se;
	struct task_struct *p;
	struct wrr_rq *wrr_rq;

	wrr_rq = &rq->wrr;

	if (!wrr_rq->wrr_nr_running)  //no tasks in the group
		return NULL;

	if (wrr_rq_throttled(wrr_rq))  //
		return NULL;

	do {
		wrr_se = pick_next_wrr_entity(rq, wrr_rq);
		BUG_ON(!wrr_se);
		wrr_rq = group_wrr_rq(wrr_se);  //the my_group
	} while (wrr_rq);

	p = wrr_task_of(wrr_se);  //find the task_struct 
	p->se.exec_start= rq->clock_task;  //set the start time

	return p;
}

static struct task_struct *pick_next_task_wrr(struct rq *rq)
{
	struct task_struct *p = _pick_next_task_wrr(rq);

	/* The running task is never eligible for pushing */
	if (p)
		dequeue_pushable_task(rq, p);

#ifdef CONFIG_SMP
	/*
	 * We detect this state here so that we can avoid taking the RQ
	 * lock again later if there is no need to push
	 */
	rq->post_schedule = has_pushable_tasks(rq);
#endif

	return p;
}

static void put_prev_task_wrr(struct rq *rq, struct task_struct *p)
{
	update_curr_wrr(rq);  //update the static 

	/*
	 * The previous task needs to be made eligible for pushing
	 * if it is still active
	 */
	if (on_wrr_rq(&p->wrr) && p->wrr.nr_cpus_allowed > 1)
		enqueue_pushable_task(rq, p);    //can be push when it come from the 
}

#ifdef CONFIG_SMP
static void set_cpus_allowed_wrr(struct task_struct *p,
				const struct cpumask *new_mask)
{
	int weight = cpumask_weight(new_mask);

	//BUG_ON(!wrr_task(p));  // 

	/*
	 * Update the migration status of the RQ if we have an wrr task
	 * which is running AND changing its weight value.
	 */
	if (p->on_rq && (weight != p->wrr.nr_cpus_allowed)) {
		struct rq *rq = task_rq(p);

		if (!task_current(rq, p)) {
			/*
			 * Make sure we dequeue this task from the pushable list
			 * before going fuwrrher.  It will either remain off of
			 * the list because we are no longer pushable, or it
			 * will be requeued.
			 */
			if (p->wrr.nr_cpus_allowed > 1)
				dequeue_pushable_task(rq, p);

			/*
			 * Requeue if our weight is changing and still > 1
			 */
			if (weight > 1)
				enqueue_pushable_task(rq, p);

		}

		if ((p->wrr.nr_cpus_allowed <= 1) && (weight > 1)) {
			rq->wrr.wrr_nr_migratory++;
		} else if ((p->wrr.nr_cpus_allowed > 1) && (weight <= 1)) {
			BUG_ON(!rq->wrr.wrr_nr_migratory);
			rq->wrr.wrr_nr_migratory--;
		}

		update_wrr_migration(&rq->wrr);
	}
}







static void select_task_rq_wrr(struct rq *rq ,struct task_struct *p)
{
}
static void rq_online_wrr(struct rq *rq ,struct task_struct *p)
{
}
static void rq_offline_wrr(struct rq *rq ,struct task_struct *p)
{
}
static void pre_schedule_wrr(struct rq *rq ,struct task_struct *p)
{
}
static void post_schedule_wrr(struct rq *rq ,struct task_struct *p)
{
}
static void task_woken_wrr(struct rq *rq ,struct task_struct *p)
{
}
#endif


static void task_fork_wrr(struct rq *rq ,struct task_struct *p)
{
}

static void switch_from_wrr(struct rq *rq ,struct task_struct *p)
{
}
static void switched_to_wrr(struct rq *rq ,struct task_struct *p)
{
}

static void prio_changed_wrr(struct rq *rq ,struct task_struct *p)
{
}



static void set_curr_task_wrr(struct rq *rq)  //set the time of current task
{
	struct task_struct *p = rq->curr;

	p->se.exec_start = rq->clock_task;

	/* The running task is never eligible for pushing */
	dequeue_pushable_task(rq, p);
}
static void watchdog(struct rq *rq, struct task_struct *p)
{
	unsigned long soft, hard;

	/* max may change after cur was read, this will be fixed next tick */
	soft = task_rlimit(p, RLIMIT_RTTIME);
	hard = task_rlimit_max(p, RLIMIT_RTTIME);

	if (soft != RLIM_INFINITY) {
		unsigned long next;

		p->wrr.timeout++;
		next = DIV_ROUND_UP(min(soft, hard), USEC_PER_SEC/HZ);
		if (p->rt.timeout > next)
			p->cputime_expires.sched_exp = p->se.sum_exec_runtime;
	}
}


static void task_tick_wrr(struct rq *rq, struct task_struct *p, int queued)
{
	struct sched_wrr_entity *wrr_se = &p->wrr;

	update_curr_wrr(rq);

	watchdog(rq, p);

	/*
	 * RR tasks need a special form of timeslice management.
	 * FIFO tasks have no timeslices.
	 */
	if (p->policy != SCHED_WRR)
		return;

	if (--p->wrr.time_slice)  //the time is not used up
		return;


/* judge the task is a backgroup or foregroup, give the different time*/

	

	struct task_group *taskgroup = p->sched_task_group;
      char group_path[1024];
     if (!(autogroup_path(taskgroup, group_path, 1024)))
    {
        if (!taskgroup->css.cgroup) {
            group_path[0] = '\0';
        }
        cgroup_path(taskgroup->css.cgroup, group_path, 1024);
    }

    if(group_path[1]=='b')
    {
        p->wrr.time_slice = WRRB_TIMESLICE;
    
        printk("set the timeslice for backgroup\n");
    }
    else if(group_path[1]!='b')
    {
        p->wrr.time_slice = WRRF_TIMESLICE;

        printk("set the timeslice for foregroup\n");
    }
	

	/*
	 * Requeue to the end of queue if we (and all of our ancestors) are the
	 * only element on the queue
	 */
	for_each_sched_wrr_entity(wrr_se) {
		if (wrr_se->run_list.prev != wrr_se->run_list.next) {
			requeue_task_wrr(rq, p, 0);
			set_tsk_need_resched(p);
			return;
		}
	}
}

static unsigned int get_rr_interval_wrr(struct rq *rq, struct task_struct *p)
{
	struct task_group *taskgroup = p->sched_task_group;
      char group_path[1024];
     if (!(autogroup_path(taskgroup, group_path, 1024)))
    {
        if (!taskgroup->css.cgroup) {
            group_path[0] = '\0';
        }
        cgroup_path(taskgroup->css.cgroup, group_path, 1024);
    }

    if(group_path[1]=='b')
    	return WRRB_TIMESLICE;
    else 
        return WRRF_TIMESLICE;

 }






const struct sched_class wrr_sched_class = {
	.next		=&fair_sched_class,  //required  next class 
	.enqueue_task	=enqueue_task_wrr,  //required  
	.dequeue_task	=dequeue_task_wrr,  //required
	.yield_task	=yield_task_wrr, 	 //required  give up the cup initiative
	.check_preempt_curr =check_preempt_curr_wrr,	 //required   
	.pick_next_task		=pick_next_task_wrr,	 //required
	.put_prev_task		=put_prev_task_wrr,		 //required
	.task_fork		=task_fork_wrr,		//required
	
#ifdef CONFIG_SMP
	.select_task_rq	=select_task_rq_wrr,
	.set_cpus_allowed	=set_cpus_allowed_wrr,  //required
	.rq_online		=rq_online_wrr,
	.rq_offline		=rq_offline_wrr,
	.pre_schedule		=pre_schedule_wrr,
	.post_schedule		=post_schedule_wrr,
	.task_woken		=task_woken_wrr,
#endif
	.switched_from		=switch_from_wrr,
	.set_curr_task		=set_curr_task_wrr,		 //required
	.task_tick		=task_tick_wrr,		 //required
	.get_rr_interval	=get_rr_interval_wrr,
	.prio_changed		=prio_changed_wrr,
	.switched_to		=switched_to_wrr,


};


#ifdef CONFIG_SCHED_DEBUG
extern void print_wrr_rq(struct seq_file *m, int cpu, struct wrr_rq *wrr_rq);  //

void print_wrr_stats(struct seq_file *m, int cpu)
{
	wrr_rq_iter_t iter;
	struct wrr_rq *wrr_rq;

	rcu_read_lock();
	for_each_wrr_rq(wrr_rq, iter, cpu_rq(cpu))
		print_wrr_rq(m, cpu, wrr_rq);
	rcu_read_unlock();
}
#endif /* CONFIG_SCHED_DEBUG */















