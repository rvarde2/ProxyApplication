all: proxy

proxy: main.o cbuf.o
	gcc -Wall -Werror -o $@ main.o cbuf.o
	rm -f main.o cbuf.o
main.o:
	gcc -c main.c
cbuf.o:
	gcc -c cbuf.c
clean:
	@rm -f proxy
	rm -f main.o cbuf.o

