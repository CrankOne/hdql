# Makefile generating parser for domain-specific language detector selection
# This will require you to have YACC/LEX installed.
# The resulting sources may be used as is, however (without those dependencies)

YACC=bison
LEX=flex

CFLAGS=-g -ggdb -Wall -x c -Iinclude/ -DBUILD_GT_UTEST=1 -DHDQL_TYPES_DEBUG=1
CCFLAGS=-g -ggdb -Wall -Iinclude/ -DBUILD_GT_UTEST=1 -DHDQL_TYPES_DEBUG=1

all: hdql

grammar: hdql.tab.c lex.yy.c

hdql: obj/query.o obj/hdql.tab.o obj/lex.yy.o obj/main.o obj/compound.o \
 obj/context.o obj/types.o obj/value.o obj/events-struct.o obj/iteration-tests.o \
 obj/operations.o obj/function.o obj/errors.o
	g++ $^ -o $@ -lgtest

obj/hdql.tab.o: hdql.tab.c grammar
	gcc -c $(CFLAGS) $< -o $@

obj/lex.yy.o: lex.yy.c grammar
	gcc -c $(CFLAGS) $< -o $@

obj/main.o: test/main.cc grammar
	gcc -c $(CCFLAGS) $< -o $@

obj/events-struct.o: test/events-struct.cc test/events-struct.hh
	gcc -c $(CCFLAGS) $< -o $@


lex.yy.c: hdql.l hdql.tab.h
	$(LEX) --header-file=lex.yy.h $<

hdql.tab.h hdql.tab.c: hdql.y
	$(YACC) -Wcounterexamples -d -p hdql_ $<


obj/%.o: src/%.c
	gcc -c $(CFLAGS) $< -o $@

obj/%.o: src/%.cc
	gcc -c $(CCFLAGS) $< -o $@

obj/%-tests.o: test/%-tests.cc
	g++ -c $(CCFLAGS) $< -o $@

clean:
	rm -fv hdql.tab.h y.tab.h \
		   hdql.tab.c hdql.tab.c \
		   lex.yy.h lex.yy.c \
		   obj/*.o \
		   hdql

.PHONY: all clean grammar
