# CS330: Operating Systems


# Assignment 1

## Question 1: Chain of Unary Operations

### Overview
Implemented three C programs, `square.c`, `double.c`, and `sqroot.c`, performing square, double, and square root operations, respectively, on a non-negative integer.

### Example
```
$ ./square 8 | ./sqroot | ./double | ./sqroot
4
```

## Question 2: Directory Space Usage

### Overview
Wrote a program (`myDU.c`) to find the space used by a directory, including its files, sub-directories, and their contents.

### Example
```bash
$ ./myDU <relative path to a directory>
```

## Question 3: Dynamic Memory Management Library

### Overview
Implemented two library functions, `memalloc()` and `memfree()`, equivalent to the C standard library functions `malloc()` and `free()`.

### Instructions
- `memalloc()` served memory allocation requests from 4MB chunks allocated using `mmap()`.
- Used a First Fit approach for memory allocation.
- Maintained metadata to keep track of allocated and free memory regions within the larger chunk.
- Coalesced memory chunks during deallocation to optimize space.
