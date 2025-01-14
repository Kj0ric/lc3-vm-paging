#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm_dbg.h"

#define NOPS (16)

#define OPC(i) ((i) >> 12)
#define DR(i) (((i) >> 9) & 0x7)
#define SR1(i) (((i) >> 6) & 0x7)
#define SR2(i) ((i) & 0x7)
#define FIMM(i) ((i >> 5) & 01)
#define IMM(i) ((i) & 0x1F)
#define SEXTIMM(i) sext(IMM(i), 5)
#define FCND(i) (((i) >> 9) & 0x7)
#define POFF(i) sext((i) & 0x3F, 6)
#define POFF9(i) sext((i) & 0x1FF, 9)
#define POFF11(i) sext((i) & 0x7FF, 11)
#define FL(i) (((i) >> 11) & 1)
#define BR(i) (((i) >> 6) & 0x7)
#define TRP(i) ((i) & 0xFF)

/* New OS declarations */

// OS bookkeeping constants
#define PAGE_SIZE       (4096)  // Page size in bytes
#define OS_MEM_SIZE     (2)     // OS Region size. Also the start of the page tables' page
#define Cur_Proc_ID     (0)     // id of the current process
#define Proc_Count      (1)     // total number of processes, including ones that finished executing.
#define OS_STATUS       (2)     // Bit 0 shows whether the PCB list is full or not
#define OS_FREE_BITMAP  (3)     // Bitmap for free pages

// Process list and PCB related constants
#define PCB_SIZE  (3)  // Number of fields in a PCB
#define PID_PCB   (0)  // Holds the pid for a process
#define PC_PCB    (1)  // Value of the program counter for the process
#define PTBR_PCB  (2)  // Page table base register for the process

#define CODE_SIZE       (2)  // Number of pages for the code segment
#define HEAP_INIT_SIZE  (2)  // Number of pages for the heap segment initially

bool running = true;

typedef void (*op_ex_f)(uint16_t i);
typedef void (*trp_ex_f)();

enum { trp_offset = 0x20 };
enum regist { R0 = 0, R1, R2, R3, R4, R5, R6, R7, RPC, RCND, PTBR, RCNT };
enum flags { FP = 1 << 0, FZ = 1 << 1, FN = 1 << 2 };

uint16_t mem[UINT16_MAX] = {0};
uint16_t reg[RCNT] = {0};
uint16_t PC_START = 0x3000;

void initOS();
int createProc(char *fname, char *hname);
void loadProc(uint16_t pid);
uint16_t allocMem(uint16_t ptbr, uint16_t vpn, uint16_t read, uint16_t write);  // Can use 'bool' instead
int freeMem(uint16_t ptr, uint16_t ptbr);
static inline uint16_t mr(uint16_t address);
static inline void mw(uint16_t address, uint16_t val);
static inline void tbrk();
static inline void thalt();
static inline void tyld();
static inline void trap(uint16_t i);

