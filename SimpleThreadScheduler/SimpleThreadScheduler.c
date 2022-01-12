/***** READ ME *****/
/*
* To change number of CEXECS to 2, set global variable 
* int numthreads = 2;
*/

#include "queue.h"
#include "SimpleThreadScheduler.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include <pthread.h>
#include <ucontext.h>
#include <time.h>
#include <signal.h>
#include <stdbool.h>

typedef struct __threaddesc
{
	char *threadstack;
	ucontext_t threadcontext;
} threaddesc;


int THREAD_STACK_SIZE = 1024*64;
char* TOKEN = "~"; // Token used to seperate strings in strtok()

int numthreads = 1;

ucontext_t parent; //initial context
ucontext_t cexex_context;
ucontext_t cexec2_context;

pthread_t CEXEC;
pthread_t IEXEC;
pthread_t CEXEC2;

struct queue waitQueue; // queue to store tasks going to IEXEC
struct queue taskReadyQueue; //queue to store tasks going to CEXEC
struct queue messageQueue; // queue to store message to IEXEC specifying the task
struct queue returnQueue; // queue to store return information from IEXEC for sut_open()
struct queue bufferQueue; // queue to store memory location IEXEC should write to for sut_read()
struct queue threaddescQueue;

pthread_mutex_t clck;
pthread_mutex_t ioLck;

int taskDone = 0; // set to one when a task has been complete
int taskReadyQueue_empty = 0;
int waitQueue_empty = 0;

int numTasks = 0; //number of tasks currently running in scheduler
int MAX_TASKS = 30;

/*Schedules operations on CPU*/
void *CEXEC_scheduler()
{
    struct timespec time1, time2;
    time1.tv_sec = 0;
    time1.tv_nsec = 100000;

    while(!taskReadyQueue_empty || !waitQueue_empty)
    {
        struct queue_entry *taskToRun;

        pthread_mutex_lock(&clck);
        taskToRun = queue_pop_head(&taskReadyQueue);
        pthread_mutex_unlock(&clck);

        //if there is a task enqueued execute the task
        if (taskToRun != NULL)
        {
            //get context of task we want to run
            ucontext_t taskToRun_context = *(ucontext_t *)taskToRun->data;

            if (numthreads == 2 && pthread_equal(CEXEC2, pthread_self()))
            {
                //if we are using the second CEXEC we must make sure to use the context of the second
                // cexec not the first
                swapcontext(&cexec2_context, &taskToRun_context);
            } else 
            {
                swapcontext(&cexex_context, &taskToRun_context);
            }
            taskDone = 1;
            free(taskToRun); //free allocated memory for the queue_entry

        } else if (taskDone && numTasks == 0){
            pthread_mutex_lock(&clck);
            taskReadyQueue_empty = 1;
            pthread_mutex_unlock(&clck);
        }

        nanosleep(&time1, &time2);
    }

    return 0;
}

//used to set up the first CEXEC and create a context for it
void *CEXEC_init()
{
    getcontext(&cexex_context);
    cexex_context.uc_stack.ss_sp= (char *)malloc(THREAD_STACK_SIZE);
    cexex_context.uc_stack.ss_size = THREAD_STACK_SIZE;
    cexex_context.uc_link = &parent;    // setting up parent context, in case task returns or has an error
    makecontext(&cexex_context, CEXEC_scheduler, 0);
    swapcontext(&parent, &cexex_context);
}

//used to set up the second CEXEC and create a context for it
void *CEXEC2_init()
{
    getcontext(&cexec2_context);
    cexec2_context.uc_stack.ss_sp= (char *)malloc(THREAD_STACK_SIZE);
    cexec2_context.uc_stack.ss_size = THREAD_STACK_SIZE;
    cexec2_context.uc_link = &parent;    // setting up parent context, in case task returns or has an error
    makecontext(&cexec2_context, CEXEC_scheduler, 0);
    swapcontext(&parent, &cexec2_context); 
}

