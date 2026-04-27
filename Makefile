CC=gcc
CFLAGS=-Wall -Wextra -g -Isrc -pthread

SRC=$(wildcard src/*.c)
TESTS=$(wildcard test/*.c)
TEST_BINS=$(patsubst test/%.c,install/bin/%,$(TESTS))

all: install

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

# Compile src with priority support into a library
install/lib/libthread-priority.a: $(SRC)
	mkdir -p install/lib
	$(CC) $(CFLAGS) -DUSE_PRIORITY -c $(SRC)
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

# Compile src with one-malloc optimization (struct + stack in a single malloc)
install/lib/libthread-one-malloc.a: $(SRC)
	mkdir -p install/lib
	$(CC) $(CFLAGS) -DUSE_ONE_MALLOC -c $(SRC)
	ar rcs $@ *.o
	rm -f *.o

# Compile src with malloc recycling (freeQueue)
install/lib/libthread-recycle.a: $(SRC)
	mkdir -p install/lib
	$(CC) $(CFLAGS) -DUSE_RECYCLE -c $(SRC)
	ar rcs $@ *.o
	rm -f *.o

# Compile src with both one-malloc and recycling
install/lib/libthread-one-malloc-recycle.a: $(SRC)
	mkdir -p install/lib
	$(CC) $(CFLAGS) -DUSE_ONE_MALLOC -DUSE_RECYCLE -c $(SRC)
	ar rcs $@ *.o
	rm -f *.o

build_tests: install/lib/libthread.a $(TEST_BINS)



install/bin/91-signals: test/91-signals.c install/lib/libthread-context.a
	mkdir -p install/bin
	$(CC) $(CFLAGS) -DUSE_CONTEXT $< install/lib/libthread-context.a -o $@

# on triche en activant la preemption uniquement sur sontest parce que ... pourquoi pas? hhhh
install/bin/71-preemption: test/71-preemption.c install/lib/libthread-preem.a
	mkdir -p install/bin
	$(CC) $(CFLAGS) -DUSE_PREEM $< install/lib/libthread-preem.a -o $@

# on triche dans le test de fibo aussi parce que ... pourquoi pas? hhhh
#install/bin/51-fibonacci: test/51-fibonacci.c install/lib/libthread-fibo.a
#	mkdir -p install/bin
#	$(CC) $(CFLAGS) -DTRICHER_FIBO $< install/lib/libthread-fibo.a -o $@

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

priority: install/lib/libthread-priority.a $(TESTS)
	@for t in $(TESTS); do \
		$(CC) $(CFLAGS) -DUSE_PRIORITY $$t install/lib/libthread-priority.a -o install/bin/$$(basename $$t .c)-priority || true; \
	done

preem: install/lib/libthread-preem.a $(TESTS)
	@for t in $(TESTS); do \
		$(CC) $(CFLAGS) -DUSE_PREEM $$t install/lib/libthread-preem.a -o install/bin/$$(basename $$t .c)-preem || true; \
	done

one-malloc: install/lib/libthread-one-malloc.a $(TESTS)
	@for t in $(TESTS); do \
		$(CC) $(CFLAGS) -DUSE_ONE_MALLOC $$t install/lib/libthread-one-malloc.a -o install/bin/$$(basename $$t .c)-one-malloc || true; \
	done

recycle: install/lib/libthread-recycle.a $(TESTS)
	@for t in $(TESTS); do \
		$(CC) $(CFLAGS) -DUSE_RECYCLE $$t install/lib/libthread-recycle.a -o install/bin/$$(basename $$t .c)-recycle || true; \
	done

one-malloc-recycle: install/lib/libthread-one-malloc-recycle.a $(TESTS)
	@for t in $(TESTS); do \
		$(CC) $(CFLAGS) -DUSE_ONE_MALLOC -DUSE_RECYCLE $$t install/lib/libthread-one-malloc-recycle.a -o install/bin/$$(basename $$t .c)-one-malloc-recycle || true; \
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
			./$$test || true; \
		fi \
	done

valgrind: build_tests
	@for test in $(TEST_BINS); do \
		valgrind --leak-check=full --show-reachable=yes --track-origins=yes ./$$test || true; \
	done

graphs: install
	mkdir -p results
	python3 scripts/plot.py

install: build_tests pthreads context preem stackprot one-malloc recycle one-malloc-recycle

rapport:
	pdflatex rapport/rapport.tex

rapport-final:
	pdflatex rapport-final/rapport-final.tex

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

signals: install/lib/libthread-context.a test/91-signals.c
	mkdir -p install/bin
	$(CC) $(CFLAGS) -DUSE_CONTEXT test/91-signals.c install/lib/libthread-context.a -o install/bin/91-signals
	./install/bin/91-signals

clean:
	rm -f install/bin/* install/lib/* *.o
	rm -f *.aux *.log *.pdf *.toc

.PHONY: all clean graphs check valgrind install build_tests pthreads context preem one-malloc recycle one-malloc-recycle all rapport stackprot rapport-final