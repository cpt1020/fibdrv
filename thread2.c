/* thread2.c */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#define FIB_DEV "/dev/fibonacci"
#define THREAD_NUM 4
#define START 11
#define END 15

typedef struct my_thread {
    int thread_id;
    pthread_t thread;
} my_thread;

void *get_fib_num(void *id)
{
    long long sz;
    char buf[64];

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Thread %d: Failed to open character device\n", *(int *)id);
        goto end;
    }

    for (int i = START; i <= END; i++) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 1);
        printf("Thread %d: Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%lld.\n",
               *(int *)id, i, sz);
    }

end:
    close(fd);
}

int main()
{
    my_thread threads[THREAD_NUM];

    for (int i = 0; i < THREAD_NUM; ++i) {
        threads[i].thread_id = i;
        pthread_create((pthread_t *)&(threads[i].thread), NULL, get_fib_num, (void *)&(threads[i].thread_id));
    }

    for (int i = 0; i < THREAD_NUM; ++i) {
        pthread_join(threads[i].thread, NULL);
    }
    
    pthread_exit(NULL);

    return 0;
}