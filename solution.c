#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "a2_helper.h"

#define NR_THREADS 4
#define BAR_THREADS 42
#define SYNC_THREADS 6
#define BARRIER 6

typedef struct {
    int proc_nr;
    int th_nr;
    pthread_mutex_t *lock;
    pthread_mutex_t *lock1;
    pthread_mutex_t *lock2;
    pthread_cond_t *cond;
    pthread_cond_t *cond1;
    pthread_cond_t *cond2;
    sem_t *sync1;
    sem_t *sync2;
} TH_STRUCT; //thread structure for process 5

typedef struct{
    int proc_nr;
    int th_nr;
    sem_t *mutex;
    pthread_mutex_t *lock;
    pthread_mutex_t *lock1;
    pthread_cond_t *cond;
    pthread_cond_t *cond1;
    sem_t *barr1;
    sem_t *barr2;
} TH_PARAM; //thread structure for process 4

typedef struct{
    int proc_nr;
    int th_nr;
    sem_t *sync1;
    sem_t *sync2;
}TH_SYNC; //thread structure for process 7

int turn = 1, closed = 0, ok = 0;
int count = 0, first_th = 0;
int closed1 = 0;
sem_t *sync1 ;
sem_t *sync2 ;


void* thread_sync_diff_proc(void* arg)
{  
    TH_SYNC *params = (TH_SYNC*)arg;
    if (params->th_nr == 3)
    {
       sem_wait(sync1);
    }
    info(BEGIN, params->proc_nr, params->th_nr);

    info(END, params->proc_nr, params->th_nr);
    closed1 = params->th_nr;
    if (closed1 == 5)
    {
        sem_post(sync2);
    }
    return NULL;
}

void* thread_barrier (void* arg)
{
    TH_PARAM *params = (TH_PARAM*)arg;

    pthread_mutex_lock(params->lock1);
    while(params->th_nr != 10 && first_th == 0)
    pthread_cond_wait(params->cond1, params->lock1);
    pthread_mutex_unlock(params->lock1);

    sem_wait(params->barr1); // barrier - permissions = 6

    info(BEGIN, params->proc_nr, params->th_nr);
    sem_wait(params->mutex);
    count++;
    pthread_cond_broadcast(params->cond);
    sem_post(params->mutex);

    sem_wait(params->barr2); // meeting point

    pthread_mutex_lock(params->lock);
    while (params->th_nr == 10 && count < 6)
    {
        first_th = 1; //signals that thread 10 started
        pthread_cond_broadcast(params->cond1);
        pthread_cond_wait(params->cond, params->lock);
    }
    pthread_mutex_unlock(params->lock);

    
    sem_wait(params->mutex);
    count--;
    info(END, params->proc_nr, params->th_nr);
    sem_post(params->mutex);
    
    sem_post(params->barr2);
  
    sem_post(params->barr1);

    return NULL;
}

void* thread_sync_same_proc(void* arg)
{  
    TH_STRUCT *params = (TH_STRUCT*)arg;
    
    if (params->th_nr == 1)
    {
        sem_wait(sync2); //5.1 won't start until 7.5 ends
    }

    pthread_mutex_lock(params->lock);

    while(params->th_nr != turn)
    {
        pthread_cond_wait(params->cond, params->lock);
    }
    turn = params->th_nr + 1;
    pthread_cond_broadcast(params->cond);

    pthread_mutex_unlock(params->lock);

    
    if (params->th_nr == 2)
    {
        pthread_mutex_lock(params->lock1);
        while(params->th_nr == 2 && ok != 1) //thread 2 won't start until thread 4 started
        {
            pthread_cond_wait(params->cond1, params->lock1);
        }
        pthread_mutex_unlock(params->lock1);
    }
    

    info(BEGIN, params->proc_nr, params->th_nr);

    if(params->th_nr == 4)
    {
        pthread_mutex_lock(params->lock2);
        ok = 1;
        pthread_cond_broadcast(params->cond1); //signals that thread 4 has started
        while(params->th_nr == 4 && closed != 2) //thread 4 won't end until thread 2 ended
        {
            pthread_cond_wait(params->cond2, params->lock2);
        }
        pthread_mutex_unlock(params->lock2);
    }

    info(END, params->proc_nr, params->th_nr);
    closed = params->th_nr;
    pthread_cond_broadcast(params->cond2); //signals that thread 2 ended
    
    if (closed == 1)
    {
        sem_post(sync1); //signals that thread 1 ended
    }
    return NULL;
}

