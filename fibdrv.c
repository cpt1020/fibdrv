#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/version.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/ktime.h>
#include <linux/uaccess.h>
#include <linux/string.h>

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

#define MAX_LENGTH 500

static dev_t fib_dev = 0;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);
static int major = 0, minor = 0;

#define BIGNUM char

/* how much byte to store length of the char array */
/* in this case, I use sizeof(int) bytes to store it */
#define LEN_BYTE sizeof(int)

/* a macro to retrieve the number of length */
/* bn_ptr is a pointer to BIGNUM */
#define GET_LEN(bn_ptr) *(int *)(bn_ptr)

#define FREE_BIGNUM(bn_ptr) kfree(bn_ptr)
/* bn_ptr is a pointer to BIGNUM */

/* function to new a BIGNUM
/* @len: contains null terminator */
BIGNUM *bignum_new(int len)
{
    BIGNUM *res = (BIGNUM *)kmalloc(sizeof(BIGNUM) * (len + LEN_BYTE), GFP_KERNEL);
    *(unsigned int *)res = len;

    for (int i = LEN_BYTE; i < (len + LEN_BYTE); ++i) {
        res[i] = '0';
    }

    res[len + LEN_BYTE - 1] = '\0';
    return res;
}

BIGNUM *bignum_add(BIGNUM *num1, BIGNUM *num2)
{
    BIGNUM *res = bignum_new(GET_LEN(num2) + 1);

    int idx = 0, carry = 0;
    while (idx <= (GET_LEN(num1)) - 2) {
        int sum = ((char)*(num1 + LEN_BYTE + idx) - '0') + ((char)*(num2 + LEN_BYTE + idx) - '0') + carry;

        carry = 0;
        if (sum >= 2) {
            sum -= 2;
            carry = 1;
        }

        *(res + LEN_BYTE + idx) = (char)(sum + '0');
        idx++;
    }

    while (idx <= (GET_LEN(num2)) - 2) {
        int sum = (*(num2 + LEN_BYTE + idx) - '0') + carry;

        carry = 0;
        if (sum >= 2) {
            sum -= 2;
            carry = 1;
        }

        *(res + LEN_BYTE + idx) = (char)(sum + '0');
        idx++;
    }

    if (carry == 1) {
        *(res + LEN_BYTE + idx) = '1';
        idx++;
    }
    else {
        *(int *)res -= 1;
    }

    *(res + LEN_BYTE + idx) = '\0';
    return res;
}

BIGNUM *bignum_fib(long long k)
{
    BIGNUM *n1 = bignum_new(2);
    BIGNUM *n2 = bignum_new(2);
    *(n2 + LEN_BYTE) = '1';

    BIGNUM *sum;

    if (k == 0) {
        FREE_BIGNUM(n2);
        return n1;
    }
    else if (k == 1) {
        FREE_BIGNUM(n1);
        return n2;
    }

    for (int i = 2; i <= k; ++i) {
        sum = bignum_add(n1, n2);
        FREE_BIGNUM(n1);
        n1 = n2;
        n2 = sum;
    }
    FREE_BIGNUM(n1);

    return n2;
}

char *bignum_to_decimal(const BIGNUM *binary)
{
    size_t len = GET_LEN(binary)/3 + 2;
    char *decimal = (char *)kmalloc(sizeof(char) * len, GFP_KERNEL);

    for (int i = 0; i < len; ++i) {
        *(decimal + i) = '0';
    }

    for (int i = GET_LEN(binary) - 2 + LEN_BYTE; i >= LEN_BYTE; --i) {
        int digit = *(binary + i) - '0';

        for (int j = 0; j < len; ++j) {
            int tmp = (decimal[j] - '0') * 2 + digit;
            decimal[j] = (tmp % 10) + '0';
            digit = tmp / 10;
        }
    }

    // 找到 \0 應該要放的位置
    int idx = len - 1;
    for (int i = len - 1; i >= 0; --i) {
        if (decimal[i - 1] != '0') {
            decimal[i] = '\0';
            idx = i;
            break;
        }
    }

    // 把 string 反轉
    for (int i = 0; i < idx/2; ++i) {
        decimal[i] ^= decimal[idx - i - 1];
        decimal[idx - i- 1] ^= decimal[i];
        decimal[i] ^= decimal[idx - i - 1];
    }

    return decimal;
}

