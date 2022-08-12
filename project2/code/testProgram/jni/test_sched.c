
/******************************************************************
purpose: This program is used to test the wrr scheduler and give out the imformation of the process after change the scheduling policy.

It has two functions, set_scheduler() and main().

The set_scheduler() function accept the pid of the process that need to be changed the scheduling policy. After obtaining the policy and priority, it calls the sched_setscheduler() function to set the policy for the process. Then we use the sched_getscheduler() function and  sched_rr_get_interval() function to get the new policy and timeslice of the process, and print them out.

The main() function is used to call the set_scheduler() function.

*******************************************************************/



#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <linux/unistd.h>
#include <linux/sched.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h> 
#include <sched.h> 

#define SCHED_NORMAL    0
#define SCHED_FIFO      1
#define SCHED_RR        2
#define SCHED_BATCH     3
#define SCHED_IDLE      5
#define SCHED_WRR       6
static void  set_scheduler()
{
    int wrr_time_slice, temp, ret;
    struct sched_param param, param1;
    struct timespec time_slice;
 
    pid_t pid;
    int policy;
    
    /* input the process's pid*/
    printf("Input the process id (PID) you want to modify: ");
    scanf("%d", &temp);
    pid = temp;

    /* choose the scheduling algorithem of the process */
    printf("Please input the choice of scheduling algorithm(0-NORMAL, 1-FIFO, 2-RR, 6-WRR): ");
    scanf("%d", &policy);
    if(policy != 0 && policy != 1 && policy!= 2&& policy!=6)
    {
        perror("Wrong schedule policy!");
        exit(-1);
    }
    
    /* set priority of the process */
    printf("Set process's priority(1-99): ");
    scanf("%d",&temp);
    
    /* for NORMAl , the priority is 0*/
    if(policy==0)  
    	temp=0;
    param.sched_priority = temp;


   printf("The Process's PID %d\n", pid);
   printf("Old policy: %d\n", sched_getscheduler(pid));  
   
   /* set the scheduling algorithm for the process */
   ret = sched_setscheduler(pid, policy, &param); 

  if (ret < 0) {
        perror("Changing scheduler failed!");
        exit(-1);
    }
    
    /* get current time_slice for the process */
     sched_rr_get_interval(pid, &time_slice);
     wrr_time_slice = time_slice.tv_nsec/1000000;  //the tv_nsec is ns, need converting into ms 

          /* get the priority */
     sched_getparam(pid, &param1);
     
     /* print the current time_slice, priority, timeslice */
	printf("Current policy: %d.\n", sched_getscheduler(pid)); 
	printf("Current priority: %d.\n", param1.sched_priority); 
	printf("Timeslice: %d.\n",  wrr_time_slice); 
}


int main() {
   set_scheduler();
    printf("Switch finish!\n");
}
