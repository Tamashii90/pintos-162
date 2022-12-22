1) Is the program’s output the same each time it is run? Why or why not?

        No, because we can't predict the order in which the threads will execute.

2) Based on the program’s output, do multiple threads share the same stack?

        Not at all.

3) Based on the program’s output, do multiple threads have separate copies of global variables?

        No, they share the same copy.

4) Based on the program’s output, what is the value of `void *threadid`? How does this relate to the variable’s type `(void *)`?

        It's the thread's ID.
        It's casted to void* by the for-loop (to pass an argument to a thread using pthread_create, it must be void*)
        then casted back to long inside thread's function.

5) Using the first command line argument, create a large number of threads in `pthread`. Do all threads run before the program exits? Why or why not?

        Yes, because main ends with pthread_exit(NULL) which only terminates the main thread, allowing the rest to continue.
