#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"
#define BUFFER_SIZE 512
#define OFFSET 100

int main()
{
    long long sz;
    char buf[BUFFER_SIZE];

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }
    
    char *print_title[] = {"Fibonacci by fib_sequence",
                           "Fibonacci by fast doubling",
                           "Fibonacci by fast doubling with clz",
                           "Fibonacci by bignum_decimal",
                           "Fibonacci by bignum_bin fibonacci",
                           "Fibonacci by bignum_bin fast_doubling",
                           "Fibonacci by bignum_bin fast_doubling_clz",
                           "Fibonacci by BIGNUM fast_doubling_clz"};
    
    for (int j = 0; j < 3; j++) {
        printf("\n%s\n", print_title[j]);

        for (int i = 1; i <= OFFSET; i++) {
            lseek(fd, i, SEEK_SET);
            /* use SEEK_SET to set "loff_t *offset" as i
            * so while calling read function call,
            * we don't need to specifiy the "loff_t *offset" parameter
            */
    
            sz = read(fd, buf, j);
            /* Here, I use the "size_t size" parameter of read function call to
             * specify which fibonacci function to call
             *
             * j == 0: will call fib_sequence in fibdrv module
             * j == 1: will call fast_doubling in fibdrv module
             * j == 2: will call fast_doubling_clz in fibdrv module
             */
            
            printf("Fib num (%d): %lld.\n", i, sz);
        }
    }

    close(fd);
    return 0;
}