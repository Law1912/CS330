#include <types.h>
#include <mmap.h>
#include <fork.h>
#include <v2p.h>
#include <page.h>

/* 
 * You may define macros and other helper functions here
 * You must not declare and use any static/global variables 
 * */

#define error __INT32_MAX__ - 2101247 + 4096

long vm_area_init(struct exec_context *current)
{
	struct vm_area *vm = os_alloc(sizeof(struct vm_area));
	if (!vm)
		return -EINVAL;
	vm->access_flags = 0;
	vm->vm_end = MMAP_AREA_START + (1 << 12);
	vm->vm_next = NULL;
	vm->vm_start = MMAP_AREA_START;
	stats->num_vm_area = 1;
	current->vm_area = vm;
	
	return 0;
}

/**
 * mprotect System call Implementation.
 */
long vm_area_mprotect(struct exec_context *current, u64 addr, int length, int prot)
{
	struct vm_area *head = current->vm_area->vm_next;
	struct vm_area *prev = current->vm_area;

	if (addr < prev->vm_end || length <=0 || (prot != PROT_READ && prot != (PROT_READ | PROT_WRITE)))
		return -EINVAL;

	while (head != NULL && head->vm_start < addr)
		prev = head, head = head->vm_next;

	if (prev->vm_end > addr && prot != prev->access_flags)
	{
		struct vm_area *vm = os_alloc(sizeof(struct vm_area));
		if (!vm)
			return -EINVAL;
		*vm = *prev;
		(stats->num_vm_area)++;
		vm->vm_start = addr;
		head = vm;
		prev->vm_next = head;
		prev->vm_end = addr;
	}

	length = (length + (1 << 12) - 1) >> 12;
	length <<= 12;

	while (head != NULL && head->vm_end <= addr + length)
	{
		if (prev->vm_end == head->vm_start && prev->access_flags == prot && head->access_flags == prot)
		{
			prev->vm_end = head->vm_end;
			prev->vm_next = head->vm_next;
			struct vm_area *tmp = head;
			head = head->vm_next;
			os_free(tmp, sizeof(struct vm_area));
			(stats->num_vm_area)--;
			continue;
		}
		if (head->access_flags == prot)
		{
			prev = head;
			head = head->vm_next;
			continue;
		}
		
		for (unsigned long tmp_addr = head->vm_start; tmp_addr < head->vm_end; tmp_addr += (1 << 12))
		{
			unsigned long offset[] = {(tmp_addr >> 39) & 0x1FF, (tmp_addr >> 30) & 0x1FF, (tmp_addr >> 21) & 0x1FF, (tmp_addr >> 12) & 0x1FF};
			unsigned long *ptr = (unsigned long *)osmap(current->pgd);
			int flag = 1;
			for (int i = 0; i < 3; i++)
				if (flag = ptr[offset[i]] & 1)
					ptr = (unsigned long *)osmap(ptr[offset[i]] >> 12);

			if (flag && (ptr[offset[3]] & 1) && get_pfn_refcount(ptr[offset[3]] >> 12) == 1)
				ptr[offset[3]] ^= (1 << 3);
			asm volatile("invlpg (%0)" ::"r" (tmp_addr) : "memory");
		}

		if (prev->vm_end == head->vm_start && prev->access_flags == prot)
		{
			prev->vm_end = head->vm_end;
			prev->vm_next = head->vm_next;
			struct vm_area *tmp = head;
			head = head->vm_next;
			os_free(tmp, sizeof(struct vm_area));
			(stats->num_vm_area)--;
			continue;
		}

		head->access_flags = prot;
		prev = head;
		head = head->vm_next;
	}

	if (head && head->vm_start < addr + length)
	{
		if (head->access_flags != prot)
			for (unsigned long tmp_addr = head->vm_start; tmp_addr < addr + length; tmp_addr += (1 << 12))
			{
				unsigned long offset[] = {(tmp_addr >> 39) & 0x1FF, (tmp_addr >> 30) & 0x1FF, (tmp_addr >> 21) & 0x1FF, (tmp_addr >> 12) & 0x1FF};
				unsigned long *ptr = (unsigned long *)osmap(current->pgd);
				int flag = 1;
				for (int i = 0; i < 3; i++)
					if (flag = ptr[offset[i]] & 1)
						ptr = (unsigned long *)osmap(ptr[offset[i]] >> 12);
				
				if (flag && (ptr[offset[3]] & 1) && get_pfn_refcount(ptr[offset[3]] >> 12) == 1)
					ptr[offset[3]] ^= (1 << 3);
				asm volatile("invlpg (%0)" ::"r" (tmp_addr) : "memory");
			}

		if (prev->vm_end == head->vm_start && prev->access_flags == prot && prev->access_flags == head->access_flags)
		{
			prev->vm_end = head->vm_end;
			prev->vm_next = head->vm_next;
			os_free(head, sizeof(struct vm_area));
			(stats->num_vm_area)--;
		}
		else if (prev->vm_end == head->vm_start && prev->access_flags == prot)
		{
			prev->vm_end = addr + length;
			head->vm_start = prev->vm_end;
		}
		else
		{
			struct vm_area *vm = os_alloc(sizeof(struct vm_area));
			if (!vm)
				return -EINVAL;

			vm->access_flags = prot;
			vm->vm_end = addr + length;
			vm->vm_next = head;
			vm->vm_start = head->vm_start;
			(stats->num_vm_area)++;

			prev->vm_next = vm;
			head->vm_start = vm->vm_end;
		}
	}

	if (prev->vm_end == head->vm_start && prev->access_flags == prot && prev->access_flags == head->access_flags)
	{
		prev->vm_end = head->vm_end;
		prev->vm_next = head->vm_next;
		os_free(head, sizeof(struct vm_area));
		(stats->num_vm_area)--;
	}

	return 0;
}

