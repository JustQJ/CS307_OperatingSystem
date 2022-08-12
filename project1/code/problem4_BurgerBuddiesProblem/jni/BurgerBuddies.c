/*
To start this problem, it need input 4 integer paramters by the number of cook, the number of cashier, the number of customer and the size of rack.

In this program, there are four functions, Cooks(), Cashiers(), Customers(), pthread_error() and main().

Cooks() function is used to create cook thread. In this function, it need to cook burger all the time until the rack is full. And for every single cooker, it need 1 second to do a burger.

Cashiers() function is used to create cashier thread. In this function, cashier need wait for the customer coming and accept an order. After a cashier accept an order, it need go to take a burger to the customer. If there is burger, it must wait. After the cashier takes a burger to the customer, the customer can leave.It needs 1 second for  the cashier to be ready for serving other customers. 

Customers() function is used to create customer thread. In this function, the customer can wait a cashier to order and after get the burger from a cashier, it can leave. Between two customers coming, it will be 0.3 second intervel.


pthread_error() function can print the error message when creating pthread is not successful.

main() function is used to create thread.

*/



#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<pthread.h>
#include<semaphore.h>
#include<unistd.h>

#define sleep_time 1    //second
#define usleep_time 300000  //microsecond
sem_t empty, full; // the state of the rack. full is number of burger in the rack, empty equals racksize minus full
sem_t customers,customerMutex; //customers is the number of customer coming and wait for ordering. customerMutex is protection in customers()
sem_t cashier; //cashier is the number of cashier who can accept a order. 
sem_t order; // the queue that the customer has odered and wait for the burger.


//cooks function
void* Cooks(void* id) //id is the identifier of the cook.
{
    do{
    	
        sem_wait(&empty);         //wait when rack is full
         sleep(sleep_time);       //need 1 second to cook the burger
        printf("Cook[%d] make a burger.\n", *((int*)id));  //cooked a burger
        sem_post(&full);          //increase the number of burger
      
    }while(1);

}



//cashier function
void* Cashiers(void* id) //id is the identifier of the cashier.
{
    do{
    
    sem_wait(&customers);        // waite when there is no customer
    printf("Cashier[%d] accepts an order.\n", *((int*)id));    //accept an order when customer is coming 
    
   
    sem_wait(&full);             //wait when there is no burger      
    printf("Cashier[%d] takes a burger to customer.\n",*((int*)id));  //take a burger to  the customer
    sem_post(&empty);        //the empty increases one when the cashier take a burger 
    sem_post(&order);        //the customer can leave after get the burger
    sleep(sleep_time);        //need 1 second to prepare for the next serving
    sem_post(&cashier);      //ready for serving 
    }while(1);

}
void* Customers(void* a)
{
   
    sem_wait(&customerMutex);
   
    printf("Customer[%d] comes.\n", *((int*)a));  // a customer coming
       usleep(usleep_time);                 //the intervel between two customer coming 
    sem_post(&customerMutex);
    sem_post(&customers);             //increasing the customer 
    sem_wait(&cashier);            //wait when there is no cashier can serve 
    sem_wait(&order);              // wait after ordering 
    
    
}

//pthread_error() function
void pthread_error()   
{
printf("pthread create error!\n");  //print the error messege

//destory all the semaphore 
sem_destroy(&empty); 
sem_destroy(&full);
sem_destroy(&cashier);

sem_destroy(&customers);

sem_destroy(&order);
sem_destroy(&customerMutex);
}

int main(int argc, char *argv[])
{
   	// when the number of paramters doesn't equal 5, print the error information and return
	if(argc != 5)
	{
	printf("the number of paramters is error!\n");
	return 0;
	}
	
	// get the number from the argv[] using the atoi() function.
	int cooksnum=atoi(argv[1]);       // get the number of cooks
	int cashiersnum=atoi(argv[2]);    //get the number of cashiers
	int customersnum=atoi(argv[3]);   //get the number of customers
	int racksize=atoi(argv[4]);      //get the size of rack

	 
	//when the number isn't right, print the error information and return
	if(cooksnum<=0||cashiersnum<=0||customersnum<=0||racksize<=0)
	{
	printf("the value of paramters is error!\n");
	return 0;
	}
	  
	//used to store the id of the thread
	int cookp[cooksnum];         
	int cashierp[cashiersnum];
	int customerp[customersnum];
	
	//print the begining message	
    printf("Cooks[%d],Cashiers[%d],Customer[%d]\n",cooksnum,cashiersnum,customersnum);
    printf("Begin run.\n");


	//initialize the semaphore
    sem_init(&empty,0,racksize);
    sem_init(&full,0,0);
    sem_init(&customers,0,0);
    sem_init(&cashier,0,cashiersnum);
     sem_init(&customerMutex,0,1);
    sem_init(&order,0,0);
     
     
     //the array to store the pthread
    pthread_t customer[customersnum];
    pthread_t cook[cooksnum];
    pthread_t cashiers[cashiersnum];
    
    
    pthread_attr_t attr;
    pthread_attr_init(&attr);

    int i;
    
    
    //cooks thread 

     for(i=0;i<cooksnum;i++)
     	{
     	cookp[i]=i+1;  //identifier of the thread     
     	 int check=pthread_create(&cook[i],&attr,Cooks,&cookp[i]);  //creat the thread 
     	 if(check)   //when creation is unsuccessful, print the error message and return
     	 {
     	 	pthread_error();
     	 	return 0;
     	 }
     	}
       
	//customers thread
    for(i=0;i<customersnum;i++)
    	{
    	customerp[i]=i+1;  //identifier of the thread 
    	
        int check=pthread_create(&customer[i],&attr,Customers,&customerp[i]); //creat the thread 
        if(check)  //when creation is unsuccessful, print the error message and return
     	 {
     	 	pthread_error();
     	 	return 0;
     	 }
	}

	//cashiers thread
     for(i=0;i<cashiersnum;i++)
     	{ 
     	cashierp[i]=i+1;  //identifier of the thread 
        int check=pthread_create(&cashiers[i],&attr,Cashiers,&cashierp[i]);  //creat the thread 
        if(check)  //when creation is unsuccessful, print the error message and return
     	 {
     	 	pthread_error();
     	 	return 0;
     	 }
	}
	
	
//when all customers have get the burger, the program ends. 
  for(i=0;i<customersnum;i++)    
       pthread_join(customer[i],NULL);
       
       
  sem_destroy(&empty);
  sem_destroy(&full);
  sem_destroy(&cashier);
  sem_destroy(&customers);
  sem_destroy(&order);
  sem_destroy(&customerMutex);
  
   printf("job end!\n");
   return 0; 



}












