#include "codeGvisitor.hpp"
#include "output.hpp"
#include "iostream"
#include <memory>
#include <cassert>
using namespace std;

using namespace ast;

codeGvisitor::codeGvisitor(output::CodeBuffer* cb): cb(cb)
{
 cb->emit(R"(
declare i32 @scanf(i8*, ...)
declare i32 @printf(i8*, ...)
declare void @exit(i32)
@.int_specifier_scan = constant [3 x i8] c"%d\00"
@.int_specifier = constant [4 x i8] c"%d\0A\00"
@.str_specifier = constant [4 x i8] c"%s\0A\00"
@.oob_str = private unnamed_addr constant [20 x i8] c"Error out of bounds\00", align 1

define i32 @readi(i32) {
    %ret_val = alloca i32
    %spec_ptr = getelementptr [3 x i8], [3 x i8]* @.int_specifier_scan, i32 0, i32 0
    call i32 (i8*, ...) @scanf(i8* %spec_ptr, i32* %ret_val)
    %val = load i32, i32* %ret_val
    ret i32 %val
}

define void @printi(i32) {
    %spec_ptr = getelementptr [4 x i8], [4 x i8]* @.int_specifier, i32 0, i32 0
    call i32 (i8*, ...) @printf(i8* %spec_ptr, i32 %0)
    ret void
}

define void @print(i8*) {
    %spec_ptr = getelementptr [4 x i8], [4 x i8]* @.str_specifier, i32 0, i32 0
    call i32 (i8*, ...) @printf(i8* %spec_ptr, i8* %0)
    ret void
}
    )");
}
///////////////////////////////////ArrayType////////////////////////////////////
void codeGvisitor::visit(ast::ArrayType& node) {
    node.length->accept(*this);
    std::string elementTypeLLVM = output::changeType(node.type);
    std::string arrayTypeLLVM = "[" + std::to_string(node.len) + " x " +
            elementTypeLLVM + "]";
    node.newVar = arrayTypeLLVM;
}
//////////////////////////////////FuncDecl//////////////////////////////////////
void codeGvisitor::visit(FuncDecl& node) {
    // Resolve and validate return type
    node.return_type->accept(*this);
    auto returnTypeNode = std::dynamic_pointer_cast<PrimitiveType>(node.return_type);
    if (!returnTypeNode) {
        output::errorMismatch(node.line);
    }
    currentReturnTypeFunc = returnTypeNode->type;

    std::string llvmReturnType = output::changeType(returnTypeNode->type);

    // Emit function signature
    std::string funcName = node.id->value;
    cb->emit("define " + llvmReturnType + " @" + funcName + "(");

    // Visit and emit parameter declarations
    node.formals->accept(*this);
    cb->emit(") {");
    cb->emit(""); // spacing for readability

    // Emit function entry label
    cb->emitLabel("entry");

    // Allocate local variable space
    if (node.offset > 0) {
        cb->emit("%local_vars = alloca i32, i32 " + std::to_string(node.offset));
    }

    // 🌟 Store each parameter in local_vars
    for (auto& formal : node.formals->formals) {
        const std::string& name = formal->id->value;
        int offset = formal->id->offset;  // this should be set in semantic phase
        std::string llvmType = output::changeType(formal->id->type);
            // %ptr = getelementptr i32, i32* %local_vars, i32 offset
            std::string ptrVar = cb->freshVar();
            cb->emit(ptrVar + " = getelementptr i32, i32* %local_vars, i32 " +
                     std::to_string(offset));

            // %typed_ptr = bitcast i32* %ptr to TYPE*
            std::string typedPtr = cb->freshVar();
            cb->emit(
                    typedPtr + " = bitcast i32* " + ptrVar + " to " + llvmType +
                    "*");

            // store TYPE %param, TYPE* %typed_ptr
            cb->emit(
                    "store " + llvmType + " %" + name + ", " + llvmType + "* " +
                    typedPtr);

    }

    // Emit function body
    node.body->accept(*this);

    // Ensure a return is always present
    if (node.body->statements.empty() ||
        !std::dynamic_pointer_cast<Return>(node.body->statements.back())) {
        std::string llvmType = output::changeType(currentReturnTypeFunc);
        if (currentReturnTypeFunc == ast::BuiltInType::VOID) {
            cb->emit("ret void");
        } else {
            std::string defaultValue = (currentReturnTypeFunc == ast::BuiltInType::BOOL) ? "false" : "0";
            cb->emit("ret " + llvmType + " " + defaultValue);
        }
    }

    cb->emit("}");
    cb->emit("");
}
///////////////////////////////VarDecl//////////////////////////////////////////
void codeGvisitor::visit(VarDecl& node) {
    node.id->accept(*this);
    std::string llvmType = output::changeType(node.id->type);
    std::string ptrVar = cb->freshVar();

    std::string arrayOffReg = cb->freshVar();
    cb->emit(arrayOffReg + " = add i32 0, 0");



    // Always compute GEP from %local_vars, which is i32*
    cb->emit(ptrVar + " = getelementptr i32, i32* %local_vars, i32 " +
             std::to_string(node.id->offset));

    std::string finalPtr = ptrVar;


    // If we're dealing with a smaller type (like bool or byte), bitcast to the correct pointer type
    if (llvmType == "i1" || llvmType == "i8") {
        finalPtr = cb->freshVar();
        cb->emit(finalPtr + " = bitcast i32* " + ptrVar + " to " + llvmType + "*");
    }
    if (node.id->len > 1 && node.init_exp == nullptr) {
        for (int i = 0; i < node.id->len; ++i) {
            std::string slotPtr = cb->freshVar();
            cb->emit(slotPtr + " = getelementptr i32, i32* %local_vars, i32 " +
                     std::to_string(node.id->offset + i));
            if (llvmType != "i32") {
                std::string castPtr = cb->freshVar();
                cb->emit(castPtr + " = bitcast i32* " + slotPtr + " to " +
                         llvmType + "*");
                cb->emit("store " + llvmType + " 0, " + llvmType + "* " + castPtr);
            } else {
                cb->emit("store i32 0, i32* " + slotPtr);
            }
        }
    }else {
        if (node.init_exp) {
            node.init_exp->accept(*this);
            std::string initValueVar = node.init_exp->newVar;
            cb->emit(
                    "store " + llvmType + " " + initValueVar + ", " + llvmType +
                    "* " + finalPtr);
        } else {
            std::string defaultValue = (node.id->type == BuiltInType::BOOL)
                                       ? "false" : "0";
            cb->emit(
                    "store " + llvmType + " " + defaultValue + ", " + llvmType +
                    "* " + finalPtr);
        }
    }
}
///////////////////////////////Return///////////////////////////////////////////
void codeGvisitor::visit(Return& node) {
    if (currentReturnTypeFunc == BuiltInType::VOID) {
        cb->emit("ret void");
    } else {
        if(node.exp) {
            node.exp->accept(*this);
            std::string resultVar = node.exp->newVar;
            std::string llvmType = output::changeType(
                    currentReturnTypeFunc);
            cb->emit("ret " + llvmType + " " + resultVar);
        }
    }
}
///////////////////////////////While////////////////////////////////////////////
/*Here or any nested loops/blocks we need to save the Begin and Endl label so
 * we don't lose them that's why i added stacks*/
