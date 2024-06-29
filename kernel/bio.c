// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

//定义哈希表结构体
struct hash_map {
  struct buf head;
  struct spinlock lock;
};

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct hash_map hmap[NHASH];   //去除head,添加哈希表

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  for(int i = 0; i < NHASH; i++) {
    initlock(&bcache.hmap[i].lock, "bcachehash");
  }

  // 初始化hashtable
  uint i = 0;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    uint num = i % NHASH;
    i++;
    b->next = bcache.hmap[num].head.next;
    initsleeplock(&b->lock, "buffer");
    bcache.hmap[num].head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  const uint num = blockno % NHASH;
  struct hash_map* bucket;
  bucket = &bcache.hmap[num]; 
  acquire(&bucket->lock);

  // Is the block already cached?
  // 遍历桶
  for(b = bucket->head.next; b; b = b->next) {
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bucket->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  
  // Not cached.
  // 需要从其他桶中获取空闲的buf
  release(&bucket->lock);
  acquire(&bcache.lock);

  uint min_ticks = 0xffffffff;
  struct buf *before_freebuf = 0;     // freebuf前面的节点，方便从旧桶中删除
  struct hash_map *max_bucket = 0;    // 对应freebuf所在的hash桶，找到新的freebuf时释放这个桶的锁
  // 遍历所有的桶寻找LRU的空闲buf
  for(int i = 0; i < NHASH; i++) {
    //找最近最长时间未使用的buf
    int found = 0;    // 判断遍历的桶中有没有新的最近最少使用的空闲buf
    struct hash_map *bucket = &bcache.hmap[i];
    acquire(&bucket->lock);
    // 遍历当前桶中的buf
    for(b = &bucket->head; b->next; b = b->next) {
      if(b->next->refcnt == 0 && b->next->bticks < min_ticks) {        
        found = 1;
        min_ticks = b->next->bticks;    // 更新min_ticks
        before_freebuf = b;              // 更新freebuf
      }
    }
    if(found) {
      if(max_bucket != 0) release(&max_bucket->lock);   // 持有之前那个freebuf的桶需要释放锁
      max_bucket = bucket;                              // 持有当前freebuf的桶的锁
    } else {
      release(&bucket->lock);                           // 当前桶没有freebuf，需要释放锁
    }
  }

  // 没找到freebuf
  if(before_freebuf == 0) {
    panic("bget: no buffers");
  }

  // res是找到的freebuf，下面需要从原桶中删除并加入到目标桶
  // 删除
  struct buf *res = before_freebuf->next;   
  if(res != 0) {
    before_freebuf->next = before_freebuf->next->next;
    release(&max_bucket->lock);
  }

  acquire(&bucket->lock);
  // 重新获得目标桶的锁，中间的过程其他线程可能改变了桶中的buf使其
  // 恰好满足dev和blockno，所以需要重新检查一遍以满足每个块最多缓
  // 存一个副本的不变量
  for(b = bucket->head.next; b; b = b->next) {
    if(b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bucket->lock);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }  
  
  // 将freebuf加到目标桶的头部
  if(res != 0) {
    res->next = bucket->head.next;
    bucket->head.next = res;
  } else {
    panic("bget: no buffers");
  }

  res->dev = dev;
  res->blockno = blockno;
  res->valid = 0;
  res->refcnt = 1;
  release(&bucket->lock);
  release(&bcache.lock);
  acquiresleep(&res->lock);
  return res;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  uint num = b->blockno % NHASH;
  acquire(&bcache.hmap[num].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->bticks = ticks;
  }
  
  release(&bcache.hmap[num].lock);
}

void
bpin(struct buf *b) {
  uint num = b->blockno % NHASH;
  acquire(&bcache.hmap[num].lock);
  b->refcnt++;
  release(&bcache.hmap[num].lock);
}

void
bunpin(struct buf *b) {
  uint num = b->blockno % NHASH;
  acquire(&bcache.hmap[num].lock);
  b->refcnt--;
  release(&bcache.hmap[num].lock);
}


