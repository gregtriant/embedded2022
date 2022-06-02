
TO RUN THE CODE type:
- make
- ./problem

1. The FIFO stores workFunction Elements
    struct workFunction {
        void * (*work)(void *);
        void * arg;
    }

2. We have 
    - p threads of producers that place in queue: pointers to Functions like the one above.
    - q threads of consumers that recieve: the pointers from the queue and execute the functions.
    - the Functions are simple: -> calculate sin() for some corners, OR print something

3. - Dont use the sleep() fucntion nor the "second loop".
   - Repeat the first loop for a large number of loops.
   - The Consumer loop must be infinate ( while(1) ).


Questions: 

1. What is the wait time of a function in the queue? Present some Statistics.
2. Find the number of consumers the minimizes the Avarage wait time.



Notes:

In the example given we have one producer and one consumer.
We need to make p producers and q consumers that all access the same queue.