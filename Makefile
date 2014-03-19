VERSION=0.9

FUSE_LDFLAGS+=$(shell pkg-config fuse --libs)
# should be something like -pthread -lfuse -lrt -ldl

FUSE_CFLAGS+=$(shell pkg-config fuse --cflags)
# should be something like -D_FILE_OFFSET_BITS=64 -I/usr/include/fuse
MYSQL_LDFLAGS=-lmysqlclient -rdynamic

CC?=gcc


CFLAGS+=-DMY_DEBUG -Wall -g -Wformat -Wextra

all: check wikkafs

rebuild: clean all

check:
	@if [ "$(FUSE_LDFLAGS)" = "" ] || [ "$(FUSE_CFLAGS)" = "" ]; then \
		echo "Missing fuse dev"; \
		exit 1;\
	fi
	@if [ ! -f "/usr/include/mysql/mysql.h" ]; then \
		echo "Missing libmysql-client" ;\
		exit 1;\
	fi

wikkafs: fuse.o fuse_v2.o sql.o main.o
	$(CC) $^ -o $@  $(FUSE_LDFLAGS) $(MYSQL_LDFLAGS)

%.o: %.c wikkafs.h
	$(CC) $< -c $(FUSE_CFLAGS) $(CFLAGS) $(MYSQL_CFLAGS)

clean:
	-rm -f wikkafs *.o

debug:
	$(MAKE) CC=clang CFLAGS=-O0 rebuild

dist:
	-rm wikkafs-$(VERSION).tar.bz2
	mkdir -p wikkafs-$(VERSION)
	cp *.c *.h Makefile COPYING AUTHORS wikkafs-$(VERSION)
	tar cvfj wikkafs-$(VERSION).tar.bz2 wikkafs-$(VERSION)
