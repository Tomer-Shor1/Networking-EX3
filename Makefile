CC = gcc
FLAGS = -Wall -g


.PHONY: all clean


all: RUDP_Receiver RUDP_Sender

RUDP_Receiver: RUDP_Receiver.o RUDP_API.h
	gcc -Wall -o RUDP_Receiver RUDP_Receiver.o RUDP_API.c RUDP_API.h

RUDP_Sender: RUDP_Sender.o RUDP_API.h
	gcc -Wall -o RUDP_Sender RUDP_Sender.o RUDP_API.c RUDP_API.h

RUDP_Receiver.o: RUDP_Receiver.c RUDP_API.h
	$(CC) $(FLAGS) -c RUDP_Receiver.c

RUDP_Sender.o: RUDP_Sender.c RUDP_API.h
	$(CC) $(FLAGS) -c RUDP_Sender.c

clean: 
	rm -f RUDP_Receiver RUDP_Sender *.o