BIGNUM *bignum_mul(BIGNUM *n1, BIGNUM *n2)
{
    BIGNUM *res = bignum_new(GET_LEN(n1) - 1 + GET_LEN(n2) - 1 + 1);
    int n1_len = GET_LEN(n1), n2_len = GET_LEN(n2);

    for (int i = LEN_BYTE; i < n1_len - 1 + LEN_BYTE; ++i) {
        if (*(n1 + i) == '1') {
            int carry = 0;
            int digit = *(n1 + i) - '0';

            int j = LEN_BYTE;
            for (; j < n2_len - 1 + LEN_BYTE; ++j) {
                int tmp = (digit & (*(n2 + j) - '0')) + (*(res + i + j - LEN_BYTE) - '0') + carry;

                carry = 0;
                if (tmp >= 2) {
                    carry = 1;
                    tmp -= 2;
                }
                *(res + i + j - LEN_BYTE) = tmp + '0';
            }
            
            if (carry == 1) {
                *(res + i + j - LEN_BYTE) = '1';
            }
        }
    }

    for (int i = GET_LEN(res) - 1 + LEN_BYTE; i >= LEN_BYTE; --i) {
        if (*(res + i - 1) != '0') {
            *(res + i) = '\0';
            *(int *)res = i + 1 - LEN_BYTE;
            break;
        }
    }

    return res;
}

BIGNUM *bignum_lshift(BIGNUM *n, int offset)
{
    BIGNUM *res = bignum_new(GET_LEN(n) + offset);

    for (int i = LEN_BYTE; i < GET_LEN(n) - 1 + LEN_BYTE; ++i) {
        *(res + i + offset) = *(n + i);
    }

    return res;
}

BIGNUM *bignum_sub(BIGNUM *n1, BIGNUM *n2)
{
    int n1_len = GET_LEN(n1);
    int n2_len = GET_LEN(n2);
    BIGNUM *neg_n1 = bignum_new(n2_len);

    int carry = 1;
    int i = LEN_BYTE;
    for (; i < LEN_BYTE + n1_len - 1; ++i) {
        int tmp = ((*(n1 + i) - '0') ^ 1) + (*(n2 + i) - '0') + carry;
        carry = 0;

        if (tmp >= 2) {
            carry = 1;
            tmp -= 2;
        }

        *(neg_n1 + i) = tmp + '0';
    }

    for (; i < LEN_BYTE + n2_len - 1; ++i) {
        int tmp = (*(n2 + i) - '0') + 1 + carry;
        carry = 0;

        if (tmp >= 2) {
            carry = 1;
            tmp -= 2;
        }
        
        *(neg_n1 + i) = tmp + '0';
    }
    *(neg_n1 + i) = '\0';

    i = n2_len - 1 + LEN_BYTE;
    for (; i >= LEN_BYTE; --i) {
        if (*(neg_n1 + i - 1) != '0') {
            *(neg_n1 + i) = '\0';
            *(int *)neg_n1 = i + 1 - LEN_BYTE;
            break;
        }
    }

    return neg_n1;
}

BIGNUM *bignum_fast_doubling_clz(long long n)
{
    BIGNUM *a = bignum_new(2);
    BIGNUM *b = bignum_new(2);
    *(b + LEN_BYTE) = '1';

    for (unsigned long long i = 1 << (31 - __builtin_clzll(n)); i; i >>= 1) {
        BIGNUM *double_b = bignum_lshift(b, 1);
        BIGNUM *db_minus_a = bignum_sub(a, double_b);
        BIGNUM *t1 = bignum_mul(a, db_minus_a);

        BIGNUM *a_square = bignum_mul(a, a);
        BIGNUM *b_square = bignum_mul(b, b);
        BIGNUM *t2 = bignum_add(a_square, b_square);

        FREE_BIGNUM(a);
        FREE_BIGNUM(b);
        FREE_BIGNUM(double_b);
        FREE_BIGNUM(db_minus_a);
        FREE_BIGNUM(a_square);
        FREE_BIGNUM(b_square);

        if ((n & i) != 0) {
            a = t2;
            b = bignum_add(t1, t2);
            FREE_BIGNUM(t1);
        }
        else {
            a = t1;
            b = t2;
        }
    }

    FREE_BIGNUM(b);
    return a;
}