void codeGvisitor::visit(While& node) {
    // Create labels for control flow
    std::string condLabel = cb->freshLabel();     // start of condition check
    std::string bodyLabel = cb->freshLabel();     // loop body
    std::string endLabel = cb->freshLabel();      // after the loop

    beginLabels.push(bodyLabel);
    endLabels.push(endLabel);

    // Unconditional branch to condition check
    cb->emit("br label " + condLabel);
    cb->emit("");
    cb->emitLabel(condLabel);

    // Generate code for the loop condition
    node.condition->accept(*this);
    std::string condVar = node.condition->newVar;

    // br i1 %cond, label %body_label, label %end_label
    cb->emit("br i1 " + condVar + ", label " + bodyLabel + ", label " + endLabel);
    cb->emit("");

    // Emit body block
    cb->emitLabel(bodyLabel);
    node.body->accept(*this); // emit body code

    // After body, jump back to condition check
    cb->emit("br label " + condLabel);
    cb->emit("");

    // Emit loop exit label
    cb->emitLabel(endLabel);

    beginLabels.pop();
    endLabels.pop();
}
///////////////////////////////Break////////////////////////////////////////////
/*LLVM syntax: br label <end_label>*/
void codeGvisitor::visit(Break& node) {
    if(!endLabels.empty()){
        cb->emit("br label " + endLabels.top());
    }
}
////////////////////////////////Continue////////////////////////////////////////
/*LLVM syntax: br label <start_label>*/
void codeGvisitor::visit(Continue& node) {
    if(!beginLabels.empty()){
        cb->emit("br label " + beginLabels.top());
    }
}
////////////////////////////////Call////////////////////////////////////////////
/*LLVM syntax: %result = call <return_type> @function_name(<arg_type1> <arg1>, <arg_type2> <arg2>, ...)*/
void codeGvisitor::visit(Call& node) {
    // Visit arguments to evaluate expressions and get their result variables
    std::vector<std::string> argValues;
    std::vector<std::string> argTypes;
    node.args->accept(*this);

    for (int i = 0; i < node.args->exps.size(); i++) {
        const auto &arg = node.args->exps[i];
        arg->accept(*this);
        argTypes.push_back(output::changeType(arg->type));
        if (arg->type == ast::BuiltInType::STRING) {//
            string strArray = cb->emitString(arg->newVar);
            std::string ptrVar = cb->freshVar();
            int len = arg->newVar.length() + 1;
            cb->emit(ptrVar + " = getelementptr [" + std::to_string(len) + " x i8], [" +
                    std::to_string(len) + " x i8]* " + strArray + ", i32 0, i32 0");
            argValues.push_back(ptrVar);
        }
        else if (arg->type == ast::BuiltInType::BYTE &&
                 node.typesOfArgs[i] == ast::BuiltInType::INT) { //maybe should push the new type!
            std::string promotedVar = cb->freshVar();
            cb->emit(promotedVar + " = zext i8 " + arg->newVar + " to i32");
            argValues.push_back(promotedVar);
        }else{
            argValues.push_back(arg->newVar);
        }
    }



    std::string funcName = node.func_id->value;
    BuiltInType returnType = node.type;
    std::string llvmRetType = output::changeType(returnType);

    // Emit call instruction
    std::string callInstr;
    std::string resultVar;

    std::string argsJoined;

    for (size_t i = 0; i < argValues.size(); ++i) {
        argsJoined += argTypes[i] + " " + argValues[i];
        if (i < argValues.size() - 1)
            argsJoined += ", ";
    }

    if (returnType == BuiltInType::VOID) {
        cb->emit("call " + llvmRetType + " @" + funcName + "(" + argsJoined + ")");
    } else {
        resultVar = cb->freshVar();
        cb->emit(resultVar + " = call " + llvmRetType + " @" + funcName + "(" + argsJoined + ")");
        node.newVar = resultVar;
    }
}
/////////////////////////////////If/////////////////////////////////////////////
/*LLVM syntax: br i1 <cond>, label <then_label>, label <else_or_end_label>*/
void codeGvisitor::visit(If& node) {
    node.condition->accept(*this);
    std::string condVal = node.condition->newVar;

    std::string thenLabel = cb->freshLabel(); //Then label
    std::string elseL = node.otherwise ? cb->freshLabel() : "";
    std::string endLabel = cb->freshLabel();
    cb->emit("br i1 "+ condVal + ", label " + thenLabel + ", label " +
    ( elseL.empty() ? endLabel : elseL));

    cb->emitLabel(thenLabel);
    node.then->accept(*this);
    cb->emit("br label "+endLabel);

    if(node.otherwise)
    {
        cb->emit("");
        cb->emitLabel(elseL);
        node.otherwise->accept(*this);
        cb->emit("br label "+ endLabel);

    }
    cb->emitLabel(endLabel);
    cb->emit("");
}
/////////////////////////////////BinOp//////////////////////////////////////////
/*LLVM syntax: %res = add i32 %a, %b <-> a+b*/
void codeGvisitor::visit(BinOp& node) {
    // Visit both operands first
    node.left->accept(*this);
    node.right->accept(*this);

    std::string lhs = node.left->newVar;
    std::string rhs = node.right->newVar;

    BuiltInType resultType = node.type;
    std::string resultLLVMType = output::changeType(resultType);

    // Determine operation type (i32 if either operand is INT or if result is INT)
    bool needsWidening = resultType == BuiltInType::INT;
    std::string opType = needsWidening ? "i32" : "i8";

    // Widen operands to i32 only if needed
    if (needsWidening) {
        if (node.left->type == BuiltInType::BYTE) {
            std::string widenedLHS = cb->freshVar();
            cb->emit(widenedLHS + " = zext i8 " + lhs + " to i32");
            lhs = widenedLHS;
        }
        if (node.right->type == BuiltInType::BYTE) {
            std::string widenedRHS = cb->freshVar();
            cb->emit(widenedRHS + " = zext i8 " + rhs + " to i32");
            rhs = widenedRHS;
        }
    }

    std::string resultVar = cb->freshVar();
    std::string op;
    switch (node.op) {
        case BinOpType::ADD:
            op = "add";
            break;
        case BinOpType::SUB:
            op = "sub";
            break;
        case BinOpType::MUL:
            op = "mul";
            break;
        case BinOpType::DIV: {
            op = "sdiv";
            std::string DivByZeroLabel = cb->freshLabel();
            std::string ValidLabel = cb->freshLabel();
            std::string endLabel = cb->freshLabel();

            std::string isZero = cb->freshVar();
            cb->emit(isZero + " = icmp eq i32 " + rhs + ", 0");
            cb->emit("br i1 " + isZero + ", label " + DivByZeroLabel +
                     ", label " + ValidLabel);

            cb->emitLabel(DivByZeroLabel);
            std::string DivByZeroMsg = cb->emitString("Error division by zero");
            std::string DivByZeroptr = cb->freshVar();
            cb->emit(DivByZeroptr + " = getelementptr [23 x i8], [23 x i8]* " +
                     DivByZeroMsg + ", i32 0, i32 0");
            cb->emit("call void @print(i8* " + DivByZeroptr + ")");
            cb->emit("call void @exit(i32 1)");
            cb->emit("br label " +
                     ValidLabel);  // Not needed since @exit will terminate, but fine for LLVM.

            cb->emitLabel(ValidLabel);
            cb->emit(resultVar + " = sdiv i32 " + lhs + ", " + rhs);
            cb->emit("br label " + endLabel);

            cb->emitLabel(endLabel);

            node.newVar = resultVar;
            return;
        }
        default:
            return; // Unknown op; skip codegen
    }


    cb->emit(resultVar + " = " + op + " " + opType + " " + lhs + ", " + rhs);

    // Truncate result back to i8 if final result should be BYTE
    if (resultType == BuiltInType::BYTE && needsWidening) {
        std::string truncVar = cb->freshVar();
        cb->emit(truncVar + " = trunc i32 " + resultVar + " to i8");
        node.newVar = truncVar;
    } else {
        node.newVar = resultVar;
    }
}
//////////////////////////////////Statements////////////////////////////////////
void codeGvisitor::visit(Statements& node){
    for (size_t i = 0; i < node.statements.size(); ++i) {
        node.statements[i]->accept(*this);
    }
}
////////////////////////////////////////////////////////////////////////////////
void codeGvisitor::visit(Assign& node)
     {
    node.exp->accept(*this);
    node.id->accept(*this);
    std::string arrayOffReg = cb->freshVar();
    auto expNewVar = node.exp->newVar;
    cb->emit(arrayOffReg + " = add i32 0, 0");

    if(auto isArray = std::dynamic_pointer_cast<ArrayDereference>(node.exp)){
        cb->emit(arrayOffReg + " = add i32 0, " + isArray->index->newVar);
    }

   
    int off = node.id->offset; 
    std::string offPoi = cb->freshVar();
    cb->emit(offPoi + " = add i32 " + offPoi + ", " + arrayOffReg);
    cb->emit(offPoi + " = getelementptr i32, i32* %local_vars, i32 " + std::to_string(off));

  
    std::string tyoechanged = output::changeType(node.exp->type); 
    std::string castPtr = cb->freshVar();
    cb->emit(castPtr + " = bitcast i32* " + offPoi + " to " + tyoechanged + "*");

   //  cb->emit(elemPtr +
     //   " = getelementptr " + typeLL + ", " + typeLL + "* " +
     //   basePtr + ", i32 " + indexVar);
    cb->emit("store " + tyoechanged + " " + expNewVar + ", " + tyoechanged + "* " + castPtr + ", align 4");
     }