/**
 * mmap system call implementation.
 */
long vm_area_map(struct exec_context *current, u64 addr, int length, int prot, int flags)
{
	if (!(stats->num_vm_area) && vm_area_init(current) == -EINVAL)
		return -EINVAL;

	struct vm_area *head = current->vm_area->vm_next;
	struct vm_area *prev = current->vm_area;

	if (length <= 0 || length > (1 << 21) || (prot != PROT_READ && prot != (PROT_READ | PROT_WRITE)) || (flags != 0 && flags != MAP_FIXED) || (flags == MAP_FIXED && (addr == 0 || addr < prev->vm_end)))
		return -EINVAL;
		
	length = (length + (1 << 12) - 1) >> 12;
	unsigned long new_length = length << 12;


	
	int flag = 1;

	if (addr != 0)
	{
		while (head != NULL && head->vm_start < addr)
			prev = head, head = head->vm_next;
	
		if (flags == MAP_FIXED)
		{
			if (!(prev->vm_end <= addr && (head == NULL ? 1 : (addr + new_length <= head->vm_start))))
				return -EINVAL;
			flag = 0;
		}
		if (flags == 0)
		{
			if (prev->vm_end <= addr && (head == NULL ? 1 : addr + new_length <= head->vm_start))
				flag = 0;
			else
				head = current->vm_area->vm_next, prev = current->vm_area;
		}
	}

	if (flag)
	{
		while (head != NULL && prev->vm_end + new_length > head->vm_start)
			prev = head, head = head->vm_next;
		addr = prev->vm_end;
	}
	
	if (prev->vm_end == addr && prev->access_flags == prot && head != NULL && head->vm_start == addr + new_length && head->access_flags == prot)
	{
		prev->vm_end += new_length + head->vm_end - head->vm_start;
		prev->vm_next = head->vm_next;
		os_free(head, sizeof(struct vm_area));
		(stats->num_vm_area)--;
	}
	else if (head != NULL && head->vm_start == addr + new_length && head->access_flags == prot)
		head->vm_start = addr;
	else if (prev->vm_end == addr && prev->access_flags == prot)
		prev->vm_end += new_length;
	else
	{
		struct vm_area *vm = os_alloc(sizeof(struct vm_area));
		if (!vm)
			return -EINVAL;
		vm->access_flags = prot;
		vm->vm_end = addr + new_length;
		vm->vm_next = head;
		vm->vm_start = addr;
		(stats->num_vm_area)++;
		prev->vm_next = vm;
	}

	return addr;
}

/**
 * munmap system call implemenations
 */