/*Schedules operations requiring input*/
void *IEXEC_scheduler()
{
    struct timespec time1, time2;
    time1.tv_sec = 0;
    time1.tv_nsec = 100000;

    while (!taskReadyQueue_empty || !waitQueue_empty)
    {
        // Message is passed to IEXEC as a string stored message queue indicating whether to
        // execute read, write, open, or close
        struct queue_entry *message_node;
        char message[200];
        char msg[200];

        // get message
        pthread_mutex_lock(&ioLck);
        message_node = queue_pop_head(&messageQueue);
        pthread_mutex_unlock(&ioLck);

        // if there is a message then determine what command is store in the message
        // and isolate parameters passed in message
        if (message_node != NULL)
        {
            sprintf(msg, "%s", (char *)message_node->data);
            strcpy(message, msg);

            char *command = strtok(message, TOKEN);

            if(strcmp("open", command) == 0)
            {
                // if open is command in message, then the file to open 
                // will also be in the message
                char *dest = strtok(NULL, TOKEN);

                // open the file
                int fd = open(dest, O_RDWR|O_CREAT, 0666);
                
                // We want to return fd so add fd to returnQueue so CEXEC can access return value
                char fdstring[200];
                sprintf(fdstring, "%d", fd);
                struct queue_entry *returnFd_node = queue_new_node(fdstring); 

                pthread_mutex_lock(&clck);
                queue_insert_tail(&returnQueue, returnFd_node);
                pthread_mutex_unlock(&clck);

                // Take this task off the wait queue and add it to the back of the ready queue
                pthread_mutex_lock(&clck);
                struct queue_entry *context_node = queue_pop_head(&waitQueue);
                if(context_node != NULL)
                {
                    queue_insert_tail(&taskReadyQueue, context_node);
                } 
                pthread_mutex_unlock(&clck);    
                
            } 
            else if(strcmp("write", command) == 0) 
            {
                // if write is command in message then file descriptor of file to open, string to write,
                // and the size of the string will also be stored in the message
                char* dest = strtok(NULL, TOKEN);
                int fd = atoi(dest);
                char *toWrite = strtok(NULL, TOKEN);
                char* size = strtok(NULL, TOKEN);
                int sizeint = atoi(size);

                // write string to file
                write(fd, toWrite, sizeint);

                // Take this task off the wait queue and add it to the back of the ready queue
                pthread_mutex_lock(&clck);
                struct queue_entry *context_node = queue_pop_head(&waitQueue);
                if(context_node != NULL)
                {
                    queue_insert_tail(&taskReadyQueue, context_node);
                } 
                pthread_mutex_unlock(&clck); 
            } 
            else if (strcmp("close", command) == 0)
            {
                // if close is the command in message, then file descriptor of file to close 
                // will also be store in the message
                char* fd = strtok(NULL, TOKEN);

                // close the file
                int closed = close(atoi(fd));

                // Take this task off the wait queue and add it to the back of the ready queue
                pthread_mutex_lock(&clck);
                struct queue_entry *context_node = queue_pop_head(&waitQueue);
                if(context_node != NULL)
                {
                    queue_insert_tail(&taskReadyQueue, context_node);
                } 
                pthread_mutex_unlock(&clck); 

            }
            else if (strcmp("read", command) == 0)
            {
                // if read is the command in the message, then file descriptor of the file to read,
                // and the size of the string to read will also be stored in the message.
                char* fd = strtok(NULL, TOKEN);
                char* size = strtok(NULL, TOKEN);

                // bufferQueue stores the memory location passed to sut_read to read into
                struct queue_entry *buffer;
                pthread_mutex_lock(&ioLck);
                buffer = queue_pop_head(&bufferQueue);
                pthread_mutex_unlock(&ioLck);
                
                //read string
                if (buffer != NULL) {
                    read(atoi(fd), (char *) buffer->data, atoi(size));
                }

                // Take this task off the wait queue and add it to the back of the ready queue
                pthread_mutex_lock(&clck);
                struct queue_entry *context_node = queue_pop_head(&waitQueue);
                if(context_node != NULL)
                {
                    queue_insert_tail(&taskReadyQueue, context_node);
                } 
                pthread_mutex_unlock(&clck); 
            }

            free(message_node);

        } else if(taskDone && numTasks == 0) {
            waitQueue_empty = 1;
        }
        
        nanosleep(&time1, &time2);
    }
    return 0;
}

