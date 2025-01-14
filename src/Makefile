C = gcc
CFLAGS = -std=c11 -Wall -g

MAIN = main.c
VM = vm

PROGRAM1 = programs/simple
PROGRAM2 = programs/brk
PROGRAM3 = programs/brk2
PROGRAM4 = programs/yld

OBJ1 = programs/simple_code.obj programs/simple_heap.obj
OBJ2 = programs/brk_code.obj programs/brk_heap.obj
OBJ3 = programs/brk2_code.obj programs/brk2_heap.obj
OBJ4 = programs/yld_code.obj programs/yld_heap.obj

all: clean programs sample

programs: $(PROGRAM1).c $(PROGRAM2).c $(PROGRAM3).c $(PROGRAM4).c
	@$(C) $(CFLAGS) $(PROGRAM1).c -o $(PROGRAM1)
	@$(C) $(CFLAGS) $(PROGRAM2).c -o $(PROGRAM2)
	@$(C) $(CFLAGS) $(PROGRAM3).c -o $(PROGRAM3)
	@$(C) $(CFLAGS) $(PROGRAM4).c -o $(PROGRAM4)

	@$(PROGRAM1)
	@$(PROGRAM2)
	@$(PROGRAM3)
	@$(PROGRAM4)

	@rm $(PROGRAM1) $(PROGRAM2) $(PROGRAM3) $(PROGRAM4)

sample: $(MAIN)
	@$(C) $(CFLAGS) $(MAIN) -o $(VM)

clean:
	@rm -f $(OBJ1) $(OBJ2) $(OBJ3) $(OBJ4) $(VM)