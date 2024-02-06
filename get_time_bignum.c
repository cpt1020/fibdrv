#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"
#define BUFFER_SIZE 512
#define OFFSET 500

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
        unsigned long long time3 = write(fd, buf, 3);
        unsigned long long time4 = write(fd, buf, 4);
        unsigned long long time5 = write(fd, buf, 5);
        unsigned long long time6 = write(fd, buf, 6);
        unsigned long long time7 = write(fd, buf, 7);
        /* Here, I use the "size_t size" parameter of write system call to
         * specify which fibonacci function to call
         *
         * size == 3: will call bignum_decimal
         * size == 4: will call bignum_bin_fibonacci
         * size == 5: will call bignum_bin_fast_doubling
         * size == 6: will call bignum_bin_fast_doubling_clz
         * size == 7: will call BIGNUM_fast_doubling_clz
         */
        printf("%d %llu %llu %llu %llu %llu\n", i, time3, time4, time5, time6, time7);
    }
    close(fd);
    return 0;
}