void sut_init()
{
    taskReadyQueue = queue_create();
    queue_init(&taskReadyQueue);

    waitQueue = queue_create();
    queue_init(&waitQueue);

    messageQueue = queue_create();
    queue_init(&messageQueue);

    returnQueue = queue_create();
    queue_init(&returnQueue);

    bufferQueue = queue_create();
    queue_init(&bufferQueue);
    
    threaddescQueue = queue_create();
    queue_init(&threaddescQueue);

    pthread_create(&CEXEC, NULL, CEXEC_init , NULL);

    if(numthreads == 2) {
        pthread_create(&CEXEC2, NULL, CEXEC2_init, NULL);
    }

    pthread_create(&IEXEC, NULL, IEXEC_scheduler, NULL);

    pthread_mutex_init(&clck, NULL);
    
    pthread_mutex_init(&ioLck, NULL);
}
/*Create task to be put on CPU*/
bool sut_create(sut_task_f fn)
{
    //Create user level thread
    threaddesc *tdescptr;

    if (numTasks >= MAX_TASKS) 
	{
		printf("FATAL: Maximum task limit reached... creation failed! \n");
		return -1;
	}

	tdescptr = malloc(sizeof(threaddesc));
	getcontext(&(tdescptr->threadcontext));
	tdescptr->threadstack = (char *)malloc(THREAD_STACK_SIZE);
	tdescptr->threadcontext.uc_stack.ss_sp = tdescptr->threadstack;
	tdescptr->threadcontext.uc_stack.ss_size = THREAD_STACK_SIZE;
	tdescptr->threadcontext.uc_link = &parent;
	tdescptr->threadcontext.uc_stack.ss_flags = 0;

	makecontext(&(tdescptr->threadcontext), fn, 0);

    pthread_mutex_lock(&clck);
    //TODO: Add mutex lock
    struct queue_entry *task_node = queue_new_node(&(tdescptr->threadcontext));
    queue_insert_tail(&taskReadyQueue, task_node);
    pthread_mutex_unlock(&clck);
	
    pthread_mutex_lock(&clck);
    numTasks++;
    pthread_mutex_unlock(&clck);

    struct queue_entry *tdescptr_node = queue_new_node(tdescptr);
    pthread_mutex_lock(&clck);
    queue_insert_tail(&threaddescQueue, tdescptr_node);
    pthread_mutex_unlock(&clck);

	return 1;
}

/*
* Another task that is present in the ready queue will start running and the task running the 
* yield will get enqueued at the back of the ready queue
*/
void sut_yield()
{
    /*
    * Step 1: Get current context to be yeilded
    * Step 2: Add current context to back of the task ready queue
    * Step 3: Swap back to main context
    */
    ucontext_t context;
    getcontext(&context);

    struct queue_entry *task_node = queue_new_node(&context);
    pthread_mutex_lock(&clck);
    queue_insert_tail(&taskReadyQueue, task_node);
    pthread_mutex_unlock(&clck);

    if (numthreads == 2 && pthread_equal(CEXEC2, pthread_self()))
    {
        swapcontext(&context, &cexec2_context);
    } else 
    {
        swapcontext(&context, &cexex_context);
    }
}

void sut_exit()
{
    /* Same as sut_yield() but not put back on task ready queue */

    ucontext_t context;
    getcontext(&context);

    pthread_mutex_lock(&clck);
    numTasks--;
    pthread_mutex_unlock(&clck);

    free(context.uc_stack.ss_sp);

    if (numthreads == 2 && pthread_equal(CEXEC2, pthread_self()))
    {
        swapcontext(&context, &cexec2_context);
    } else 
    {
        swapcontext(&context, &cexex_context);
    }
}

/*function to open a file*/
int sut_open(char *dest)
{
    //Get context currently running on CEXEC
    struct queue_entry *currentTask_node;
    ucontext_t context;
    getcontext(&context);
    currentTask_node = queue_new_node(&context);

    //Add to waitQueue to run on IEXEC
    pthread_mutex_lock(&clck);
    queue_insert_tail(&waitQueue, currentTask_node);
    pthread_mutex_unlock(&clck);

    // Create message storing indicating open command and dest parameter
    char message[200];

    sprintf(message, "open%s%s", TOKEN, dest);
    char *message_copy = strdup(message);

    struct queue_entry *message_node = queue_new_node(message_copy);
    pthread_mutex_lock(&ioLck);
    queue_insert_tail(&messageQueue, message_node);
    pthread_mutex_unlock(&ioLck);

    if (numthreads == 2 && pthread_equal(CEXEC2, pthread_self()))
    {
        swapcontext(&context, &cexec2_context);
    } else 
    {
        swapcontext(&context, &cexex_context);
    }
    
    // when task resumes on CEXEC get fd from the open command stored on returnQueue
    pthread_mutex_lock(&clck);
    struct queue_entry *returnMessage = queue_pop_head(&returnQueue);
    pthread_mutex_unlock(&clck);
    if(returnMessage != NULL)
    {
        char* fdstring = (char *) returnMessage->data;
        int fd = atoi(fdstring);
        free(returnMessage);
        return fd;
    }

    return -1;
    
}

