#include "SemanticVisitor.hpp"
#include "output.hpp"
#include <memory>
#include <cassert>
#include "iostream"
using namespace ast;

//TODO: we must enforce handling string only in PRINT!
std::vector<std::string> TypeToString(std::vector<BuiltInType> types)
{
    std::vector<std::string> converted;
    for (const auto &type : types)
    {
        switch (type)
        {
            case ast::BuiltInType::VOID:
                converted.push_back("VOID");
                break;
            case ast::BuiltInType::BOOL:
                converted.push_back("BOOL");
                break;
            case ast::BuiltInType::BYTE:
                converted.push_back("BYTE");
                break;
            case ast::BuiltInType::INT:
                converted.push_back("INT");
                break;
            case ast::BuiltInType::STRING:
                converted.push_back("STRING");
                break;
        }
    }
    return converted;
}


SemanticVisitor::SemanticVisitor(output::ScopePrinter* printer)
        : printer(printer) {
    symbols.declareFunc("print", {BuiltInType::VOID, {BuiltInType::STRING}});
    symbols.declareFunc("printi", {BuiltInType::VOID, {BuiltInType::INT}});
    printer->emitFunc("print", BuiltInType::VOID, {BuiltInType::STRING});
    printer->emitFunc("printi", BuiltInType::VOID, {BuiltInType::INT});
}
//---------------------------------Var Dec--------------------------------------
//Problem 1: We must pass to the symbols table the size of array to align the
// offsets
void SemanticVisitor::visit(VarDecl& node) {

    const std::string& varName = node.id->value;
    BuiltInType declaredType;
    bool isArray = false;

    // Check for duplicate variable declarations (var or func)
    if (symbols.lookupVar(varName) || symbols.lookupFunc(varName)) {
        output::errorDef(node.line, varName);
        return;
    }

    // Extract declared type
    auto primType = std::dynamic_pointer_cast<PrimitiveType>(node.type);
    auto arrayType = std::dynamic_pointer_cast<ArrayType>(node.type);

    if (primType) {
        declaredType = primType->type;
    } else if (arrayType) {
        arrayType->accept(*this);
        isArray = true;
        declaredType = arrayType->type;
        node.len = arrayType->len;
        node.id->len = arrayType->len;
    } else {
        output::errorMismatch(node.line);
        return;
    }
    // Analyze initializer expression if exists
    if (node.init_exp) {
        node.init_exp->accept(*this);

        auto initID = std::dynamic_pointer_cast<ast::ID>(node.init_exp);
        if (initID) {
            auto symbolEntry = symbols.lookupVar(initID->value);
            if (!symbolEntry) {
                output::errorUndef(node.line, initID->value);
            }
            else{
                if(symbolEntry->isArray){
                    output::errorMismatch(node.line);
                }
            }
        }

        auto initCall = std::dynamic_pointer_cast<ast::Call>(node.init_exp);
        if (initCall) {
            auto funcEntry = symbols.lookupFunc(initCall->func_id->value);
            if (!funcEntry || funcEntry->returnType != declaredType) {
                output::errorMismatch(node.line);
                return;
            }
        } else {
            if (declaredType != node.init_exp->type &&
                !(declaredType == BuiltInType::INT && node.init_exp->type == BuiltInType::BYTE)) {
                output::errorMismatch(node.line);
                return;
            }
        }
    }

    // Declare variable in symbol table
    bool success = symbols.declareVar(varName, declaredType, isArray, node.len);
    if (!success) {
        output::errorDef(node.line, varName);
        return;
    }

    // Emit variable or array declaration
    node.id->type = declaredType;
    int offset = symbols.getOffset(varName);
    node.id->offset = offset + currParamsNum; //this is needed for the alloca list
    if (isArray) {
        printer->emitArr(varName, declaredType, node.len, offset);
    } else {
        printer->emitVar(varName, declaredType, offset);
    }
}


