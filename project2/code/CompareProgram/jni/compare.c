/*********************************************************
This program is used to compare the performance of the NORMAL, WRR, FIFO, RR.
we can use the time command to get the executing time of the program with different policies. Then through the time, we can compare their performance.
the command is like this: time ./compare policy    
where the policy is integer, including 0,1,2,6.

time ./compare 0    we can get the executing time of NORMAL  

time ./compare 1    we can get the executing time of RR 

time ./compare 2    we can get the executing time of FIFO

time ./compare 6    we can get the executing time of WRR

*********************************************************/
#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<pthread.h>
#include<semaphore.h>
#include<unistd.h>
#include<sys/time.h>
#include <sys/types.h>
#include <sys/syscall.h>


void* Customers()
{
	
	int i;
	 pid_t pid;
	 struct sched_param param1;
	pid=gettid();
	 sched_getparam(pid, &param1);
	 for(i=20000; i>0; i--)
	 	printf("%d",param1.sched_priority);

}


int main(int arg, char *argv[])
{
    if(arg != 2)
    {
    printf("parameter error!");
    return;
    }
    
    int policy=atoi(argv[1]);
    if(policy!=0 && policy!=1 && policy!=2 && policy !=6)
    	 {
   	 printf("parameter error!");
   	 return;
    	}
    
    int customersnum=200;
    int i;

    
    

    pthread_t customer[customersnum];

    
    
    pthread_attr_t attr;   
    pthread_attr_init(&attr);
    struct sched_param param;

   
   pthread_attr_setschedpolicy(&attr, policy);  //set the scheduling policy for these threads.
   
   
   time_t t;
   srand(time(&t));
    for(i=0;i<customersnum;i++)  //creat  threads
    	{
    	
    	 param.sched_priority = rand()%99 +1;
    	 pthread_attr_setschedparam(&attr, &param);
        int check=pthread_create(&customer[i],&attr,Customers,NULL); 
        if(check)  
     	 {
     	 	printf("error!");
     	 	return 0;
     	 }
	}

  for(i=0;i<customersnum;i++)    
       pthread_join(customer[i],NULL);
       
       
   pthread_attr_destroy(&attr);
   
   
   
   return 0; 



}












