#include <memory.h>
#include <printk.h>
#include <string.h>
#include <x86.h>


#define PHYSICAL_POOL_PAGES  64
#define PHYSICAL_POOL_BYTES  (PHYSICAL_POOL_PAGES << 12)
#define BITSET_SIZE          (PHYSICAL_POOL_PAGES >> 6)


extern __attribute__((noreturn)) void die(void);

static uint64_t bitset[BITSET_SIZE];

static uint8_t pool[PHYSICAL_POOL_BYTES] __attribute__((aligned(0x1000)));


paddr_t alloc_page(void)
{
	size_t i, j;
	uint64_t v;

	for (i = 0; i < BITSET_SIZE; i++) {
		if (bitset[i] == 0xffffffffffffffff)
			continue;

		for (j = 0; j < 64; j++) {
			v = 1ul << j;
			if (bitset[i] & v)
				continue;

			bitset[i] |= v;
			return (((64 * i) + j) << 12) + ((paddr_t) &pool);
		}
	}

	printk("[error] Not enough identity free page\n");
	return 0;
}

void free_page(paddr_t addr)
{
	paddr_t tmp = addr;
	size_t i, j;
	uint64_t v;

	tmp = tmp - ((paddr_t) &pool);
	tmp = tmp >> 12;

	i = tmp / 64;
	j = tmp % 64;
	v = 1ul << j;

	if ((bitset[i] & v) == 0) {
		printk("[error] Invalid page free %p\n", addr);
		die();
	}

	bitset[i] &= ~v;
}


/*
 * Memory model for Rackdoll OS
 *
 * +----------------------+ 0xffffffffffffffff
 * | Higher half          |
 * | (unused)             |
 * +----------------------+ 0xffff800000000000
 * | (impossible address) |
 * +----------------------+ 0x00007fffffffffff
 * | User                 |
 * | (text + data + heap) |
 * +----------------------+ 0x2000000000
 * | User                 |
 * | (stack)              |
 * +----------------------+ 0x40000000
 * | Kernel               |
 * | (valloc)             |
 * +----------------------+ 0x201000
 * | Kernel               |
 * | (APIC)               |
 * +----------------------+ 0x200000
 * | Kernel               |
 * | (text + data)        |
 * +----------------------+ 0x100000
 * | Kernel               |
 * | (BIOS + VGA)         |
 * +----------------------+ 0x0
 *
 * This is the memory model for Rackdoll OS: the kernel is located in low
 * addresses. The first 2 MiB are identity mapped and not cached.
 * Between 2 MiB and 1 GiB, there are kernel addresses which are not mapped
 * with an identity table.
 * Between 1 GiB and 128 GiB is the stack addresses for user processes growing
 * down from 128 GiB.
 * The user processes expect these addresses are always available and that
 * there is no need to map them exmplicitely.
 * Between 128 GiB and 128 TiB is the heap addresses for user processes.
 * The user processes have to explicitely map them in order to use them.
 */


void map_page(struct task *ctx, vaddr_t vaddr, paddr_t paddr)
{
	uint64_t mask_array[4] = {0x00000000001FF000, 0x000000003FE00000, 
		                      0x0000007FC0000000, 0x0000FF8000000000}; 
	uint64_t addr_mask = 0x0007FFFFFFFFF800;
	uint64_t usr_rw_mask = 0x0000000000000007;
	uint64_t *page_entry = (uint64_t *)ctx->pgt;
	int offset, i;

	printk("Mapping v_addr %p to p_addr %p...\n", vaddr ,paddr);

	for(i = 3; i > 0; --i)
	{
		offset = (vaddr & mask_array[i]) >> (21 + i*9);
		page_entry = page_entry + offset;
		if(!page_entry || !(*page_entry & 0x0000000000000001)) 	// if empty or invalid
		{
			printk("NOPE\n");
			memcpy(page_entry, (uint64_t*)alloc_page(), 8);
			*page_entry |= usr_rw_mask;							//creating new page with good rights
		}

		page_entry = (uint64_t*) (*page_entry & addr_mask);
	}
	offset = (vaddr & mask_array[i]) >> (12 + i*9);
	page_entry = page_entry + offset;

	paddr |= usr_rw_mask;
	memcpy(page_entry, &paddr, 8);
}

void load_task(struct task *ctx)
{
}

void set_task(struct task *ctx)
{
}

void mmap(struct task *ctx, vaddr_t vaddr)
{
}

void munmap(struct task *ctx, vaddr_t vaddr)
{
}

void pgfault(struct interrupt_context *ctx)
{
	printk("Page fault at %p\n", ctx->rip);
	printk("  cr2 = %p\n", store_cr2());
	asm volatile ("hlt");
}

void duplicate_task(struct task *ctx)
{
}
