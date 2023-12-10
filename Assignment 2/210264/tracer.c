#include<context.h>
#include<memory.h>
#include<lib.h>
#include<entry.h>
#include<file.h>
#include<tracer.h>

int * N= (int[]){
    [SYSCALL_CLONE] = 2,
    [SYSCALL_CLOSE] = 1,
    [SYSCALL_CONFIGURE] = 2,
    [SYSCALL_DUMP_PTT] = 1,
    [SYSCALL_DUP2] = 2,
    [SYSCALL_DUP] = 1,
    [SYSCALL_END_STRACE] = 0,
    [SYSCALL_EXIT] = 1,
    [SYSCALL_EXPAND] = 2,
    [SYSCALL_FTRACE] = 4,
    [SYSCALL_GET_COW_F] = 0,
    [SYSCALL_GET_USER_P] = 0,
    [SYSCALL_GETPID] = 0,
    [SYSCALL_LSEEK] = 3,
    [SYSCALL_MMAP] = 4,
    [SYSCALL_MPROTECT] = 3,
    [SYSCALL_MUNMAP] = 2,
    [SYSCALL_OPEN] = 2,
    [SYSCALL_PHYS_INFO] = 0,
    [SYSCALL_PMAP] = 1,
    [SYSCALL_READ] = 3,
    [SYSCALL_READ_FTRACE] = 3,
    [SYSCALL_READ_STRACE] = 3,
    [SYSCALL_SIGNAL] = 2,
    [SYSCALL_SLEEP] = 1,
    [SYSCALL_START_STRACE] = 2,
    [SYSCALL_STATS] = 0,
    [SYSCALL_STRACE] = 2,
    [SYSCALL_TRACE_BUFFER] = 1,
    [SYSCALL_WRITE] = 3
};

int strace_base = 0;
int ftrace_base = 0;
u64 delimiter = -1;


///////////////////////////////////////////////////////////////////////////
//// 		Start of Trace buffer functionality 		      /////
///////////////////////////////////////////////////////////////////////////

int is_valid_mem_range(unsigned long buff, u32 count, int access_bit)
{
	struct exec_context *ctx = get_current_ctx();
	struct mm_segment *mms = ctx->mms;
	struct vm_area *vm_area = ctx->vm_area;
	if(mms[MM_SEG_CODE].start <= buff && mms[MM_SEG_CODE].end > buff + count)
		return access_bit & 1;
	if(mms[MM_SEG_RODATA].start <= buff && mms[MM_SEG_RODATA].end > buff + count)
		return access_bit & 1;
	if(mms[MM_SEG_DATA].start <= buff && mms[MM_SEG_DATA].end > buff + count)
		return access_bit & 3;
	if(mms[MM_SEG_STACK].start <= buff && mms[MM_SEG_STACK].end > buff + count)
		return access_bit & 3;
	while(vm_area)
	{
		if(vm_area->vm_start <= buff && vm_area->vm_end > buff + count)
			return access_bit & vm_area->access_flags;
		vm_area = vm_area->vm_next;
	}
	return 0;
}


long trace_buffer_close(struct file *filep)
{
	if(!filep || !(filep->trace_buffer) || !(filep->fops) || !(filep->trace_buffer->buffer))
		return -EINVAL;

	os_page_free(USER_REG, filep->trace_buffer->buffer);
	os_free(filep->trace_buffer, sizeof(struct trace_buffer_info));
	os_free(filep->fops, sizeof(struct fileops));
	filep->fops = NULL;
	filep->trace_buffer = NULL;
	filep->trace_buffer = NULL;
	os_free(filep, sizeof(struct file));
	struct exec_context *ctx = get_current_ctx();
	int i = 0;
	while(i < MAX_OPEN_FILES && (ctx->files)[i] != filep && i++);
	(ctx->files)[i] = NULL;
	return 0;
}


int trace_buffer_read(struct file *filep, char *buff, u32 count)
{
	if(is_valid_mem_range((unsigned long) buff, count, 2) == 0)
		return -EBADMEM;

	if(!filep || !(filep->trace_buffer))
		return -EINVAL;

	int rp = filep->trace_buffer->R;
	int wp = filep->trace_buffer->W;
	int can_do = filep->trace_buffer->can_do;
	if(!can_do & 1)
		return 0;
	char *buffer = filep->trace_buffer->buffer;

	int limit = wp;
	if(wp < rp)
		limit += TRACE_BUFFER_MAX_SIZE;
	limit -= rp;
	if(limit > count)
		limit = count;
	else
		can_do &= 2;
	
	int i = 0;
	while(i < limit)
	{
		buff[rp] = buffer[i];
		i++, rp++;
		rp %= TRACE_BUFFER_MAX_SIZE;
	}

	filep->trace_buffer->R = rp;
	filep->trace_buffer->can_do = can_do;
    return limit;
}


