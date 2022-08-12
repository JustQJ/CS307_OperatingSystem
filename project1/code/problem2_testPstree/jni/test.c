/*
This program is to test the pstree system call in the problem1, mainly including printftree() function and main() function.
The printftree() function tends to print the structure of process tree using the \t before every process according to their relationship. The relationship can be identified by the depth member of the prinfo struct.
The main() function is to call the pstreecall system call to copy the information into the buf and call the printftree() function to print the tree structure.
*/




#include "test.h"
#define num 1024


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


//print function
void printftree(struct prinfo *buf, int *nr)
{
int i;                 //gcc can't define the i in for loop
for(i=0;i<*nr;i++)
{
	int depth=buf[i].depth;
	
	//print the \t before the information
	while(depth>0) 
	{
	printf("\t");
	depth--;
	}
	printf("%s, %d, %ld, %d, %d, %d, %d\n", buf[i].comm, buf[i].pid,buf[i].state,buf[i].parent_pid,buf[i].first_child_pid, buf[i].next_sibling_pid, buf[i].uid);
	
	
}
}
int main()
{
   // allocate the memory to the buf
   struct prinfo *buf=(struct prinfo*)malloc(sizeof(struct prinfo)*num);
   int *nr=(int*)malloc(sizeof(int)*1);
   if(buf==NULL||nr==NULL)
   	{printf("error!");
   	exit(0);
   	}
   *nr=0;	
   
   
   //call the pstreecall
    int  i=syscall(356,buf,nr);
    
    //print the result
    printf("this is the pstree:\n");
    printftree(buf,nr);
    free(buf);
    free(nr);
    return 0;
}
