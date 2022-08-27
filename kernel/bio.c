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

#define BUCKETNUM 13
#define NULL (void *)0

extern uint ticks;

// Every bucket has its own lock to protect the linked list
// bufhead_ptr is the head of a doubly linked list
struct bucket {
  struct spinlock lock;
  struct buf* bufhead_ptr;
};


struct {
  struct spinlock global_lock;
  struct buf buf[NBUF];

  // Use a hashtable to reduce the granularity of lock
  struct bucket hash_table[BUCKETNUM];
} bcache;



int hash_function(uint dev, uint blockno) {
  return (dev + blockno) % BUCKETNUM;
}

// This function changes linked list, the correspond lock must be held 
void linkedlist_insert(struct buf** list, struct buf* item) {
  struct buf *temp;

  if (!(*list)) {
    *list = item;
    item->next = NULL;
    item->prev = NULL;
  } else {
    temp = *list;
    *list = item;
    item->next = temp;
    temp->prev = item;
    item->prev = NULL;
  }
}

// This function changes linked list, the correspond lock must be held 
void linkedList_delete(struct buf** list, struct buf* item) {
  struct buf* prev;

  if (!(item->prev)) {
    // the first node in this linked list
    (*list) = item->next;
    if (item->next) {
      // Not the only node in this linked list
      item->next->prev = NULL;
    }
  } else {
    prev = item->prev;
    prev->next = item->next;
    if (item->next) {
      item->next->prev = prev;
    }
  }

  item->prev = NULL;
  item->next = NULL;
}

void
binit(void)
{
  uint timestamp = ticks;
  struct buf *b;
  int i = 0;

  initlock(&bcache.global_lock, "bcache.global_lock");
  for (int i = 0; i < BUCKETNUM; i++) {
    initlock(&bcache.hash_table[i].lock, "bcache.bucket");
    bcache.hash_table[i].bufhead_ptr = NULL;
  }

  for(b = bcache.buf; b < bcache.buf+NBUF; b++, i++){
    initsleeplock(&b->lock, "buffer");
    b->last_use = timestamp;
    linkedlist_insert(&bcache.hash_table[i%BUCKETNUM].bufhead_ptr, b);
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *linkedlist_head;
  struct buf *tran;
  struct buf *lru_buf = NULL;
  uint min_ticks = ~0;
  int new_min_exist = 0;
  int current_hold_index = -1;
  int key = hash_function(dev, blockno);
  
  acquire(&bcache.hash_table[key].lock);
  linkedlist_head = bcache.hash_table[key].bufhead_ptr;
  for (tran = linkedlist_head; tran; tran = tran->next) {
    if (tran->dev == dev && tran->blockno == blockno) {
      tran->refcnt++;
      release(&bcache.hash_table[key].lock);
      acquiresleep(&tran->lock);
      return tran;
    }
  }

  // Not cached. Now we need to tranverse the hashtable to find the least recently used buffer based on ticks, which will 
  // lead to 2 results :
  //  - The LRU buffer has exactly the same hash index as the inquiring one. In this case, we don't need to move buffer from 
  //    one linked list to another. 
  //  - The LRU buffer is not in the linked list that we inquire, which means we need fist delete the buffer from the original
  //    linked list and then insert it to the inquiring linked list. Unfortunately, it seems that we try to hold more than one
  //    lock, and this will lead to deadlock(circular wait). To prevent the deadlock diaster, we apparently should release the 
  //    current holding lock before try to acqure another one, but that will the move operation unatomic. So, we should use a 
  //    global lock here to freeze all linked list to keep their invariants.

  release(&bcache.hash_table[key].lock);

  acquire(&bcache.global_lock);
  // Now suppose we make two same inquiry consecutively, the first inquiry would hold the lock and eventually find out the 
  // inquiring buffer is not cached. Now it needs to evict the LRU buffer to make room for the new one, but at this point
  // it has to release the linked list lock to acquire the global one. Therefore, a small window time exists here. The busy 
  // waiting second inquiry would hold the lock immediately and be disappointed to find that the inquiry buffer is stll not 
  // cached, which it should!!! So we lost the atomity here, eventually the second inquiry would also invoke the LRU buffer
  // replace procedure and it turns out we would have two identical buffer cached in bcache.  
  // Thus, we need to do a second check to ensure the inquiry buffer is definitely not cached!
  
  linkedlist_head = bcache.hash_table[key].bufhead_ptr;
  for (tran = linkedlist_head; tran; tran = tran->next) {
    if (tran->dev == dev && tran->blockno == blockno) {
      acquire(&bcache.hash_table[key].lock);
      tran->refcnt++;
      release(&bcache.hash_table[key].lock);
      release(&bcache.global_lock);
      acquiresleep(&tran->lock);
      return tran;
    }
  }

  //min_ticks = ticks;
  // Now we have 100% certainty to say we do need a replace.
  for (int i = 0; i < BUCKETNUM; i++) {
    acquire(&bcache.hash_table[i].lock);
    new_min_exist = 0;
    linkedlist_head = bcache.hash_table[i].bufhead_ptr;
    for(tran = linkedlist_head; tran; tran = tran->next){
      if(tran->refcnt == 0 && tran->last_use < min_ticks) {
        min_ticks = tran->last_use;
        lru_buf = tran;
        new_min_exist = 1;
      }
    }
    if (new_min_exist) {
        if (current_hold_index != -1) {
          release(&bcache.hash_table[current_hold_index].lock);
        }
        current_hold_index = i;
    } else {
        release(&bcache.hash_table[i].lock);
      }
  }

  if (lru_buf == NULL) {
    panic("bget: no buffers");
  }

  lru_buf->dev = dev;
  lru_buf->blockno = blockno;
  lru_buf->valid = 0;
  lru_buf->refcnt = 1;

  if (current_hold_index != key) {
    // Not in the right bucket, need to move
    // First delete from original bucket, then insert to the right bucket

    linkedList_delete(&bcache.hash_table[current_hold_index].bufhead_ptr, lru_buf);
    release(&bcache.hash_table[current_hold_index].lock);
    
    acquire(&bcache.hash_table[key].lock);
    linkedlist_insert(&bcache.hash_table[key].bufhead_ptr, lru_buf);
  } 
  release(&bcache.hash_table[key].lock);
  release(&bcache.global_lock);
  acquiresleep(&lru_buf->lock);
  return lru_buf;
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
  int key = hash_function(b->dev, b->blockno);
  acquire(&bcache.hash_table[key].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    b->last_use = ticks;
  }
  release(&bcache.hash_table[key].lock);
}

void
bpin(struct buf *b) {
  int key = hash_function(b->dev, b->blockno);
  acquire(&bcache.hash_table[key].lock);
  b->refcnt++;
  release(&bcache.hash_table[key].lock);
}

void
bunpin(struct buf *b) {
  int key = hash_function(b->dev, b->blockno);
  acquire(&bcache.hash_table[key].lock);
  b->refcnt--;
  release(&bcache.hash_table[key].lock);
}