int trace_buffer_write(struct file *filep, char *buff, u32 count)
{
	if(is_valid_mem_range((unsigned long) buff, count, 1) == 0)
		return -EBADMEM;

	if(!filep || !(filep->trace_buffer))
		return -EINVAL;
	
	int rp = filep->trace_buffer->R;
	int wp = filep->trace_buffer->W;
	int can_do = filep->trace_buffer->can_do;
	if(!can_do & 2)
		return 0;
	char *buffer = filep->trace_buffer->buffer;

	int limit = rp;
	if(rp < wp)
		limit += TRACE_BUFFER_MAX_SIZE;
	limit -= wp;
	if(limit > count)
		limit = count;
	else
		can_do &= 1;
	
	int i = 0;
	while(i < limit)
	{
		buffer[wp] = buff[i];
		i++, wp++;
		wp %= TRACE_BUFFER_MAX_SIZE;
	}

	filep->trace_buffer->W = wp;
	filep->trace_buffer->can_do = can_do;
    return limit;
}


int sys_create_trace_buffer(struct exec_context *current, int mode)
{
	int i = 0;
	while(i < MAX_OPEN_FILES && (current->files)[i] != NULL && i++);
	if(i == MAX_OPEN_FILES)
		return -EINVAL;

	char *buff = (char *) os_page_alloc (USER_REG);
	if(!buff)
		return -EINVAL;
	
	struct trace_buffer_info *buffer_info = (struct trace_buffer_info *)os_alloc(sizeof(struct trace_buffer_info));
	if(!buffer_info)
		return -EINVAL;

	buffer_info->buffer = buff;
	buffer_info->can_do = 3;
	buffer_info->R = 0;
	buffer_info->W = 0;

	struct fileops *buffer_ops = (struct fileops *)os_alloc(sizeof(struct fileops));
	if(!buffer_ops)
		return -EINVAL;

	buffer_ops->read = NULL;
	buffer_ops->write = NULL;
	buffer_ops->close = trace_buffer_close;

	if(mode & O_READ)
		buffer_ops->read = trace_buffer_read;
	if(mode & O_WRITE)
		buffer_ops->write = trace_buffer_write;

	struct file *buffer = (struct file *)os_alloc(sizeof(struct file));

	buffer->fops = buffer_ops;
	buffer->inode = NULL;
	buffer->mode = mode;
	buffer->offp = 0;
	buffer->ref_count = 1;
	buffer->trace_buffer = buffer_info;
	buffer->type = TRACE_BUFFER;

	(current->files)[i] = buffer;
	printk("%d", i);
	return i;
}

///////////////////////////////////////////////////////////////////////////
//// 		Start of strace functionality 		      	      /////
///////////////////////////////////////////////////////////////////////////

int make_strace_base(struct exec_context *ctx)
{
	struct strace_head *head = (struct strace_head *)os_alloc(sizeof(struct strace_head));
	if(!head)
		return -EINVAL;
	head->count = 0;
	head->is_traced = 1;
	head->last = NULL;
	head->next = NULL;
	ctx->st_md_base = head;
	strace_base = 1;
}


int perform_tracing(u64 syscall_num, u64 param1, u64 param2, u64 param3, u64 param4)
{
	char buff[40];
	int count = 8;
	struct exec_context *ctx = get_current_ctx();

	if(ctx->st_md_base->is_traced == 0)
		return 0;

	if(syscall_num == SYSCALL_END_STRACE || syscall_num == SYSCALL_START_STRACE || syscall_num == SYSCALL_FORK || syscall_num == SYSCALL_VFORK || syscall_num == SYSCALL_CFORK)
		return 0;

	struct file *filep = ctx->files[ctx->st_md_base->strace_fd];

	if(ctx->st_md_base->tracing_mode == FILTERED_TRACING)
	{
		struct strace_info *head = ctx->st_md_base->next;
		int cnt = ctx->st_md_base->count;
		int flag = 0;
		for(int i = 0; i < cnt; i++)
		{
			if(head->syscall_num == syscall_num)
			{
				flag = 1;
				break;
			}
			head = head->next;
		}
		if(flag == 0)
			return 0;
	}

	int ret = trace_buffer_write(filep, (char *) &syscall_num, 8);

	if(ret == 8 && N[syscall_num] > 0)
		ret = trace_buffer_write(filep, (char *) &param1, 8);
	if(ret == 8 && N[syscall_num] > 1)
		ret = trace_buffer_write(filep, (char *) &param2, 8);
	if(ret == 8 && N[syscall_num] > 2)
		ret = trace_buffer_write(filep, (char *) &param3, 8);
	if(ret == 8 && N[syscall_num] > 3)
		ret = trace_buffer_write(filep, (char *) &param4, 8);
	
    return (ret == 8 ? 0 : -EINVAL);
}


