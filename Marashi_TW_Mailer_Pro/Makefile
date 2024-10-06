CC=g++

all: twmailer-server twmailer-client

clean:
	rm all

twmailer-client: client.cpp
	${CC} client.cpp -o twmailer-client

twmailer-server: server.cpp
	${CC} server.cpp -o twmailer-server -lldap -llber


