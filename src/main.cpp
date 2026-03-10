#include "codegen/output.hpp"
#include "ast/nodes.hpp"
#include "semantic/SemanticVisitor.hpp"
#include "codegen/codeGvisitor.hpp"
#include <memory>
#include <iostream>

extern int yyparse();
extern int yylineno;
extern std::shared_ptr<ast::Node> program;

void yyerror(const char* s) {
    output::errorSyn(yylineno);  // Print a syntax error using provided line number
}

int main() {
    output::ScopePrinter printer;

    int result = yyparse();  // Run the parser (uses the lexer internally)
    if (result == 0) {
        // Only run semantic analysis if parsing succeeded
        SemanticVisitor semVisitor(&printer);

        if (program) {
            program->accept(semVisitor);
        }

//        std::cout << printer;

output::CodeBuffer code;
codeGvisitor gvisitor(&code);
program->accept(gvisitor);
std::cout << code;

    }



    return result;
}
