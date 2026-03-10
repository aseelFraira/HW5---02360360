#ifndef HW3___02360360_SYMBOLTABLE_HPP
#define HW3___02360360_SYMBOLTABLE_HPP
#pragma once

#include <unordered_map>
#include <vector>
#include <string>
#include "codegen/output.hpp"  // for ast::BuiltInType

struct VariableInfo {
    ast::BuiltInType type;
    bool isArray;
    int offset;
    int length;
};

struct FunctionInfo {
    ast::BuiltInType returnType;
    std::vector<ast::BuiltInType> paramTypes;
};

class SymbolTable {
public:

    void beginScope();                // Enter a new nested block
    void endScope();                  // Exit a block

    void beginFunction();             // Start a new function: resets offset, clears scopes
    void endFunction();

    bool declareVar(const std::string& name, ast::BuiltInType type,bool isArr,int size);
    bool declareParam(const std::string& name, ast::BuiltInType type,bool isArr,int size);
    bool declareFunc(const std::string& name, FunctionInfo info);


    VariableInfo* lookupVar(const std::string& name);
    FunctionInfo* lookupFunc(const std::string& name);

    int getOffset(const std::string& name) const;
    int getCurrentOffset() const;




        private:
    //HW3___02360360_SYMBOLTABLE_HPP
/* We use a vector as a stack to manage nested scopes.
 * Each element in the vector represents one scope (a symbol table for that level).
 * We always search from the end (innermost scope) to the beginning (outermost),
 * which mimics how variables are resolved in nested blocks.
 */
    std::vector<std::unordered_map<std::string, VariableInfo>> varScopes;
    std::unordered_map<std::string, FunctionInfo> funcTable;
    int currentOffset = 0;
    int ParamOffset = -1;
};

#endif