/* binary big_num struct definition */
typedef struct binary_big_num 
{
    int len;        /* the length of the char array, which contains the null terminator */
    char *number;   /* a pointer to a char array */
} bignum_bin;
/*
 * For example, for the number 28, whose binary form is 11100
 * this number will be stored in bignum_bin as following:
 * 
 * number: '0' '0' '1' '1' '1' '\0'
 * (index)  0   1   2   3   4   5
 * 
 * len: 6
 */

/*
 * function to create a new bignum_bin with designated length
 * the new bignum_bin will be all set to '0'
 * and the last element will set to '\0'
 * @len: the length of the char array, including '\0'
 */
bignum_bin *bignum_bin_new(int len)
{
    bignum_bin *num = (bignum_bin *)kmalloc(sizeof(bignum_bin), GFP_KERNEL);
    num->number = (char *)kmalloc(sizeof(char) * len, GFP_KERNEL);
    num->len = len;

    for (int i = 0; i < num->len - 1; i++) {
        num->number[i] = '0';
    }

    num->number[len - 1] = '\0';
    return num;
}

void bignum_bin_free(bignum_bin *num)
{
    kfree(num->number);
    kfree(num);
}

bignum_bin *bignum_bin_add(bignum_bin *num1, bignum_bin *num2)
{
    bignum_bin *res = bignum_bin_new(num2->len + 1);

    int idx = 0, carry = 0;
    while (idx <= (num1->len - 2)) {
        int sum = (int)(num1->number[idx] - '0') + (int)(num2->number[idx] - '0') + carry;
        carry = 0;
        
        if (sum >= 2)
        {
            sum -= 2;
            carry = 1;
        }

        res->number[idx] = (char)(sum + '0');
        idx++;
    }

    while (idx <= (num2->len -2)) {
        int sum = (int)(num2->number[idx] - '0') + carry;
        carry = 0;
        
        if (sum >= 2)
        {
            sum -= 2;
            carry = 1;
        }

        res->number[idx] = (char)(sum + '0');
        idx++;
    }

    if (carry == 1) {
        res->number[idx++] = '1';
    } 
    else {
        res->len -= 1;
    }

    res->number[idx] = '\0';

    return res;
}

bignum_bin *bignum_bin_fibonacci(long long k)
{
    bignum_bin *a = bignum_bin_new(2);
    bignum_bin *b = bignum_bin_new(2);
    b->number[0] = '1';
    bignum_bin *sum;

    if (k == 0) {
        bignum_bin_free(b);
        return a;
    }
    else if (k == 1) {
        bignum_bin_free(a);
        return b;
    }

    for (int i = 2; i <= k; ++i) {
        sum = bignum_bin_add(a, b);
        bignum_bin_free(a);
        a = b;
        b = sum;
    }
    bignum_bin_free(a);

    return b;
}

char *bignum_bin_to_decimal(const bignum_bin *binary)
{
    /* 
     * calculate the digit for decimal
     * a digit of a binary number is approximately 0.3 digit of decimal 
     */
    size_t len = (binary->len/3) + 2;

    char *decimal = (char *)kmalloc(len, GFP_KERNEL);
    
    memset(decimal, '0', len);

    /* convert binary to decimal */
    for (int i = binary->len - 2; i >= 0; --i) {
        int digit = binary->number[i] - '0';

        for (int j = 0; j < len; ++j) {
            int tmp = (decimal[j] - '0') * 2 + digit;
            decimal[j] = (tmp % 10) + '0';
            digit = tmp / 10;
        }
    }

    /* find the correct position for null terminator */
    int idx = len - 1;
    for (int i = len - 1; i >= 0; --i) {
        if (decimal[i - 1] != '0') {
            decimal[i] = '\0';
            idx = i;
            break;
        }
    }

    /* reverse the string */
    for (int i = 0; i < idx/2; ++i) {
        decimal[i] ^= decimal[idx - i - 1];
        decimal[idx - i- 1] ^= decimal[i];
        decimal[i] ^= decimal[idx - i - 1];
    }

    return decimal;
}

