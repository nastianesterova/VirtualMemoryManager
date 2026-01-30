# Virtual Memory Manager

A C implementation of a virtual memory manager that simulates address translation, page table management, TLB caching, and page replacement algorithms.

```
┌─────────────────┐     ┌─────────────┐     ┌──────────────┐     ┌─────────────────┐
│ Logical Address │ ──▶ │     TLB     │ ──▶ │  Page Table  │ ──▶ │ Physical Memory │
│   (16-bit)      │     │ (16 entries)│     │ (256 entries)│     │   (frames)      │
└─────────────────┘     └─────────────┘     └──────────────┘     └─────────────────┘
                              │                    │
                              │  miss              │ page fault
                              ▼                    ▼
                        ┌─────────────────────────────┐
                        │      BACKING_STORE.bin      │
                        │     (secondary storage)     │
                        └─────────────────────────────┘
```

## Overview

This project demonstrates core operating system concepts by implementing a virtual memory system that translates 16-bit logical addresses to physical addresses. The implementation handles:

- **Address Translation**: Converts logical addresses to physical addresses using page tables
- **TLB Management**: Implements a Translation Lookaside Buffer with FIFO replacement
- **Page Fault Handling**: Loads pages from backing store on demand
- **Dirty Page Tracking**: Tracks modified pages for write-back

## Project Structure

```
VirtualMemoryManager/
├── data/
│   ├── addresses.txt        # Input file with logical addresses
│   └── BACKING_STORE.bin    # Simulated secondary storage (64KB)
├── part1/
│   ├── intro.c              # Introduction to address parsing
│   ├── mem_manager.c        # Basic memory manager (256 frames)
│   ├── Makefile
│   └── out.txt              # Sample output
└── part2/
    ├── virtual_manager.c    # Advanced manager with page replacement
    ├── Makefile
    └── out.txt              # Sample output
```

## Part 1: Basic Memory Manager

The basic implementation assumes physical memory equals virtual memory size (256 frames × 256 bytes = 64KB).

### Features
- **256 page table entries** (8-bit page numbers)
- **16-entry TLB** with FIFO replacement policy
- **256-byte page size**
- **No page replacement needed** (enough physical frames for all pages)

### Address Format
```
┌────────────────────────────────────┐
│ 17-bit input format                │
├──────────┬──────────┬──────────────┤
│ Write Bit│ Page No. │    Offset    │
│  (bit 16)│ (bits 8-15)│ (bits 0-7) │
└──────────┴──────────┴──────────────┘
```

### Build & Run
```bash
cd part1
make
./mem_manager ../data/addresses.txt ../data/BACKING_STORE.bin
```

## Part 2: Virtual Manager with Page Replacement

The advanced implementation uses only **128 physical frames** (half of virtual memory), requiring page replacement when memory is full.

### Features
- **128 physical frames** (32KB physical memory)
- **FIFO page replacement** when no free frames available
- **Write-back of dirty pages** before eviction
- **TLB invalidation** when pages are replaced

### Page Replacement Algorithm
When a page fault occurs and no free frames exist:
1. Select victim frame using FIFO (circular pointer)
2. If victim page is dirty, write it back to `BACKING_STORE.bin`
3. Invalidate victim's page table entry
4. Remove victim from TLB if present
5. Load requested page into freed frame

### Build & Run
```bash
cd part2
make
./virtual_manager ../data/addresses.txt ../data/BACKING_STORE.bin
```

## Output Format

Both programs output address translations in the following format:
```
LOGICAL_ADDR PHYSICAL_ADDR VALUE DIRTY_BIT
```

Example:
```
0x4214 0x1A14 42 0
0x7A3D 0x0C3D 15 1
```

At completion, statistics are displayed:
```
Page-fault rate: 0.244000
TLB hit rate: 0.054800
Number of dirty pages: 12
```

## Data Structures

### Page Table Entry (PTE)
```c
struct PTE {
    int frame_no;   // Physical frame number
    int valid;      // Valid/invalid bit
    int dirty;      // Modified bit
};
```

### TLB Entry (TLBE)
```c
struct TLBE {
    int page_no;    // Logical page number
    struct PTE pte; // Associated page table entry
};
```

### TLB (FIFO Circular Buffer)
```c
struct TlbFifo {
    struct TLBE *tlb;   // Array of TLB entries
    int first;          // Index of oldest entry
    int size;           // Current number of entries
    int num_hits;       // Hit counter for statistics
};
```

## System Parameters

| Parameter | Part 1 | Part 2 |
|-----------|--------|--------|
| Virtual Address Space | 64 KB | 64 KB |
| Physical Memory | 64 KB | 32 KB |
| Page Size | 256 bytes | 256 bytes |
| Number of Pages | 256 | 256 |
| Number of Frames | 256 | 128 |
| TLB Entries | 16 | 16 |

## Input Files

### addresses.txt
Contains logical addresses (one per line) to be translated. Values above 16 bits indicate a write operation (bit 16 = 1).

### BACKING_STORE.bin
A 64KB binary file representing secondary storage. Pages are loaded from this file on page faults and dirty pages are written back (Part 2 only).

## Building

To build all components:
```bash
# Part 1
cd part1 && make && cd ..

# Part 2
cd part2 && make
```

To clean build artifacts:
```bash
cd part1 && make clean && cd ..
cd part2 && make clean
```

## Key Concepts Demonstrated

- **Demand Paging**: Pages loaded only when accessed
- **Address Translation**: Logical to physical address mapping
- **TLB Caching**: Fast address translation via hardware cache simulation
- **Page Replacement**: FIFO algorithm when physical memory is exhausted
- **Write-Back Policy**: Dirty pages written to storage only when evicted

## Requirements

- GCC compiler
- Make build system
- POSIX-compliant system (Linux, macOS, WSL)

## License

Educational project for operating systems coursework.
