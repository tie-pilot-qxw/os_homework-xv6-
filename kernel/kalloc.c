// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "kalloc.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

struct ref ref[(PHYSTOP  - KERNBASE)>> PGSHIFT]; // reference num of each page


void
kinit()
{
  for (int i = 0; i < ((PHYSTOP  - KERNBASE)>> PGSHIFT); i++) {
    ref[i].num = 1;
    initlock(&ref[i].lock, "ref");
  }
  for (int i = 0; i < NCPU; i++) {
    initlock(&kmem[i].lock, "kmem");
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  int cpu_id = 0;
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    kfree(p, cpu_id);
    cpu_id = (cpu_id + 1) % NCPU;
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa, int cpu_id)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  if (cpu_id == -1) {
    push_off(); // disable interrupts to get cpuid
    cpu_id = cpuid();
    pop_off();
  }

  acquire(&ref[PAGEREFID(pa)].lock);
  ref[PAGEREFID(pa)].num--;
  if(ref[PAGEREFID(pa)].num > 0) {
    release(&ref[PAGEREFID(pa)].lock);
    return;
  }
  release(&ref[PAGEREFID(pa)].lock);

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

  push_off(); // disable interrupts to get cpuid
  int cpu_id = cpuid();
  pop_off();

  acquire(&kmem[cpu_id].lock);
  r = kmem[cpu_id].freelist;
  if(r) {
    kmem[cpu_id].freelist = r->next;
    release(&kmem[cpu_id].lock);
    memset((char*)r, 5, PGSIZE); // fill with junk

    acquire(&ref[PAGEREFID(r)].lock);
    ref[PAGEREFID(r)].num = 1;
    release(&ref[PAGEREFID(r)].lock);

    return (void*)r;
  } else {
    release(&kmem[cpu_id].lock);
    for (int i = 0; i < NCPU; i++) {
      acquire(&kmem[i].lock);
      r = kmem[i].freelist;
      if(r) {
        kmem[i].freelist = r->next;
        release(&kmem[i].lock);
        memset((char*)r, 5, PGSIZE); // fill with junk

        acquire(&ref[PAGEREFID(r)].lock);
        ref[PAGEREFID(r)].num = 1;
        release(&ref[PAGEREFID(r)].lock);

        return (void*)r;
      }
      release(&kmem[i].lock);
    }
    return 0;
  }
}

// Collect the amount of free memory.
uint64
kcollect(void)
{
  uint64 amount = 0;
  struct run *r;

  for (int i = 0; i < NCPU; i++) {
    acquire(&kmem[i].lock);
    for(r = kmem[i].freelist; r; r = r->next) amount += PGSIZE;
    release(&kmem[i].lock);
  }
  
  return amount;
}

void
kref(void *pa)
{
  acquire(&ref[PAGEREFID(pa)].lock);
  ref[PAGEREFID(pa)].num++;
  release(&ref[PAGEREFID(pa)].lock);
}