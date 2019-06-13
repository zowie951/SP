CC = gcc
CFLAGS = -ldl
all:inf-bonbon-server
inf-bonbon-server:inf-bonbon-server.c cJSON.c
	$(CC) $(CFLAGS) inf-bonbon-server.c cJSON.c  -o inf-bonbon-server
clean:
	rm -rf inf-bonbon-server
