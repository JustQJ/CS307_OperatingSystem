
/*
This program use the pscopy() function to copy the information of processes to the buf array. Because the information of process stores in the task_struct, the pscopy() function gets the information from the task_struct using DFS algorithm and puts this information into the buf array.  In order to get the information in task_struct, mainly using the list_for_each() function and the list_entry() function. The list_for_each() function can traverse  all child processes of the parent process following the list that links all childern and parent. But because the list pointer doesn't point the head of the task_struct struct but the sibling position, we need to use the list_entry() to find the head of this process's task_struct.  After that, then we can get the information in  task_struct struct.
sys_pstreecall() function can call the pscopy() function and give the parameters, including the buf, the size, and the init_task's adress, to do the information copying. The init_task is the father of all processes, so we can use it to find other processes.
*/



#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/sched.h>
#include<linux/unistd.h>
#include <linux/syscalls.h>
#include <linux/slab.h>     
#include <linux/uaccess.h>	
#include <linux/list.h>   

MODULE_LICENSE("Dual BSD/GPL");
#define __NR_pstreecall 356
struct prinfo{
pid_t parent_pid;           //process id of parent 
pid_t pid;                //process id 
pid_t first_child_pid;  // pid of youngest child
pid_t next_sibling_pid;    // pid of the older sibling
long state;             //current state of state
long uid;              //user id of process owner
char comm[64];          //name of the program
int depth;             //the depth of this proogress

};


static void pscopy(struct prinfo *buf, struct task_struct *currentTask, int *nr, int depth)
{
struct task_struct *nextTask; //the nexttask we want to get
struct list_head *listHead;    //list_head to get the entry of the task_struct

list_for_each(listHead, &currentTask->children)
{
	//get the entry of this task_struct of the process
	nextTask=list_entry(listHead,struct task_struct, sibling);  
	
	//copy the value of this process
	buf[*nr].parent_pid=nextTask->parent->pid;
	buf[*nr].pid=nextTask->pid;
	
	if(list_empty(&nextTask->children))  //judeg the process whether it has children or not
		buf[*nr].first_child_pid=0;
	else
		{
		//get the children' pid
		struct task_struct *tempTask=list_entry((nextTask->children).next,struct task_struct, sibling);  
		buf[*nr].first_child_pid=tempTask->pid;
		}
		
	
	struct task_struct *tempTask1=list_entry((nextTask->sibling).next,struct task_struct, sibling); // using to judge whether this process has sibling, if not, the pointer pointing to his sibling will point its father 
	
	if(tempTask1==nextTask->parent) //this process has no sibling
		buf[*nr].next_sibling_pid=0;
	else
		buf[*nr].next_sibling_pid=tempTask1->pid;  //get the  sibling pid
		
		
	buf[*nr].state=nextTask->state; //get the state of this process
	
	buf[*nr].uid=nextTask->cred->uid;  //get the user id of this process
	
	
	buf[*nr].depth=depth; //get the depth of this process for printing the \t to give  the relationship
	
	        
	get_task_comm(buf[*nr].comm,nextTask); //get the name of the process   
	
	*nr=*nr+1;  //size of the buffer add one
	
	//use DFS to get the child of this process ,when this process has child, it can get into the list_for_each() function, then can return.
	pscopy(buf, nextTask, nr, depth+1);  
}
}






static int (*oldcall)(void);

//call the pscopy() function to doing the information copy.
static int sys_pstreecall(struct prinfo *buf, int *nr)
{
   pscopy(buf, &init_task, nr, 0); 
   return 0; 
}





static int addsyscall_init(void)
{
    long *syscall=(long*)0xc000d8c4;
    oldcall = (int(*)(void))(syscall[__NR_pstreecall]);
    syscall[__NR_pstreecall] = (unsigned long)sys_pstreecall;
    printk(KERN_INFO"module load!\n");
    return 0;
}
static void addsyscall_exit(void)
{
    long *syscall=(long*)0xc000d8c4;
    syscall[__NR_pstreecall]=(unsigned long)oldcall;
    printk(KERN_INFO"module exit!\n");
   
}

module_init(addsyscall_init);
module_exit(addsyscall_exit);