/*
 * function that multiply two bignum_bin
 * @num1: num1 must be <= num2
 */
bignum_bin *bignum_bin_mul(bignum_bin *num1, bignum_bin *num2) {

    /* the digit of the product is atmost
     * (num1->len - 1) + (num2->len - 1) + 1
     */
    bignum_bin *res = bignum_bin_new((num1->len - 1) + (num2->len - 1) + 1);

    /* 
     *      num2
     *   ×  num1
     *   -------
     */

    /* do the multiplication */
    for (int i = 0; i < num1->len - 1; ++i) {
        /* 
         * if the current digit of num1->number[i] is '0'
         * we can skip this digit
         * Otherwise, we have to do the calculation
         */
        if (num1->number[i] == '1') {
            int carry = 0;
            int digit = num1->number[i] - '0';

            int j = 0;
            for (; j < num2->len - 1; ++j) {
                int tmp = (digit & (num2->number[j] - '0')) + (res->number[i + j] - '0') + carry;
                carry = 0;
                if (tmp >= 2) {
                    carry = 1;
                    tmp %= 2;
                }
                res->number[i + j] = tmp + '0';
            }

            if (carry == 1) {
                res->number[i + j] = '1';
            }
        }
    }

    /* find the correct position for null terminator */
    for (int i = res->len - 1; i >= 0; --i) {
        if (res->number[i - 1] != '0') {
            res->number[i] = '\0';
            res->len = i + 1;
            break;
        }
    }

    return res;
}

/*
 * function that does left shift of bignum_bin
 * @num: the bignum_bin to perform left shift
 * @offset: how many digit to left shift
 */
bignum_bin *bignum_bin_lshift(bignum_bin *num, int offset)
{
    bignum_bin *res = bignum_bin_new(num->len + offset);

    for (int i = 0; i < num->len - 1; ++i) {
        res->number[i + offset] = num->number[i];
    }

    return res;
}

/*
 * function that performs num2 - num1
 * @num1: num1 must be <= num2
 * @num2: num2 must be >= num1
 * this function first create minum num1, then perform num2 + (minus num1)
 * minus num1: first toggle the bit of num1, then add one to it
 */
bignum_bin *bignum_bin_sub(bignum_bin *num1, bignum_bin *num2)
{
    /*
     * num2 is bigger than num1
     * num2 + (-num1)
     * (-num1) = num1 的 1/0 互換，最後 +1
     */

    /*
     * the negative num1 should have the same digit as num2
     * the rest of the digit will be filled with 1s for representing negative number
     */
    bignum_bin *neg_num1 = bignum_bin_new(num2->len);
    /* ex. 11000 (24) - 11 (3)
     * in this case, the binary form of -3 is 11101 (-16 + 8 + 4 + 1 = -3)
     */

    int carry = 1;
    // 因為 1/0 反轉後要 +1，這個 1 是加在 least significant bit
    // 也就是加在以下的 i == 0 的時候的
    // 所以當 i == 0，反轉完 least significant bit 之後，就給他加上 1 (也就是 carry)
    // 因為每次計算 tmp 都會 + carry，所以這裡利用 carry
    int i = 0;
    for (; i < num1->len - 1; ++i) {
        int tmp = ((num1->number[i] - '0') ^ 1) + (num2->number[i] - '0') + carry;
        carry = 0;

        if (tmp >= 2) {
            carry = 1;
            tmp -= 2;
        }

        neg_num1->number[i] = tmp + '0';
    }

    // the rest of the digit should be 1s
    // for representing negative number
    for (; i < num2->len - 1; ++i) {
        int tmp = (num2->number[i] - '0') + 1 + carry;
        carry = 0;

        if (tmp >= 2) {
            carry = 1;
            tmp -= 2;
        }
        neg_num1->number[i] = tmp + '0';
    }
    neg_num1->number[i] = '\0';

    // find the correct position for null terminator
    i = neg_num1->len - 1;
    for (; i >= 0; --i) {
        if (neg_num1->number[i - 1] != '0') {
            neg_num1->number[i] = '\0';
            neg_num1->len = i + 1;
            break;
        }
    }

    return neg_num1;
}

