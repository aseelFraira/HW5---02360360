#include "SymbolTable.hpp"

void SymbolTable::beginScope() {
    varScopes.emplace_back();
}

void SymbolTable::endScope() {
    if (!varScopes.empty()) {
        varScopes.pop_back();
    }
}

void SymbolTable::beginFunction() {
    currentOffset = 0;      // Reset offset for new function
    ParamOffset = -1;
    varScopes.clear();      // Clear any leftover scopes
    beginScope();           // Start the top-level scope for this function
}
void SymbolTable::endFunction() {
    currentOffset = 0;      // Reset offset for new function
    ParamOffset = -1;
    varScopes.clear();      // Clear any leftover scopes
    endScope();           // Start the top-level scope for this function
}

bool SymbolTable::declareVar(const std::string& name, ast::BuiltInType type,bool isArr,int size) {

    if (varScopes.empty()) beginScope(); // always have one scope - safety

    auto& currentScope = varScopes.back();
    if (currentScope.count(name)) {
        return false;  // Variable already declared in this scope
    }
    currentScope[name] = { type, isArr,currentOffset,size };

    currentOffset += size;

    return true;
}
bool SymbolTable::declareParam(const std::string& name, ast::BuiltInType type,bool isArr,int size) {

    if (varScopes.empty()) beginScope(); // always have one scope - safety

    auto& currentScope = varScopes.back();
    if (currentScope.count(name)) {
        return false;  // Variable already declared in this scope
    }
    currentScope[name] = { type, isArr,ParamOffset-- };

    return true;
}

bool SymbolTable::declareFunc(const std::string& name, FunctionInfo info) {
    if (funcTable.count(name)) { // In functions we only have one table, funcs are global
        return false; // Function already defined
    }
    funcTable[name] = info;
    return true;
}

VariableInfo* SymbolTable::lookupVar(const std::string& name) {
    //Search from the last element in the vector of the scopes..
    for (auto it = varScopes.rbegin(); it != varScopes.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return &found->second;
    }
    return nullptr;
}

FunctionInfo* SymbolTable::lookupFunc(const std::string& name) {
    auto found = funcTable.find(name);
    if (found != funcTable.end()) return &found->second;
    return nullptr;
}


int SymbolTable::getOffset(const std::string& name) const {
    for (auto it = varScopes.rbegin(); it != varScopes.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return found->second.offset;
        }
    }
    throw std::runtime_error("Variable not found: " + name);
}
int SymbolTable::getCurrentOffset() const{
    return currentOffset;
}
