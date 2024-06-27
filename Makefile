# Makefile for vm1

make:
	cl -Zi -o vm.exe ./Source/*.c


.PHONY: clean

clean:
	del *.exe *.obj *.pdb