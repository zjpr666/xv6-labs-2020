# sleep
1. 修改Makefile文件
```c
UPROGS=\
    ... 
    $U/_sleep\		//add
```

2. 添加user/sleep.c文件
```c
#include "user.h"
#include "kernel/types.h"

int main(int argc, char* const argv[])
{
    if(argc != 2)
    {
        fprintf(2,"usage: sleep <time>\n");
        exit(1);
    }
    sleep(atoi(argv[1]));
    exit(0);
}
```
```c
int atoi(const char* s)
{
    int n = 0;
    while('0' <= *s && *s <= '9')
    {
        n = n * 10 + (*s++ - '0');
    }
    return n;
}
```
# pingpong

1. 修改Makefile文件
```c
UPROGS=\
    ... 
    $U/_pingpong\		//add
```

2. 添加user/pingpong.c文件

对于每一个进程来说都是只向另外一个管道write，自己只read另外一个管道
```c
#include "user.h"
#include "kernel/types.h"

int main()
{
    int p1[2];
    int p2[2];
    pipe(p1);
    pipe(p2);
    char byte = 'p';
    int status;
    if(fork() == 0)
    {
        close(p1[1]);
        close(p2[0]);
        read(p1[0], &byte, sizeof byte);
        printf("%d: received ping\n", getpid());
        write(p2[1], &byte, sizeof byte);
    } else {
        close(p1[0]);
        close(p2[1]);
        write(p1[1], &byte, sizeof byte);
        read(p2[0], &byte, sizeof byte);
        printf("%d: received pong\n", getpid());
        wait(&status);
    }
    exit(0);
}
```
# primes

1. 修改Makefile文件
```c
UPROGS=\
    ... 
    $U/_primes\		//add
```

2. 添加user/primes.c文件
```c
#include "user.h"
#include "kernel/types.h"

void solve(int *p)
{
    /**
     * 在子进程中不需要父进程的写端，关闭
     * 判断父进程是否读完
     * 没读完就创建字进程，递归处理，关闭子进程的写端和父进程的读端
    */
    //关闭父进程的写端
    close(p[1]);
    int prime = -1;
    if(read(p[0], &prime, sizeof prime) == 0) return;
    printf("prime %d\n", prime);
    //创建子进程
    int pr[2];
    pipe(pr);
    if(fork() == 0)
    {
        close(pr[1]);
        close(p[0]);
        solve(pr);
        exit(0);
    }
    //父进程
    close(pr[0]);
    for(int i; read(p[0], &i, sizeof i) != 0;)
    {
        if(i % prime != 0)
        {
            write(pr[1], &i, sizeof i);
        }
    }
    close(pr[1]);
    wait(0);
}

int main(int argc, char* argv[])
{
    /**
     * 父进程
     *   创建第一个进程把所有的数据全都写到第一个管道里
     *   关闭读取端p[0]
     *   循环输入数据到写端p[1]
     *   关闭写端
     *   等待子进程结束
     * 子进程
     *   递归解决函数
     * 退出exit
     */
    int p[2];
    pipe(p);
    if(fork() != 0)
    {
        close(p[0]);
        for(int i = 2;i < 36;i++)
        {
            write(p[1], &i, sizeof i);
        }
        close(p[1]);

        wait(0);
    }
    else
    {
        solve(p);
    }
    exit(0);
}
```
# find

1. 修改Makefile文件
```c
UPROGS=\
    ... 
    $U/_find\		//add
```

2. 添加user/find.c文件
# xargs

1. 修改Makefile文件
```c
UPROGS=\
    ... 
    $U/_xargs\		//add
```

2. 添加user/xargs.c文件
