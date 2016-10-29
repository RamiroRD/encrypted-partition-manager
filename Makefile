IDIR=include/
ODIR=obj/
SDIR=src/
BDIR=bin/
TDIR=tests/

CC=g++
CFLAGS=-std=c++14  -g -lpthread -O0 -Wall -I $(IDIR)

	
$(TDIR)partition_manager_test: $(TDIR)partition_manager_test.cpp \
							$(ODIR)PartitionManager.o 
	$(CC) $(CFLAGS) -o $(BDIR)partition_manager_test \
						$(TDIR)partition_manager_test.cpp \
						$(ODIR)PartitionManager.o
                     
$(ODIR)PartitionManager.o: $(SDIR)logic/PartitionManager.cpp \
                            $(IDIR)logic/PartitionManager.h 
	$(CC) $(CFLAGS) -c -o $(ODIR)PartitionManager.o \
					$(SDIR)logic/PartitionManager.cpp 

.PHONY: all
all: $(TDIR)partition_manager_test
	
.PHONY: install
install: all

.PHONY: clean

clean:
	rm -rvf $(ODIR)*.o $(BDIR)* 