int sys_strace(struct exec_context *current, int syscall_num, int action)
{
	if(!strace_base)
		if(make_strace_base(current) != 0)
			return -EINVAL;
	
	struct strace_head *head = current->st_md_base;
	struct strace_info *info_head = current->st_md_base->next;
	struct strace_info *tmp = NULL;
	int count = current->st_md_base->count;

	while(info_head != NULL && info_head->syscall_num != syscall_num)
	{
		tmp = info_head;
		info_head = info_head->next;
	}

	if(action == ADD_STRACE)
	{
		if(info_head != NULL || count == STRACE_MAX)
			return -EINVAL;
		
		struct strace_info *info_head = (struct strace_info *)os_alloc(sizeof(struct strace_info));

		info_head->next = NULL;
		info_head->syscall_num = syscall_num;

		if(!(current->st_md_base->last))
			current->st_md_base->next = info_head;
		else
			current->st_md_base->last->next = info_head;
		current->st_md_base->last = info_head;

		current->st_md_base->count = count + 1;

		return 0;
	}
	if(action == REMOVE_STRACE)
	{
		if(!info_head)
			return -EINVAL;
		if(tmp)
			tmp->next = info_head->next;
		else
			current->st_md_base->next = info_head->next;
		
		if(info_head == current->st_md_base->last)
			current->st_md_base->last = tmp;

		os_free(info_head, sizeof(struct strace_info));
		current->st_md_base->count = count - 1;
		return 0;
	}
	return -EINVAL;
}


int sys_read_strace(struct file *filep, char *buff, u64 count)
{
	if(!filep || !(filep->trace_buffer) || !buff)
		return -EINVAL;
	
	int bytes_read = 0;

	for(int i = 0, val = 0; i < count; i++)
	{
		val = trace_buffer_read(filep, buff + bytes_read, 8);
		if(val != 8)
			return -EINVAL;
		bytes_read += val;

		u32 syscall_num = ((u32*) buff)[0];

		for(int j = 0; j < 4 && val == 8; j++, bytes_read+= val)
			if(N[syscall_num] > j)
				val = trace_buffer_read(filep, buff + bytes_read, 8);
		if(val != 8)
			return -EINVAL;
	}

	return bytes_read;
}


int sys_start_strace(struct exec_context *current, int fd, int tracing_mode)
{
	if(fd >= MAX_OPEN_FILES || fd < 0 || tracing_mode != FILTERED_TRACING || tracing_mode != FULL_TRACING)
		return -EINVAL;

	if(!strace_base)
		if(make_strace_base(current) != 0)
			return -EINVAL;
	
	struct strace_head *head = current->st_md_base;
	head->strace_fd = fd;
	head->tracing_mode = tracing_mode;

	return 0;
}


int sys_end_strace(struct exec_context *current)
{
	if(!current->st_md_base)
		return -EINVAL;
	struct strace_info *head = current->st_md_base->next;
	current->st_md_base->next = NULL;
	current->st_md_base->last = NULL;
	while(head != NULL)
	{
		struct strace_info *tmp = head;
		head = head->next;
		tmp->next = NULL;
		os_free(tmp, sizeof(struct strace_info));
	}
	os_free(current->st_md_base, sizeof(struct strace_head));
	current->st_md_base = NULL;
	strace_base = 0;
	return 0;
}



///////////////////////////////////////////////////////////////////////////
//// 		Start of ftrace functionality 		      	      /////
///////////////////////////////////////////////////////////////////////////


int make_ftrace_base(struct exec_context *ctx)
{
	struct ftrace_head *head = (struct ftrace_head *)os_alloc(sizeof(struct ftrace_head));
	if(!head)
		return -EINVAL;

	head->count = 0;
	head->last = NULL;
	head->next = NULL;
	ctx->ft_md_base = head;
	ftrace_base = 1;
	return 0;
}


