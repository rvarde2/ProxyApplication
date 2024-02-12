all: proxy no_buf

proxy: main.o cbuf.o
	gcc -Wall -Werror -o $@ main.o cbuf.o
	rm -f main.o cbuf.o
main.o:
	gcc -c main.c
cbuf.o:
	gcc -c cbuf.c
no_buf:
	gcc -o no_buf no_buf.c
clean:
	@rm -f proxy no_buf
	rm -f main.o cbuf.o