//-------------------------------FuncDecl---------------------------------------
void SemanticVisitor::visit(FuncDecl& node) {
    auto retTypeNode = std::dynamic_pointer_cast<PrimitiveType>(node.return_type);
    if (!retTypeNode) {
        output::errorMismatch(node.line);
        return;
    }


    currentReturnType = retTypeNode->type;

    symbols.beginFunction();
    currParamsNum = node.formals->formals.size();
    printer->beginScope();

    for (const auto& formal : node.formals->formals) {
        auto typeNode = std::dynamic_pointer_cast<PrimitiveType>(formal->type);
        if (!typeNode) {
            output::errorMismatch(node.line);
            continue;
        }
        const std::string& varName = formal->id->value;
        formal->id->type = typeNode->type;

        bool success = symbols.declareParam(varName, typeNode->type, false, node.len);
        if (!success) {
            output::errorDef(formal->line, varName);
        } else {
            node.id->offset = symbols.getOffset(varName) - currParamsNum;
            printer->emitVar(varName, typeNode->type, symbols.getOffset(varName));
        }
    }
    funcBegin = true;
    node.body->accept(*this);
    printer->endScope();
    node.offset = symbols.getCurrentOffset() + node.formals->formals.size();
    currParamsNum = 0;
    symbols.endFunction();

}
//---------------------------------Array type-----------------------------------
void SemanticVisitor::visit(ArrayType& node) {
    if (node.length) {
        auto n1 = std::dynamic_pointer_cast<Num>(node.length);
        auto n2 = std::dynamic_pointer_cast<NumB>(node.length);
        if(n1){
            node.len = n1->value;
            n1->accept(*this);
        }
        if(n2){
            node.len = n2->value;
            n2->accept(*this);
        }
        if(!n1 && !n2) {
            output::errorMismatch(node.line);
        }
    }
}


void SemanticVisitor::visit(Return& node) {
    if (node.exp) {
        node.exp->accept(*this);
        BuiltInType actualType = node.exp->type;
        if (actualType != currentReturnType) {
            output::errorMismatch(node.line);
        }
    } else if (currentReturnType != BuiltInType::VOID) {
        output::errorMismatch(node.line);

    }
}

//void SemanticVisitor::visit(Formals& node) {}

void SemanticVisitor::visit(Statements& node) {

    bool openedScope = false;
    if(!funcBegin){
        printer->beginScope();
        symbols.beginScope();
        openedScope = true;
    }
    funcBegin = false;

    for (auto& stmt : node.statements) {
        stmt->accept(*this);
    }
    if(openedScope){
        printer->endScope();
        symbols.endScope();
    }
}



void SemanticVisitor::visit(ExpList& node) {
    for (auto& exp : node.exps) {
        exp->accept(*this);
    }
}

