PROGRAM=timestamp-test
topsrc_dir=../../../../..
CPPFLAGS=-I$(topsrc_dir)/include -I$(shell $(PGBIN)/pg_config --includedir)
CFLAGS=-Wall -O0 -g -std=gnu99
CC=gcc

OBJS=main.o \
	 $(topsrc_dir)/utils/strlcpy.o \
	 $(topsrc_dir)/utils/psprintf.o \
	 $(topsrc_dir)/main/pool_globals.o \
	 $(topsrc_dir)/rewrite/pool_timestamp.o \
	 $(topsrc_dir)/parser/libsql-parser.a

all: all-pre $(PROGRAM)

all-pre:
	$(MAKE) -C $(topsrc_dir)/utils strlcpy.o
	$(MAKE) -C $(topsrc_dir)/utils psprintf.o
	$(MAKE) -C $(topsrc_dir)/main pool_globals.o
	$(MAKE) -C $(topsrc_dir)/rewrite pool_timestamp.o
	$(MAKE) -C $(topsrc_dir)/parser

$(PROGRAM): $(OBJS)
	$(CC) $(OBJS) -o $(PROGRAM)

main.o: main.c

test: $(PROGRAM)
	./run-test parse_schedule

clean:
	-rm *.o
	-rm $(PROGRAM)
	-rm result/*.out
	-rm result.txt
	-rm test.diff

.PHONY: all all-pre test clean