/* Function to write to a file */
void sut_write(int fd, char *buf, int size)
{
    //Get context currently running on CEXEC
    //printf("In sut_write\n");
    struct queue_entry *currentTask_node;
    ucontext_t context;
    getcontext(&context);
    currentTask_node = queue_new_node(&context);

    //Add to waitQueue to run on IEXEC
    pthread_mutex_lock(&clck);
    queue_insert_tail(&waitQueue, currentTask_node);
    pthread_mutex_unlock(&clck);

    // Create message storing write command, fd, buf, and size parameters
    char message[200];
    sprintf(message, "write%s%d%s%s%s%d", TOKEN, fd, TOKEN, buf, TOKEN, size);
    char *message_copy = strdup(message);
    //printf("%s\n", message_copy);


    struct queue_entry *message_node = queue_new_node(message_copy);
    pthread_mutex_lock(&ioLck);
    queue_insert_tail(&messageQueue, message_node);
    pthread_mutex_unlock(&ioLck);

    if (numthreads == 2 && pthread_equal(CEXEC2, pthread_self()))
    {
        swapcontext(&context, &cexec2_context);
    } else 
    {
        swapcontext(&context, &cexex_context);
    }
}

/* Function to close a file */
void sut_close(int fd)
{
    //printf("Entered sut_close\n");
    //Get context currently running on CEXEC
    struct queue_entry *currentTask_node;
    ucontext_t context;
    getcontext(&context);
    currentTask_node = queue_new_node(&context);

    //Add to waitQueue to run on IEXEC
    pthread_mutex_lock(&clck);
    queue_insert_tail(&waitQueue, currentTask_node);
    pthread_mutex_unlock(&clck);

    // Create message storing close command and fd parameter
    char message[200];
    sprintf(message, "close%s%d", TOKEN, fd);
    char *message_copy = strdup(message);

    struct queue_entry *message_node = queue_new_node(message_copy);
    pthread_mutex_lock(&ioLck);
    queue_insert_tail(&messageQueue, message_node);
    pthread_mutex_unlock(&ioLck);

    if (numthreads == 2 && pthread_equal(CEXEC2, pthread_self()))
    {
        swapcontext(&context, &cexec2_context);
    } else 
    {
        swapcontext(&context, &cexex_context);
    }

}

/* Function to read a file */
char *sut_read(int fd, char *buf, int size)
{
    //printf("Enter sut_read\n");
    //Get context currently running on CEXEC
    struct queue_entry *currentTask_node;
    ucontext_t context;
    getcontext(&context);
    currentTask_node = queue_new_node(&context);

    //Add to waitQueue to run on IEXEC
    pthread_mutex_lock(&clck);
    queue_insert_tail(&waitQueue, currentTask_node);
    pthread_mutex_unlock(&clck);

    // Create message storing read command, and fd and size parameters
    char message[200];
    sprintf(message, "read%s%d%s%d", TOKEN, fd, TOKEN, size);
    char *message_copy = strdup(message);

    // Store memory location to read into given by buf parameter on bufferQueue
    struct queue_entry *buffer_node = queue_new_node(buf);
    pthread_mutex_lock(&ioLck);
    queue_insert_tail(&bufferQueue, buffer_node);
    pthread_mutex_unlock(&ioLck);

    struct queue_entry *message_node = queue_new_node(message_copy);
    pthread_mutex_lock(&ioLck);
    queue_insert_tail(&messageQueue, message_node);
    pthread_mutex_unlock(&ioLck);

    if (numthreads == 2 && pthread_equal(CEXEC2, pthread_self()))
    {
        swapcontext(&context, &cexec2_context);
    } else 
    {
        swapcontext(&context, &cexex_context);
    }

    // when this task resumes on CEXEC, return buf;
    return buf;
}

void sut_shutdown()
{
    pthread_join(CEXEC, NULL);
    pthread_join(IEXEC, NULL);
    if(numthreads == 2){
        pthread_join(CEXEC2, NULL);
        free(cexec2_context.uc_stack.ss_sp);
    }
    free(cexex_context.uc_stack.ss_sp);

    struct queue_entry *clear = queue_pop_head(&threaddescQueue);
    while(clear != NULL) {
        free(clear->data);
        free(clear);
        clear = queue_pop_head(&threaddescQueue);
    }

}