void SemanticVisitor::visit(Call& node) {
    auto sig = symbols.lookupFunc(node.func_id->value);  // Look up function signature
    auto asVar = symbols.lookupVar(node.func_id->value);
    if(asVar){
        output::errorDefAsVar(node.line,node.func_id->value);
    }
    if (!sig) {
        output::errorUndefFunc(node.line, node.func_id->value);
        return;
    }

    BuiltInType retType = sig->returnType;
    std::vector<BuiltInType> paramTypes = sig->paramTypes;

    node.args->accept(*this);

    node.typesOfArgs = symbols.lookupFunc(node.func_id->value)->paramTypes;

    // START: HANDLING THE PRINT FUNCTION
    if (node.func_id->value == "print") {

        if (node.args->exps.size() != 1) {
            return;
        }

        auto argType = node.args->exps[0]->type;
        if (argType != ast::STRING) {
            return;
        }

        node.type = ast::VOID;
        return;
    }

    if (node.func_id->value == "printi") {
        if (node.args->exps.size() != 1) {
            return;
        }

        auto argType = node.args->exps[0]->type;
        if (argType != ast::INT && argType != ast::BYTE) {
            return;
        }

        node.type = ast::VOID;
        return;
    }
    //END :HANDLING PRINT FUNCTION

    // Visit and check all argument expressions
    if (node.args) {
        node.args->accept(*this);

        if (node.args->exps.size() != paramTypes.size()) {
            auto params = TypeToString(paramTypes);
            output::errorPrototypeMismatch(node.line, node.func_id->value, params);
        }

        for (size_t i = 0; i < paramTypes.size(); ++i) {
            if (!SemanticVisitor::isAssignableTo(node.args->exps[i]->type,paramTypes[i])) {
                auto params = TypeToString(paramTypes);
                output::errorPrototypeMismatch(node.line, node.func_id->value, params);
            }
            auto argumentID = std::dynamic_pointer_cast<ast::ID>(node.args->exps[i]);
            if(argumentID){
                if(auto varData = symbols.lookupVar(argumentID->value)){
                    if(varData->isArray){
                        auto params = TypeToString(paramTypes); //CANT USE TO STRING IN OUTPUD!!!!
                        output::errorPrototypeMismatch(node.line, node.func_id->value, params);
                    }
                }
            }

        }
    } else if (!paramTypes.empty()) {
        // No args provided, but expected some
        auto params = TypeToString(paramTypes);
        output::errorPrototypeMismatch(node.line, node.func_id->value, params);
    }

    node.type = retType;
}

void SemanticVisitor::visit(Assign& node) {
    node.exp->accept(*this); // RHS
    node.id->accept(*this);  // LHS
    BuiltInType rhsType = node.exp->type;

    // Check LHS
    auto lhsID = std::dynamic_pointer_cast<ast::ID>(node.id);
    if (!lhsID) {
        output::errorUndef(node.line, node.id->value);
        return;
    }

    if (symbols.lookupFunc(lhsID->value)) {
        output::errorDefAsFunc(node.line, lhsID->value);
        return;
    }

    auto lhs = symbols.lookupVar(lhsID->value);
    if (!lhs) {
        output::errorUndef(node.line, lhsID->value);
        return;
    }

    if (lhs->isArray && !std::dynamic_pointer_cast<ast::ArrayDereference>(node.id)) {
        output::ErrorInvalidAssignArray(node.line, lhsID->value);
        return;
    }

    auto rhsID = std::dynamic_pointer_cast<ast::ID>(node.exp);

    //checking right side
    if(rhsID) {

        if (symbols.lookupFunc(rhsID->value)) {
            output::errorDefAsFunc(node.line, rhsID->value);
        }
        auto rhs = symbols.lookupVar(rhsID->value);
        if (!rhs) {
            output::errorUndef(node.line, rhsID->value);
        }
        if(rhs->isArray && !lhs->isArray || lhs->isArray && !rhs->isArray){
            output::errorMismatch(node.line);
        }
        if (rhs->isArray &&
            !std::dynamic_pointer_cast<ast::ArrayDereference>(rhsID)) {
            output::ErrorInvalidAssignArray(node.line, rhsID->value);
        }

    }

    // Type check
    if (lhs->type != rhsType && !(lhs->type == ast::BuiltInType::INT && rhsType == ast::BuiltInType::BYTE)) {
        output::errorMismatch(node.line);
    }

}

void SemanticVisitor::visit(ast::ArrayAssign& node) { // should allow normal assigns "arr[1] = 2;"
    node.id->accept(*this);
    node.index->accept(*this);
    node.exp->accept(*this);

    auto iType = node.index->type;

    if (iType != ast::INT && iType!= ast::BYTE)
    {
        output::errorMismatch(node.line);
    }
    auto var = symbols.lookupVar(node.id->value);

    if (!var)
    {
        output::errorUndef(node.line, node.id->value);
    }
    if (!var->isArray)
    {
        output::errorMismatch(node.line);
    }
    auto assignTo = std::dynamic_pointer_cast<ID>(node.exp);
    if (assignTo)
    {
        auto assignTo_var = symbols.lookupVar(assignTo->value);
        if (assignTo_var && assignTo_var->isArray)
        {
            output::errorMismatch(node.line); // RHS is an array → illegal
        }
    }

    ast::BuiltInType elemType = var->type;
    if (elemType != node.exp->type && !(elemType == ast::BuiltInType::INT && node.exp->type == ast::BuiltInType::BYTE))
    {
        output::errorMismatch(node.line);
    }

}


