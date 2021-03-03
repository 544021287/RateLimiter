BINDIR=.
CC=cc
CFLAGS=-c -g -m64 -fno-builtin -Werror -D_GNU_SOURCE -std=c++14
includedir=-I./
LDFLAGS=-m64
libdir=
LDSHARED=-lpthread -lgfortran -lstdc++

SRC1= rate_limiter_e.cpp test_e.cpp

.cpp.o: 
	$(CC) $(CFLAGS) $(includedir) $< -o $@

all: $(BINDIR)/test clean

$(BINDIR)/test: $(SRC1:.cpp=.o)
	$(CC) $(LDFLAGS) $(SRC1:.cpp=.o) -o $@ $(libdir) $(LDSHARED)

.PHONY: clean
clean:
	@rm -f *.o
