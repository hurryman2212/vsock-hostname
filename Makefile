CC=gcc
CXX=g++

DEBUG_FLAGS=-Og -g -g3 -ggdb
RELEASE_FLAGS=-DNDEBUG -march=native -Ofast -flto

COMMON_FLAGS=-fverbose-asm -save-temps -Wall -Wextra -Werror

CFLAGS=-std=gnu11 $(COMMON_FLAGS)
CXXFLAGS=-std=c++11 $(COMMON_FLAGS)

LDFLAGS=-z noexecstack

all: debug

debug: CFLAGS += $(DEBUG_FLAGS)
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: build

release: CFLAGS += $(RELEASE_FLAGS)
release: CXXFLAGS += $(RELEASE_FLAGS)
release: build

build: vsock-hostname test

vsock-hostname: vsock-hostname.o
	$(CC) -o $@ $(LDFLAGS) $(CFLAGS) $^
test: test.o
	$(CC) -o $@ $(LDFLAGS) $(CFLAGS) $^

vsock-hostname.o: vsock-hostname.c
	$(CC) -c $(CFLAGS) $^
test.o: test.c
	$(CC) -c $(CFLAGS) $^

clean:
	find . ! \( -name 'Makefile' -o -name '.git' -prune -o -name '.gitignore' -o -name '*.c' -o -name '*.h' \) -type f -exec rm -f {} +
