# NAME: Chaoran Lin
# EMAIL: linmc@ucla.edu
# ID: 004674598

CC=gcc
CFLAGS = -Wall -g -lm -pthread -lrt -o
EXECNAME = lab2_list
CSVFILENAME = lab2b_list.csv
PROFILERPATH = /u/eng/class/classnin/lib/

default: lab2_list.c SortedList.h SortedList.c
	$(CC) $(CFLAGS) $(EXECNAME) lab2_list.c SortedList.c
	chmod +x ./$(EXECNAME)

tests: test.sh
	chmod +x ./test.sh
	./test.sh

profile:
	-rm -f ./raw.gperf
	touch $(PROFILERPATH)libprofiler.so.0
	LD_PRELOAD=$(PROFILERPATH)libprofiler.so.0 CPUPROFILE=raw.gperf ./lab2_list --threads=12 --iterations=1000 --sync=s
	pprof --text ./lab2_list ./raw.gperf > ./profile.out
	pprof --list=listFunc ./lab2_list ./raw.gperf >> ./profile.out
	rm -f ./raw.gperf

# export PATH=/usr/local/cs/bin:$$PATH

graphs: lab2_list.gp
	export PATH=/usr/local/cs/bin:$$PATH
	gnuplot ./lab2_list.gp

dist: default tests profile graphs
	tar cvzf lab2b-004674598.tar.gz lab2_list.c SortedList.h SortedList.c Makefile $(CSVFILENAME) profile.out *.png test.sh lab2_list.gp README

clean:
	rm -f $(EXECNAME) *~ *.png *.csv profile.out *.tar.gz
