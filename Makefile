CXX=g++
CXXFLAGS=-g -Og -Wall -Werror -pedantic -std=c++17 -fsanitize=address -fsanitize=undefined -D_GLIBCXX_DEBUG

all: libfat.a fat_test fat_shell

fat_test: fat_test.o libfat.a
	$(CXX) $(CXXFLAGS) -o $@ $^

fat_shell: fat_shell.o libfat.a
	$(CXX) $(CXXFLAGS) -o $@ $^

fat_internal.h: fat.h

fat.o: fat.cc fat_internal.h

libfat.a: fat.o
	ar cr $@ $^
	ranlib $@

fat_test.o: fat_test.cc fat.h

SUBMIT_FILENAME=fat-submission-$(shell date +%Y%m%d%H%M%S).tar.gz

archive:
	tar -zcf $(SUBMIT_FILENAME) $(wildcard *.cc *.h *.hh *.H *.cpp *.C *.c *.txt *.md *.pdf) Makefile 
	@echo "Created $(SUBMIT_FILENAME); please upload and submit this file."

submit: archive

clean:
	rm -f *.o

.PHONY: submit archive all clean
