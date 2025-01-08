#############################################################################################
# Makefile
#############################################################################################
# G++ is part of GCC (GNU compiler collection) and is a compiler best suited for C++
CC=g++

# Compiler Flags: https://linux.die.net/man/1/g++
#############################################################################################
# -g: produces debugging information (for gdb)
# -Wall: enables all the warnings
# -Wextra: further warnings
# -Werror: treat warnings as errors
# -O: Optimizer turned on
# -std: use the C++ 14 standard
# -c: says not to run the linker
# -pthread: Add support for multithreading using the POSIX threads library. This option sets 
#           flags for both the preprocessor and linker. It does not affect the thread safety 
#           of object code produced by the compiler or that of libraries supplied with it. 
#           These are HP-UX specific flags.
#############################################################################################
CFLAGS=-g -Wall -Wextra -Werror -O -std=c++14 -pthread
LIBS=-lldap -llber

rebuild: clean all
all: ./bin/ldapclient

clean:
	clear
	rm -f bin/* obj/*

./obj/ldapclient.o: myldap.c
	${CC} ${CFLAGS} -o obj/ldapclient.o myldap.c -c

./obj/mypw.o: mypw.c
	${CC} ${CFLAGS} -o obj/mypw.o mypw.c -c

./bin/ldapclient: ./obj/ldapclient.o ./obj/mypw.o
	${CC} ${CFLAGS} -o bin/ldapclient obj/mypw.o obj/ldapclient.o ${LIBS}
