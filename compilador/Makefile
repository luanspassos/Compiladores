CXX      ?= g++
CXXFLAGS ?= -std=c++11 -Wall -Wextra -Wpedantic -O2
SRC      := parser.cpp semantics.cpp symtab.cpp lalr.cpp grammar.cpp lexer.cpp

all: ssl tests/test_compilador

ssl: main.cpp $(SRC) $(wildcard *.hpp)
	$(CXX) $(CXXFLAGS) -o $@ main.cpp $(SRC)

tests/test_compilador: tests/test_compilador.cpp tests/vm.hpp $(SRC) $(wildcard *.hpp)
	$(CXX) $(CXXFLAGS) -o $@ tests/test_compilador.cpp $(SRC)

# testes da VF (unitarios + exemplos) e da VC (lexer original, na raiz do repositorio)
test: all
	./tests/test_compilador
	sh tests/run_examples.sh
	$(MAKE) -C .. -f compilador/Makefile.vc test

clean:
	rm -f ssl tests/test_compilador

.PHONY: all test clean
