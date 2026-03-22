.PHONY: all library init user1 user2 run-init run-user1 run-user2 run-user3 run-user4 clean distclean
all: init user1 user2


library: transport/ksocket.o
	ar rs libksocket.a transport/ksocket.o


transport/ksocket.o: transport/ksocket.c transport/ksocket.h
	gcc -g -Wall -Itransport -c transport/ksocket.c -o transport/ksocket.o


init: library server/initksocket.c
	gcc -g -Wall -Itransport -L. -o initksocket server/initksocket.c -lksocket


user1: library application/user1.c
	gcc -g -Wall -Itransport -L. -o user1 application/user1.c -lksocket


user2: library application/user2.c
	gcc -g -Wall -Itransport -L. -o user2 application/user2.c -lksocket


run-init: init
	-./initksocket > logs/server.log


run-user1: user1
	-./user1 127.0.0.1 30003 127.0.0.1 30004 > logs/user1.log

run-user2: user2
	-./user2 127.0.0.1 30004 127.0.0.1 30003 > logs/user2.log


run-user3: user1
	-./user1 127.0.0.1 30005 127.0.0.1 30006 > logs/user3.log

run-user4: user2
	-./user2 127.0.0.1 30006 127.0.0.1 30005 > logs/user4.log


clean:
	-rm -f transport/*.o user1 user2 initksocket logs/*.log results/*

distclean: clean
	-rm -f libksocket.a