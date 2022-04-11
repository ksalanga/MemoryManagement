CC = gcc
CFLAGS = -g -c -m32 -lm -pthread
AR = ar -rc
RANLIB = ranlib

PGSIZE = 4k

all: my_vm.a

my_vm.a: my_vm.o
	$(AR) libmy_vm.a my_vm.o
	$(RANLIB) libmy_vm.a

my_vm.o: my_vm.h

ifeq ($(PGSIZE), 4k)
	$(CC)	$(CFLAGS)  -DPGSIZE=4096 my_vm.c
else ifeq ($(PGSIZE), 8k)
	$(CC)	$(CFLAGS)  -DPGSIZE=8192 my_vm.c
else ifeq ($(PGSIZE), 16k)
	$(CC)	$(CFLAGS)  -DPGSIZE=16384 my_vm.c
else ifeq ($(PGSIZE), 32k)
	$(CC)	$(CFLAGS)  -DPGSIZE=32768 my_vm.c
else ifeq ($(PGSIZE), 64k)
	$(CC)	$(CFLAGS)  -DPGSIZE=65536 my_vm.c
endif

clean:
	rm -rf *.o *.a