int main(){
    init();

    info(BEGIN, 1, 0);

    pthread_t tid[NR_THREADS];
    pthread_t b_tid[BAR_THREADS];
    pthread_t s_tid[SYNC_THREADS];
    TH_STRUCT params[NR_THREADS];
    TH_PARAM b_params[BAR_THREADS];
    TH_SYNC s_params[SYNC_THREADS];
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t lock1 = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t lock2 = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t barr_lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t barr_lock1 = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;
    pthread_cond_t cond2 = PTHREAD_COND_INITIALIZER;
    pthread_cond_t barr_cond = PTHREAD_COND_INITIALIZER;
    pthread_cond_t barr_cond1 = PTHREAD_COND_INITIALIZER;
    sem_t mutex, barr1, barr2;
    sync1 = sem_open("sem1_process", O_CREAT, 0644, 0);
    sync2 = sem_open("sem2_process", O_CREAT, 0644, 0);

    pid_t pid1 = fork();
    pid_t pid3, pid4, pid5, pid6, pid7, pid8;
    if (pid1 == 0)
    {
        info(BEGIN, 2,0);
        pid3 = fork();
        if (pid3 == 0)
        {
            info(BEGIN, 3, 0);
            pid4 = fork();
            if(pid4 == 0)
            {
                info(BEGIN, 4, 0);
                pid6 = fork();
                if (pid6 == 0)
                {
                    info(BEGIN, 6, 0);
                    info(END, 6, 0);
                    return 6;
                }
                else
                {
                    int status6 = 0;
                    waitpid(pid6, &status6, 0);
                }

                //

                sem_init(&mutex, 0, 1);
                sem_init(&barr1, 0, BARRIER);
                sem_init(&barr2, 0, 1);

                for(int i=0; i<BAR_THREADS; i++)
                {
                    b_params[i].proc_nr = 4;
                    b_params[i].th_nr = i+1;
                    b_params[i].mutex = &mutex;
                    b_params[i].barr1 = &barr1;
                    b_params[i].barr2 = &barr2;
                    b_params[i].lock = &barr_lock;
                    b_params[i].lock1 = &barr_lock1;
                    b_params[i].cond = &barr_cond;
                    b_params[i].cond1 = &barr_cond1;
                    
                    pthread_create(&b_tid[i], NULL, thread_barrier, &b_params[i]);
                    
                }

                for (int i = 0; i<BAR_THREADS; i++)
                {
                    pthread_join(b_tid[i], NULL);
                }
                
                pthread_mutex_destroy(&barr_lock);
                pthread_mutex_destroy(&barr_lock1);
                pthread_cond_destroy(&barr_cond);
                pthread_cond_destroy(&barr_cond1);
                sem_destroy(&mutex);
                sem_destroy(&barr1);
                sem_destroy(&barr2);
                //

                info(END, 4, 0);
                return 4;
            }
            else
            {
                int status4 = 0;
                waitpid(pid4, &status4, 0);
            }
            
            info(END, 3, 0);
            return 3;
        }
        else
        {
            pid8 = fork();
            if(pid8 == 0)
            {
                info(BEGIN, 8, 0);
                info(END, 8, 0);
                return 8;
            }
            else
            {
                int status3 = 0, status8 = 0;
                waitpid(pid8, &status8, 0);
                waitpid(pid3, &status3, 0);
            }
        }
        info(END, 2, 0);
        return 2;
    }
    else 
    {
        pid5 = fork();
        if (pid5 == 0)
        {
            info(BEGIN, 5, 0);

            //
            pid7 = fork();

                for(int i=0; i<NR_THREADS; i++)
                {
                    params[i].proc_nr = 5;
                    params[i].th_nr = i+1;
                    params[i].lock = &lock;
                    params[i].lock1 = &lock1;
                    params[i].lock2 = &lock2;
                    params[i].cond = &cond;
                    params[i].cond1 = &cond1;
                    params[i].cond2 = &cond2;
                    pthread_create(&tid[i], NULL, thread_sync_same_proc, &params[i]);
                    
                }

            //

            if(pid7 == 0)
            {
                info(BEGIN, 7, 0);
                
                //
                    for (int i = 0; i<SYNC_THREADS; i++)
                    {
                        s_params[i].proc_nr = 7;
                        s_params[i].th_nr = i+1;

                        pthread_create(&s_tid[i], NULL, thread_sync_diff_proc, &s_params[i]);
                    }

                    for (int i = 0; i<SYNC_THREADS; i++)
                    {
                        pthread_join(s_tid[i], NULL);
                    }
                    
                    
                //

                info(END, 7, 0);
                return 7;
            }
            else
            {
                int status7 = 0;
                waitpid(pid7, &status7, 0);
            }
            //
                
                for (int i = 0; i<NR_THREADS; i++)
                {
                    pthread_join(tid[i], NULL);
                }
                
                pthread_mutex_destroy(&lock);
                pthread_mutex_destroy(&lock1);
                pthread_mutex_destroy(&lock2);
                pthread_cond_destroy(&cond);
                pthread_cond_destroy(&cond1);
                pthread_cond_destroy(&cond2);

            //
            info(END, 5, 0);
            return 5;
        }
        else
        {
            int status5 = 0, status2 = 0;
            waitpid(pid5, &status5, 0);
            waitpid(pid1, &status2, 0);
        }
    }
    
    info(END, 1, 0);

    sem_unlink("sem1_process");
    sem_unlink("sem2_process");
    sem_destroy(sync1);
    sem_destroy(sync2);
    return 0;
}
