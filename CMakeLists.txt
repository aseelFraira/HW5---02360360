cmake_minimum_required(VERSION 3.24)
project(HW5___02360360)

set(CMAKE_CXX_STANDARD 17)

include_directories(src)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Generate lexer and parser
find_program(FLEX_EXECUTABLE flex)
find_program(BISON_EXECUTABLE bison)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c
    COMMAND ${FLEX_EXECUTABLE} -o ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c ${CMAKE_CURRENT_SOURCE_DIR}/src/lexer/scanner.lex
    DEPENDS src/lexer/scanner.lex
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.c ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.h
    COMMAND ${BISON_EXECUTABLE} -d -o ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.c ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/parser.y
    DEPENDS src/parser/parser.y
)

add_executable(HW5___02360360
    ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c
    ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.c
    src/main.cpp
    src/ast/nodes.cpp
    src/semantic/SemanticVisitor.cpp
    src/semantic/SymbolTable.cpp
    src/codegen/codeGvisitor.cpp
    src/codegen/output.cpp
)
