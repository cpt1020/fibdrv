/* thread_time.c */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#define FIB_DEV "/dev/fibonacci"
#define THREAD_NUM 2
#define ITERATIONS 1
#define FIB_START 1
#define FIB_END 100

/* for part I test */ 
typedef struct my_thread1 {
    int thread_id;
    int file;
    pthread_t thread;
} my_thread1;

/* for part II test */
typedef struct my_thread2 {
    int thread_id;
    pthread_t thread;
} my_thread2;

/* for part I test */ 
void *get_fib_num1(void *my_td)
{
    long long sz;
    char buf[64];
    int fd = (*(my_thread1 *)my_td).file;
    int id = (*(my_thread1 *)my_td).thread_id;

    for (int i = FIB_START; i <= FIB_END; i++) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 1);
    }
}

/* for part II test */
void *get_fib_num2(void *id)
{
    long long sz;
    char buf[64];
    int tid = *(int *)id;

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        goto end;
    }

    for (int i = FIB_START; i <= FIB_END; i++) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 1);
    }

end:
    close(fd);
}

int main()
{
    long long sum1 = 0, sum2 = 0, sum3 = 0;
    struct timespec t1_start, t1_end, t2_start, t2_end, t3_start, t3_end;
    
    for (int i = 0; i < ITERATIONS; ++i) {
        
        /* (Part I test) start of shared file descriptor, multiple-thread */
        clock_gettime(CLOCK_MONOTONIC, &t1_start);

        int fd = open(FIB_DEV, O_RDWR);
        if (fd < 0) {
            goto end;
        }

        my_thread1 threads1[THREAD_NUM];

        for (int i = 0; i < THREAD_NUM; ++i) {
            threads1[i].thread_id = i;
            threads1[i].file = fd;
            pthread_create((pthread_t *)&(threads1[i].thread), NULL, get_fib_num1, (void *)&threads1[i]);
        }

        for (int i = 0; i < THREAD_NUM; ++i) {
            pthread_join(threads1[i].thread, NULL);
        }

    end:
        close(fd);
        clock_gettime(CLOCK_MONOTONIC, &t1_end);
        /* (Part I test) end of shared file descriptor, multiple-thread */

        
        
        
        /* (Part II test) start of non-shared file descriptor, multiple-thread */
        clock_gettime(CLOCK_MONOTONIC, &t2_start);
        my_thread2 threads2[THREAD_NUM];

        for (int i = 0; i < THREAD_NUM; ++i) {
            threads2[i].thread_id = i;
            pthread_create((pthread_t *)&(threads2[i].thread), NULL, get_fib_num2, (void *)&(threads2[i].thread_id));
        }

        for (int i = 0; i < THREAD_NUM; ++i) {
            pthread_join(threads2[i].thread, NULL);
        }

        clock_gettime(CLOCK_MONOTONIC, &t2_end);
        /* (Part II test) end of non-shared file descriptor, multiple-thread */

        
        long long elapsed_ns1 = (t1_end.tv_sec - t1_start.tv_sec) * 1000000000 + (t1_end.tv_nsec - t1_start.tv_nsec);
        long long elapsed_ns2 = (t2_end.tv_sec - t2_start.tv_sec) * 1000000000 + (t2_end.tv_nsec - t2_start.tv_nsec);

        sum1 += elapsed_ns1;
        sum2 += elapsed_ns2;
    }
    
    
    
    
    /* (Part III test) start of single-thread */
    clock_gettime(CLOCK_MONOTONIC, &t3_start);
    
    long long sz;
    char buf[64];
    
    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        goto end2;
    }

    for (int i = 0; i < ITERATIONS; ++i) {
        for (int j = 0; j <= THREAD_NUM; ++j) {
            for (int j = FIB_START; j <= FIB_END; j++) {
                lseek(fd, j, SEEK_SET);
                sz = read(fd, buf, 1);
            }
        }
    }
    
    end2:
        close(fd);
    
    clock_gettime(CLOCK_MONOTONIC, &t3_end);
    /* (Part III test) end of single-thread */
    
    
    
    
    sum3 = (t3_end.tv_sec - t3_start.tv_sec) * 1000000000 + (t3_end.tv_nsec - t3_start.tv_nsec);

    printf("%lld %lld %lld\n", (sum1/ITERATIONS), (sum2/ITERATIONS), (sum3/ITERATIONS));

    pthread_exit(NULL);
    return 0;
}