
struct ref{
  struct spinlock lock;
  uint32 num;
};

#define PAGEREFID(pa) (((uint64)pa - KERNBASE) >> PGSHIFT)