/*
 * function that create a bignum_bin of number n
 * @n: the number to be converted into bignum_bin form
 */
bignum_bin *bignum_bin_new_with_num(long long int n)
{
    int len = (31 - __builtin_clz(n)) + 1;
    bignum_bin *num = bignum_bin_new(len);

    long long mask = 1;
    for (int i = 0; i < len - 1; ++i) {
        if ((n & mask) != 0) {
            num->number[i] = '1';
        }

        mask <<= 1;
    } 

    return num;
}

bignum_bin *bignum_bin_fast_doubling(long long n)
{
    bignum_bin *a = bignum_bin_new(2);
    bignum_bin *b = bignum_bin_new(2);
    b->number[0] = '1';

    for (unsigned int i = (1 << 31); i; i >>= 1) {
        /* calculate t1 */
        bignum_bin *double_b = bignum_bin_lshift(b, 1);
        bignum_bin *db_minus_a = bignum_bin_sub(a, double_b);
        bignum_bin *t1 = bignum_bin_mul(a, db_minus_a);

        /* calculate t2 */
        bignum_bin *a_square = bignum_bin_mul(a, a);
        bignum_bin *b_square = bignum_bin_mul(b, b);
        bignum_bin *t2 = bignum_bin_add(a_square, b_square);

        bignum_bin_free(a);
        bignum_bin_free(b);
        bignum_bin_free(double_b);
        bignum_bin_free(db_minus_a);
        bignum_bin_free(a_square);
        bignum_bin_free(b_square);
        
        if ((n & i) != 0) {
            a = t2;
            b = bignum_bin_add(t1, t2);
            bignum_bin_free(t1);
        }
        else {
            a = t1;
            b = t2;
        }
    }

    bignum_bin_free(b);
    return a;
}

bignum_bin *bignum_bin_fast_doubling_clz(long long n)
{
    bignum_bin *a = bignum_bin_new(2);
    bignum_bin *b = bignum_bin_new(2);
    b->number[0] = '1';

    for (unsigned long long i = 1 << (31 - __builtin_clzll(n)); i; i >>= 1) {
        /* calculate t1 */
        bignum_bin *double_b = bignum_bin_lshift(b, 1);
        bignum_bin *db_minus_a = bignum_bin_sub(a, double_b);
        bignum_bin *t1 = bignum_bin_mul(a, db_minus_a);

        /* calculate t2 */
        bignum_bin *a_square = bignum_bin_mul(a, a);
        bignum_bin *b_square = bignum_bin_mul(b, b);
        bignum_bin *t2 = bignum_bin_add(a_square, b_square);

        bignum_bin_free(a);
        bignum_bin_free(b);
        bignum_bin_free(double_b);
        bignum_bin_free(db_minus_a);
        bignum_bin_free(a_square);
        bignum_bin_free(b_square);
        
        if ((n & i) != 0) {
            a = t2;
            b = bignum_bin_add(t1, t2);
            bignum_bin_free(t1);
        }
        else {
            a = t1;
            b = t2;
        }
    }

    bignum_bin_free(b);
    return a;
}



/* bignum_decimal struct definition */
typedef struct bignum_decimal 
{
    int len;        /* the length of the char array, which contains the null terminator */
    char *number;   /* a pointer to a char array */
} bignum_decimal;

/* ex. 24680 這個數字會存成：
 * 
 * idx:     0   1   2   3   4   5 
 * number: '0' '8' '6' '4' '2' '\0'
 * len 就是 6
 */ 

/* function to dynamically allocate a new bignum_decimal
 * @len: the digits of the number to create plus null terminator
 * return: a pointer to bignum_decimal
 */
bignum_decimal *new_bignum_decimal(int len) 
{
    bignum_decimal *new_num = (bignum_decimal *)kmalloc(sizeof(bignum_decimal), GFP_KERNEL);
    new_num->number = (char *)kmalloc(sizeof(char) * len, GFP_KERNEL);
    new_num->len = len;

    for (int i = 0; i < len; ++i) {
        new_num->number[i] = '0';
    }
    new_num->number[len - 1] = '\0';
    
    return new_num;
}

