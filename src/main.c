#include "vm.c"

int main(int argc, char **argv) {
    initOS();
    for (int i = 1; i < argc; i += 2) {
        createProc(argv[i], argv[i+1]);
    }

    fprintf(stdout, "Occupied memory after program load:\n");
    fprintf_mem_nonzero(stdout, mem, UINT16_MAX);
    uint16_t currentProc = 0;
    loadProc(currentProc);
    fprintf_reg_all(stdout, reg, RCNT);
    fprintf(stdout, "program execution starts.\n");
    run(argv[1], argv[2]);
    fprintf(stdout, "program execution ends.\n");
    fprintf(stdout, "Occupied memory after program execution:\n");
    fprintf_mem_nonzero(stdout, mem, UINT16_MAX);   
    fprintf_reg_all(stdout, reg, RCNT);
    return 0;
}