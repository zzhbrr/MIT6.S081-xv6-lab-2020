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

struct hash_entry {
  struct buf head;
  struct spinlock hash_lock;
};

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  struct hash_entry hash_table[13];
  
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  for (int i = 0; i < 13; i ++) {
    initlock(&bcache.hash_table[i].hash_lock, "hash lock");
    bcache.hash_table[i].head.prev = &bcache.hash_table[i].head;
    bcache.hash_table[i].head.next = &bcache.hash_table[i].head;
  }
  int key = 0;
  for (b = bcache.buf; b < bcache.buf + NBUF; b ++) {
    initsleeplock(&b->lock, "buffer");
    key = (key + 1) % 13;
    b->next = bcache.hash_table[key].head.next;
    b->prev = &bcache.hash_table[key].head;
    bcache.hash_table[key].head.next->prev = b;
    bcache.hash_table[key].head.next = b;
  }
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  // for(b = bcache.buf; b < bcache.buf+NBUF; b++){
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   initsleeplock(&b->lock, "buffer");
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int key = blockno % 13;
  acquire(&bcache.hash_table[key].hash_lock);
  for (b = bcache.hash_table[key].head.next; b != &bcache.hash_table[key].head; b = b->next) {
    // printf("b = %p\n", b);
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      b->time_stamp = ticks;
      release(&bcache.hash_table[key].hash_lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.hash_table[key].hash_lock);
  acquire(&bcache.lock);
  acquire(&bcache.hash_table[key].hash_lock);
  // 需要再次在bucket[key]上查找一下，因为前面先release了bucket[key]的锁，有可能其他进程在这段时间已经在bucket[key]中插入了本block
  for (b = bcache.hash_table[key].head.next; b != &bcache.hash_table[key].head; b = b->next) {
    // printf("b = %p\n", b);
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      b->time_stamp = ticks;
      release(&bcache.hash_table[key].hash_lock);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  struct buf *resbuf = b;
  uint min_ticks = ticks*10;
  int res_bucket = -1;
  for (int i = 0; i < 13; i ++) { 
    if (i != key)
      acquire(&bcache.hash_table[i].hash_lock);
    for (b = bcache.hash_table[i].head.next; b != &bcache.hash_table[i].head; b = b->next) {
      if (b->refcnt == 0) {
        if (b->time_stamp < min_ticks) {
          min_ticks = b->time_stamp;
          resbuf = b;
          res_bucket = i;
        }
      }
    }
    if (i != key)
      release(&bcache.hash_table[i].hash_lock);
  }  
  if (res_bucket == -1) {
    panic("bget: no buffers");
  }
  if (res_bucket == key) {
    // printf("res_bucket == key\n");
    resbuf->dev = dev;
    resbuf->blockno = blockno;
    resbuf->valid = 0;
    resbuf->refcnt = 1;
    // resbuf->time_stamp = ticks;
    release(&bcache.hash_table[key].hash_lock);
    release(&bcache.lock);
    acquiresleep(&resbuf->lock);
    return resbuf;
  }
  if (res_bucket != key) {
    // printf("res_bucket < key\n");
    acquire(&bcache.hash_table[res_bucket].hash_lock);
    resbuf->dev = dev;
    resbuf->blockno = blockno;
    resbuf->valid = 0;
    resbuf->refcnt = 1;
    // resbuf->time_stamp = ticks;
    resbuf->prev->next = resbuf->next;
    resbuf->next->prev = resbuf->prev;
    resbuf->next = bcache.hash_table[key].head.next;
    resbuf->prev = &bcache.hash_table[key].head;
    bcache.hash_table[key].head.next->prev = resbuf;
    bcache.hash_table[key].head.next = resbuf;
    release(&bcache.hash_table[key].hash_lock);
    release(&bcache.hash_table[res_bucket].hash_lock);
    release(&bcache.lock);
    acquiresleep(&resbuf->lock);
    return resbuf;
  }


  panic("bget: no buffers [end]");

  // release(&bcache.hash_table[key].hash_lock);

  // struct buf *resbuf = b;
  // uint min_ticks = ticks*10;
  // int res_bucket = -1;

  // acquire(&bcache.lock);

  // for (int i = 0; i < 13; i ++) {
  //   acquire(&bcache.hash_table[i].hash_lock);
  //   for (b = bcache.hash_table[i].head.next; b != &bcache.hash_table[i].head; b = b->next) {
  //     if (b->refcnt == 0) {
  //       if (b->time_stamp < min_ticks) {
  //         min_ticks = b->time_stamp;
  //         resbuf = b;
  //         res_bucket = i;
  //       }
  //     }
  //   }
  //   release(&bcache.hash_table[i].hash_lock);
  // }
  // if (res_bucket == -1) {
  //   panic("bget: no buffers");
  // }
  // if (res_bucket == key) {
  //   // printf("res_bucket == key\n");
  //   acquire(&bcache.hash_table[key].hash_lock);
  //   resbuf->dev = dev;
  //   resbuf->blockno = blockno;
  //   resbuf->valid = 0;
  //   resbuf->refcnt = 1;
  //   // resbuf->time_stamp = ticks;
  //   release(&bcache.hash_table[key].hash_lock);
  //   release(&bcache.lock);
  //   acquiresleep(&resbuf->lock);
  //   return resbuf;
  // }
  // if (res_bucket < key) {
  //   // printf("res_bucket < key\n");
  //   acquire(&bcache.hash_table[res_bucket].hash_lock);
  //   acquire(&bcache.hash_table[key].hash_lock);
  //   resbuf->dev = dev;
  //   resbuf->blockno = blockno;
  //   resbuf->valid = 0;
  //   resbuf->refcnt = 1;
  //   // resbuf->time_stamp = ticks;
  //   resbuf->prev->next = resbuf->next;
  //   resbuf->next->prev = resbuf->prev;
  //   resbuf->next = bcache.hash_table[key].head.next;
  //   resbuf->prev = &bcache.hash_table[key].head;
  //   bcache.hash_table[key].head.next->prev = resbuf;
  //   bcache.hash_table[key].head.next = resbuf;
  //   release(&bcache.hash_table[key].hash_lock);
  //   release(&bcache.hash_table[res_bucket].hash_lock);
  //   release(&bcache.lock);
  //   acquiresleep(&resbuf->lock);
  //   return resbuf;
  // }
  // if (res_bucket > key) {
  //   // printf("res_bucket > key\n");
  //   acquire(&bcache.hash_table[key].hash_lock);
  //   acquire(&bcache.hash_table[res_bucket].hash_lock);
  //   resbuf->dev = dev;
  //   resbuf->blockno = blockno;
  //   resbuf->valid = 0;
  //   resbuf->refcnt = 1;
  //   // resbuf->time_stamp = ticks;
  //   resbuf->prev->next = resbuf->next;
  //   resbuf->next->prev = resbuf->prev;
  //   resbuf->next = bcache.hash_table[key].head.next;
  //   resbuf->prev = &bcache.hash_table[key].head;
  //   bcache.hash_table[key].head.next->prev = resbuf;
  //   bcache.hash_table[key].head.next = resbuf;
  //   release(&bcache.hash_table[res_bucket].hash_lock);
  //   release(&bcache.hash_table[key].hash_lock);
  //   release(&bcache.lock);
  //   acquiresleep(&resbuf->lock);
  //   return resbuf;    
  // }
  // panic("bget: no buffers [end]");
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
  int key = b->blockno % 13;
  releasesleep(&b->lock);
  acquire(&bcache.hash_table[key].hash_lock);
  b->refcnt--;
  if (b->refcnt == 0) b->time_stamp = ticks;
  release(&bcache.hash_table[key].hash_lock);
  // acquire(&bcache.lock);
  // b->refcnt--;
  // if (b->refcnt == 0) {
  //   // no one is waiting for it.
  //   b->next->prev = b->prev;
  //   b->prev->next = b->next;
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }
  
  // release(&bcache.lock);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


