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

# Compile src with preem support into a library
install/lib/libthread-preem.a: $(SRC)
	mkdir -p install/lib
	$(CC) $(CFLAGS) -DUSE_PREEM -c $(SRC)
	ar rcs $@ *.o
	rm -f *.o

build_tests: install/lib/libthread.a $(TEST_BINS)

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

valgrind: build_tests
	@for test in $(TEST_BINS); do \
		valgrind --leak-check=full --show-reachable=yes --track-origins=yes ./$$test || true; \
	done

graphs: install
	mkdir -p results
	python3 scripts/plot.py

install: build_tests pthreads context preem

rapport:
	pdflatex rapport/rapport.tex

clean:
	rm -f install/bin/* install/lib/* *.o
	rm -f *.aux *.log *.pdf *.toc

.PHONY: all clean graphs check valgrind install build_tests pthreads context preem all rapport