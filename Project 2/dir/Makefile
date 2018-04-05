# NAME: Chaoran Lin
# EMAIL: linmc@ucla.edu
# ID: 004674598

CC=gcc
CFLAGS = -Wall -g -lm -lpthread -lrt -o
EXECNAME1 = lab2_add
EXECNAME2 = lab2_list
CSVFILENAME = lab2_add.csv

lab2: lab2_add lab2_list

lab2_add: lab2_add.c
	$(CC) $(CFLAGS) $(EXECNAME1) lab2_add.c
	chmod +x ./$(EXECNAME1)

lab2_list: lab2_list.c SortedList.h SortedList.c
	$(CC) $(CFLAGS) $(EXECNAME2) lab2_list.c SortedList.c
	chmod +x ./$(EXECNAME2)

tests: test.sh
	chmod +x ./test.sh
	./test.sh

graphs: lab2_add.gp lab2_list.gp
	export PATH=/usr/local/cs/bin:$$PATH
	gnuplot ./lab2_add.gp
	gnuplot ./lab2_list.gp

dist:
	tar cvzf lab2a-004674598.tar.gz lab2_add.c lab2_list.c SortedList.h SortedList.c Makefile *.csv *.png test.sh lab2_add.gp lab2_list.gp README

clean:
	rm -f $(EXECNAME1) $(EXECNAME2) *.csv *.png *~
