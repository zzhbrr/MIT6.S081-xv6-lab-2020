// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end, int cpu_num, int cpu_id);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

void
kinit()
{
  for(int i = 0; i < NCPU; i ++) {
    initlock(&kmem[i].lock, "kmem");
    freerange(end, (void*)PHYSTOP, NCPU, i);
  }
}

void
freerange(void *pa_start, void *pa_end, int cpu_num, int cpu_id)
{
  char *p;
  int pg_num = ((char*)pa_end - (char*)PGROUNDUP((uint64)pa_start)) / PGSIZE;
  char *start_p = (char*)PGROUNDUP((uint64)pa_start) + cpu_id * pg_num / cpu_num * PGSIZE;
  char *end_p = cpu_id==cpu_num-1 ? (char*)pa_end : start_p + pg_num / cpu_num * PGSIZE;
  // printf("pa_start: %p, pa_end: %p\n",  (char*)PGROUNDUP((uint64)pa_start), pa_end);
  // printf("pg_num: %d\n", pg_num);
  // printf("start_p:%p, end_p:%p\n", start_p, end_p);
  for(p = start_p; p + PGSIZE <= end_p; p += PGSIZE)
    kfree(p);
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
  
  int cpu_id = ((char*)pa - (char*)end) / (((char*)PHYSTOP - (char*)end)/NCPU);

  if(cpu_id > NCPU || cpu_id < 0) 
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem[cpu_id].lock);
  r->next = kmem[cpu_id].freelist;
  kmem[cpu_id].freelist = r;
  release(&kmem[cpu_id].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int cpu_id = cpuid();
  pop_off();

  acquire(&kmem[cpu_id].lock);
  r = kmem[cpu_id].freelist;
  if(r)
    kmem[cpu_id].freelist = r->next;
  else {
    for(int i = 0; i < NCPU; i ++) {
      if (i == cpu_id) continue;
      acquire(&kmem[i].lock);
      if (kmem[i].freelist) {
        r = kmem[i].freelist;
        kmem[i].freelist = r->next;
        release(&kmem[i].lock);
        break;
      }
      release(&kmem[i].lock);
    }
  }
  release(&kmem[cpu_id].lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