long vm_area_unmap(struct exec_context *current, u64 addr, int length)
{
	struct vm_area *head = current->vm_area->vm_next;
	struct vm_area *prev = current->vm_area;


	if (addr < prev->vm_end || length <=0)
		return -EINVAL;

	while (head != NULL && head->vm_start < addr)
		prev = head, head = head->vm_next;

	if (prev->vm_end > addr)
	{
		struct vm_area *vm = os_alloc(sizeof(struct vm_area));
		if (!vm)
			return -EINVAL;
		*vm = *prev;
		(stats->num_vm_area)++;
		vm->vm_start = addr;
		head = vm;
		prev->vm_next = head;
		prev->vm_end = addr;
	}

	length = (length + (1 << 12) - 1) >> 12;
	length <<= 12;

	while (head != NULL && head->vm_end <= addr + length)
	{
		struct vm_area *curr = head;
		
		for (unsigned long tmp_addr = head->vm_start; tmp_addr < head->vm_end; tmp_addr += (1 << 12))
		{
			unsigned long offset[] = {(tmp_addr >> 39) & 0x1FF, (tmp_addr >> 30) & 0x1FF, (tmp_addr >> 21) & 0x1FF, (tmp_addr >> 12) & 0x1FF};
			unsigned long *ptr = (unsigned long *)osmap(current->pgd);
			int flag = 1;
			for (int i = 0; i < 3; i++)
				if (flag = ptr[offset[i]] & 1)
					ptr = (unsigned long *)osmap(ptr[offset[i]] >> 12);
			// printk("%d\n", get_pfn_refcount(ptr[offset[3]] >> 12));
			
			if (flag && (ptr[offset[3]] & 1))
			{
				put_pfn(ptr[offset[3]] >> 12);
				if (get_pfn_refcount(ptr[offset[3]] >> 12) == 0)
					os_pfn_free(USER_REG, (ptr[offset[3]] >> 12));
				ptr[offset[3]] = 0;
			}

			asm volatile("invlpg (%0)" ::"r" (tmp_addr) : "memory");
		}

		head = head->vm_next;
		os_free(curr, sizeof(struct vm_area));
		(stats->num_vm_area)--;
	}

	if (head && head->vm_start < addr + length)
	{
		for (unsigned long tmp_addr = head->vm_start; tmp_addr < addr + length; tmp_addr += (1 << 12))
		{
			unsigned long offset[] = {(tmp_addr >> 39) & 0x1FF, (tmp_addr >> 30) & 0x1FF, (tmp_addr >> 21) & 0x1FF, (tmp_addr >> 12) & 0x1FF};
			unsigned long *ptr = (unsigned long *)osmap(current->pgd);
			int flag = 1;
			for (int i = 0; i < 3; i++)
				if (flag = ptr[offset[i]] & 1)
					ptr = (unsigned long *)osmap(ptr[offset[i]] >> 12);

			if (flag && (ptr[offset[3]] & 1))
			{
				put_pfn(ptr[offset[3]] >> 12);
				if (get_pfn_refcount(ptr[offset[3]] >> 12) == 0)
					os_pfn_free(USER_REG, (ptr[offset[3]] >> 12));
				ptr[offset[3]] = 0;
			}
			asm volatile("invlpg (%0)" ::"r" (tmp_addr) : "memory");
		}

		head->vm_start = addr + length;
	}
	
	prev->vm_next = head;
	return 0;
}



/**
 * Function will invoked whenever there is page fault for an address in the vm area region
 * created using mmap
 */

long vm_area_pagefault(struct exec_context *current, u64 addr, int error_code)
{
	struct vm_area *head = current->vm_area->vm_next;
	if (addr < head->vm_start)
		return -EINVAL;

	while (head != NULL && head->vm_end < addr)
		head = head->vm_next;
	// printk("pf: %d %d \n", error_code, head->access_flags);
	
	if (!head || head->vm_start > addr || (head->access_flags == PROT_READ && (error_code == 6 || error_code == 7)))
		return -1;

	unsigned long offset[] = {(addr >> 39) & 0x1FF, (addr >> 30) & 0x1FF, (addr >> 21) & 0x1FF, (addr >> 12) & 0x1FF};
	unsigned long *ptr = (unsigned long *)osmap(current->pgd);
	for (int i = 0; i < 3; i++)
	{
		if (!(ptr[offset[i]] & 1))
		{
			unsigned long pfn = (unsigned long)os_pfn_alloc(OS_PT_REG);
			if(!pfn)
				return -1;
			ptr[offset[i]] = (pfn << 12) | 0x19;
		}
		
		ptr = (unsigned long *)osmap(ptr[offset[i]] >> 12);
	}

	if (!(ptr[offset[3]] & 1))
	{
		unsigned long pfn = (unsigned long)os_pfn_alloc(USER_REG);
			if(!pfn)
				return -1;
		ptr[offset[3]] = (pfn << 12) | 0x11 | (head->access_flags == (PROT_READ | PROT_WRITE)) << 3;
	}

	if (error_code == 7 && (ptr[offset[3]] & 8) == 0 && head->access_flags == (PROT_READ | PROT_WRITE))
		return handle_cow_fault(current, addr, head->access_flags);

	return 1;
}

/**
 * cfork system call implemenations
 * The parent returns the pid of child process. The return path of
 * the child process is handled separately through the calls at the 
 * end of this function (e.g., setup_child_context etc.)
 */

