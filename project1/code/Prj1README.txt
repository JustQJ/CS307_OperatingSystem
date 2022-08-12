Problem1: In the /problem1_pstree/pstreecall.c, there is the source code of the pstree system call.

Problem2: In the /problem2_testPstree/jni/test.c, there is the test source code to use the system call mode when we install the pstree mode in the android. In the test.c, there is a print function to print the result in the buffer that we store the process information we get, and a main function to call the mode and print function. And using the ndk-build in the jni file, we can get the testARM. Pushing the testARM to the android, then we can execute the testARM to get the pstree result.

Problem3: In the /problem3_ParentAndChild/jni/problem3.c, there is a main function to creat a parent process and fork a child process and use the testARM file that we get in problem 2 to call the pstreecall.

Problem4: In the /problem4_BurgerBuddiesProblem/jni/BurgerBuddies.c, there is the source code of this problem. Using the ndk-build in the jni file, we can get the BBCARM file, then push it to the android, we can execute it. When executing, we just need to input the number of cook, cashier, customer, and racksize.  
