/*
This program is to show the relationship between the Parent process and the Child process. In the main() function, using the fork() function to create the child process. In the child process, printing parent pid and child pid and  using the execl() function to call the pstree function to display the relationship of parent and child. In the parent process, using the wait() function to wait child process finish it's job, then the process end.
*/





#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>

int main()
{
int i=1;
pid_t pid;

// creat a child
pid=fork();
if(pid<0)
{
printf("error!");
return -1;
}
else if(pid==0)  //excute in child process
{
printf("517020910038Parent: %d\n", getppid());   //print the father pid
printf("517020910038Child: %d\n", getpid());      //print the child pid
execl("testARM","testARM",(char *)0);                //call the pstree, the file is the testARM in the android/data/misc and testARM comes from problem2 
}
else
{
wait(NULL);   ///must wait for child, otherwise it will finish before child, then child will be killed by the init_task.
}
return 0;
}
