CFLAGS = -Wall -g -o0

all: server subscriber


server: server.cpp


subscriber: subscriber.cpp


clean:
	rm -f server subscriber