/* thread1.c */
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
    int file;
    pthread_t thread;
} my_thread;

void *get_fib_num(void *my_td)
{
    long long sz;
    char buf[64];
    int fd = (*(my_thread *)my_td).file;
    int id = (*(my_thread *)my_td).thread_id;

    for (int i = START; i <= END; i++) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 1);
        printf("Thread %d: Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%lld.\n",
               id, i, sz);
    }
}

int main()
{
    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device\n");
        goto end;
    }
    
    my_thread threads[THREAD_NUM];

    for (int i = 0; i < THREAD_NUM; ++i) {
        threads[i].thread_id = i;
        threads[i].file = fd;
        pthread_create((pthread_t *)&(threads[i].thread), NULL, get_fib_num, (void *)&threads[i]);
    }

    for (int i = 0; i < THREAD_NUM; ++i) {
        pthread_join(threads[i].thread, NULL);
    }
    
    pthread_exit(NULL);

end:
    close(fd);

    return 0;
}