static inline uint16_t sext(uint16_t n, int b) { return ((n >> (b - 1)) & 1) ? (n | (0xFFFF << b)) : n; }
static inline void uf(enum regist r) {
    if (reg[r] == 0)
        reg[RCND] = FZ;
    else if (reg[r] >> 15)
        reg[RCND] = FN;
    else
        reg[RCND] = FP;
}
static inline void add(uint16_t i)  { reg[DR(i)] = reg[SR1(i)] + (FIMM(i) ? SEXTIMM(i) : reg[SR2(i)]); uf(DR(i)); }
static inline void and(uint16_t i)  { reg[DR(i)] = reg[SR1(i)] & (FIMM(i) ? SEXTIMM(i) : reg[SR2(i)]); uf(DR(i)); }
static inline void ldi(uint16_t i)  { reg[DR(i)] = mr(mr(reg[RPC]+POFF9(i))); uf(DR(i)); }
static inline void not(uint16_t i)  { reg[DR(i)]=~reg[SR1(i)]; uf(DR(i)); }
static inline void br(uint16_t i)   { if (reg[RCND] & FCND(i)) { reg[RPC] += POFF9(i); } }
static inline void jsr(uint16_t i)  { reg[R7] = reg[RPC]; reg[RPC] = (FL(i)) ? reg[RPC] + POFF11(i) : reg[BR(i)]; }
static inline void jmp(uint16_t i)  { reg[RPC] = reg[BR(i)]; }
static inline void ld(uint16_t i)   { reg[DR(i)] = mr(reg[RPC] + POFF9(i)); uf(DR(i)); }
static inline void ldr(uint16_t i)  { reg[DR(i)] = mr(reg[SR1(i)] + POFF(i)); uf(DR(i)); }
static inline void lea(uint16_t i)  { reg[DR(i)] =reg[RPC] + POFF9(i); uf(DR(i)); }
static inline void st(uint16_t i)   { mw(reg[RPC] + POFF9(i), reg[DR(i)]); }
static inline void sti(uint16_t i)  { mw(mr(reg[RPC] + POFF9(i)), reg[DR(i)]); }
static inline void str(uint16_t i)  { mw(reg[SR1(i)] + POFF(i), reg[DR(i)]); }
static inline void rti(uint16_t i)  {} // unused
static inline void res(uint16_t i)  {} // unused
static inline void tgetc()        { reg[R0] = getchar(); }
static inline void tout()         { fprintf(stdout, "%c", (char)reg[R0]); }
static inline void tputs() {
  uint16_t *p = mem + reg[R0];
  while(*p) {
    fprintf(stdout, "%c", (char) *p);
    p++;
  }
}
static inline void tin()      { reg[R0] = getchar(); fprintf(stdout, "%c", reg[R0]); }
static inline void tputsp()   { /* Not Implemented */ }
static inline void tinu16()   { fscanf(stdin, "%hu", &reg[R0]); }
static inline void toutu16()  { fprintf(stdout, "%hu\n", reg[R0]); }

trp_ex_f trp_ex[10] = {tgetc, tout, tputs, tin, tputsp, thalt, tinu16, toutu16, tyld, tbrk};
static inline void trap(uint16_t i) { trp_ex[TRP(i) - trp_offset](); }
op_ex_f op_ex[NOPS] = {/*0*/ br, add, ld, st, jsr, and, ldr, str, rti, not, ldi, sti, jmp, res, lea, trap};

/**
  * Load an image file into memory.
  * @param fname the name of the file to load
  * @param offsets the offsets into memory to load the file
  * @param size the size of the file to load
*/
void ld_img(char *fname, uint16_t *offsets, uint16_t size) {
    FILE *in = fopen(fname, "rb");
    if (NULL == in) {
        fprintf(stderr, "Cannot open file %s.\n", fname);
        exit(1);
    }

    for (uint16_t s = 0; s < size; s += PAGE_SIZE) {
        uint16_t *p = mem + offsets[s / PAGE_SIZE];
        uint16_t writeSize = (size - s) > PAGE_SIZE ? PAGE_SIZE : (size - s);
        fread(p, sizeof(uint16_t), (writeSize), in);
    }
    
    fclose(in);
}

void run(char *code, char *heap) {
  while (running) {
    uint16_t i = mr(reg[RPC]++);
    op_ex[OPC(i)](i);
  }
}

// YOUR CODE STARTS HERE

// Additional Bitmap related definitions
#define PAGE_USED 0
#define PAGE_FREE 1
#define BITMAP_HIGH (OS_FREE_BITMAP)
#define BITMAP_LOW (OS_FREE_BITMAP + 1)
#define GET_BITMAP() ((uint32_t)mem[BITMAP_HIGH] << 16 | mem[BITMAP_LOW]) 
// Macros for PTE creation 
#define PFN_SHIFT (11)
#define PFN_MASK (0x1F)     // Mask to extract PFN (0001 1111)
#define WRITE_BIT (0x0004)  // 0000 0000 0000 0100
#define READ_BIT  (0x0002)  // 0000 0000 0000 0010
#define VALID_BIT (0x0001)  // 0000 0000 0000 0001
// Additional PCB and Page Table related definitions
#define PCB_LIST_BASE (12)
#define PAGE_TABLE_BASE (0x1000)      // Start of the 3rd page which is 8KB
#define PAGE_TABLE_SIZE_IN_WORDS (32)
// Additional process creation definitions
#define CODE_VPN_START (6)
#define HEAP_VPN_START (8)
#define MAX_PROCESS_NUM (4084/PCB_SIZE)
#define PAGE_SIZE_IN_WORDS (PAGE_SIZE / 2)
// Additional definitions for mr and mw methods
#define VPN_SHIFT (11)
#define NOT_RESERVED_START_VPN (0x06)
#define SEG_FAULT_OUTPUT (0xFFFF)
// Additional definitions for tyld and tbrk
#define INVALID_PID (UINT16_MAX)

