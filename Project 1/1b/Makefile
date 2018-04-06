# NAME: Chaoran Lin
# EMAIL: linmc@ucla.edu
# ID: 004674598

CC=gcc
CFLAGS = -o
EXECNAME1=lab1b-client
EXECNAME2=lab1b-server
lab1b: lab1b-client.c lab1b-server.c
	$(CC) $(CFLAGS) $(EXECNAME1) lab1b-client.c -lmcrypt
	chmod +x ./$(EXECNAME1)
	$(CC) $(CFLAGS) $(EXECNAME2) lab1b-server.c -lmcrypt
	chmod +x ./$(EXECNAME2)

clean:
	rm -f *.tar.gz *~ $(EXECNAME)

dist:
	tar cvzf lab1b-004674598.tar.gz lab1b-client.c lab1b-server.c my.key Makefile README
