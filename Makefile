CC=gcc 
CFLAGS= -Wall -pedantic -std=gnu99 -pthread
LFLAGS= -I/local/courses/csse2310/include -L/local/courses/csse2310/lib -lcsse2310a3 -lcsse2310a4 -lstringstore
LIBCFLAGS =-fPIC -Wall -pedantic -std=gnu99
LIBCFLAGS += -I/local/courses/csse2310/include
.PHONY: all clean
.DEFAULT_GOAL := all
all: dbclient dbserver libstringstore.so

dbclient: dbclient.c
	$(CC) $(CFLAGS) $(LFLAGS) $^ -o $@ -g

dbserver: dbserver.c
	$(CC) $(CFLAGS) $(LFLAGS) $^ -o $@ -g

stringstore: stringstore.o

# Turn stringstore.c into stringstore.o
stringstore.o: stringstore.c
	$(CC) $(LIBCFLAGS) -c $<
# Turn stringstore.o into shared library libstringstore.so
libstringstore.so: stringstore.o
	$(CC) -shared -o $@ stringstore.o



clean:
	rm dbclient *.c