/* function to free bignum_decimal
 */
void free_bignum_decimal(bignum_decimal *num)
{
    kfree(num->number);
    kfree(num);
}

/* function to add 2 bignum_decimal
 * @num2 must be bigger than num1
 */
bignum_decimal *add_two_bignum_decimal(bignum_decimal *num1, bignum_decimal *num2)
{
    bignum_decimal *res = new_bignum_decimal(num2->len + 1);
    /* assume the result carries, so the length is (num2->len + 1)
    * if no carry, the length will be corrected lastly
    */
   res->number[num2->len] = '\0';

   int idx = 0, carry = 0;
   
   while (idx <= (num1->len - 2)) {
       int tmp = (int)(num1->number[idx] - '0') + (int)(num2->number[idx] - '0') + carry;
       carry = 0;

       if (tmp >= 10) {
           tmp -= 10;
           carry = 1;
       }

       res->number[idx] = (char)(tmp + '0');
       idx++;
   }

   while (idx <= (num2->len - 2)) {
        int tmp = (int)(num2->number[idx] - '0') + carry;
        carry = 0;

        if (tmp >= 10) {
            tmp -= 10;
            carry = 1;
        }

        res->number[idx] = (char)(tmp + '0');
        idx++;
   }

   if (carry == 1) {
       res->number[idx++] = '1';
   } 
   else {
       res->len -= 1;
   }

   res->number[idx] = '\0';

   return res;
}

bignum_decimal *bignum_decimal_fibonacci(long long k)
{
    bignum_decimal *num1 = new_bignum_decimal(2);
    bignum_decimal *num2 = new_bignum_decimal(2);
    num2->number[0] = '1';
    bignum_decimal *sum;

    if (k == 0) {
        free_bignum_decimal(num2);
        return num1;
    }
    else if (k == 1) {
        free_bignum_decimal(num1);
        return num2;
    }

    for (int i = 2; i <= k; ++i) {
        sum = add_two_bignum_decimal(num1, num2);
        free_bignum_decimal(num1);
        num1 = num2;
        num2 = sum;
    }
    free_bignum_decimal(num1);

    return num2;
}

char *reverse_bignum_decimal_string(bignum_decimal *n)
{
    int len = (n->len - 1);
    for (int i = 0; i < len >> 1; ++i) {
        n->number[i] ^= n->number[len - i - 1];
        n->number[len - i - 1] ^= n->number[i];
        n->number[i] ^= n->number[len - i - 1];
    }
    
    return n->number;
}


static long long fib_sequence(long long k)
{
    /* if F0 or F1, then return F0 or F1 */
    if (k < 2) {
        return k;
    }

    /* F0 = 0 = n1
     * F1 = 1 = n2
     * F2 = 1 = n3
     */
    long long n1 = 0, n2 = 1, n3 = 1;
    
    for (int i = 2; i <= k; ++i) {
        n3 = n1 + n2;
        n1 = n2;
        n2 = n3;
    }

    return n2;
}

/* function that calculates fibonacci number using fast doubling
 */
static unsigned long long fast_doubling(long long n)
{
    long long a = 0, b = 1;

    for (unsigned int i = 1U << 31; i; i >>= 1)
    {
        long long t1 = a * (2 * b - a);
        long long t2 = a * a + b * b;

        if ((n & i) != 0)
        {
            a = t2;
            b = t1 + t2;
        } else
        {
            a = t1;
            b = t2;
        }
    }
    
    return a;
}

/* function that calculates fibonacci number using fast doubling
 * with clz for speeding up
 */
static long long fast_doubling_clz(long long n)
{
    long long a = 0, b = 1;

    for (unsigned int i = (1U << (31 - __builtin_clz(n))); i; i >>= 1)
    {
        long long t1 = a * (2 * b - a);
        long long t2 = a * a + b * b;

        if ((n & i) != 0)
        {
            a = t2;
            b = t1 + t2;
        } else
        {
            a = t1;
            b = t2;
        }
    }

    return a;
}

