BIN=bin
MAIN=main
HOST=localhost
PORT=3998
FILE_SRV=$(BIN)/file_srv
FILE_CLT=$(BIN)/file_clt
EXEC=$(BIN)/$(MAIN)
EXEC_CMD_CLT=./$(EXEC) $(HOST) $(PORT) $(FILE_CLT)
EXEC_CMD_SRV=./$(EXEC) $(PORT) $(FILE_SRV)

compile: $(BIN)
	gcc -g -Wall -pedantic $(MAIN).c -o $(BIN)/$(MAIN)
run_srv:
	$(EXEC_CMD_SRV)
run_clt: create_file
	$(EXEC_CMD_CLT)
create_file:
	cp $(EXEC) $(FILE_CLT)
valg_srv:
	valgrind $(EXEC_CMD_SRV)
valg_clt:
	valgrind $(EXEC_CMD_CLT)
diff:
	diff $(FILE_CLT) $(FILE_SRV)
gdb:
	gdb $(EXEC)
clean:
	rm -r -f $(BIN)
$(BIN):
	mkdir $(BIN)
