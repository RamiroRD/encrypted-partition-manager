IDIR=include/
ODIR=obj/
SDIR=src/
BDIR=bin/
TDIR=tests/

CC=g++
CFLAGS=-std=c++14 -O3 -Wall -Werror -I $(IDIR)

	
$(TDIR)partition_manager_test: $(TDIR)partition_manager_test.cpp \
                               $(ODIR)partition_manager.o 
	$(CC) $(CFLAGS) -o $(TDIR)partition_manager_test \
						$(TDIR)partition_manager_test.cpp \
						$(ODIR)partition_manager.o 
                     
$(ODIR)partition_manager.o: $(SDIR)logic/partition_manager.cpp \
                            $(IDIR)/logic/partition_manager.h 
	$(CC) $(CFLAGS) -c -o $(ODIR)partition_manager.o \
					$(SDIR)logic/partition_manager.cpp 

.PHONY: all
all: $(TDIR)partition_manager_test
	
.PHONY: install
install: all

.PHONY: clean

clean:
	rm -rvf $(ODIR)*.o $(BDIR)* 
