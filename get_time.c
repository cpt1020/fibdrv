#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"
#define BUFFER_SIZE 512
#define OFFSET 93

int main()
{
    char buf[BUFFER_SIZE];

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 1; i <= OFFSET; ++i) {
        lseek(fd, i, SEEK_SET);
        unsigned long long time1 = write(fd, buf, 0);
        unsigned long long time2 = write(fd, buf, 1);
        unsigned long long time3 = write(fd, buf, 2);
        /* Here, I use the "size_t size" parameter of write system call to
         * specify which fibonacci function to call
         *
         * j == 0: will call fibonacci_sequence in fibdrv module
         * j == 1: will call fast_doubling in fibdrv module
         * j == 2: will call fast_doubling_clz in fibdrv module
         */
        printf("%d %llu %llu %llu\n", i, time1, time2, time3);
    }
    close(fd);
    return 0;
}