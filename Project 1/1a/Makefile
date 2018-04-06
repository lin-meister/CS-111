# NAME: Chaoran Lin
# EMAIL: linmc@ucla.edu
# ID: 004674598

CC=gcc
CFLAGS = -o
EXECNAME=lab1a

lab1a: lab1a.c
	$(CC) $(CFLAGS) $(EXECNAME) lab1a.c
	chmod +x ./$(EXECNAME)

clean:
	rm -f *.tar.gz *~ $(EXECNAME)

dist:
	tar cvzf lab1a-004674598.tar.gz lab1a.c Makefile README