void SemanticVisitor::visit(ArrayDereference &node) {
    node.id->accept(*this);  // Visit the array
    node.index->accept(*this);  // Visit the index
    auto arrInfo = symbols.lookupVar(node.id->value);


    // Only get the element type and set this node’s type
    if (!arrInfo->isArray) {
        output::errorMismatch(node.line);
        return;
    }
    node.type = arrInfo->type;
}


void SemanticVisitor::visit(If& node) {
    node.condition->accept(*this);
    if (node.condition->type != BuiltInType::BOOL) {
        output::errorMismatch(node.line);
    }

    symbols.beginScope();
    printer->beginScope();
    node.then->accept(*this);
    printer->endScope();
    symbols.endScope();


    if (node.otherwise) {
        symbols.beginScope();
        printer->beginScope();
        node.otherwise->accept(*this);
        printer->endScope();
        symbols.endScope();
    }

}
void SemanticVisitor::visit(Cast &node) {
    node.exp->accept(*this);
    node.target_type->accept(*this);

    // Get the type of the expression
    BuiltInType exprType = node.exp->type;  // Assume 'currentType' is updated in previous accept()
    BuiltInType targetType = node.target_type->type;

    // Type checking: allow only numeric casts (e.g., int <-> byte)
    if (!isNumericType(targetType)  || !isNumericType(exprType)) {
        output::errorMismatch(node.line);
    }
    node.type = targetType;

}
bool SemanticVisitor::isNumericType(BuiltInType type) {
    return type == ast::BYTE || type == ast::INT ;
}

void SemanticVisitor::visit(RelOp &node) {
    node.left->accept(*this);
    node.right->accept(*this);

    BuiltInType lhs = node.left->type;
    BuiltInType rhs = node.right->type;

    auto lhsID = std::dynamic_pointer_cast<ID>(node.left);
    if(lhsID){
        auto lhsVar = symbols.lookupVar(lhsID->value);
        if(!lhsVar){
            output::errorUndef(node.line, lhsID->value);
        }
        else if(lhsVar->isArray){
            output::errorMismatch(node.line);

        }
    }
    auto rhsID = std::dynamic_pointer_cast<ID>(node.right);
    if(rhsID){
        auto rhsVar = symbols.lookupVar(rhsID->value);
        if(!rhsVar){
            output::errorUndef(node.line, rhsID->value);
        }
        else if(rhsVar->isArray){
            output::errorMismatch(node.line);
        }
    }

    if(!SemanticVisitor::isNumericType(lhs) || !SemanticVisitor::isNumericType(rhs)){
        output::errorMismatch(node.line);
    }
    node.type = ast::BOOL;

}

void SemanticVisitor::visit(Or &node) {
    node.left->accept(*this);
    node.right->accept(*this);

    BuiltInType lhs = node.left->type;
    BuiltInType rhs = node.right->type;

    if(lhs != ast::BOOL || rhs != ast::BOOL){
        output::errorMismatch(node.line);
    }
    node.type = ast::BOOL;
}
void SemanticVisitor::visit(And &node) {
    node.left->accept(*this);
    node.right->accept(*this);

    BuiltInType lhs = node.left->type;
    BuiltInType rhs = node.right->type;

    if(lhs != ast::BOOL || rhs != ast::BOOL){
        output::errorMismatch(node.line);
    }
    node.type = ast::BOOL;

}
void SemanticVisitor::visit(Not &node) {
    node.exp->accept(*this);
    if(node.exp->type != ast::BOOL){
        output::errorMismatch(node.line);
    }
    node.type = ast::BOOL;
}

