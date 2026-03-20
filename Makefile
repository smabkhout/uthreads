IDIR=../include
CC=gcc
CFLAGS=-Wall -Wextra -I$(IDIR)

SRC=src/*.c
TEST=test/*.c
TEST_EXEC=alltests

pthreads: $(SRC) $(TEST)
	$(CC) $(CFLAGS) -DUSE_PTHREAD  $^ -o pthreads	
	
build_tests: $(TEST) $(SRC)
	$(CC) $(CFLAGS) $^ -o $(TEST_EXEC)

check: build_tests #argruments to be added if needed
	./$(TEST_EXEC) 

valgrind: build_tests
	valgrind --leak-check=full --show-reachable=yes --track-origins=yes ./$(TEST_EXEC)

graph: #graph generation?

install: #baqi maert ch nder feha!?

clean:
	rm -f install/* $(TEST_EXEC) pthreads

.PHONY: clean graph check valgrind install build_tests pthreads
