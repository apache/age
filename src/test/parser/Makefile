# $Header$

PROGRAM=parser-test
PGPOOL_SRC=../../../src/parser
PARSER_INCLUDES=../../../src/include/parser
PARSER_OBJS=gram.o parser.o list.o makefuncs.o value.o nodes.o pool_memory.o main.o keywords.o outfuncs.o copyfuncs.o kwlookup.o scansup.o wchar.o

#ENABLE_GCOV=1

CFLAGS=-Wall -c -g -include pool.h -I. -I $(PARSER_INCLUDES)
CFLAGS+=-O0 -g -DPARSER_TEST
LDFLAGS=

ifdef ENABLE_GCOV
CFLAGS+= -fprofile-arcs -ftest-coverage
LDFLAGS+= -lgcov
GENHTML_OUTDIR=genhtml
endif

all: $(PROGRAM)

.SUFFIXES:
.SUFFIXES: .o .c

.c.o:
	gcc $(CFLAGS) $<

$(PROGRAM): $(PARSER_OBJS)
	gcc $(PARSER_OBJS) -o $(PROGRAM) $(LDFLAGS)

main.o: main.c

keywords.o: $(PGPOOL_SRC)/keywords.c
	gcc $(CFLAGS) $<

outfuncs.o: $(PGPOOL_SRC)/outfuncs.c
	gcc $(CFLAGS) $<

gram.o: gram.c scan.c
	gcc $(CFLAGS) $<

gram.c: $(PGPOOL_SRC)/gram.y scan.c
	bison -d $(PGPOOL_SRC)/gram.y
ifdef ENABLE_GCOV
	cp gram.tab.c gram.c
	cp gram.tab.h gram.h
else
	mv -f gram.tab.c gram.c
	mv -f gram.tab.h gram.h
endif

parser.o : $(PGPOOL_SRC)/parser.c
	gcc $(CFLAGS) $<

list.o : $(PGPOOL_SRC)/list.c
	gcc $(CFLAGS) $<

makefuncs.o : $(PGPOOL_SRC)/makefuncs.c
	gcc $(CFLAGS) $<

value.o : $(PGPOOL_SRC)/value.c
	gcc $(CFLAGS) $<

nodes.o : $(PGPOOL_SRC)/nodes.c
	gcc $(CFLAGS) $<

scansup.o : $(PGPOOL_SRC)/scansup.c
	gcc $(CFLAGS) $<

scan.c : $(PGPOOL_SRC)/scan.l
	flex -o$@ $<

pool_memory.o: $(PGPOOL_SRC)/pool_memory.c $(PARSER_INCLUDES)/pool_memory.h
	gcc $(CFLAGS) $<

copyfuncs.o: $(PGPOOL_SRC)/copyfuncs.c
	gcc $(CFLAGS) $<

kwlookup.o: $(PGPOOL_SRC)/kwlookup.c
	gcc $(CFLAGS) $<

wchar.o: $(PGPOOL_SRC)/wchar.c
	gcc $(CFLAGS) $<

test: $(PROGRAM)
	./run-test parse_schedule

cov:
	test -d $(GENHTML_OUTDIR) || mkdir $(GENHTML_OUTDIR)
	lcov --directory . --capture --output-file $(PROGRAM).info
	genhtml -o $(GENHTML_OUTDIR) -f $(PROGRAM).info

clean-cov:
ifdef ENABLE_GCOV
	rm -f *.gcda *.gcno
	rm -f parser-test.info
	rm -rf $(GENHTML_OUTDIR)
endif

clean: clean-cov
	rm -f $(PROGRAM)
	rm -f *.o
	rm -f gram.c scan.c gram.h
	rm -f gram.tab.c gram.tab.h

.PHONY: all test clean-cov clean cov
