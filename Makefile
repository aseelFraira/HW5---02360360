.PHONY: all clean

CC = g++
CFLAGS = -std=c++17 -I src

all: clean
	flex -o build/lex.yy.c src/lexer/scanner.lex
	bison -Wcounterexamples -d -o build/parser.tab.c src/parser/parser.y
	$(CC) $(CFLAGS) -I build -o hw5 build/*.c src/*.cpp src/ast/*.cpp src/semantic/*.cpp src/codegen/*.cpp

clean:
	rm -rf build hw5
	mkdir -p build
