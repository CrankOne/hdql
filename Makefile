# Makefile generating parser for domain-specific language detector selection
# This will require you to have YACC/LEX installed.
# The resulting sources may be used as is, however (without those dependencies)

YACC=bison
LEX=flex

CFLAGS=-g -ggdb -Wall -x c -MMD -Iinclude/ -DBUILD_GT_UTEST=1 -DHDQL_TYPES_DEBUG=1 -Wfatal-errors
CCFLAGS=-g -ggdb -Wall -MMD -Iinclude/ -DBUILD_GT_UTEST=1 -DHDQL_TYPES_DEBUG=1 -Wfatal-errors

C_FILES=$(wildcard src/*.c) $(wildcard src/ifaces/*.c)
CXX_FILES=$(wildcard src/*.cc) $(wildcard test/*.cc)

OBJS:=obj/types.o \
	  obj/errors.o \
	  obj/attr-def.o \
	  obj/compound.o \
	  obj/query.o \
	  obj/query-key.o \
	  obj/query-product.o \
	  obj/context.o \
	  obj/operations.o \
	  obj/value.o \
	  obj/function.o \
	  obj/lex.yy.o \
	  obj/hdql.tab.o \
	  obj/main.o \
	  obj/events-struct.o \
	  obj/samples.o \
	  obj/iteration-tests.o \
	  obj/converters.o \
	  obj/iface-fwd-query-as-collection.o \
	  obj/iface-fwd-query-as-scalar.o \
	  obj/iface-arith-op-as-scalar.o \
	  obj/iface-arith-op-as-collection.o \
	  obj/iface-filtered-v-compound.o

DEPS=$(OBJS:%.o=%.d)

all: hdql

# Include deps files
-include $(DEPS)

hdql: $(OBJS)
	g++ $^ -o $@ -lgtest

obj/hdql.tab.o: hdql.tab.c
	gcc -c $(CFLAGS) $< -o $@

obj/lex.yy.o: lex.yy.c
	gcc -c $(CFLAGS) $< -o $@

obj/main.o: test/main.cc
	gcc -c $(CCFLAGS) $< -o $@

obj/events-struct.o: test/events-struct.cc test/events-struct.hh
	gcc -c $(CCFLAGS) $< -o $@

obj/samples.o: test/samples.cc
	gcc -c $(CCFLAGS) $< -o $@

lex.yy.h lex.yy.c: hdql.l hdql.tab.h
	$(LEX) --header-file=lex.yy.h $<

hdql.tab.h hdql.tab.c: hdql.y
	$(YACC) -Wcounterexamples -d -p hdql_ $<

obj/%.o: src/%.c
	gcc -c $(CFLAGS) $< -o $@

obj/%.o: src/%.cc
	gcc -c $(CCFLAGS) $< -o $@

obj/iface-%.o: src/ifaces/%.c
	gcc -c $(CFLAGS) $< -o $@

obj/%-tests.o: test/%-tests.cc
	g++ -c $(CCFLAGS) $< -o $@

clean:
	rm -fv hdql.tab.h y.tab.h \
		   hdql.tab.c hdql.tab.c \
		   lex.yy.h lex.yy.c \
		   obj/*.o obj/*.d \
		   test/events-struct_.cc \
		   hdql

.PHONY: all clean
