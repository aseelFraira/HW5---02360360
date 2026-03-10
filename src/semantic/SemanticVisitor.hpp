#ifndef HW3___02360360_SEMANTICVISITOR_HPP
#define HW3___02360360_SEMANTICVISITOR_HPP
#pragma once

#include "SymbolTable.hpp"    // Symbol table implementation
#include "codegen/output.hpp"         // ScopePrinter, BuiltInType
using namespace ast;

class SemanticVisitor : public Visitor {
public:
    SemanticVisitor(output::ScopePrinter* printer);

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
  //  void visit(Statement& node) override;
   // void visit(Exp& node) override;

    // Add for expression types:
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
    bool isAssignableTo(BuiltInType from, BuiltInType to);
    bool isNumericType(BuiltInType type);
    SymbolTable* getTable();


        private:
            output::ScopePrinter* printer;
            SymbolTable symbols;
            ast::BuiltInType currentReturnType;
            int loopDepth = 0;  // for break/continue
            int currParamsNum = 0;
            bool funcBegin = false;
};


#endif //HW3___02360360_SEMANTICVISITOR_HPP
