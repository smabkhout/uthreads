CC=gcc
CFLAGS=-Wall -Wextra -Isrc -pthread

SRC=$(wildcard src/*.c)
TESTS=$(wildcard test/*.c)
TEST_BINS=$(patsubst test/%.c,install/bin/%,$(TESTS))

all: build_tests

# Compile src into a library in install/lib
install/lib/libthread.a: $(SRC)
	mkdir -p install/lib
	$(CC) $(CFLAGS) -c $(SRC)
	ar rcs $@ *.o
	rm -f *.o

build_tests: install/lib/libthread.a $(TEST_BINS)

# Compile each test individually into install/bin
install/bin/%: test/%.c install/lib/libthread.a
	$(CC) $(CFLAGS) $< install/lib/libthread.a -o $@

pthreads: $(TESTS)
	@for t in $(TESTS); do \
		$(CC) $(CFLAGS) -DUSE_PTHREAD $$t -lpthread -o install/bin/$$(basename $$t .c)-pthread || true; \
	done

check: build_tests
	@for test in $(TEST_BINS); do \
		echo "Running $$test..."; \
		./$$test || true; \
	done

valgrind: build_tests
	@for test in $(TEST_BINS); do \
		valgrind --leak-check=full --show-reachable=yes --track-origins=yes ./$$test || true; \
	done

graph: #graph generation?

install: build_tests pthreads

clean:
	rm -f install/bin/* install/lib/* *.o

.PHONY: clean graph check valgrind install build_tests pthreads all