/* HELPER FUNCTIONS */
// Check if there are enough free pages in memory for the given number of pages
bool checkFreePages(int requiredPages) {
  uint32_t bitmap = GET_BITMAP();
  int freePages = 0;

  // Count the free pages by checking each bit in the bitmap 
  for (int i = 31; i >= 0; i--) {
    if (bitmap & (1 << i)) {
      freePages++;
    }
  }
  return freePages >= requiredPages;
}

// Set bitmap to physical mem
void setBitmap(uint32_t bitmap) {
  mem[BITMAP_HIGH] = (uint16_t)(bitmap >> 16) & UINT16_MAX; // Extract the upper 16 bits
  mem[BITMAP_LOW] = (uint16_t)(bitmap & UINT16_MAX);        // Extract the lower 16 bits
}

// Allocate page table for a process
uint16_t allocatePageTable(uint16_t pid) {
  // Calculate the base address of the page table
  uint16_t ptb = PAGE_TABLE_BASE + pid * PAGE_TABLE_SIZE_IN_WORDS;
  return ptb;
}

// Free allocated resources in case of allocation failure in createProc
void freeAllocatedResources(uint16_t pageTableBase, uint16_t startVPN, uint16_t endVPN) {
  for (uint16_t vpn = startVPN; vpn <= endVPN; vpn++) {
    freeMem(vpn, pageTableBase); 
  }
}

void handleSegFault(char* msg) {
  printf("%s\n", msg);
  running = false;
  exit(1);  // Terminate the simulation for good
}

/* Initialize OS-related parts of physical mem */
void initOS() {
  // Set curProcID to 0xFFFF
  mem[Cur_Proc_ID] = UINT16_MAX;
  // Set procCount to 0
  mem[Proc_Count] = 0;
  // Set OSStatus to 0x0000
  mem[OS_STATUS] = 0x0000;

  // Initialize bitmap for free pages. 
  // Bitmap is 32 bits long. Each bit represents a page.
  // mem[3] and mem[4] are used to store the bitmap
  // 0 for used, 1 for free

  // First two pages are reserved to OS. Third page is for Page Table
  // mem[3] = 0001 1111 1111 1111
  // mem[4] = 1111 1111 1111 1111
  mem[BITMAP_HIGH] = 0x1FFF;
  mem[BITMAP_LOW] = UINT16_MAX;

  // Initialize the padding between bitmap and PCB list
  for (uint16_t i = BITMAP_LOW + 1; i < PCB_LIST_BASE; i++) {
    mem[i] = 0;
  } 

  // PCB list and page table will be initialized in createProc
  return;
}

// Process functions to implement

