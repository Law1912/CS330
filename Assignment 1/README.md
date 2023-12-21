# CS330: Operating Systems - Assignment 1

## Question 1: Chain of Unary Operations

### Task
Write three C programs, `square.c`, `double.c`, and `sqroot.c`, to perform square, double, and square root operations on non-negative integers. The executables can be chained for composite operations on a given input.

### Allowed System Calls and Functions
- `fork` - `malloc` - `exec* family` - `free` - `strcpy` - `strcat` - `strcmp` - `strto* family` - `ato* family` - `wait/waitpid` - `printf, sprintf` - `sqrt` - `round` - `exit` - `strlen`

### Example
```bash
$ ./square 8 | ./sqroot | ./double | ./sqroot
4
```

## Question 2: Directory Space Usage

### Task
Develop a C program (`myDU.c`) that calculates the space used by a directory and its contents. The program should take the relative path to the root directory as input and print the total size of the contents of the root directory in bytes.

### Key Instructions
1. **Efficient Calculation:** Utilize multiple processes to calculate space usage efficiently. Each immediate sub-directory is processed by a separate child process, and results are communicated back to the parent process.
   
2. **Symbolic Links:** Handle symbolic links by resolving them and finding the size of the file/directory to which the symbolic link points.

### Allowed System Calls and Functions
- `fork` - `malloc` - `exec* family` - `pipe` - `stat` - `opendir` - `lstat` - `readdir` - `readlink` - `closedir` - `strlen` - `read` - `open` - `write` - `close` - `strcpy` - `strcat` - `strcmp` - `strto* family` - `ato* family` - `wait/waitpid` - `printf family` - `exit` - `dup/dup2`

### Example
```bash
$ ./myDU <relative path to a directory>
```

## Question 3: Dynamic Memory Management Library

### Task
Implement two library functions, `memalloc()` and `memfree()`, equivalent to the C standard library functions `malloc()` and `free()`. `memalloc()` allocates dynamic memory, and `memfree()` frees previously allocated memory.

### Instructions
- `memalloc()` serves memory allocation requests from 4MB chunks allocated using `mmap()`.
- Use a First Fit approach for memory allocation.
- Maintain metadata to keep track of allocated and free memory regions within the larger chunk.
- Coalesce memory chunks during deallocation to optimize space.

### Allowed System Calls and Functions
- `mmap`

