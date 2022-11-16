// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

int reference_count[PHYSTOP / PGSIZE]; // 物理页最多200页
struct spinlock pgreflock;

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&pgreflock, "pgref");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  // printf("pa_start:%p, pa_end:%p\n", p, pa_end);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    reference_count[pa2index(p)] = 1;
    kfree(p);
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  acquire(&pgreflock);
  if((--reference_count[pa2index(pa)]) > 0) {
    release(&pgreflock);
    return ;
  }
  release(&pgreflock);
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    acquire(&pgreflock);
    reference_count[pa2index(r)] = 1; 
    release(&pgreflock);
  }
  release(&kmem.lock);

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
  }
  return (void*)r;
}

void* kcopy_page_cow(uint64 pa) {
  acquire(&pgreflock);
  if (reference_count[pa2index(pa)] <= 1) {
    release(&pgreflock);
    return (void*)pa;
  }
  char* mem = kalloc();
  if(mem == 0) {
    release(&pgreflock);
    return 0; // out of memory
  }
  memmove((void*)mem, (void*)pa, PGSIZE);
  // reference_count[pa2index(pa)]--;
  release(&pgreflock);
  return (void*)mem;
}

int add_reference_count(uint64 pa) {
  acquire(&pgreflock);
  reference_count[pa2index(pa)] ++;
  release(&pgreflock);
  return 1;
}

int minus_refrence_coun(uint64 pa) {
  if (reference_count[pa2index(pa)] <= 0) return 0;
  reference_count[pa2index(pa)]--;
  return 1;
}