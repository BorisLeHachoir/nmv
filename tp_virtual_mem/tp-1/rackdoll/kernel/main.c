#include <idt.h>                            /* see there for interrupt names */
#include <memory.h>                               /* physical page allocator */
#include <printk.h>                      /* provides printk() and snprintk() */
#include <string.h>                                     /* provides memset() */
#include <syscall.h>                         /* setup system calls for tasks */
#include <task.h>                             /* load the task from mb2 info */
#include <types.h>              /* provides stdint and general purpose types */
#include <vga.h>                                         /* provides clear() */
#include <x86.h>                                    /* access to cr3 and cr2 */
#include <inttypes.h>


__attribute__((noreturn))
void die(void)
{
	/* Stop fetching instructions and go low power mode */
	asm volatile ("hlt");

	/* This while loop is dead code, but it makes gcc happy */
	while (1)
		;
}

void pgt_to_human(uint64_t tpe)
{
	printk("##############################\n");
	printk("Page addr: %p\n", tpe & 0x0007FFFFFFFFF800);
	printk("NX: %c\n", tpe & 0x8000000000000000 ? '1': '0');
	printk("G: %c\n", tpe & 0x0000000000000100 ? '1': '0');
	printk("PS: %c\n", tpe & 0x0000000000000080 ? '1': '0');
	printk("D: %c\n", tpe & 0x0000000000000040 ? '1': '0');
	printk("A: %c\n", tpe & 0x0000000000000020 ? '1': '0');
	printk("PCD: %c\n", tpe & 0x0000000000000010 ? '1': '0');
	printk("PWT: %c\n", tpe & 0x0000000000000008 ? '1': '0');
	printk("U/S: %c\n", tpe & 0x0000000000000004 ? '1': '0');
	printk("R/W: %c\n", tpe & 0x0000000000000002 ? '1': '0');
	printk("P: %c\n", tpe & 0x0000000000000001 ? '1': '0');
	printk("##############################\n");
}

void print_pgt(paddr_t pml, uint8_t lvl)
{
	uint64_t *page_entry = (uint64_t*)pml;
	printk("PML%d -> %p\n", lvl, page_entry);
	int i;
	paddr_t next_pml;
	
	for(i = 0 ; i<512 ; ++i)
	{
		if(*page_entry & 0x0000000000000001)		/* if page is valid */
		{
			next_pml = *page_entry & 0x0007FFFFFFFFF800;

			if(*page_entry & 0x0000000000000080)	/* if huge page */
				printk("huge page found -> %p\n", next_pml);

			else if(lvl == 1)						/* if last level */
				printk("Page found -> %p\n", next_pml);

			else
				print_pgt(next_pml, --lvl);
		}
		page_entry++;
	}
}

__attribute__((noreturn))
void main_multiboot2(void *mb2)
{
	clear();                                     /* clear the VGA screen */
	printk("Rackdoll OS\n-----------\n\n");                 /* greetings */

	setup_interrupts();                           /* setup a 64-bits IDT */
	setup_tss();                                  /* setup a 64-bits TSS */
	interrupt_vector[INT_PF] = pgfault;      /* setup page fault handler */

	disable_pic();                         /* disable anoying legacy PIC */
	sti();                                          /* enable interrupts */


	struct task fake;
	paddr_t new;
	fake.pgt = store_cr3();
	new = alloc_page();
	map_page(&fake, 0x201000, new);


	print_pgt(store_cr3(), 4);

	load_tasks(mb2);                         /* load the tasks in memory */
	run_tasks();                                 /* run the loaded tasks */

	printk("\nGoodbye!\n");                                 /* fairewell */
	die();                        /* the work is done, we can die now... */
}
