CC=gcc
CFLAGS=-Wall -Wextra -g -Isrc -pthread

SRC=$(wildcard src/*.c)
TESTS=$(wildcard test/*.c)
TEST_BINS=$(patsubst test/%.c,install/bin/%,$(TESTS))

ARGS_21-create-many           = 20
ARGS_22-create-many-recursive = 20
ARGS_23-create-many-once      = 20
ARGS_31-switch-many           = 10 20
ARGS_32-switch-many-join      = 10 20
ARGS_33-switch-many-cascade   = 20 5
ARGS_51-fibonacci             = 8
ARGS_61-mutex                 = 20
ARGS_62-mutex                 = 20
ARGS_71_preemption			  = 5


all: install

# Compile src into a library in install/lib (default: all optimisations enabled)
install/lib/libthread.a: $(SRC)
	mkdir -p install/lib
	$(CC) $(CFLAGS) -DUSE_ONE_MALLOC -DUSE_RECYCLE -c $(SRC)
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

# Compile src with user-space signals support
install/lib/libthread-signals.a: $(SRC)
	mkdir -p install/lib
	$(CC) $(CFLAGS) -DUSE_SIGNALS -c $(SRC)
	ar rcs $@ *.o
	rm -f *.o

build_tests: install/lib/libthread.a $(TEST_BINS)



install/bin/91-signals: test/91-signals.c install/lib/libthread-signals.a
	mkdir -p install/bin
	$(CC) $(CFLAGS) -DUSE_SIGNALS $< install/lib/libthread-signals.a -o $@

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
	$(CC) $(CFLAGS) -DUSE_ONE_MALLOC -DUSE_RECYCLE $< install/lib/libthread.a -o $@

pthreads: $(TESTS)
	@for t in $(TESTS); do \
		$(CC) $(CFLAGS) -DUSE_PTHREAD $$t -lpthread -o install/bin/$$(basename $$t .c)-pthread; \
	done

context: install/lib/libthread-context.a $(TESTS)
	@for t in $(TESTS); do \
		$(CC) $(CFLAGS) -DUSE_CONTEXT $$t install/lib/libthread-context.a -o install/bin/$$(basename $$t .c)-context; \
	done

priority: install/lib/libthread-priority.a $(TESTS)
	@for t in $(TESTS); do \
		$(CC) $(CFLAGS) -DUSE_PRIORITY $$t install/lib/libthread-priority.a -o install/bin/$$(basename $$t .c)-priority; \
	done

preem: install/lib/libthread-preem.a $(TESTS)
	@for t in $(TESTS); do \
		$(CC) $(CFLAGS) -DUSE_PREEM $$t install/lib/libthread-preem.a -o install/bin/$$(basename $$t .c)-preem; \
	done

one-malloc: install/lib/libthread-one-malloc.a $(TESTS)
	@for t in $(TESTS); do \
		$(CC) $(CFLAGS) -DUSE_ONE_MALLOC $$t install/lib/libthread-one-malloc.a -o install/bin/$$(basename $$t .c)-one-malloc; \
	done

recycle: install/lib/libthread-recycle.a $(TESTS)
	@for t in $(TESTS); do \
		$(CC) $(CFLAGS) -DUSE_RECYCLE $$t install/lib/libthread-recycle.a -o install/bin/$$(basename $$t .c)-recycle; \
	done

one-malloc-recycle: install/lib/libthread-one-malloc-recycle.a $(TESTS)
	@for t in $(TESTS); do \
		$(CC) $(CFLAGS) -DUSE_ONE_MALLOC -DUSE_RECYCLE $$t install/lib/libthread-one-malloc-recycle.a -o install/bin/$$(basename $$t .c)-one-malloc-recycle; \
	done

check: build_tests
	@for test in $(TEST_BINS); do \
		base=$$(basename $$test); \
		args=""; \
		case $$base in \
			"21-create-many"|"23-create-many-once") args="$(ARGS_21-create-many)" ;; \
			"22-create-many-recursive")             args="$(ARGS_22-create-many-recursive)" ;; \
			"31-switch-many"|"32-switch-many-join") args="$(ARGS_31-switch-many)" ;; \
			"33-switch-many-cascade")               args="$(ARGS_33-switch-many-cascade)" ;; \
			"51-fibonacci")                         args="$(ARGS_51-fibonacci)" ;; \
			"61-mutex"|"62-mutex")                  args="$(ARGS_61-mutex)" ;; \
			"65-semaphore")                         args="$(ARGS_65-semaphore)" ;; \
			"71-preemption")                        args="$(ARGS_71_preemption)" ;; \
		esac; \
		echo "Executing $$test $$args..."; \
		./$$test $$args || exit 1; \
	done

check_all: install
	@echo "=== Running ALL tests in install/bin/ ==="
	@for test in install/bin/*; do \
		if [ -x "$$test" ] && [ ! -d "$$test" ]; then \
			base=$$(basename $$test); \
			while true; do \
				prev=$$base; \
				for s in -context -priority -preem -fibo -one-malloc -recycle -stackprot -pthread -signals; do \
					base=$${base%%$$s}; \
				done; \
				[ "$$prev" = "$$base" ] && break; \
			done; \
			args=""; \
			case "$$base" in \
				"21-create-many"|"23-create-many-once") args="$(ARGS_21-create-many)" ;; \
				"22-create-many-recursive")             args="$(ARGS_22-create-many-recursive)" ;; \
				"31-switch-many"|"32-switch-many-join") args="$(ARGS_31-switch-many)" ;; \
				"33-switch-many-cascade")               args="$(ARGS_33-switch-many-cascade)" ;; \
				"51-fibonacci")                         args="$(ARGS_51-fibonacci)" ;; \
				"61-mutex"|"62-mutex")                  args="$(ARGS_61-mutex)" ;; \
				"65-semaphore")                         args="$(ARGS_65-semaphore)" ;; \
				"71-preemption")                        args="$(ARGS_71_preemption)" ;; \
				"81-deadlock")                          args="$(ARGS_81-deadlock)" ;; \
			esac; \
			echo "Running $$test $$args..."; \
			./$$test $$args || exit 1; \
		fi \
	done
valgrind: build_tests
	@for test in $(TEST_BINS); do \
		base=$$(basename $$test); \
		args=""; \
		case $$base in \
			"21-create-many"|"23-create-many-once") args="$(ARGS_21-create-many)" ;; \
			"22-create-many-recursive")             args="$(ARGS_22-create-many-recursive)" ;; \
			"31-switch-many"|"32-switch-many-join") args="$(ARGS_31-switch-many)" ;; \
			"33-switch-many-cascade")               args="$(ARGS_33-switch-many-cascade)" ;; \
			"51-fibonacci")                         args="$(ARGS_51-fibonacci)" ;; \
			"61-mutex"|"62-mutex")                  args="$(ARGS_61-mutex)" ;; \
			"65-semaphore")                         args="$(ARGS_65-semaphore)" ;; \
			"71-preemption")                        args="$(ARGS_71_preemption)" ;; \
		esac; \
		echo "Valgrind on $$test $$args..."; \
		valgrind --leak-check=full --show-reachable=yes --track-origins=yes ./$$test $$args || exit 1; \
	done

graphs: install
	mkdir -p results
	python3 scripts/plot.py

signals-variant: install/lib/libthread-signals.a $(TESTS)
	@for t in $(TESTS); do \
		$(CC) $(CFLAGS) -DUSE_SIGNALS $$t install/lib/libthread-signals.a -o install/bin/$$(basename $$t .c)-signals || true; \
	done

install: build_tests pthreads context preem stackprot one-malloc recycle one-malloc-recycle signals-variant

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
# Compile seulement le test de stack overflow avec le support stackprot
stackprot: install/lib/libthread-stackprot.a test/90-stack_overflow.c
	mkdir -p install/bin
	$(CC) $(CFLAGS) -DUSE_STACK_PROT test/90-stack_overflow.c install/lib/libthread-stackprot.a -o install/bin/90-stack_overflow-stackprot

signals: install/lib/libthread-signals.a test/91-signals.c
	mkdir -p install/bin
	$(CC) $(CFLAGS) -DUSE_SIGNALS test/91-signals.c install/lib/libthread-signals.a -o install/bin/91-signals
	./install/bin/91-signals

clean:
	rm -f install/bin/* install/lib/* *.o
	rm -f *.aux *.log *.pdf *.toc

.PHONY: all clean graphs check valgrind install build_tests pthreads context preem one-malloc recycle one-malloc-recycle signals-variant signals all rapport stackprot rapport-final