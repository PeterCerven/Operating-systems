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

#define NBUCKET 13

uint64 my_ticks;

struct {
  struct spinlock lock;
  struct spinlock bucket_lock[NBUCKET];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf check;
  struct buf head[NBUCKET];
} bcache;

void
move_bucket(struct buf *b, int  destionation_bucket) {
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head[destionation_bucket].next;
    b->prev = &bcache.head[destionation_bucket];
    bcache.head[destionation_bucket].next->prev = b;
    bcache.head[destionation_bucket].next = b;  
}

void
binit(void)
{
  struct buf *b;
  //struct buf *c;
  my_ticks = 0;

  initlock(&bcache.lock, "bcache");
  for (int i = 0; i < NBUCKET; i++) {
    initlock(&bcache.bucket_lock[i], "bcache.bucket");

  // Create linked lists of buffers
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
  }
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->ticks = 0;
    b->blockno = 0;
    b->next = bcache.head[0].next;
    b->prev = &bcache.head[0];
    initsleeplock(&b->lock, "buffer");
    bcache.head[0].next->prev = b;
    bcache.head[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b, *cur_best;

  int bucket = blockno % NBUCKET;
  // printf("acquire bucket: %d\n", bucket);
  acquire(&bcache.bucket_lock[bucket]);

  // Is the block already cached?
  for(b = bcache.head[bucket].next; b != &bcache.head[bucket]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucket_lock[bucket]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  // printf("acquire bcache.lock: \n");
  acquire(&bcache.lock);

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.

  //find first with 0 references
  for (int j = 0; j < NBUF; j++) {
    cur_best = bcache.buf + j;
    int candidate_bucket = cur_best->blockno % NBUCKET;
    cur_best->lock_num = candidate_bucket;
    if (candidate_bucket != bucket) {
      // printf("first acquire bucket: %d cur_best: %d\n", bucket, cur_best->lock_num);
      acquire(&bcache.bucket_lock[cur_best->lock_num]);
    }
    if(cur_best->refcnt == 0) {
      break;
    } else if (candidate_bucket != bucket) {
      // printf("first release bucket: %d cur_best: %d\n", bucket, cur_best->lock_num);
      release(&bcache.bucket_lock[cur_best->lock_num]);
    }
  }
  
  // find the best LRU
  for(int i = 0; i < NBUF; i++){
    b = bcache.buf + i;
    int candidate_bucket = b->blockno % NBUCKET;
    if (candidate_bucket != bucket && candidate_bucket != cur_best->lock_num) {
      // printf("second acquire bucket: %d b->lock_num: %d cur_best: %d\n", bucket, b->lock_num, cur_best->lock_num);
      acquire(&bcache.bucket_lock[candidate_bucket]);
    }
    b->lock_num = candidate_bucket;
    if(b->refcnt == 0) {
      if (b->ticks < cur_best->ticks) {
        int prev = cur_best->lock_num;
        cur_best = b;
        if (prev != bucket && prev != cur_best->lock_num) {
          // printf("second release bucket: %d b->lock_num: %d cur_best: %d\n", bucket, b->lock_num, cur_best->lock_num);
          release(&bcache.bucket_lock[prev]);
        }
      } else {
        if (candidate_bucket != bucket && candidate_bucket != cur_best->lock_num) {
          // printf("third release bucket: %d b->lock_num: %d cur_best: %d\n", bucket, b->lock_num, cur_best->lock_num);
          release(&bcache.bucket_lock[b->lock_num]);
        }
      }
    } else {
      if (candidate_bucket != bucket && candidate_bucket != cur_best->lock_num){
        // printf("fourth release bucket: %d b->lock_num: %d cur_best: %d\n", bucket, b->lock_num, cur_best->lock_num);
        release(&bcache.bucket_lock[b->lock_num]);
      }
    }
    
      
  }
  
  if (cur_best->refcnt == 0) {
    cur_best->dev = dev;
    cur_best->blockno = blockno;
    cur_best->valid = 0;
    cur_best->refcnt = 1;
    if (cur_best->lock_num != bucket)  {
        move_bucket(cur_best, bucket);
        // printf("fifth release bucket: %d b->lock_num: %d cur_best: %d\n", bucket, b->lock_num, cur_best->lock_num);
        release(&bcache.bucket_lock[cur_best->lock_num]);
        // printf("success\n");
    }
    release(&bcache.lock);
    release(&bcache.bucket_lock[bucket]);
    acquiresleep(&cur_best->lock);
    // printf("sus\n");
    return cur_best;
  }
  if (cur_best->lock_num != bucket) {
      // printf("sixth release bucket: %d b->lock_num: %d cur_best: %d\n", bucket, b->lock_num, cur_best->lock_num);
      release(&bcache.bucket_lock[cur_best->lock_num]);
      // printf("success\n");

  }
  // for(b = bcache.buf; b != bcache.buf + NBUF; b++){
  //   printf("reference: %d\n", b->refcnt);
  // }

  panic("bget: no buffers");
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

  int bucket = b->blockno % NBUCKET;
  acquire(&bcache.bucket_lock[bucket]);
  b->refcnt--;
  if (b->refcnt == 0) {
    //TODO set ticks
    b->ticks = my_ticks;
    my_ticks++;
    //printf("ticks: %d\n", b->ticks);
  }
  
  release(&bcache.bucket_lock[bucket]);
}

void
bpin(struct buf *b) {
  int bucket = b->blockno % NBUCKET;
  acquire(&bcache.bucket_lock[bucket]);

  b->refcnt++;
  release(&bcache.bucket_lock[bucket]);
}

void
bunpin(struct buf *b) {
  int bucket = b->blockno % NBUCKET;
  acquire(&bcache.bucket_lock[bucket]);

  b->refcnt--;
  release(&bcache.bucket_lock[bucket]);
}


