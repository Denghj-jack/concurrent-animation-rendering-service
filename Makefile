CC = gcc
CFLAGS = -Wall -Wextra  -pthread
ANIMATE_INC = -Ilibanimate/include
ANIMATE_LIB = -Llibanimate/lib -lanimate

SERVER_OBJS = animate_server.o common_io_use.o authentication.o rpc_preprocess.o server_resources.o \
 			  server_dispatch_work.o server_worker.o server_connection.o handlers.o
CLIENT_OBJS = animate_client.o common_io_use.o client_connection.o client_response.o

.PHONY: all clean libanimate

all: animate_server animate_client

libanimate:
	@if [ ! -d $@ ]; then \
		echo "ERROR: libanimate not found" > /dev/stderr; \
		echo "Download and unzip libanimate.zip from P2 resources" > /dev/stderr; \
		false; \
	fi


animate_server: $(SERVER_OBJS) | libanimate
	$(CC) $(CFLAGS) $(SERVER_OBJS) $(ANIMATE_LIB) -o $@

animate_client: $(CLIENT_OBJS)
	$(CC) $(CFLAGS) $(CLIENT_OBJS) -o $@


animate_server.o: animate_server.c server_worker.h server_connection.h server_resources.h
	$(CC) $(CFLAGS) $(ANIMATE_INC) -c animate_server.c -o $@

authentication.o: authentication.c authentication.h configuration.h
	$(CC) $(CFLAGS) -c authentication.c -o $@

rpc_preprocess.o: rpc_preprocess.c rpc_preprocess.h server_struct_types.h configuration.h
	$(CC) $(CFLAGS) -c rpc_preprocess.c -o $@

server_resources.o: server_resources.c server_resources.h animate_tables.h server_struct_types.h configuration.h
	$(CC) $(CFLAGS) $(ANIMATE_INC) -c server_resources.c -o $@

server_dispatch_work.o: server_dispatch_work.c server_dispatch_work.h handlers.h server_struct_types.h rpc_preprocess.h
	$(CC) $(CFLAGS) $(ANIMATE_INC) -c server_dispatch_work.c -o $@

server_worker.o: server_worker.c server_worker.h server_struct_types.h common_io_use.h server_dispatch_work.h configuration.h
	$(CC) $(CFLAGS) $(ANIMATE_INC) -c server_worker.c -o $@

server_connection.o: server_connection.c server_connection.h server_struct_types.h common_io_use.h rpc_preprocess.h authentication.h server_worker.h server_resources.h configuration.h
	$(CC) $(CFLAGS) $(ANIMATE_INC) -c server_connection.c -o $@

handlers.o: handlers.c handlers.h server_struct_types.h rpc_preprocess.h server_resources.h server_connection.h authentication.h
	$(CC) $(CFLAGS) $(ANIMATE_INC) -c handlers.c -o $@

common_io_use.o: common_io_use.c common_io_use.h configuration.h
	$(CC) $(CFLAGS) -c common_io_use.c -o $@

animate_client.o: animate_client.c client_struct_types.h client_connection.h client_response.h common_io_use.h configuration.h
	$(CC) $(CFLAGS) -c animate_client.c -o $@

client_connection.o: client_connection.c client_connection.h configuration.h common_io_use.h
	$(CC) $(CFLAGS) -c client_connection.c -o $@

client_response.o: client_response.c client_response.h client_struct_types.h common_io_use.h configuration.h
	$(CC) $(CFLAGS) -c client_response.c -o $@

clean:
	rm -f *.o animate_server animate_client