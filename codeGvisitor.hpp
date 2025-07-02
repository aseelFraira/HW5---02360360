#ifndef CODEGVISITOR_HPP
#define CODEGVISITOR_HPP
#pragma once
#include <stack>

#include "output.hpp"
#include "SymbolTable.hpp"

using namespace ast;

class codeGvisitor : public Visitor {
public:
    codeGvisitor(output::CodeBuffer* cb);
    std::string emitOobCheck(const std::string& idxVar,
                                         int length);
    void visit(ast::ArrayType &node) override;
    void visit(FuncDecl& node) override;
    void visit(VarDecl& node) override;
    void visit(Return& node) override;
    void visit(Assign& node) override;
    void visit(If& node) override;
    void visit(While& node) override;
    void visit(Break& node) override;
    void visit(Continue& node) override;
    void visit(Call& node) override;
    void visit(ExpList& node) override;
    void visit(ast::ArrayAssign &node) override;
    void visit(ast::Funcs& node) override;
    void visit(ast::PrimitiveType &node) override;
    void visit(ast::Formal &node) override;
    void visit(ast::Formals &node) override;



    void visit(Statements& node) override;
    void widenByte(std::string& valss, ast::BuiltInType typee);
    void visit(Num& node) override;
    void visit(NumB& node) override;
    void visit(String& node) override;
    void visit(Bool& node) override;
    void visit(ID& node) override;
    void visit(BinOp& node) override;
    void visit(RelOp& node) override;
    void visit(Not& node) override;
    void visit(And& node) override;
    void visit(Or& node) override;
    void visit(ArrayDereference& node) override;
    void visit(Cast& node) override;



private:
        output::CodeBuffer *cb;
        std::stack<std::string> beginLabels;
        std::stack<std::string> endLabels;
        BuiltInType currentReturnTypeFunc;

    std::string printRegister(std::string msg, std::vector<std::string> regs);
};


#endif //HW3___02360360_SEMANTICVISITOR_HPP