/* Create process. Return 0 on fail, 1 on success. */
int createProc(char *fname, char *hname) {
  // 1. Check if OS region of mem is full. Then cannot allocate new PCB
  if (mem[OS_STATUS] & 0x0001) {
    printf("The OS memory region is full. Cannot create a new PCB.\n");
    return 0;
  }

  // 2. Check if enough free pages for allocating code segment
  if (!checkFreePages(CODE_SIZE)) {
    printf("Cannot create code segment.\n");
    return 0;
  }
    
  // 3. Check if enough free pages for allocating heap segment 
  if (!checkFreePages(HEAP_INIT_SIZE)) {
    printf("Cannot create heap segment.\n");
    return 0;
  }

  // Process variables
  uint16_t pid = mem[Proc_Count];
  mem[Proc_Count]++; // Increment Proc_Count
  uint16_t pcbIndex = PCB_LIST_BASE + pid * PCB_SIZE;

  // 4. Fill in PCB for the process
  mem[pcbIndex + PID_PCB] = pid;
  mem[pcbIndex + PC_PCB] = PC_START;
  uint16_t pageTableBase = allocatePageTable(pid);
  mem[pcbIndex + PTBR_PCB] = pageTableBase;

  // 6. Allocate memory (2 pages) for code via allocMem
  uint16_t codeOffsets[2];
  codeOffsets[0] = allocMem(pageTableBase, CODE_VPN_START, UINT16_MAX, 0);
  codeOffsets[1] = allocMem(pageTableBase, CODE_VPN_START + 1, UINT16_MAX, 0);
  if (codeOffsets[0] == 0 || codeOffsets[1] == 0) {
    printf("Cannot allocate memory for code segment.\n");
    freeAllocatedResources(pageTableBase, CODE_VPN_START, CODE_VPN_START + 1);
    return 0;
  }
  // Initialize code segment by reading fname using ld_img
  ld_img(fname, codeOffsets, CODE_SIZE * PAGE_SIZE_IN_WORDS);

  // 7. Allocate memory (2 pages) for heap via allocMem
  uint16_t heapOffsets[2];
  heapOffsets[0] = allocMem(pageTableBase, HEAP_VPN_START, UINT16_MAX, UINT16_MAX);
  heapOffsets[1] = allocMem(pageTableBase, HEAP_VPN_START + 1, UINT16_MAX, UINT16_MAX);
  if (heapOffsets[0] == 0 || heapOffsets[1] == 0) {
    printf("Cannot allocate memory for heap segment.\n");
    freeAllocatedResources(pageTableBase, CODE_VPN_START, CODE_VPN_START + 1);
    freeAllocatedResources(pageTableBase, HEAP_VPN_START, HEAP_VPN_START + 1);
    return 0;
  }
  // Initialize heap segment by reading hname using ld_img
  ld_img(hname, heapOffsets, HEAP_INIT_SIZE * PAGE_SIZE_IN_WORDS);

  if (mem[Proc_Count] == MAX_PROCESS_NUM) {
    mem[OS_STATUS] |= 0x0001;   // OS memory is full, mark as 1
  }
  return 1;
}

void loadProc(uint16_t pid) {
  // Calculate the PCB index based on pid
  uint16_t pcbIndex = PCB_LIST_BASE + pid * PCB_SIZE;
  // Retrieve PC, PTBR values
  uint16_t pc = mem[pcbIndex + PC_PCB];
  uint16_t ptbr = mem[pcbIndex + PTBR_PCB];

  // Restore them into CPU registers
  reg[RPC] = pc;
  reg[PTBR] = ptbr;
  // Set the current process ID
  mem[Cur_Proc_ID] = pid;
}

/* Return 0 on fail, otherwise return physical address of the page frame allocated */
uint16_t allocMem(uint16_t ptbr, uint16_t vpn, uint16_t read, uint16_t write) {
  int freePFNidx = -1;
  // 1. Find the first free page frame by searching the bitmap
  uint32_t bitmap = GET_BITMAP();
  for (int i = 31; i >= 0; i--) {
    if (bitmap & (1 << i)){
      freePFNidx = i;
      break;
    }
  }

  // If no free pages left, return 0
  if (freePFNidx == -1) return 0; 

  // 2. Calculate physical address of PTE for this VPN
  uint16_t pte = mem[ptbr + vpn];

  // 3. Check if a page frame already allocated for this VPN
  if (pte & 0x0001) return 0;

  // 4. Allocate the page frame and update the bitmap 
  bitmap &= ~(1 << freePFNidx); // Mark the page frame as used (0) in the bitmap
  // Update the bitmap in memory 
  setBitmap(bitmap);

  // If no free pages left, set OS_STATUS bit to 1
  //if (bitmap == 0) mem[OS_STATUS] |= 0x0001;

  // 5. Construct the PTE
  int PFN = 31- freePFNidx; // Calculate the PFN based on the freePFN index
  pte = PFN << PFN_SHIFT;
  if (read == UINT16_MAX) pte |= READ_BIT;
  if (write == UINT16_MAX) pte |= WRITE_BIT;
  pte |= VALID_BIT;

  // 6. Write the PTE into the page table 
  mem[ptbr + vpn] = pte;

  uint16_t offset = PFN * PAGE_SIZE_IN_WORDS;
  return offset; // Return offset of the page frame into memory
}

