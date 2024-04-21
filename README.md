# trace
###### 调用流程(read为例）
![2024-04-21_14-46.png](https://cdn.nlark.com/yuque/0/2024/png/34511493/1713682037957-ce64102c-c23c-4161-b847-7fdcf0c72f98.png#averageHue=%23fbfbfb&clientId=u659d1797-080e-4&from=drop&id=u32c69fed&originHeight=823&originWidth=683&originalType=binary&ratio=1.3043478260869565&rotation=0&showTitle=false&size=77706&status=done&style=none&taskId=ud500fd9e-1eb8-49e5-a42a-0484826889c&title=)

###### Makefile文件修改
```c
UPROGS=\
    ...
    $U/_trace\      	//add
```
###### user/user.h修改
```c
// system calls
...
int trace(int);			//add
```
###### user/usys.pl修改
```c
entry("trace");			//add
```
###### syscall.h修改
```c
#define SYS_trace  22	//add
```
###### proc.h修改
进程结构体中添加掩码变量，后续在sysproc.c中为其赋值，在syscall.c中获取掩码并打印相关信息
```c
struct proc {
  ...
  int trace_mask;
};
```
###### sysproc.c修改
添加sys_trace函数
```c
uint64
sys_trace(void)
{
  int mask;    //掩码mask 如trace 32 grep hello README中的32
  struct proc *p = myproc();
  if(argint(0, &mask) < 0)
    return -1;
  p->trace_mask = mask;
  return 0;
}
```
将跟踪掩码从父进程复制到子进程(解决不使用trace时输出参数的问题)
```c
int 
fork(void)
{
    ...
    //copy trace_mask from parent to child
    np->trace_mask = p->trace_mask;
}
```
###### syscall.c修改
```c
extern uint64 sys_trace(void);		//add
...
[SYS_trace]   sys_trace,			//add
...
const char* syscall_names[] = {
  "fork",
  "exit",
  "wait",
  "pipe",
  "read",
  "kill",
  "exec",
  "fstat",
  "chdir",
  "dup",
  "getpid",
  "sbrk",
  "sleep",
  "uptime",
  "open",
  "write",
  "mknod",
  "unlink",
  "link",
  "mkdir",
  "close",
  "trace",
};

void
syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {    
    /**
     * 系统调用的返回值
     * 接收mask
     * 判断mask是否有效
     * 有效就打印相关参数
     */
    p->trapframe->a0 = syscalls[num]();
    int mask = p->trace_mask;
    if((mask >> num) & 1)
    {
      printf("%d syscall %s -> %d\n", p->pid, syscall_names[num - 1], p->trapframe->a0);
    }
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
```
# sysinfo
###### Makefile修改
```c
	$U/_sysinfotest\		//add
```
###### **user/user.h修改**
```c
struct sysinfo;
...
int sysinfo(struct sysinfo *);
```
###### **user/usys.pl修改**
```c
entry("sysinfo");		//add
```
生成汇编指令调用ecall进入内核(程序运行自动生成，不需要修改）
```c
.global sysinfo
sysinfo:
 li a7, SYS_sysinfo
 ecall
 ret
```
###### **kernel/syscall.h修改**
```c
#define SYS_sysinfo 23
```
###### **kernel/sysproc.c修改**
添加sys_sysinfo函数，根据**"sys_fstat()(_kernel/sysfile.c_)和filestat()(_kernel/file.c_)以获取如何使用copyout()"**来实现，**两个acquire函数需要在defs.h中声明,分别在kalloc.c和proc.c中定义**
```c
#include "sysinfo.h"
...
uint64
sys_sysinfo(void)
{
  struct sysinfo info;
  uint64 addr;
  info.freemem = acquire_freemem();
  info.nproc = acquire_nproc();
  struct proc *p = myproc();
  //0表示接收的第0个参数
  if(argaddr(0, &addr) < 0)
    return -1;
  //把info的数据拷贝到addr里
  if(copyout(p->pagetable, addr, (char *)&info, sizeof(info)) < 0)
    return -1;
  return 0;
}
```
###### **kernel/defs.h修改**
```c
// kalloc.c
...
uint64          acquire_freemem(void);	//add

// proc.c
...
uint64          acquire_nproc(void);	//add
```
###### **kernel/syscall.c修改**
```c
extern uint64 sys_sysinfo(void);	//add
...
static uint64 (*syscalls[])(void) = {
...
[SYS_sysinfo] sys_sysinfo,			//add
}

const char* syscall_names[] = {
...
"sysinfo",  						//add
}
```
###### **kernel/kalloc.c修改**
```c
//while循环获得链表的长度即可,乘以每页的字节数PGSIZE
uint64
acquire_freemem(void)
{
  struct run *r;
  uint64 count = 0;
  acquire(&kmem.lock);
  r = kmem.freelist;
  while(r)
  {
    r = r->next;
    count++;
  }
  release(&kmem.lock);
  return PGSIZE * count;
}
```
###### **kernel/proc.c修改**
```c
//参考allocproc()函数，添加计数器就行
uint64
acquire_nproc(void)
{
  struct proc *p;
  int count = 0;
  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state != UNUSED) {
      count++;
    }
    release(&p->lock);
  }
  return count;
}
```
