# NAME: Chaoran Lin
# EMAIL: linmc@ucla.edu
# ID: 004674598

CC=gcc
CFLAGS = -g -Wall -lmraa -lm -o
SSL = -lssl -lcrypto
EXECNAME1=lab4c_tcp
EXECNAME2=lab4c_tls

default: lab4c_tcp.c lab4c_tls.c
	$(CC) $(CFLAGS) $(EXECNAME1) lab4c_tcp.c
	chmod +x $(EXECNAME1)
	$(CC) $(CFLAGS) $(EXECNAME2) $(SSL) lab4c_tls.c
	chmod +x $(EXECNAME2)
	rm /etc/localtime
	ln -s /usr/share/zoneinfo/America/Los_Angeles /etc/localtime

clean:
	rm -f $(EXECNAME1) $(EXECNAME2) *~ *.txt *.tar.gz

dist: lab4c_tcp.c lab4c_tls.c Makefile README
	tar cvzf lab4c-004674598.tar.gz lab4c_tcp.c lab4c_tls.c Makefile README
