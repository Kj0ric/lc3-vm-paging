#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/*
Our goal is to take the ten numbers which are stored in memory locations x300A through
x3013, and add them together, leaving the result in register 1.
*/

uint16_t program[] = {        
    /*mem[0x3000]=*/   0x5260,    //  0101 0010 0110 0000             AND R1,R1,x0    ;clear R1, to be used for the running sum               
    /*mem[0x3001]=*/   0x5920,    //  0101 1001 0010 0000             AND R4,R4,x0    ;clear R4, to be used as a counter                      
    /*mem[0x3002]=*/   0x192A,    //  0001 1001 0010 1010             ADD R4,R4,xB    ;load R4 with #10, the number of times to add   
    /*mem[0x3003]=*/   0xE408,    //  1110 0100 0000 1000             LEA R2,x8       ;load the address of the starting address of the data  Modified by adding two to the address
    /*mem[0x3004]=*/   0x6480,    //  0110 0100 1000 0000             LEA R2, x0      ;load the starting address of the data
    /*mem[0x3005]=*/   0x6680,    //  0110 0110 1000 0000     LOOP    LDR R3,R2,x0    ;load the next number to be added    
    /*mem[0x3006]=*/   0x14A1,    //  0001 0100 1010 0001             ADD R2,R2,x1    ;increment the pointer
    /*mem[0x3007]=*/   0x1243,    //  0001 0010 0100 0011             ADD R1,R1,R3    ;add the next number to the running sum
    /*mem[0x3008]=*/   0x193F,    //  0001 1001 0011 1111             ADD R4,R4,x-1   ;decrement the counter
    /*mem[0x3009]=*/   0x03FB,    //  0000 0011 1111 1011             BRp LOOP        ;do it again if the counter is not yet zero
    /*mem[0x300A]=*/   0xF028,    //  1111 0000 0010 1000             YIELD           ;yield to another process (and save PCB)
    /*mem[0x300B]=*/   0xF025,    //  1111 0000 0010 0101             HALT            ;halt    
    /*mem[0x300C]=*/   0x4000,    //  0100 0000 0000 0000             HEAP Beginning Address  
};
uint16_t heap[] = {
    /* --memory-- */
    /*mem[0x4000]=*/   0x0005, /* 5 */
    /*mem[0x4001]=*/   0x0002, /* +2 = 7 */
    /*mem[0x4002]=*/   0x0001, /* +1 = 8 */
    /*mem[0x4003]=*/   0x0002, /* +2 = 10 */
    /*mem[0x4004]=*/   0x0003, /* +3 = 13 */
    /*mem[0x4005]=*/   0x0001, /* +1 = 14 */
    /*mem[0x4006]=*/   0x0002, /* +2 = 16 */
    /*mem[0x4007]=*/   0x0001, /* +1 = 17 */
    /*mem[0x4008]=*/   0x0002, /* +2 = 19 */
    /*mem[0x4009]=*/   0x0001, /* +1 = 20 */      
};

int main(int argc, char** argv) {
    char *outf = "programs/simple_code.obj";
    FILE *f = fopen(outf, "wb");
    if (NULL==f) {
        fprintf(stderr, "Cannot write to file %s\n", outf);
    }
    size_t writ = fwrite(program, sizeof(uint16_t), sizeof(program)/sizeof(uint16_t), f);
    fprintf(stdout, "Written size_t=%lu to file %s\n", writ, outf);
    fclose(f);


    char *outff = "programs/simple_heap.obj";
    FILE *ff = fopen(outff, "wb");
    if (NULL==ff) {
        fprintf(stderr, "Cannot write to file %s\n", outff);
    }
    writ = fwrite(heap, sizeof(uint16_t), sizeof(heap)/sizeof(uint16_t), ff);
    fprintf(stdout, "Written size_t=%lu to file %s\n", writ, outff);
    fclose(f);
    return 0;
}