static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use\n");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    char *fib_num = NULL;
    if (size == 0) {
        return (ssize_t) fib_sequence(*offset);
    }
    else if (size == 1) {
        return (ssize_t) fast_doubling(*offset);
    }
    else if (size == 2) {
        return (ssize_t) fast_doubling_clz(*offset);
    }
    else if (size == 3) {
        bignum_decimal *num = bignum_decimal_fibonacci(*offset);
        fib_num = reverse_bignum_decimal_string(num);
    }
    else if (size == 4) {
        bignum_bin *num = bignum_bin_fibonacci(*offset);
        fib_num = bignum_bin_to_decimal(num);
    }
    else if (size == 5) {
        bignum_bin *num = bignum_bin_fast_doubling(*offset);
        fib_num = bignum_bin_to_decimal(num);
    }
    else if (size == 6) {
        bignum_bin *num = bignum_bin_fast_doubling_clz(*offset);
        fib_num = bignum_bin_to_decimal(num);
    }
    else if (size == 7) {
        BIGNUM *num = bignum_fast_doubling_clz(*offset);
        fib_num = bignum_to_decimal(num);
    }
    else {
        return 0;
    }
    
    ssize_t len = strlen(fib_num) + 1;
    ssize_t retval = copy_to_user(buf, fib_num, len);
    if (retval == 0) {
        retval = len;
    }
    else {
        retval = -EFAULT;
    }
    
    return retval;
}

static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    ktime_t start_time, end_time;
    s64 elapsed_time;

    if (size == 0) {
        /* test the execution time of iterative version of fibonacci number */
        start_time = ktime_get();
        fib_sequence(*offset);
        end_time = ktime_get();
    } else if (size == 1) {
        /* test the execution time of fast doubling version */
        start_time = ktime_get();
        fast_doubling(*offset);
        end_time = ktime_get();
    } else if (size == 2) {
        /* test the execution time of fast doubling version using clz */
        start_time = ktime_get();
        fast_doubling_clz(*offset);
        end_time = ktime_get();
    } else if (size == 3) {
        /* test the execution time of bignum_decimal */
        start_time = ktime_get();
        bignum_decimal *num = bignum_decimal_fibonacci(*offset);
        char *fib_num = reverse_bignum_decimal_string(num);
        end_time = ktime_get();
    } else if (size == 4) {
        /* test the execution time of bignum_bin_fibonacci
         * the iterative version of fibonacci
         */
        start_time = ktime_get();
        bignum_bin *num = bignum_bin_fibonacci(*offset);
        char *fib_num = bignum_bin_to_decimal(num);
        end_time = ktime_get();
    } else if (size == 5) {
        /* test the execution time of bignum_bin_fast_doubling_clz */
        start_time = ktime_get();
        bignum_bin *num = bignum_bin_fast_doubling(*offset);
        char *fib_num = bignum_bin_to_decimal(num);
        end_time = ktime_get();
    } else if (size == 6) {
        /* test the execution time of bignum_bin_fast_doubling_clz */
        start_time = ktime_get();
        bignum_bin *num = bignum_bin_fast_doubling_clz(*offset);
        char *fib_num = bignum_bin_to_decimal(num);
        end_time = ktime_get();
    } else if (size == 7) {
        /* test the execution time of bignum_fast_doubling_clz */
        start_time = ktime_get();
        BIGNUM *num = bignum_fast_doubling_clz(*offset);
        char *fib_num = bignum_to_decimal(num);
        end_time = ktime_get();
    }
    
    elapsed_time = ktime_to_ns(ktime_sub(end_time, start_time));
    return elapsed_time;
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;
    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = major = register_chrdev(major, DEV_FIBONACCI_NAME, &fib_fops);
    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev\n");
        rc = -2;
        goto failed_cdev;
    }
    fib_dev = MKDEV(major, minor);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    fib_class = class_create(DEV_FIBONACCI_NAME);
#else
    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);
#endif
    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class\n");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device\n");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
failed_cdev:
    unregister_chrdev(major, DEV_FIBONACCI_NAME);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    unregister_chrdev(major, DEV_FIBONACCI_NAME);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