long do_add_ftrace(struct exec_context *ctx, unsigned long faddr, long nargs, int fd_trace_buffer)
{
	if(!ftrace_base)
		if(make_ftrace_base(ctx) != 0)
			return -EINVAL;
	
	struct ftrace_head *head = ctx->ft_md_base;
	struct ftrace_info *info_head = head->next;
	int count = head->count;

	if (count == FTRACE_MAX)
		return -EINVAL;

	for(int i = 0; (i < count) && info_head; i++, info_head = info_head->next)
		if(info_head->faddr == faddr)
			return -EINVAL;

	struct ftrace_info *info = (struct ftrace_info *)os_alloc(sizeof(struct ftrace_info));
	if(!info)
		return -EINVAL;
	
	info->capture_backtrace = 0;
	for(int i = 0; i < 4; i++)
		(info->code_backup)[i] = 0;
	
	info->faddr = faddr;
	info->fd = fd_trace_buffer;
	info->next = NULL;
	info->num_args = nargs;

	if(!(ctx->ft_md_base->last))
		ctx->ft_md_base->next = info;
	else
		ctx->ft_md_base->last->next = info;
	
	ctx->ft_md_base->last = info;

	head->count = count + 1;
	return 0;
}


long do_remove_ftrace(struct exec_context *ctx, unsigned long faddr, long nargs, int fd_trace_buffer)
{
	struct ftrace_head *head = ctx->ft_md_base;
	struct ftrace_info *info_head = head->next;
	struct ftrace_info *tmp = NULL;
	int count = head->count;

	for(int i = 0; i < count; i++)
	{
		if(!info_head || info_head->faddr == faddr)
			break;
		tmp = info_head;
		info_head = info_head->next;
	}

	if(!info_head)
		return -EINVAL;
	
	if(((u32 *)faddr)[0] == -1)
		if(do_ftrace(ctx, faddr, DISABLE_FTRACE, nargs, fd_trace_buffer) < 0)
			return -EINVAL;

	if(tmp)
		tmp->next = info_head->next;
	else
		ctx->ft_md_base->next = info_head->next;
	
	if(info_head == ctx->ft_md_base->last)
		ctx->ft_md_base->last = tmp;

	os_free(info_head, sizeof(struct ftrace_info));
	ctx->ft_md_base->count = count - 1;

	return 0;
}


long do_enable_ftrace(struct exec_context *ctx, unsigned long faddr, long nargs, int fd_trace_buffer)
{
	struct ftrace_head *head = ctx->ft_md_base;
	struct ftrace_info *info_head = head->next;
	int count = head->count;
	int flag = 0;

	for(int i = 0; i < count; i++)
	{
		if(!info_head)
			return -EINVAL;
		if(info_head->faddr == faddr)
		{
			flag = 1;
			break;
		}
	}
	if(!flag)
		return -EINVAL;

	if(((u32 *)faddr)[0] == -1)
		return 0;

	for(int i = 0; i < 4; i++)
		info_head->code_backup[i] = ((u8 *)faddr)[i];

	((u32 *)faddr)[0] = -1;
	
	return 0;
}


long do_disable_ftrace(struct exec_context *ctx, unsigned long faddr, long nargs, int fd_trace_buffer)
{
	struct ftrace_head *head = ctx->ft_md_base;
	struct ftrace_info *info_head = head->next;
	int count = head->count;
	int flag = 0;

	for(int i = 0; i < count; i++)
	{
		if(!info_head)
			return -EINVAL;
		if(info_head->faddr == faddr)
		{
			flag = 1;
			break;
		}
	}
	if(!flag)
		return -EINVAL;

	if(!(*((u32 *)faddr) == -1))
		return 0;

	for(int i = 0; i < 4; i++)
		((u8 *)faddr)[i] = info_head->code_backup[i];

	return 0;
}


long do_enable_backtrace(struct exec_context *ctx, unsigned long faddr, long nargs, int fd_trace_buffer)
{
	struct ftrace_head *head = ctx->ft_md_base;
	struct ftrace_info *info_head = head->next;
	int count = head->count;
	int flag = 0;

	for(int i = 0; i < count; i++)
	{
		if(!info_head)
			return -EINVAL;
		if(info_head->faddr == faddr)
		{
			flag = 1;
			break;
		}
	}
	if(!flag)
		return -EINVAL;

	if(!(*((u32 *)faddr) == -1))
		if(do_ftrace(ctx, faddr, ENABLE_FTRACE, nargs, fd_trace_buffer) < 0)
			return -EINVAL;

	info_head->capture_backtrace = 1;
	
	return 0;
}