int freeMem(uint16_t vpn, uint16_t ptbr) {
  // 1. Calculate the physical address of the PTE for this VPN
  uint16_t pte = mem[ptbr + vpn];

  // 2. If a page frame is not allocated, return 0
  if (!(pte & 0x0001)) { return 0; }

  // 3. Otherwise just update the bitmap, clear valid bit in PTE
  // Clear valid bit
  pte &= ~VALID_BIT;
  mem[ptbr + vpn] = pte;
  
  // Update the bitmap
  int PFN = (pte >> PFN_SHIFT) & PFN_MASK; // Get the PFN from the PTE

  uint32_t bitmap = GET_BITMAP();          // Get the bitmap from memory
  int freePFNidx = 31 - PFN;
  bitmap |= (1 << freePFNidx);             // Mark the page frame as free (1) in the bitmap          

  setBitmap(bitmap);                       // Update the bitmap in memory
  // If bitmap is not full, set OS_STATUS bit to 0
  if (bitmap != 0) mem[OS_STATUS] &= ~0x0001;
  
  return 0;
}

// Instructions to implement
static inline void tbrk() {
  uint16_t request = reg[R0];
  uint16_t vpn = (request >> VPN_SHIFT) & PFN_MASK;
  uint16_t write_access = request & WRITE_BIT;
  uint16_t read_access = request & READ_BIT;
  uint16_t allocOrFree = request & 0x0001;

  uint16_t cur_pid = mem[Cur_Proc_ID];
  uint16_t ptbr = reg[PTBR];
  uint16_t pte = mem[ptbr + vpn];

  uint16_t valid_bit = pte & VALID_BIT;

  if (allocOrFree) {  // Allocation request
    printf("Heap increase requested by process %hu.\n", cur_pid);

    if (valid_bit) {  // 1. Already allocated
      printf("Cannot allocate memory for page %hu of pid %hu since it is already allocated.\n", vpn, cur_pid);
      return;
    }

    if (!checkFreePages(1)) { // 2. No free page frames left
      printf("Cannot allocate more space for pid %hu since there is no free page frames.\n", cur_pid);
      return;
    }

    // 3. Allocate new page frame for the VPN
    uint16_t read_arg = read_access ? UINT16_MAX : 0;
    uint16_t write_arg = write_access ? UINT16_MAX : 0;
    allocMem(ptbr, vpn, read_arg, write_arg);
  } 
  else {
    printf("Heap decrease requested by process %hu.\n", cur_pid);

    if (!valid_bit) { // 1. Already freed or not allocated at all
      printf("Cannot free memory of page %hu of pid %hu since it is not allocated.\n", vpn, cur_pid);
      return;
    }

    // 2. Free the page frame for the VPN
    freeMem(vpn, ptbr);
  }
}

static inline void tyld() {
  uint16_t cur_pid = mem[Cur_Proc_ID];
  uint16_t pcbIndex = PCB_LIST_BASE + cur_pid * PCB_SIZE;

  // 1. Find the next runnable process
  uint16_t totalProc = mem[Proc_Count];
  uint16_t next_pid = (cur_pid + 1) % totalProc;

  while (next_pid != cur_pid) {
    // Check if process has not terminated by checking validity of PID_PCB
    uint16_t nextPCBIndex = PCB_LIST_BASE + next_pid * PCB_SIZE;
    if (mem[nextPCBIndex + PID_PCB] != INVALID_PID) { // Runnable process is found
      printf("We are switching from process %d to %d.\n", cur_pid, next_pid);

      // 2. Save PC to current process' PC_PCB ONLY WHEN SWITCHING TO ANOTHER PROCESS
      mem[pcbIndex + PC_PCB] = reg[RPC];

      loadProc(next_pid); // Load the process to registers
      return;
    }
    next_pid = (next_pid + 1) % totalProc; // Move to next pid
  }

  // 3. No other runnable processes found, DO NOT LOAD IT AGAIN, just continue running the current process
  //printf("No runnable processes found. Continuing with process %d.\n", cur_pid);
}