////////////////////////////////////////////////////////////////////////////////
void codeGvisitor::visit(ExpList& node) {}//done
////////////////////////////////////////////////////////////////////////////////
void codeGvisitor::visit(ast::ArrayAssign &node) {
    // Visit child nodes
    node.id->accept(*this);
    node.index->accept(*this);
    node.exp->accept(*this);

    std::string reg = cb->freshVar();
    cb->emit(reg + " = add i32 0, " + node.index->newVar);
    node.index->newVar = reg;



    std::string indexVar = node.index->newVar;

    // Widen index if it's a BYTE (needed for GEP and OOB check)
    if (node.index->type == ast::BuiltInType::BYTE) {
        std::string zextIndex = cb->freshVar();
        cb->emit(zextIndex + " = zext i8 " + indexVar + " to i32");
        indexVar = zextIndex;
    }

    // Emit out-of-bounds check and jump to continuation label
    std::string okLabel = emitOobCheck(indexVar, node.id->len);

    // DO NOT emit: cb->emit("br label " + okLabel); — emitOobCheck already ends with it

    // --- Now we are in the okLabel block ---
    // No need to emit the label again (emitOobCheck already did)

    // Handle type adjustment of RHS expression
    std::string valueVar = node.exp->newVar;
    ast::BuiltInType elemType = node.id->type;
    std::string llvmElemType = output::changeType(elemType);

    if (node.exp->type == ast::BuiltInType::BYTE && elemType == ast::BuiltInType::INT) {
        std::string widened = cb->freshVar();
        cb->emit(widened + " = zext i8 " + valueVar + " to i32");
        valueVar = widened;
    } else if (node.exp->type == ast::BuiltInType::INT && elemType == ast::BuiltInType::BYTE) {
        std::string truncated = cb->freshVar();
        cb->emit(truncated + " = trunc i32 " + valueVar + " to i8");
        valueVar = truncated;
    }

    // Access array element pointer
    int offset = node.id->offset;
    std::string basePtrI32 = cb->freshVar();
    cb->emit(basePtrI32 + " = getelementptr i32, i32* %local_vars, i32 " + std::to_string(offset));

    std::string baseTypedPtr = cb->freshVar();
    cb->emit(baseTypedPtr + " = bitcast i32* " + basePtrI32 + " to " + llvmElemType + "*");

    std::string elemPtr = cb->freshVar();
    cb->emit(elemPtr + " = getelementptr " + llvmElemType + ", " + llvmElemType + "* " + baseTypedPtr + ", i32 " + indexVar);

    // Store the value
    cb->emit("store " + llvmElemType + " " + valueVar + ", " + llvmElemType + "* " + elemPtr + ", align 4");
}
////////////////////////////////////////////////////////////////////////////////
void codeGvisitor::visit(ast::Funcs& node){
    for (std::size_t i = 0; i < node.funcs.size(); ++i){
        node.funcs[i]->accept(*this);
    }
}
////////////////////////////////////////////////////////////////////////////////
void codeGvisitor::visit(ast::PrimitiveType &node){}
////////////////////////////////////////////////////////////////////////////////
void codeGvisitor::visit(ast::Formal &node){}
////////////////////////////////////////////////////////////////////////////////
void codeGvisitor::visit(ast::Formals &node) {
    std::string argsLine;
    for (size_t i = 0; i < node.formals.size(); ++i) {
        auto &formal = node.formals[i];
        std::string argType = output::changeType(formal->id->type);
        std::string argName = "%" + formal->id->value;
        argsLine += argType + " " + argName;
        if (i < node.formals.size() - 1) {
            argsLine += ", ";
        }
    }
    cb->emit(argsLine);
}
////////////////////////////////////////////////////////////////////////////////
void codeGvisitor::visit(Num& node) {
    node.newVar = std::to_string(node.value);
}
////////////////////////////////////////////////////////////////////////////////
void codeGvisitor::visit(NumB& node) {
    node.newVar = std::to_string(node.value);
}
////////////////////////////////////////////////////////////////////////////////
void codeGvisitor::visit(String& node) {
    node.newVar = node.value;
}
////////////////////////////////////////////////////////////////////////////////
void codeGvisitor::visit(Bool& node) {
    node.newVar = node.value ? "true" : "false";
}
////////////////////////////////////////////////////////////////////////////////
void codeGvisitor::visit(ID& node)
{//today
    if (node.offset < 0) {
        node.newVar = "%" + node.value;
        return;
    }

    // Local variables (from local_vars array)
    const int offset = node.offset;
    const std::string ltype =output::changeType(node.type);


    // Compute address of the variable: %addr = getelementptr ...
    std::string inbetweenadrrs = cb->freshVar();
        cb->emit(inbetweenadrrs + " = getelementptr i32, i32* %local_vars, i32 " + std::to_string(offset));

    // Cast the pointer to the correct LLVM type: %cast = bitcast ...
    std::string cpointer = cb->freshVar();
    cb->emit(cpointer + " = bitcast i32* " + inbetweenadrrs + " to " + ltype + "*");

    // Load the value from memory: %val = load ...
    std::string newV = cb->freshVar();
    cb->emit(newV + " = load " + ltype + ", " + ltype + "* " + cpointer + ", align 4");

    // Set the result
    node.newVar = newV;
}
////////////////////////////////////////////////////////////////////////////////
void codeGvisitor::visit(RelOp& node) {
    // Evaluate both sides
    node.left->accept(*this);
    node.right->accept(*this);

    std::string leftNewVar = node.left->newVar;
    std::string rightNewVar = node.right->newVar;

    // Handle byte-to-int promotion if needed
    codeGvisitor::widenByte(leftNewVar, node.left->type);
    codeGvisitor::widenByte(rightNewVar, node.right->type);


    // Determine comparison operation
    std::string result = cb->freshVar();
    std::string op;

    switch (node.op) {
        case ast::RelOpType::EQ: op = "eq";  break;
        case ast::RelOpType::NE: op = "ne";  break;
        case ast::RelOpType::LT: op = "slt"; break;
        case ast::RelOpType::GT: op = "sgt"; break;
        case ast::RelOpType::LE: op = "sle"; break;
        case ast::RelOpType::GE: op = "sge"; break;
    }

    cb->emit(result + " = icmp " + op + " i32 " + leftNewVar + ", " + rightNewVar);

    // Emit a label after the comparison so parent expressions can reference it
    node.beginL = cb->freshLabel();
    cb->emit("br label " + node.beginL);
    cb->emit("");
    cb->emitLabel(node.beginL);

    // Store the result
    node.newVar = result;
}
////////////////////////////////////////////////////////////////////////////////
void codeGvisitor::visit(Not& node) {
    // Visit the inner expression and get its result
    node.exp->accept(*this);
    std::string innerValue = node.exp->newVar;

    // Emit XOR with true to compute logical NOT
    std::string notResult = cb->freshVar();
    cb->emit(notResult + " = xor i1 " + innerValue + ", true");

    // Try to inherit an existing control flow label from the inner expression
    if (auto innerNot = dynamic_cast<ast::Not*>(node.exp.get())) {
        node.beginL = innerNot->beginL;
    } else if (auto relExpr = dynamic_cast<ast::RelOp*>(node.exp.get())) {
        node.beginL = relExpr->beginL;
    } else {
        // If no label exists, create a new one for control flow anchoring
        std::string fallbackLabel = cb->freshLabel();
        cb->emit("br label " + fallbackLabel);
        cb->emit("");
        cb->emitLabel(fallbackLabel);
        node.beginL = fallbackLabel;
    }

    // Store the result variable
    node.newVar = notResult;
}
////////////////////////////////////////////////////////////////////////////////
void codeGvisitor::visit(And& node) {

    std::string falseLabel = cb->freshLabel();   // Short-circuit if left is false
    std::string rightLabel = cb->freshLabel();   // Evaluate right side if left is true
    std::string joinLabel = cb->freshLabel();    // Final merge point
    std::string resultVar = cb->freshVar();      // Holds result of the `and`

    // Save join label for other nodes (like nested `And` or `Or`)
    node.finishL = joinLabel;

    // Evaluate left expression
    node.left->accept(*this);
    std::string leftVar = node.left->newVar;

    // Conditional branch based on left side
    cb->emit("br i1 " + leftVar + ", label " + rightLabel + ", label " + falseLabel);

    // ---- Right side ----
    cb->emitLabel(rightLabel);
    node.right->accept(*this);
    std::string rightVar = node.right->newVar;

    // Determine the right-side originating label if needed
    std::string rightIncomingLabel = rightLabel;
    if (auto andNode = dynamic_cast<ast::And*>(node.right.get())) {
        rightIncomingLabel = andNode->finishL;
    } else if (auto notNode = dynamic_cast<ast::Not*>(node.right.get())) {
        rightIncomingLabel = notNode->beginL;
    } else if (auto relNode = dynamic_cast<ast::RelOp*>(node.right.get())) {
        rightIncomingLabel = relNode->beginL;
    }

    cb->emit("br label " + joinLabel);

    // ---- Short-circuit false ----
    cb->emitLabel(falseLabel);
    cb->emit("br label " + joinLabel);

    // ---- Join block ----
    cb->emitLabel(joinLabel);
    cb->emit(resultVar + " = phi i1 [ false, " + falseLabel + " ], [ " + rightVar + ", " + rightIncomingLabel + " ]");

    // Save result
    node.newVar = resultVar;
    cb->emit(""); // blank line for clarity
}
////////////////////////////////////////////////////////////////////////////////
void codeGvisitor::visit(Or& node) {
    std::string evalRightLabel = cb->freshLabel();  // where to go if left is false
    std::string shortCircuitLabel = cb->freshLabel(); // if left is true (shortcut)
    std::string joinLabel = cb->freshLabel(); // merge point
    std::string resultVar = cb->freshVar();

    node.finishL = joinLabel;

    // Evaluate left side
    node.left->accept(*this);
    std::string leftVar = node.left->newVar;

    // If left is true, short-circuit to result true
    cb->emit("br i1 " + leftVar + ", label " + shortCircuitLabel + ", label " + evalRightLabel);

    // ---- Right-hand side ----
    cb->emitLabel(evalRightLabel);
    node.right->accept(*this);
    std::string rightVar = node.right->newVar;

    // Determine the label where right-side value comes from (for phi)
    std::string rightLabel = evalRightLabel;
    if (auto n = dynamic_cast<ast::Or*>(node.right.get())) {
        rightLabel = n->finishL;
    } else if (auto n = dynamic_cast<ast::Not*>(node.right.get())) {
        rightLabel = n->beginL;
    } else if (auto n = dynamic_cast<ast::RelOp*>(node.right.get())) {
        rightLabel = n->beginL;
    } else if (auto n = dynamic_cast<ast::And*>(node.right.get())) {
        rightLabel = n->finishL;
    }

    // After right-side, jump to join
    cb->emit("br label " + joinLabel);

    // ---- Short-circuit true ----
    cb->emitLabel(shortCircuitLabel);
    cb->emit("br label " + joinLabel);

    // ---- Join block ----
    cb->emitLabel(joinLabel);
    cb->emit(resultVar + " = phi i1 [ true, " + shortCircuitLabel + " ], [ " + rightVar + ", " + rightLabel + " ]");

    node.newVar = resultVar;
    cb->emit(""); // spacing
}
////////////////////////////////////////////////////////////////////////////////
void codeGvisitor::visit(ArrayDereference& node) {
    node.id->accept(*this);
    node.index->accept(*this);

    std::string indexVar = node.index->newVar;
    codeGvisitor::widenByte(indexVar, node.index->type);

    auto len = node.id->len;
    std::string okLabel = emitOobCheck(indexVar, len);  // Already emits okLabel at end
    printRegister("{index is }" , {indexVar});
    int offset = node.id->offset;
    std::string basePtrRaw = cb->freshVar();
    cb->emit(basePtrRaw + " = getelementptr i32, i32* %local_vars, i32 " + std::to_string(offset));

    auto elemType = node.id->type;
    std::string llvmElemType = output::changeType(elemType);

    std::string basePtr = cb->freshVar();
    cb->emit(basePtr + " = bitcast i32* " + basePtrRaw + " to " + llvmElemType + "*");

    std::string elemPtr = cb->freshVar();
    cb->emit(elemPtr + " = getelementptr " + llvmElemType + ", " + llvmElemType + "* " + basePtr + ", i32 " + indexVar);

    std::string loaded = cb->freshVar();
    cb->emit(loaded + " = load " + llvmElemType + ", " + llvmElemType + "* " + elemPtr + ", align 4");

    node.newVar = loaded;
}
////////////////////////////////////////////////////////////////////////////////
void codeGvisitor::visit(Cast& node)
    {
          // Visit the expression to generate its code
    node.exp->accept(*this);
    std::string expNewVar = node.exp->newVar;

    
    auto whatWeHave = node.exp->type;
    auto whatWeWant = node.target_type->type;

   
    std::string toCas = cb->freshVar();

    if (whatWeHave == ast::BuiltInType::INT && whatWeWant == ast::BuiltInType::BYTE) {
        // Truncate int to byte
        cb->emit(toCas + " = trunc i32 " + expNewVar + " to i8");
    }  
     else if (whatWeHave == ast::BuiltInType::BYTE && whatWeWant == ast::BuiltInType::INT) {
        // Zero extend byte to int
        cb->emit(toCas + " = zext i8 " + expNewVar + " to i32");}
    
     else if (whatWeHave == whatWeWant) {
   
        toCas = expNewVar;
    } else {
      //do we need an error 
       // throw std::runtime_error("Invalid cast from " + output::changeType(whatWeHave) + " to " + output::changeType(whatWeWant));
    }

   
    node.newVar = toCas;
    }//today
   //codeGvisitor::visit(ast::PrimitiveType& node) {}
   /*-----------------------------------------------------------------
 *  Helper: emit a run-time “out-of-bounds” check for an array index.
 *  If 0 ≤ idxVar < length  — fall through to the label it returns.
 *  Otherwise it prints "Error out of bounds", calls exit(1)
 *  and ends the control-flow with 'unreachable'.
 *
 *  Parameters:
 *      idxVar  – SSA name (i32) of the index
 *      length  – compile-time constant length of the array
 *
 *  Returns:
 *      std::string okLabel  – the label you should jump to
 *                             (function already opens it for you)
 *----------------------------------------------------------------*/


  void codeGvisitor::widenByte(std::string& valss, ast::BuiltInType typee)
{
    if (typee == ast::BuiltInType::BYTE) {
        std::string zs = cb->freshVar();
        cb->emit(zs + " = zext i8 " + valss + " to i32");
        valss = zs;                     
    }
}
std::string codeGvisitor::emitOobCheck(const std::string& idxVar,
                                       int length)
{
    std::string okLabel  = cb->freshLabel();
    std::string errLabel = cb->freshLabel();

    // Compare index < 0
    std::string isNeg = cb->freshVar();
    cb->emit(isNeg + " = icmp slt i32 " + idxVar + ", 0");

    // Compare index < length
    std::string inRange = cb->freshVar();
    cb->emit(inRange + " = icmp slt i32 " + idxVar + ", " + std::to_string(length));

    // !isNeg
    std::string notNeg = cb->freshVar();
    cb->emit(notNeg + " = xor i1 " + isNeg + ", true");

    // ok = !isNeg && inRange
    std::string ok = cb->freshVar();
    cb->emit(ok + " = and i1 " + inRange + ", " + notNeg);

    // Branch
    cb->emit("br i1 " + ok + ", label " + okLabel + ", label " + errLabel);

    // ----- Error block -----
    cb->emitLabel(errLabel);
    std::string msgPtr = cb->freshVar();
    cb->emit(msgPtr + " = getelementptr [20 x i8], [20 x i8]* @.oob_str, i32 0, i32 0");
    cb->emit("call void @print(i8* " + msgPtr + ")");


    cb->emit("call void @exit(i32 1)");
    cb->emit("unreachable");

    // ----- Continue here if check passed -----
    cb->emitLabel(okLabel);
    return okLabel;
}

std::string codeGvisitor::printRegister(std::string msg, std::vector<std::string> regs) {
    std::string tmp = cb->freshVar();
    cb->emit(tmp + " = getelementptr [" + std::to_string(msg.size()) + " x i8], [" + std::to_string(msg.size()) + " x i8]* @" + msg + ", i32 0, i32 0");
    cb->emit("call void @print(i8* " + tmp + ")");

    for (const auto& reg : regs) {
        cb->emit("call void @printi(i32 " + reg + ")");
    }
}

