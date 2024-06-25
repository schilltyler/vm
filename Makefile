#cl -Zi -o vm.exe vm1.c

# Makefile for memory management simulation
# Ben Williams
# June 19th, 2024

CC=cl
CFLAGS=/Zi /EHsc /I.

# Header file dependencies
DEPS = page.h

# Object files to compile
OBJ = page.obj

make: pageTable.h
	cl -Zi -o vm.exe vm1.c page.c pageTable.c


# Default target
#vm.exe: $(OBJ)
#	$(CC) $(CFLAGS) /Fevm.exe /Fo:. $^

# Compilation rules for individual files
#page.obj: page.c $(DEPS)
#	$(CC) $(CFLAGS) /c /Fo:$@ $<

#vm1.obj: vm1.c $(DEPS)
#	$(CC) $(CFLAGS) /c /Fo:$@ $<

.PHONY: clean

clean:
	del *.exe *.obj *.pdb