long do_cfork(){
	u32 pid;
	struct exec_context *new_ctx = get_new_ctx();
	struct exec_context *ctx = get_current_ctx();
	/* Do not modify above lines
	* 
	* */   
	/*--------------------- Your code [start]---------------*/

	pid = new_ctx->pid;

	new_ctx->type = ctx->type;
	new_ctx->state = ctx->state;
	new_ctx->used_mem = ctx->used_mem;
	memcpy(new_ctx->name, ctx->name, CNAME_MAX);
	memcpy(&(new_ctx->regs), &(ctx->regs), sizeof(struct user_regs));
	new_ctx->ppid = ctx->pid;
    new_ctx->pgd = os_pfn_alloc(OS_PT_REG);

    for (int i = 0; i < 3; i++)
	{
        new_ctx->mms[i].start = ctx->mms[i].start;
        new_ctx->mms[i].end = ctx->mms[i].end;
        new_ctx->mms[i].next_free = ctx->mms[i].next_free;
        new_ctx->mms[i].access_flags = ctx->mms[i].access_flags;

		for (unsigned long tmp_addr = ctx->mms[i].start; tmp_addr < ctx->mms[i].next_free; tmp_addr += (1 << 12))
		{
			unsigned long offset[] = {(tmp_addr >> 39) & 0x1FF, (tmp_addr >> 30) & 0x1FF, (tmp_addr >> 21) & 0x1FF, (tmp_addr >> 12) & 0x1FF};
			unsigned long *parent_ptr = (unsigned long *)osmap(ctx->pgd);
			unsigned long *child_ptr = (unsigned long *)osmap(new_ctx->pgd);
			int flag = 1;
			for (int i = 0; i < 3; i++)
			{
				if(!(parent_ptr[offset[i]] & 1))
				{
					flag = 0;
					break;
				}
				if ((parent_ptr[offset[i]] & 1) && !(child_ptr[offset[i]] & 1))
				{
					unsigned long pfn = (unsigned long)os_pfn_alloc(OS_PT_REG);
					if(!pfn)
						return -1;
					child_ptr[offset[i]] = (pfn << 12) | (parent_ptr[offset[i]] & 0xFFF);
				}

				child_ptr = (unsigned long *)osmap(child_ptr[offset[i]] >> 12);
				parent_ptr = (unsigned long *)osmap(parent_ptr[offset[i]] >> 12);
			}

			if (flag && (parent_ptr[offset[3]] & 1) && !(child_ptr[offset[3]] & 1))
			{
				parent_ptr[offset[3]] &= ~0x8;
				get_pfn(parent_ptr[offset[3]] >> 12);
				child_ptr[offset[3]] = parent_ptr[offset[3]];
			}
		}
    }

	new_ctx->mms[MM_SEG_STACK].start = ctx->mms[MM_SEG_STACK].start;
	new_ctx->mms[MM_SEG_STACK].end = ctx->mms[MM_SEG_STACK].end;
	new_ctx->mms[MM_SEG_STACK].next_free = ctx->mms[MM_SEG_STACK].next_free;
	new_ctx->mms[MM_SEG_STACK].access_flags = ctx->mms[MM_SEG_STACK].access_flags;
	
	for (unsigned long tmp_addr = ctx->mms[MM_SEG_STACK].start; tmp_addr < ctx->mms[MM_SEG_STACK].end; tmp_addr += (1 << 12))
	{
		unsigned long offset[] = {(tmp_addr >> 39) & 0x1FF, (tmp_addr >> 30) & 0x1FF, (tmp_addr >> 21) & 0x1FF, (tmp_addr >> 12) & 0x1FF};
		unsigned long *parent_ptr = (unsigned long *)osmap(ctx->pgd);
		unsigned long *child_ptr = (unsigned long *)osmap(new_ctx->pgd);
		int flag = 1;
		for (int i = 0; i < 3; i++)
		{
			if(!(parent_ptr[offset[i]] & 1))
			{
				flag = 0;
				break;
			}
			if ((parent_ptr[offset[i]] & 1) && !(child_ptr[offset[i]] & 1))
			{
				unsigned long pfn = (unsigned long)os_pfn_alloc(OS_PT_REG);
				if(!pfn)
					return -1;
				child_ptr[offset[i]] = (pfn << 12) | (parent_ptr[offset[i]] & 0xFFF);
			}

			child_ptr = (unsigned long *)osmap(child_ptr[offset[i]] >> 12);
			parent_ptr = (unsigned long *)osmap(parent_ptr[offset[i]] >> 12);
		}

		if (flag && (parent_ptr[offset[3]] & 1) && !(child_ptr[offset[3]] & 1))
		{
			parent_ptr[offset[3]] &= ~0x8;
			get_pfn(parent_ptr[offset[3]] >> 12);
			child_ptr[offset[3]] = parent_ptr[offset[3]];
		}
	}


	struct vm_area *parent_head = ctx->vm_area;
	if (parent_head)
		parent_head = parent_head->vm_next;
	struct vm_area *child_head = new_ctx->vm_area;
	while (parent_head != NULL)
	{
		struct vm_area *next = os_alloc(sizeof(struct vm_area));
		if (!next)
			return -1;

		next->access_flags = parent_head->access_flags;
		next->vm_start = parent_head->vm_start;
		next->vm_end = parent_head->vm_end;
		next->vm_next = parent_head->vm_next;

		child_head->vm_next = next;

		for (unsigned long tmp_addr = parent_head->vm_start; tmp_addr < parent_head->vm_end; tmp_addr += (1 << 12))
		{
			unsigned long offset[] = {(tmp_addr >> 39) & 0x1FF, (tmp_addr >> 30) & 0x1FF, (tmp_addr >> 21) & 0x1FF, (tmp_addr >> 12) & 0x1FF};
			unsigned long *parent_ptr = (unsigned long *)osmap(ctx->pgd);
			unsigned long *child_ptr = (unsigned long *)osmap(new_ctx->pgd);
			int flag = 1;
			for (int i = 0; i < 3; i++)
			{
				if (!(parent_ptr[offset[i]] & 1))
				{
					flag = 0;
					break;
				}
				if ((parent_ptr[offset[i]] & 1) && !(child_ptr[offset[i]] & 1))
				{
					unsigned long pfn = (unsigned long)os_pfn_alloc(OS_PT_REG);
					if(!pfn)
						return -1;
					child_ptr[offset[i]] = (pfn << 12) | (parent_ptr[offset[i]] & 0xFFF);
				}

				child_ptr = (unsigned long *)osmap(child_ptr[offset[i]] >> 12);
				parent_ptr = (unsigned long *)osmap(parent_ptr[offset[i]] >> 12);
			}

			if (flag && (parent_ptr[offset[3]] & 1) && !(child_ptr[offset[3]] & 1))
			{
				parent_ptr[offset[3]] &= ~0x8;
				get_pfn(parent_ptr[offset[3]] >> 12);
				child_ptr[offset[3]] = parent_ptr[offset[3]];
			}
		}

		child_head = child_head->vm_next;
		parent_head = parent_head->vm_next;
    }

	for (int i = 0; ctx->files[i] != NULL; i++)
		new_ctx->files[i] = ctx->files[i];


	/*--------------------- Your code [end] ----------------*/

	/*
	* The remaining part must not be changed
	*/
	copy_os_pts(ctx->pgd, new_ctx->pgd);
	do_file_fork(new_ctx);
	setup_child_context(new_ctx);
	return pid;
}