long do_disable_backtrace(struct exec_context *ctx, unsigned long faddr, long nargs, int fd_trace_buffer)
{
	struct ftrace_head *head = ctx->ft_md_base;
	struct ftrace_info *info_head = head->next;
	int count = head->count;
	int flag = 0;

	for(int i = 0; i < count; i++)
	{
		if(!info_head)
			return -EINVAL;
		if(info_head->faddr == faddr)
		{
			flag = 1;
			break;
		}
	}
	if(!flag)
		return -EINVAL;

	if(!(*((u32 *)faddr) == -1))
		if(do_ftrace(ctx, faddr, DISABLE_FTRACE, nargs, fd_trace_buffer) < 0)
			return -EINVAL;

	info_head->capture_backtrace = 0;

	return 0;
}


long do_ftrace(struct exec_context *ctx, unsigned long faddr, long action, long nargs, int fd_trace_buffer)
{

	if(!ftrace_base)
		if(make_ftrace_base(ctx) != 0)
			return -EINVAL;

	switch (action)
	{
		case ADD_FTRACE:
			return(do_add_ftrace(ctx, faddr, nargs, fd_trace_buffer));
			break;
		case REMOVE_FTRACE:
			return(do_remove_ftrace(ctx, faddr, nargs, fd_trace_buffer));
			break;
		case ENABLE_FTRACE:
			return(do_enable_ftrace(ctx, faddr, nargs, fd_trace_buffer));
			break;
		case DISABLE_FTRACE:
			return(do_disable_ftrace(ctx, faddr, nargs, fd_trace_buffer));
			break;
		case ENABLE_BACKTRACE:
			return(do_enable_backtrace(ctx, faddr, nargs, fd_trace_buffer));
			break;
		case DISABLE_BACKTRACE:
			return(do_disable_backtrace(ctx, faddr, nargs, fd_trace_buffer));
			break;
		default:
    		return -EINVAL;
			break;
	}
}

//Fault handler
long handle_ftrace_fault(struct user_regs *regs)
{
	struct exec_context *ctx = get_current_ctx();
	struct ftrace_head *head = ctx->ft_md_base;
	struct ftrace_info *info_head = head->next;
	int count = head->count;

	for(int i = 0; i < count; i++)
	{
		if(info_head->faddr == regs->entry_rip)
			break;
		info_head = info_head->next;
	}

	struct file *filep = ctx->files[info_head->fd];

	trace_buffer_write(filep, (char *) &(info_head->faddr), 8);
	if (info_head->num_args > 0)
		trace_buffer_write(filep, (char *) &(regs->rdi), 8);
	if (info_head->num_args > 1)
		trace_buffer_write(filep, (char *) &(regs->rsi), 8);
	if (info_head->num_args > 2)
		trace_buffer_write(filep, (char *) &(regs->rdx), 8);
	if (info_head->num_args > 3)
		trace_buffer_write(filep, (char *) &(regs->rcx), 8);
	if (info_head->num_args > 4)
		trace_buffer_write(filep, (char *) &(regs->r8), 8);
	if (info_head->num_args > 5)
		trace_buffer_write(filep, (char *) &(regs->r9), 8);

	regs->entry_rsp -= 8;
	*((u64 *)regs->entry_rsp) = regs->rbp;
	regs->rbp = regs->entry_rsp;
	regs->entry_rip += 4;

	if(info_head->capture_backtrace)
	{
		u64 *rbp = (u64 *)regs->rbp;
		u64 return_address = info_head->faddr;
		while(return_address != END_ADDR)
		{
			trace_buffer_write(filep, (char *) &return_address, 8);
			return_address = ((u64 *)((u64)rbp + 8))[0];
			rbp = (u64 *)(*rbp);
		}
	}

	trace_buffer_write(filep, (char *)(&delimiter), 8);

    return 0;
}


int sys_read_ftrace(struct file *filep, char *buff, u64 count)
{
	if(count < 0)
		return -EINVAL;

	int bytes_read = 0;

	for(int i = 0; i < count; i++)
		for(int j = 0; ; j++)
		{
			u64 tmp = 0;
			int ret = trace_buffer_read(filep, (char *) &tmp, 8);
			if(ret != 8)
				return -EINVAL;
			if(tmp == delimiter)
				break;
			((u64 *) buff)[j] = tmp;
			bytes_read += ret;
		}
	
	return bytes_read;
}