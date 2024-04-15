#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>

pthread_barrier_t barrier;
double time_wait = 0.0;

typedef struct {
    pthread_t thread_id;
    int thread_num;
    int sched_policy;
    int sched_priority;
} thread_info_t;


void *thread_func(void *arg)
{
    /* 1. Wait until all threads are ready */
    pthread_barrier_wait(&barrier);
    
    clock_t start,current;
    /* 2. Do the task */ 
    for (int i = 0; i < 3 ; i++) {
        printf("Thread %d is running\n", (int)((thread_info_t *)arg)->thread_id);
        /* Busy for <time_wait> seconds */
        start = clock();
        while(1){
            current = clock();
            double elapse = ((double)(current - start)) / CLOCKS_PER_SEC;
            if(elapse >= time_wait){
                break;
            }
        }

    }
    /* 3. Exit the function  */
    pthread_exit(NULL);
}


int main(int argc , char *argv[]) {
    /* 1. Parse program arguments */
    int thread_nums = 0;
    char *policy = NULL , *priority = NULL;
    for(int i=1 ; i<argc ; i+=2){
        if(strcmp(argv[i],"-n")==0){
            thread_nums = atoi(argv[i+1]);
        }else if(strcmp(argv[i],"-t")==0){
            char *eptr;
            time_wait = strtod(argv[i+1],&eptr);
        }else if(strcmp(argv[i],"-s")==0){
            policy = argv[i+1];
        }else if(strcmp(argv[i],"-p")==0){
            priority = argv[i+1];
        }
    }
    int count = 0;
    char policys[thread_nums][32];
    int prioritys[thread_nums]; 
    
    char *token = strtok(policy,",");
    
    while(token!=NULL){
        strcpy(policys[count++],token); 
        token = strtok(NULL,","); 
    }

    count=0;
    token = strtok(priority,",");
    while(token!=NULL){
        prioritys[count++] = atoi(token);
        token = strtok(NULL,",");
    }


    /* 2. Create <num_threads> worker threads */
    pthread_t t[thread_nums];
    thread_info_t info[thread_nums];
    pthread_attr_t attr[thread_nums]; //attr
    cpu_set_t cpuset; //affinity
    struct sched_param param[thread_nums]; //priority
    /* 3. Set CPU affinity */
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);

    /* 4. Set the attributes to each thread */
    //init barrier
    pthread_barrier_init(&barrier,NULL,thread_nums);
    for(int i=0 ; i<thread_nums ; i++){
        info[i].thread_id = (pthread_t)i;
        info[i].thread_num = thread_nums;
        info[i].sched_policy = (strcmp(policys[i],"NORMAL")==0)? SCHED_OTHER : SCHED_FIFO;
        info[i].sched_priority = prioritys[i];


        pthread_attr_init(&attr[i]);
        if(pthread_attr_setaffinity_np(&attr[i],sizeof(cpu_set_t),&cpuset)!=0){
            printf("affinity error\n");
        }

        if(pthread_attr_setschedpolicy(&attr[i],info[i].sched_policy)){
            printf("policy error\n");
        }        
        
        if(info[i].sched_policy==SCHED_FIFO){
            param[i].sched_priority = prioritys[i];            
            if(pthread_attr_setschedparam(&attr[i],&param[i])!=0){
                printf("param error\n");
            }
        }
        
        if(pthread_attr_setinheritsched(&attr[i],PTHREAD_EXPLICIT_SCHED)!=0){
            printf("inherit error \n");
            return -1;
        }
        pthread_create(&t[i],&attr[i],thread_func,(void *)&info[i]);
    }


    /* 6. Wait for all threads to finish  */ 
    for(int i=0 ; i<thread_nums ; i++){
        pthread_join(t[i],NULL);
    }
    pthread_barrier_destroy(&barrier);
    return 0;
}