void SemanticVisitor::visit(BinOp &node) {
    node.left->accept(*this);
    node.right->accept(*this);
    BuiltInType lhs = node.left->type;
    BuiltInType rhs = node.right->type;


    if(!SemanticVisitor::isNumericType(lhs) || !SemanticVisitor::isNumericType(rhs)){
        output::errorMismatch(node.line);
    }
    BuiltInType retType = BYTE;
    if(lhs == ast::INT || rhs == ast::INT){
        retType = INT;
    }
    node.type = retType;

}
void SemanticVisitor::visit(While& node) {
    node.condition->accept(*this);
    if (node.condition->type != BOOL) {
        output::errorMismatch(node.line);
    }

    symbols.beginScope();
    printer->beginScope();
    loopDepth++;

    node.body->accept(*this);
    loopDepth--;

    printer->endScope();
    symbols.endScope();

}


void SemanticVisitor::visit(Break& node) {
    if(loopDepth == 0){
        output::errorUnexpectedBreak(node.line);
    }
}

void SemanticVisitor::visit(Continue& node) {
    if(loopDepth == 0){
        output::errorUnexpectedContinue(node.line);
    }
}

void SemanticVisitor::visit(Num& node) { //Rule 1
    node.type = BuiltInType::INT;
}

void SemanticVisitor::visit(NumB& node) { //Rule 1
    if (node.value < 0 || node.value > 255)
    {
        output::errorByteTooLarge(node.line, node.value);
    }
    node.type = BuiltInType::BYTE;
}

void SemanticVisitor::visit(String& node) {
    node.type = BuiltInType::STRING;
}

void SemanticVisitor::visit(Bool& node) {
    node.type = BuiltInType::BOOL;
}

void SemanticVisitor::visit(ID& node) {
    auto t = symbols.lookupVar(node.value);
    if (!t) {
        output::errorUndef(node.line, node.value);
    } else {
        node.type = t->type;
        node.len = t->length;
    }
}

bool SemanticVisitor::isAssignableTo(BuiltInType from, BuiltInType to) {
    if (from == to) return true;
    if (from == ast::BYTE && to == ast::INT) return true;
    return false;
}
void SemanticVisitor::visit(ast::PrimitiveType &node) {}

void SemanticVisitor::visit(ast::Formal &node) {}

void SemanticVisitor::visit(ast::Formals &node) {}

void SemanticVisitor::visit(ast::Funcs &node) {
    // First pass: collect all function signatures
    for (const auto& func : node.funcs) {
        if (!func) continue;

        auto retTypeNode = std::dynamic_pointer_cast<PrimitiveType>(func->return_type);
        if (!retTypeNode) {
            output::errorMismatch(func->line);
            continue;
        }

        std::vector<BuiltInType> paramTypes;
        for (const auto& formal : func->formals->formals) {
            auto typeNode = std::dynamic_pointer_cast<PrimitiveType>(formal->type);
            if (!typeNode) {
                output::errorMismatch(func->line);
                continue;
            }
            paramTypes.push_back(typeNode->type);
        }

        bool success = symbols.declareFunc(func->id->value, {retTypeNode->type, paramTypes});
        if (!success) {
            output::errorDef(func->line, func->id->value);
        } else {
            printer->emitFunc(func->id->value, retTypeNode->type, paramTypes);
        }
    }

    // Second pass: analyse function bodies
    for (const auto& func : node.funcs) {
        if (func) {
            func->accept(*this); // visit only processes the body now
        }
    }
    auto mainInfo = symbols.lookupFunc("main");
    if (!mainInfo || mainInfo->returnType != ast::BuiltInType::VOID ||
        !mainInfo->paramTypes.empty()) {
        output::errorMainMissing();
    }
}

SymbolTable *SemanticVisitor::getTable() {
    return &symbols;
}


