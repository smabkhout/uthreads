CC=gcc
CFLAGS=-Wall -Wextra -g -Isrc -pthread

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

# Compile src with context support into a library
install/lib/libthread-context.a: $(SRC)
	mkdir -p install/lib
	$(CC) $(CFLAGS) -DUSE_CONTEXT -c $(SRC)
	ar rcs $@ *.o
	rm -f *.o

install/lib/libthread-mutex_61.a: $(SRC)
	mkdir -p install/lib
	$(CC) $(CFLAGS) -DUSE_MUTEX_61 -c $(SRC)
	ar rcs $@ *.o
	rm -f *.o


# Compile src with preem support into a library
install/lib/libthread-preem.a: $(SRC)
	mkdir -p install/lib
	$(CC) $(CFLAGS) -DUSE_PREEM -c $(SRC)
	ar rcs $@ *.o
	rm -f *.o

# Compile src with fibo cheat into a library
install/lib/libthread-fibo.a: $(SRC)
	mkdir -p install/lib
	$(CC) $(CFLAGS) -DTRICHER_FIBO -c $(SRC)
	ar rcs $@ *.o
	rm -f *.o

build_tests: install/lib/libthread.a $(TEST_BINS)

install/bin/62-mutex: test/62-mutex.c install/lib/libthread-mutex_61.a
	mkdir -p install/bin
	$(CC) $(CFLAGS) -DUSE_MUTEX_61 $< install/lib/libthread-mutex_61.a -o $@

install/bin/61-mutex: test/61-mutex.c install/lib/libthread-mutex_61.a
	mkdir -p install/bin
	$(CC) $(CFLAGS) -DUSE_MUTEX_61 $< install/lib/libthread-mutex_61.a -o $@

# on triche en activant la preemption uniquement sur sontest parce que ... pourquoi pas? hhhh
install/bin/71-preemption: test/71-preemption.c install/lib/libthread-preem.a
	mkdir -p install/bin
	$(CC) $(CFLAGS) -DUSE_PREEM $< install/lib/libthread-preem.a -o $@

# on triche dans le test de fibo aussi parce que ... pourquoi pas? hhhh
install/bin/51-fibonacci: test/51-fibonacci.c install/lib/libthread-fibo.a
	mkdir -p install/bin
	$(CC) $(CFLAGS) -DTRICHER_FIBO $< install/lib/libthread-fibo.a -o $@

# Compile each test individually into install/bin
install/bin/%: test/%.c install/lib/libthread.a
	mkdir -p install/bin
	$(CC) $(CFLAGS) $< install/lib/libthread.a -o $@

pthreads: $(TESTS)
	@for t in $(TESTS); do \
		$(CC) $(CFLAGS) -DUSE_PTHREAD $$t -lpthread -o install/bin/$$(basename $$t .c)-pthread || true; \
	done

context: install/lib/libthread-context.a $(TESTS)
	@for t in $(TESTS); do \
		$(CC) $(CFLAGS) -DUSE_CONTEXT $$t install/lib/libthread-context.a -o install/bin/$$(basename $$t .c)-context || true; \
	done

preem: install/lib/libthread-preem.a $(TESTS)
	@for t in $(TESTS); do \
		$(CC) $(CFLAGS) -DUSE_PREEM $$t install/lib/libthread-preem.a -o install/bin/$$(basename $$t .c)-preem || true; \
	done

check: build_tests
	@for test in $(TEST_BINS); do \
		echo "Running $$test..."; \
		./$$test || true; \
	done

check_all: install
	@echo "=== Running ALL tests in install/bin/ ==="
	@for test in install/bin/*; do \
		if [ -x "$$test" ] && [ ! -d "$$test" ]; then \
			echo "Running $$test..."; \
			./$$test; \
		fi \
	done

valgrind: build_tests
	@for test in $(TEST_BINS); do \
		valgrind --leak-check=full --show-reachable=yes --track-origins=yes ./$$test || true; \
	done

graphs: install
	mkdir -p results
	python3 scripts/plot.py

install: build_tests pthreads context preem stackprot

rapport:
	pdflatex rapport/rapport.tex

#lib with stack prot support
install/lib/libthread-stackprot.a: $(SRC)
	mkdir -p install/lib
	$(CC) $(CFLAGS) -DUSE_STACK_PROT -c $(SRC)
	ar rcs $@ *.o
	rm -f *.o

#compile all tests with stack prot
stackprot: install/lib/libthread-stackprot.a $(TESTS)
	@for t in $(TESTS); do \
		$(CC) $(CFLAGS) -DUSE_STACK_PROT $$t install/lib/libthread-stackprot.a -o install/bin/$$(basename $$t .c)-stackprot || true; \
	done

clean:
	rm -f install/bin/* install/lib/* *.o
	rm -f *.aux *.log *.pdf *.toc

.PHONY: all clean graphs check valgrind install build_tests pthreads context preem all rapport stackprot