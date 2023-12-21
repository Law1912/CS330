# CS330: Operating Systems - Assignment 2

## Question 1: Trace Buffer Support in gemOS

### Objective
Implement trace buffer support in gemOS to enable storing and retrieving data. The trace buffer acts as a unidirectional data channel, allowing user processes to read from and write to it using a single file descriptor.

### Key Implementations
1. **Trace Buffer Operations:**
   - Creation, write, and read functionalities implemented.

2. **Memory Management:**
   - Efficient memory allocation and deallocation using provided functions.

3. **User Buffer Validity Check:**
   - Implemented `is_valid_mem_range()` to ensure the legitimacy of user space buffers in read and write operations.

### System Calls Implemented
- `create_trace_buffer(int mode)`
- `read(int fd, void *buf, int count)`
- `write(int fd, const void *buf, int count)`
- `close(int fd)`

## Question 2: Implementing system call tracing functionality

### Objective
Implement system call tracing functionality in gemOS, similar to the strace utility in Linux. This feature allows the interception and recording of system calls invoked by a process. The captured information, including system call number and argument values, is stored in a configured trace buffer, accessible to user space through the `read_strace` system call.

### Key Implementations
1. **Initialization and Tracing:**
   - Implement `start_strace` to initialize system call tracing, specifying either FULL TRACING or FILTERED TRACING mode.
   - Implement `end_strace` to clean up metadata structures related to system call tracing.

2. **Tracing System Calls:**
   - Introduce `perform_tracing` to capture system call information, including system call number and parameter values.

3. **Configuring Traced System Calls:**
   - Implement `strace` to add or remove specific system calls for FILTERED TRACING mode.

4. **Retrieving Tracing Information:**
   - Implement `read_strace` to retrieve information about traced system calls from the trace buffer.

### Syscalls Implemented
1. `int start_strace(int fd, int tracing_mode)`
2. `int end_strace(void)`
3. `int perform_tracing(u64 syscall_num, u64 param1, u64 param2, u64 param3, u64 param4)`
4. `int strace(int syscall_num, int action)`
5. `int read_strace(int fd, void *buff, int count)`

### Notes
- Tracing information is stored in a trace buffer, with read/write offsets modified as needed.
- Traced system calls can be configured dynamically during execution.
- `read_strace` retrieves information in a specific format: system call number followed by parameter values.

## Question 3: Implementing function call tracing functionality

### Objective
Implement function call tracing functionality in gemOS, allowing the interception and recording of function calls during the execution of a process. This feature is valuable for instructional and debugging purposes.

### Key Implementations
1. **Function Tracing:**
   - Introduce `ftrace` system call to add, remove, enable, or disable tracing for specific functions.
   - Capture information such as function address, arguments, and call back-trace when a traced function is called.

2. **Fault Handling:**
   - Implement `handle_ftrace_fault` to handle invalid opcode faults and save tracing information into the trace buffer.

3. **Retrieving Tracing Information:**
   - Implement `read_ftrace` system call to retrieve information about traced functions from the trace buffer.

### Syscalls Implemented
1. `long ftrace(unsigned long func_addr, long action, long nargs, int fd_trace_buffer)`
2. `int read_ftrace(int fd, void *buff, int count)`
3. `long handle_ftrace_fault(struct user_regs *regs)`

### Actions Supported by `ftrace` System Call
- **ADD FTRACE:** Add a function to the list of functions to be traced.
- **REMOVE FTRACE:** Remove a function from the list of traced functions.
- **ENABLE FTRACE:** Start tracing an existing function.
- **DISABLE FTRACE:** Stop tracing a function.
- **ENABLE BACKTRACE:** Capture call back-trace along with the normal function trace.
- **DISABLE BACKTRACE:** Stop capturing call back-trace.
