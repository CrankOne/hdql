# Makefile generating parser for domain-specific language detector selection
# This will require you to have YACC/LEX installed.
# The resulting sources may be used as is, however (without those dependencies)

YACC=bison
LEX=flex

CFLAGS=-g -ggdb -Wall -x c -Iinclude/ -DBUILD_GT_UTEST=1 -DHDQL_TYPES_DEBUG=1 -Wfatal-errors
CCFLAGS=-g -ggdb -Wall -Iinclude/ -DBUILD_GT_UTEST=1 -DHDQL_TYPES_DEBUG=1 -Wfatal-errors

all: hdql

grammar: hdql.tab.c lex.yy.c

#obj/query.o obj/hdql.tab.o obj/lex.yy.o obj/main.o obj/compound.o \
# obj/context.o obj/types.o obj/value.o \
# obj/operations.o obj/function.o obj/errors.o
	#obj/events-struct.o obj/iteration-tests.o

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
	  obj/iface-fwd-query-as-collection.o \
	  obj/iface-fwd-query-as-scalar.o \
	  obj/iface-arith-op-as-scalar.o \
	  obj/iface-arith-op-as-collection.o

test/events-struct_.cc: templates/events-struct.cc.m4 \
					   templates/for-all-types.m4 \
					   templates/iface-implems.cc.m4 \
					   templates/pop-defs.m4 \
					   templates/push-defs-declare.m4 \
					   templates/push-defs-impl-iface.m4 \
					   templates/push-defs-register-attr-def.m4 \
					   templates/for-all-types.m4 \
					   templates/implems/arrayAtomic.cc.m4 \
					   templates/implems/arrayCompound.cc.m4 \
					   templates/implems/mapCompound.cc.m4 \
					   templates/implems/scalarAtomic.cc.m4 \
					   templates/implems/scalarCompound.cc.m4
	m4 $< > $@

hdql: $(OBJS)
	g++ $^ -o $@ -lgtest

obj/hdql.tab.o: hdql.tab.c grammar
	gcc -c $(CFLAGS) $< -o $@

obj/lex.yy.o: lex.yy.c grammar
	gcc -c $(CFLAGS) $< -o $@

obj/main.o: test/main.cc grammar
	gcc -c $(CCFLAGS) $< -o $@

obj/events-struct.o: test/events-struct.cc test/events-struct.hh
	gcc -c $(CCFLAGS) $< -o $@

obj/samples.o: test/samples.cc
	gcc -c $(CCFLAGS) $< -o $@

lex.yy.c: hdql.l hdql.tab.h
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
		   obj/*.o \
		   test/events-struct_.cc \
		   hdql

.PHONY: all clean grammar
