#include "kernel/types.h"
#include "user.h"


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