// Instructions to modify
static inline void thalt() {
  uint16_t cur_pid = mem[Cur_Proc_ID];
  uint16_t pcbIndex = PCB_LIST_BASE + cur_pid * PCB_SIZE;

  // 1. Get the PTBR for the current process
  uint16_t ptbr = reg[PTBR];

  // 2. Iterate over all PTEs and free valid pages
  for (uint16_t vpn = 0; vpn < PAGE_TABLE_SIZE_IN_WORDS; vpn++) {
    freeMem(vpn, ptbr); // freeMem already checks if the page is valid
  }

  // 3. Mark the process as terminated by setting PID_PCB to 0xffff
  mem[pcbIndex + PID_PCB] = INVALID_PID;

  // 4. Check if all processes are halted, if yes stop the VM. Otherwise switch to the next runnable process
  uint16_t totalProc = mem[Proc_Count];
  uint16_t allHalted = 1;

  for (uint16_t pid = 0; pid < totalProc; pid++) {
    uint16_t pcbIndex = PCB_LIST_BASE + pid * PCB_SIZE;
    if (mem[pcbIndex + PID_PCB] != INVALID_PID) {
      allHalted = 0;  // Found a process that is not invalid
      break;
    }
  }

  if (allHalted) { 
    //printf("All processes halted. Halting VM.\n");
    running = false; // Stop the VM 
  } 
  else { // Find the next runnable process and load it
    //printf("Switching to the next runnable process.\n");
    uint16_t next_pid = (cur_pid + 1) % totalProc;
    
    while (next_pid != cur_pid) {
      // Check if process has not terminated by checking validity of PID_PCB
      uint16_t nextPCBIndex = PCB_LIST_BASE + next_pid * PCB_SIZE;
      if (mem[nextPCBIndex + PID_PCB] != INVALID_PID) {
        // Runnable process is found
        loadProc(next_pid); // Load the process to registers
        return;
      }
      next_pid = (next_pid + 1) % totalProc; // Move to next pid
    }
  } 
}

static inline uint16_t mr(uint16_t address) {
  uint16_t vpn = address >> VPN_SHIFT;
  uint16_t offset = address & 0x07FF;

  // 1. If address belongs to reserved region 
  if (vpn < NOT_RESERVED_START_VPN) {
    handleSegFault("Segmentation fault.");
     ;
  }

  // 2. Get PTE using VPN and PTBR. Then check valid bit
  uint16_t pte = mem[reg[PTBR] + vpn];
  if (!(pte & VALID_BIT)) {
    handleSegFault("Segmentation fault inside free space.");
    return SEG_FAULT_OUTPUT;
  }

  // 3. Check if read allowed
  if (!(pte & READ_BIT)) {
    handleSegFault("Cannot read from a write-only page.");
    return SEG_FAULT_OUTPUT;
  }

  // 4. Finally read the value from memory
  // Compute the physical address using PFN and offset
  uint16_t pfn = (pte >> PFN_SHIFT) & PFN_MASK;
  uint16_t physicalAddress = pfn * PAGE_SIZE_IN_WORDS + offset;

  return mem[physicalAddress];
}

static inline void mw(uint16_t address, uint16_t val) {
  uint16_t vpn = address >> VPN_SHIFT;
  uint16_t offset = address & 0x07FF;

  // 1. If address belongs to reserved region 
  if (vpn < NOT_RESERVED_START_VPN) {
    handleSegFault("Segmentation fault.");
  }

  // 2. Get PTE using VPN and PTBR. Then check valid bit
  uint16_t pte = mem[reg[PTBR] + vpn];
  if (!(pte & VALID_BIT)) {
    handleSegFault("Segmentation fault inside free space.");
  }

  // 3. Check if write is allowed
  if (!(pte & WRITE_BIT)) {
    handleSegFault("Cannot write to a read-only page.");
  }

  // 4. Finally read the value from memory
  // Compute the physical address using PFN and offset
  uint16_t pfn = (pte >> PFN_SHIFT) & PFN_MASK;
  uint16_t physicalAddress = pfn * PAGE_SIZE_IN_WORDS + offset;

  mem[physicalAddress] = val;
}

// YOUR CODE ENDS HERE
