CROSS_COMPILE=
STATIC=
CC=$(CROSS_COMPILE)gcc
CFLAGS= -DDEBUG -g -O3

ifdef STATIC
CFLAGS += -static
endif

all: iotest

iotest: iotest.c iotest.h
	$(CC) $(CFLAGS) -o $@ $< -lpthread

install: iotest
	cp iotest /sbin/ -f

clean:
	@rm -rf *~ iotest *.o
	
