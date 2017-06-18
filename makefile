BIN=bin
MAIN=main
HOST=localhost
PORT=69
FILE_SRV=$(BIN)/file_srv
FILE_CLT=$(BIN)/file_clt
EXEC=$(BIN)/$(MAIN)
EXEC_CMD_CLT=./$(EXEC) $(HOST) $(PORT) $(FILE_CLT)
EXEC_CMD_SRV=./$(EXEC) $(HOST) $(FILE_SRV) 

compile: $(BIN)
	gcc -g -Wall -pedantic $(MAIN).c -o $(BIN)/$(MAIN)
run:
	$(EXEC_CMD_CLT); $(EXEC_CMD_SRV); diff $(FILE_CLT) $(FILE_SRV)
valg_srv:
	valgrind $(EXEC_CMD_SRV)
valg_clt:
	valgrind $(EXEC_CMD_CLT)
gdb:
	gdb $(EXEC)
$(BIN):
	mkdir $(BIN)
