BIN=bin
MAIN=main
HOST=localhost
PORT=69
FILE=$(BIN)/file
EXEC=$(BIN)/$(MAIN)
EXEC_CMD=./$(EXEC) $(HOST) $(PORT) $(FILE)

compile: $(BIN)
	gcc -g -Wall -pedantic $(MAIN).c -o $(BIN)/$(MAIN)
run:
	$(EXEC_CMD)	
valg:
	valgrind $(EXEC_CMD)
gdb:
	gdb $(EXEC)
$(BIN):
	mkdir $(BIN)
