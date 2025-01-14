<h1 align="center">
    
</h1>
A virtual memory implementation with paging support built on top of a LC-3 virtual machine architecture. Features memory management, process handling, and system calls.

## Table of Contents
- [Overview](#overview)
- [Features](#features)
- [Implementation Details](#implementation-details)
- [Building and Running](#building-and-running)
- [Sample Programs](#sample-programs)
- [Acknowledgements](#acknowledgements)
- [License](#license)

## Overview
This project extends an existing LC-3 virtual machine implementation to incorporate paging and virtual memory management capabilities. It builds upon [Andrei Ciobanu's LC-3 VM implementation](https://github.com/nomemory/lc3-vm) by adding:
- Page-based memory management 
- Virtual-to-physical address translation
- Multi-process support with PCBs (Process Control Blocks)
- Dynamic memory allocation
- System calls for process control and memory management

The implementation assumes a 16-bit address space with 4KB page size and includes features like:
- Separate code and heap segments
- Read/write access control at page level
- Process isolation through virtual address spaces
- Free page frame management using bitmaps

## Features

### Memory Management
- 128KB total memory space (16-bit addressing)
- 4KB page size
- Page-level access control (read/write permissions)
- Dynamic page allocation and freeing
- Bitmap-based free page tracking

### Process Management
- Process Control Block (PCB) support
- Context switching capabilities 
- Process creation and termination handling
- Process scheduling through yield system call

### System Calls Implemented
- `yield`: Voluntarily releases CPU control
- `brk`: Dynamic memory allocation/deallocation 
- `halt`: Process termination with cleanup

### Virtual Address Space Layout
- Reserved region (0x0000 - 0x2FFF)
- Code segment (0x3000 - 0x4FFF, 8KB fixed)
- Heap segment (0x5000+, dynamically sized)

## Implementation Details

### Key Components
1. **Page Table Entry (PTE) Structure**
   - 5 bits: Page Frame Number (PFN)
   - 8 bits: Padding
   - 3 bits: Access control (write, read, valid)

2. **Physical Memory Organization**
   - First 8KB: OS region (PCBs, metadata)
   - Third 4KB page: Page Tables
   - Remaining space: Page frames for processes

3. **Process Control Block Fields**
   - PID
   - Program Counter
   - Page Table Base Register

### Memory Access Protection
The implementation includes several protection mechanisms:
- Segmentation fault detection
- Read/write permission enforcement
- Invalid page access prevention
- Reserved memory protection

## Building and Running

```bash
# Compile the project
make

# Run single program
./vm code.obj heap.obj

# Run multiple programs
./vm code1.obj heap1.obj code2.obj heap2.obj

# Run sample programs
./samples/sample1.sh
./samples/sample2.sh
./samples/sample3.sh
./samples/sample4.sh
./samples/sample5.sh
```

## Sample Programs
The project includes several sample scripts that demonstrate different aspects of the VM:
### Sample1
- Single process that adds 10 numbers from memory
- Tests basic memory access and process execution

### Sample2
- Process that requests additional memory page via BRK before adding numbers
- Tests dynamic memory allocation and page table management

### Sample3
- Two copies of Sample1 running concurrently
- Tests basic multi-process support and context switching

### Sample4
- Three processes running together:
    - Two processes that yield explicitly (yld.c)
    - One process that allocates memory (brk.c)
- Tests mixed workload with memory allocation and context switching

### Sample5
- Two copies of a program (brk2.c) that each request two memory pages with a yield in between
- Tests interleaved memory allocation between processes

## Acknowledgements
This project builds upon the LC-3 virtual machine implementation by [Andrei Ciobanu](https://github.com/nomemory/lc3-vm). The original implementation provided the foundation for the basic VM functionality, which was then extended with paging and process management capabilities.

## License
MIT License