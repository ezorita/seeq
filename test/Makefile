vpath %.c ../src
vpath %.h ../src

CC= gcc
P= testset
OBJECTS= seeq.o
CFLAGS= -I../src `pkg-config --cflags glib-2.0` -g -Wall -std=gnu99 \
	-fprofile-arcs -ftest-coverage -O0
LDLIBS= -L`pwd` -Wl,-rpath=`pwd` `pkg-config --libs glib-2.0` \
	-lfaultymalloc
$(P): $(OBJECTS) libfaultymalloc.so

clean:
	rm -f testset *.o *.so *.gcda *.gcno *.gcov gmon.out analysis.txt

libfaultymalloc.so: faultymalloc.c
	$(CC) -fPIC -shared $(CFLAGS) -o libfaultymalloc.so faultymalloc.c

test: testset
	gtester --verbose --keep-going testset
	gcov *.gcda

testperf: testset
	#perf stat -B -e cache-references,cache-misses,cycles,instructions,branches,faults,migrations gtester --verbose --keep-going -m=perf testset
	perf stat -d gtester --verbose --keep-going -m=perf testset
	#gprof -F trie testset gmon.out > analysis.txt
	gcov *.gcda

debug: testset
	gdb --command=debug.gdb --args testset -m=perf

valgrind:
	valgrind --vgdb=yes --vgdb-error=0 ./testset -m=perf
