# NAME: Chaoran Lin
# EMAIL: linmc@ucla.edu
# ID: 004674598

CC=gcc
CFLAGS = -g -Wall -lmraa -lm -o
EXECNAME=lab4b

default: lab4b.c
	$(CC) $(CFLAGS) $(EXECNAME) lab4b.c
	chmod +x $(EXECNAME)
	rm /etc/localtime
	ln -s /usr/share/zoneinfo/America/Los_Angeles /etc/localtime

check: checkBasic checkInvalidOption checkLog checkInvalidCommand checkOff checkStop checkStart

checkBasic:
	echo "OFF" > "commands.txt"
	-./$(EXECNAME) < "commands.txt" > "output.txt"
	test $$? -eq 0
	egrep '[0-9][0-9]:[0-9][0-9]:[0-9][0-9] [0-9][0-9].[0-9]' output.txt > /dev/null
	test $$? -eq 0
	rm *.txt

checkInvalidOption:
	echo "OFF" > "commands.txt"
	-./$(EXECNAME) --bogus < "commands.txt" > "output.txt" 1> /dev/null; \
	test $?? -eq 1
	rm *.txt

# Check logging works
checkLog:
	echo "OFF" > "commands.txt"
	-./$(EXECNAME) --log="log.txt" < "commands.txt" 
	test $$? -eq 0
	ls | grep "log.txt"
	test $$? -eq 0
	rm *.txt

# Check invalid command
checkInvalidCommand:
	echo "HHH OFF" > "commands.txt"
	-./$(EXECNAME) --log="log.txt" < "commands.txt" 1> /dev/null; \
	test $$? -eq 1
	ls | grep "log.txt"
	test $$? -eq 0
	cat "log.txt" | grep "HHH OFF"
	test $$? -eq 0
	rm *.txt

# Check off option triggers shutdown
checkOff:
	echo "OFF" > "commands.txt"
	-./$(EXECNAME) --log="log.txt" < "commands.txt"
	test $$? -eq 0
	cat "log.txt" | grep "OFF"
	test $$? -eq 0
	cat "log.txt" | grep "SHUTDOWN"
	test $$? -eq 0
	rm *.txt

# Check stop option
checkStop:
	echo "STOP" > "commands.txt"
	echo "OFF" >> "commands.txt"
	-./$(EXECNAME) --log="log.txt" < "commands.txt"
	test $$? -eq 0
	cat "log.txt" | grep "STOP"
	test $$? -eq 0
	cat "log.txt" | grep "OFF"
	test $$? -eq 0
	cat "log.txt" | grep "SHUTDOWN"
	test $$? -eq 0
	rm *.txt

# Check start option
checkStart:
	echo "STOP" > "commands.txt"
	echo "START" >> "commands.txt"
	echo "OFF" >> "commands.txt"
	-./$(EXECNAME) --log="log.txt" < "commands.txt"
	test $$? -eq 0
	cat "log.txt" | grep "STOP"
	test $$? -eq 0
	cat "log.txt" | grep "START"
	test $$? -eq 0
	cat "log.txt" | grep "OFF"
	test $$? -eq 0
	cat "log.txt" | grep "SHUTDOWN"
	test $$? -eq 0
	rm *.txt

clean:
	rm -f $(EXECNAME) *~ *.txt *.tar.gz

dist:
	tar cvzf lab4b-004674598.tar.gz lab4b.c Makefile README
