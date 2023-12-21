# CS330: Operating Systems - Assignment 3

## Question 1: Memory Mapping in gemOS

### Objective
This assignment involves implementing memory mapping support in gemOS through three system calls: `mmap`, `munmap`, and `mprotect`. The focus is on managing virtual address space, allowing the creation, modification, and removal of memory mappings. The system calls are designed to emulate Linux/Unix POSIX calls, and the implementation centers around Virtual Memory Areas (VMAs) and their manipulation.

### Key Points:
- **System Calls:**
  - `mmap`: Creates a mapping in the virtual address space.
  - `munmap`: Deletes mappings for a specified address range.
  - `mprotect`: Changes access protections for memory pages.

- **Implementation Details:**
  - VMAs are maintained as a linked list.
  - Support for various scenarios, including creating new mappings, merging with existing ones, and using hint addresses.
  - Validity checks for arguments and handling different protection scenarios.
  - Maximum of 128 VMAs at any given time, including the dummy VMA.


## Question 2: Page Table Manipulations

### Overview
This section focuses on enabling lazy allocation support in gemOS through page table manipulations. The goal is to delay physical memory allocation until a virtual address is accessed for the first time. Three key functions are implemented or modified:


### Lazy Allocation Support
- **vm_area_pagefault Function:**
  - Handles page faults within a specified address range.
  - Validates addresses against VMAs and checks access against protection flags.
  - Allocates a new physical page frame and updates page table entries for valid accesses.
  - Differentiates between read and write accesses, handling various scenarios accordingly.

### Memory Management Functions
- **munmap Function:**
  - Frees allocated physical memory for a VMA during the unmapping process.
  - Updates virtual-to-physical translation to result in invalid memory error (SIGSEGV) for subsequent accesses within the unmapped range.

- **mprotect Function:**
  - Updates virtual-to-physical translation to enforce changes in access permissions when mprotect modifies protections of a virtual address region.


## Question 3: Copy-on-Write Fork Implementation

This section details the implementation of the copy-on-write (CoW) fork system call (`cfork()`) and the associated function (`handle_cow_fault()`) in gemOS. These modifications enable a more efficient process creation mechanism and handle CoW faults gracefully.

### `cfork()` System Call

#### Description
- A variant of the `fork()` system call implementing a copy-on-write policy for the address space.
- Creates a child process without copying memory content.
- Virtual-to-physical mapping is adjusted to create a copy of a page when either process performs a write.

### `handle_cow_fault()` Function

#### Description
- Called from `vm_area_pagefault()` and other functions handling page faults.
- Handles CoW faults occurring for any user space address.
- Updates virtual-to-physical translation information and adjusts the reference count of PFNs.

#### Changes to `munmap()` and `mprotect()`
- With CoW mapping, does not free any mapped page on an unmap system call.
- While changing the access flags of the VMA and the translation entries, considers the shared nature of PFN.

**Note:** The implementation of `handle_cow_fault()` ensures correct handling of CoW faults, and changes to `munmap()` and `mprotect()` accommodate CoW mapping characteristics.
