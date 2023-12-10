#include <stdio.h>
#include <sys/mman.h>

unsigned long *free_list = NULL;
unsigned long *final_mem = NULL;
unsigned long *start_mem = NULL;


void *memalloc(unsigned long size) 
{
	printf("memalloc() called\n");
	if(size < 1)
		return NULL;
	unsigned long *head = free_list;
	if(size <= 8)
		size = 9;
	size += 8 + (size % 8 != 0 ? 8 - size % 8 : 0);
	while(head && *head < size)
		head = (unsigned long *) *(head + 1);
	if(!head)
	{
		unsigned long num = size / (1 << 22) + (size % (1 << 22) != 0);
		num <<= 22;
		head = mmap(final_mem, num, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
		if(head == MAP_FAILED)
			return NULL;
		if(final_mem = NULL)
			start_mem = head;
		final_mem = head + num;
		*head = num;
	}
	else
	{
		unsigned long *next = (unsigned long *) (*(head + 1));
		unsigned long *prev = (unsigned long *) (*(head + 2));

		if(free_list == head)
			free_list = next;
		if(next != NULL)
			*(next + 2) = (unsigned long) prev;
		if(prev != NULL)
			*(prev + 1) = (unsigned long) next;
	}
	if(*head - size - 24 >=0)
	{
		unsigned long *new_head = head + size / 8;
		*new_head = *head - size;
		if(free_list != NULL)
			*(free_list + 2) = (unsigned long) new_head;
		*(new_head + 1) = (unsigned long) free_list;
		*(new_head + 2) = (unsigned long) NULL;
		free_list = new_head;
		*head = size;
	}
	return (void *) (head + 1);
}

int memfree(void *ptr)
{
	printf("memfree() called\n");
	unsigned long *head = free_list;
	unsigned long *prev = start_mem;
	unsigned long *next = final_mem;
	unsigned long *inp = (unsigned long *) ptr;
	int flag = 0;
	inp -= 1;

	while(head)
	{
		if(head > inp && head <= next)
		{
			flag |= 1;
			next = head;
		}
		else if(head < inp && head >= prev)
		{
			flag |= 2;
			prev = head;
		}
		head = (unsigned long *) (*(head + 1));
	}

	if(flag == 0 || flag == 1)
		prev = NULL;
	if(flag == 0 || flag == 2)
		next = NULL;

	if(next && *inp + inp == next)
	{
		*inp += *next;
		unsigned long *prev_next = (unsigned long *) (*(next + 2));
		unsigned long *next_next = (unsigned long *) (*(next + 1));
		if(next == free_list)
			free_list = next_next;
		if(next_next != NULL)
			*(next_next + 2) = (unsigned long) prev_next;
		if(prev_next != NULL)
			*(prev_next + 1) = (unsigned long) next_next;
	}
	if(prev && *prev + prev == inp)
	{
		*prev += *inp;
		inp = prev;
		unsigned long *prev_prev = (unsigned long *) (*(prev + 2));
		unsigned long *next_prev = (unsigned long *) (*(prev + 1));
		if(prev == free_list)
			free_list = next_prev;
		if(next_prev != NULL)
			*(next_prev + 2) = (unsigned long) prev_prev;
		if(prev_prev != NULL)
			*(prev_prev + 1) = (unsigned long) next_prev;
	}
	*(inp + 1) = (unsigned long) free_list;
	*(inp + 2) = (unsigned long) NULL;
	if(free_list != NULL)
		*(free_list + 2) = (unsigned long) inp;
	free_list = inp;

	return 0;
}