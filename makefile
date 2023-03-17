all:
	gcc -Wall -Werror -c -o threads.o threads.c
clean:
	rm threads.o