/* Cow fault handling, for the entire user address space
 * For address belonging to memory segments (i.e., stack, data) 
 * it is called when there is a CoW violation in these areas. 
 *
 * For vm areas, your fault handler 'vm_area_pagefault'
 * should invoke this function
 * */

long handle_cow_fault(struct exec_context *current, u64 vaddr, int access_flags)
{
	unsigned long offset[] = {(vaddr >> 39) & 0x1FF, (vaddr >> 30) & 0x1FF, (vaddr >> 21) & 0x1FF, (vaddr >> 12) & 0x1FF};
	unsigned long *ptr = (unsigned long *)osmap(current->pgd);
	for (int i = 0; i < 3; ptr = (unsigned long *)osmap(ptr[offset[i++]] >> 12));

	// printk("hcf : %d\n", get_pfn_refcount(ptr[offset[3]] >> 12));
	if (get_pfn_refcount(ptr[offset[3]] >> 12) == 1)
	{
		ptr[offset[3]] |= (access_flags | PROT_WRITE) << 2;
		return 1;
	}
	unsigned long *pptr = (unsigned long *)osmap(ptr[offset[3]] >> 12);
	put_pfn(ptr[offset[3]] >> 12);
	unsigned long pfn = (unsigned long)os_pfn_alloc(USER_REG);
	if(!pfn)
		return -1;
	ptr[offset[3]] = (pfn << 12) | (ptr[offset[3]] & 0xFFF) | ((access_flags | PROT_WRITE) << 2);
	
	memcpy((unsigned long *)osmap(ptr[offset[3]] >> 12), pptr, (1 << 12));

  	return 1;
}