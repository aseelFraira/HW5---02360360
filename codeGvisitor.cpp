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

    // Allocate unified local variable space using precomputed offset
    if (node.offset > 0) {
        cb->emit("%local_vars = alloca i32, i32 " + std::to_string(node.offset));
    }

    // Emit code for the function body
    node.body->accept(*this);

    // we need to check if it has a return statment
    // Check if the last statement is NOT a return
    if (node.body->statements.empty() ||
        !std::dynamic_pointer_cast<Return>(node.body->statements.back())) {

        ast::BuiltInType returnType = returnTypeNode->type;
        std::string llvmType = output::changeType(returnType);

        if (returnType == ast::BuiltInType::VOID) {
            cb->emit("ret void");
        } else {
            std::string defaultValue = (returnType == ast::BuiltInType::BOOL) ? "false" : "0";
            cb->emit("ret " + llvmType + " " + defaultValue);
        }
    }

    cb->emit("}");
    cb->emit("");

}
///////////////////////////////VarDecl//////////////////////////////////////////
void codeGvisitor::visit(VarDecl& node) {
    std::string llvmType = output::changeType(node.id->type);

    std::string ptrVar = cb->freshVar();
    cb->emit(ptrVar + " = getelementptr i32, i32* %local_vars, i32 " +
    std::to_string(node.id->offset));

    // Handle initialization
    if (node.init_exp) {
        // Visit the initializer expression and get the result value
        node.init_exp->accept(*this);
        std::string initValueVar = node.init_exp->newVar;
        cb->emit("store " + llvmType + " " + initValueVar + ", " + llvmType + "* " + ptrVar);
    } else {
        // No initializer — emit default value based on type
        if (node.id->type == BuiltInType::INT || node.id->type == BuiltInType::BYTE) {
            cb->emit("store " + llvmType + " 0, " + llvmType + "* " + ptrVar);
        } else if (node.id->type == BuiltInType::BOOL) {
            cb->emit("store " + llvmType + " false, " + llvmType + "* " + ptrVar);
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
    for (int i = 0; i < node.args->exps.size(); i++) {
        node.args->exps[i]->accept(*this);
        auto arg = node.args->exps[i];
        arg->accept(*this);
        std::cout << "{DEBUG} 1" << std::endl;

        if (arg->type == ast::BuiltInType::STRING) {//
            cb->emitString(arg->newVar);
            std::string ptrVar = cb->freshVar();//
            std::cout << "{DEBUG} 2" << std::endl;


            int len = arg->newVar.length() + 1;
            cb->emit(ptrVar + " = getelementptr [" + std::to_string(len) + " x i8], [" +
                    std::to_string(len) + " x i8]* " + arg->newVar + ", i32 0, i32 0");
            argValues.push_back(ptrVar);

        }
        else if (arg->type == ast::BuiltInType::BYTE &&
                 node.typesOfArgs[i] == ast::BuiltInType::INT) {
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
    // Visit both sides of the expression to compute their values
    node.left->accept(*this);
    node.right->accept(*this);

    std::string lhs = node.left->newVar;
    std::string rhs = node.right->newVar;

    std::string resultVar = cb->freshVar();

    std::string llvmType = output::changeType(node.type);
    std::string op;


    switch (node.op) {
        case BinOpType::ADD:  op = "add";  break;
        case BinOpType::SUB:  op = "sub";  break;
        case BinOpType::MUL:  op = "mul";  break;
        case BinOpType::DIV:  op = "sdiv"; break;

        default:
           // output::errorOpMismatch(node.line);
            return;
    }

    cb->emit(resultVar + " = " + op + " " + llvmType + " " + lhs + ", " + rhs);
    node.newVar = resultVar;
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
    auto expNewVar = node.exp->newVar;

   
    int off = node.id->offset; 
    std::string offPoi = cb->freshVar();
    cb->emit(offPoi + " = getelementptr i32, i32* %local_vars, i32 " + std::to_string(off));

  
    std::string tyoechanged = output::changeType(node.exp->type); 
    std::string castPtr = cb->freshVar();
    cb->emit(castPtr + " = bitcast i32* " + offPoi + " to " + tyoechanged + "*");

   //  cb->emit(elemPtr +
     //   " = getelementptr " + typeLL + ", " + typeLL + "* " +
     //   basePtr + ", i32 " + indexVar);
    cb->emit("store " + tyoechanged + " " + expNewVar + ", " + tyoechanged + "* " + castPtr + ", align 4");
     }

    void codeGvisitor::visit(ExpList& node) {}//done
    void codeGvisitor::visit(ast::ArrayAssign &node) {
node.id->accept(*this);
    node.index->accept(*this);
    node.exp->accept(*this);// accepting everything 
     std::string indexVar = node.index->newVar;
auto len =(node.id->len);
 emitOobCheck(indexVar, len);


codeGvisitor::widenByte(indexVar, node.index->type);


//TO DO : RE DO THE LENGTH IN THE ARRAY SO THE EMIT OOB FUNCTION COULD FUNCTION NORMALLY 
//   if (node.index->type == ast::BuiltInType::BYTE) {
//         std::string z = cb->freshVar();
//         cb->emit(z + " = zext i8 " + indexVar + " to i32");
//         indexVar = z;
//     }

    std::string expNewVar = node.exp->newVar;
     int baseoff = node.id->offset;
     std::string newbasePtrI32 = cb->freshVar();
        cb->emit(newbasePtrI32 +
        " = getelementptr i32, i32* %local_vars, i32 " +
        std::to_string(baseoff));
   
    auto arrType = node.id->type;
   

    std::string typeLL = output::changeType(arrType);

    std::string basePtr = cb->freshVar();
    cb->emit(basePtr +
        " = bitcast i32* " + newbasePtrI32 + " to " + typeLL + "*");

   
    std::string elemPtr = cb->freshVar();
    cb->emit(elemPtr +
        " = getelementptr " + typeLL + ", " + typeLL + "* " +
        basePtr + ", i32 " + indexVar);

  
    if (node.exp->type == ast::BuiltInType::BYTE &&
        arrType == ast::BuiltInType::INT) {
        std::string rhsExt = cb->freshVar();
        cb->emit(rhsExt + " = zext i8 " + expNewVar + " to i32");
        expNewVar = rhsExt;
    } else if (node.exp->type == ast::BuiltInType::INT &&
               arrType == ast::BuiltInType::BYTE) {
        std::string rhsTrunc = cb->freshVar();
        cb->emit(rhsTrunc + " = trunc i32 " + expNewVar + " to i8");
        expNewVar = rhsTrunc;
    }

   
    cb->emit("store " + typeLL + " " + expNewVar + ", " +
                    typeLL + "* " + elemPtr + ", align 4");



//     idx  = code for index-expression            ; possibly zext to i32
// rhs  = code for right-hand side expression  ; widen/trunc if needed

// base = getelementptr i32, i32* %local_vars, i32 <arrayOffset>
// elemPtr = bitcast i32* base to <elemTy>*      ; if you keep %local_vars
// elemPtr = getelementptr <elemTy>, <elemTy>* elemPtr, i32 idx   ; ← ①
// store <elemTy> rhs, <elemTy>* elemPtr, align 4                 ; ← ②


    }//today
    void codeGvisitor::visit(ast::Funcs& node)
     {for (std::size_t i = 0; i < node.funcs.size(); ++i) {
    node.funcs[i]->accept(*this);
}}
    void codeGvisitor::visit(ast::PrimitiveType &node){}//done
    void codeGvisitor::visit(ast::Formal &node) {
          std::string funcp = output::changeType(node.id->type) + " %" + node.id->value;
   cb->emit(funcp);
    }//today
    void codeGvisitor::visit(ast::Formals &node) { for (size_t i = 0; i < node.formals.size(); ++i) {
        node.formals[i]->accept(*this);
        if (i != node.formals.size() - 1) {
           cb->emit(", ");
        }
    }}

    void codeGvisitor::visit(Num& node) {
            node.newVar=std::to_string(node.value);
    }
    void codeGvisitor::visit(NumB& node)
     {node.newVar=std::to_string(node.value);}
    void codeGvisitor::visit(String& node)
     {
        node.newVar=(node.value);
     }
     void codeGvisitor::visit(Bool& node)
    {
         if(node.value==true){
             node.newVar="true";
         }
    else{
        node.newVar="false";
    }
    }
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

    void codeGvisitor::visit(RelOp& node) {
 node.left->accept(*this);
 node.right->accept(*this);
 std::string leftNewVar=node.left->newVar;
 std::string rightNewVar=node.right->newVar;

 //did w ch an 
 
    codeGvisitor::widenByte(leftNewVar, node.left ->type);
    codeGvisitor::widenByte(rightNewVar, node.right->type);
   std::string result= cb->freshVar();
    std::string op;
    switch (node.op) {
    case ast::RelOpType::EQ: op = "eq";  break;
   
    case ast::RelOpType::LT: op = "slt"; break;
    case ast::RelOpType::GT: op = "sgt"; break;
    case ast::RelOpType::LE: op = "sle"; break;
    case ast::RelOpType::NE: op = "ne";  break;
    case ast::RelOpType::GE: op = "sge"; break;
    }
    cb->emit(result + " = icmp " + op + " i32 " + leftNewVar + ", " + rightNewVar);
    node.newVar = result;
    }//TODAY
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

void codeGvisitor::visit(And& node) {
    std::string e5tesarL = cb->freshLabel();
    std::string rightSideLabel = cb->freshLabel();
   
 
    std::string finishVar = cb->freshVar();
       std::string finishL = cb->freshLabel();
    node.finishL = finishL;
   
    node.left->accept(*this);
      node.right->accept(*this);
    std::string LeftSideLabel = node.left->newVar;
  //we dont need to emit the leftside label because its in newVar leftside 

   
    cb->emit("br i1 " + LeftSideLabel + ", label " + rightSideLabel + ", label " + e5tesarL);

    
 
   
  
  cb->emitLabel(rightSideLabel);
    // mnf7s shu hene el next label 7sb shu el jump ele b3du w mn3mlha mhu 7sb kman shu nu3 el next label ta3na 
    if (auto n = dynamic_cast<ast::Or*>(node.right.get())) {
        rightSideLabel = n->finishL;
    } 
    else if (auto n = dynamic_cast<ast::Not*>(node.right.get())) {
      rightSideLabel = n->beginL;}
    else if (auto n = dynamic_cast<ast::RelOp*>(node.right.get())) {
        rightSideLabel = n->beginL;
    } 
    else if (auto n = dynamic_cast<ast::And*>(node.right.get())) {
        rightSideLabel = n->finishL;
    } 
   
    
    
    
    //i dont think we need this 
    // else if (auto funcNode = dynamic_cast<ast::Call*>(node.right.get())) {
    //     rightSideLabel = funcNode->beginL;
    // }

    std::string secondnewVar = node.right->newVar;
    cb->emit("br label " + finishL);


    //e5tesar
    cb->emitLabel(e5tesarL); 
    cb->emit("br label " + finishL);
   
    
    // phi and join
    cb->emitLabel(finishL);
    cb->emit(finishVar + " = phi i1 [ false, " + e5tesarL + " ], [ " + secondnewVar + ", " + rightSideLabel + " ]");
    
  
    node.newVar = finishVar;
  cb->emit("");
  
  
  
  
  
  
  
  
  /**
    std::string rightSideLabel= cb->freshLabel();
    std::string fLab = cb->freshLabel();
    std::string finishL= cb->freshLabel();
    std::string finishedVar= cb->freshVar();


    node.left->accept(*this);
  
    cb->emit("br i1 " + node.left->newVar +
                    ", label " + rightSideLabel +
                    ", label " + fLab);//


    //this needs an icmp before it that the is_even is true or false
// br i1 %is_even, label %even_label, label %odd_label
    node.right->accept(*this);
   cb->emitLabel(rightSideLabel);
   
  
    if (auto n = dynamic_cast<ast::Or*>(node.right.get()))
       { rightSideLabel = n->finishL;}
    else if (auto n = dynamic_cast<ast::And*>(node.right.get()))
      {  rightSideLabel = n->finishL;}
    else if (auto rel = dynamic_cast<ast::RelOp*>(node.right.get()))
       { rightSideLabel = rel->beginL;}
    else if (auto n = dynamic_cast<ast::Not*>(node.right.get()))
       { rightSideLabel = n->beginL;}
    else if (auto c = dynamic_cast<ast::Call*>(node.right.get()))
       { rightSideLabel = c->beginL;}

    cb->emit("br label " + finishL);        

   // false short-circuit
    cb->emitLabel(fLab);
    cb->emit("br label " + finishL);

    // join 
    cb->emitLabel(finishL);
    cb->emit(finishedVar +
        " = phi i1 [ 0, " + fLab + " ], [ " +
                    node.right->newVar + ", " + rightSideLabel + " ]");

    node.newVar = finishedVar;**/
    }
    void codeGvisitor::visit(Or& node) {
        //same thing as and but we alter the short circut evaluation 
    std::string rightSideLabel = cb->freshLabel();
    std::string e5tesarL = cb->freshLabel();
 
  
       std::string finishL = cb->freshLabel();
    node.finishL = finishL;
    // Generate code for first operand
    node.left->accept(*this);
     node.right->accept(*this);
    std::string leftsideNewVar = node.left->newVar;
  

    cb->emit("br i1 " + leftsideNewVar + ", label " + e5tesarL + ", label " + rightSideLabel);

    
 
   
  
    
   if (auto n = dynamic_cast<ast::Or*>(node.right.get())) {
        rightSideLabel = n->finishL;
    } 
    else if (auto n = dynamic_cast<ast::Not*>(node.right.get())) {
      rightSideLabel = n->beginL;}
    else if (auto n = dynamic_cast<ast::RelOp*>(node.right.get())) {
        rightSideLabel = n->beginL;
    } 
    else if (auto n = dynamic_cast<ast::And*>(node.right.get())) {
        rightSideLabel = n->finishL;
    } 
     cb->emitLabel(rightSideLabel);
    //I dont know if we need this 
    // } else if (auto funcNode = dynamic_cast<ast::Call*>(node.right.get())) {
    //     rightSideLabel = funcNode->beginL;
    // }

   
    cb->emit("br label " + finishL);



    cb->emitLabel(e5tesarL);
    cb->emit("br label " + finishL);
 
    std::string finishnewVar = cb->freshVar();
    std::string seconnewVar = node.right->newVar;
    cb->emitLabel(finishL);
    cb->emit(finishnewVar + " = phi i1 [ true, " + e5tesarL + " ], [ " + seconnewVar + ", " + rightSideLabel + " ]");
    
   
    node.newVar = finishnewVar;
    cb->emit("");


    }
void codeGvisitor::visit(ArrayDereference& node) {
    node.id->accept(*this);
  auto type=node.id->type;
  auto changedType=output::changeType(type);
    node.index->accept(*this);
    std::string indexVar=node.index->newVar;
     codeGvisitor::widenByte(indexVar, node.index->type);
//     if (node.index->type == ast::BuiltInType::BYTE) {           
//     std::string z = cb->freshVar();                           
//     cb->emit(z + " = zext i8 " + indexVar + " to i32");         
//     indexVar = z;                                               
// }
auto len =(node.id->len);
 emitOobCheck(indexVar, len);
    int offset=node.id->offset;
std::string getElement=cb->freshVar();
cb->emit( getElement+" = getelementptr i32, i32* %local_vars, i32 " +
std::to_string(offset));
 /* ---------- 3.  Bit-cast slot pointer to the real element type ---------- */
    std::string basePtr = cb->freshVar();                            
    cb->emit(basePtr + " = bitcast i32* " + getElement +           
             " to " + changedType + "*");                           
     //std::string indexVar = node.index->newVar;
     std::string elemPtr = cb->freshVar();

   
    cb->emit(elemPtr + " = getelementptr " + changedType + ", " +
             changedType + "* " + basePtr + ", i32 " + indexVar);

   
    // std::string label=cb->freshVar();
    // cb->emit(label+" = load "+changedType+", "+changedType+"* "+elemPtr);
  std::string label = cb->freshVar();
    cb->emit(label + " = load " + changedType + ", " + changedType +
             "* " + elemPtr + ", align 4");   

node.newVar=label;
// %print_ptr = getelementptr i32, i32* %arr, i32 %print_index   ; ① compute address
// %element   = load i32, i32* %print_ptr                        ; ② fetch value

//  cb->emit(elemPtr +
     //   " = getelementptr " + typeLL + ", " + typeLL + "* " +
     //   basePtr + ", i32 " + indexVar);


    }//today
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

    /* idx < 0 ? */
    std::string isNeg = cb->freshVar();
    cb->emit(isNeg + " = icmp slt i32 " + idxVar + ", 0");

    /* idx < length ? */
    std::string inRange = cb->freshVar();
    cb->emit(inRange + " = icmp slt i32 " + idxVar + ", "
             + std::to_string(length));

    /* ok = !isNeg  &&  inRange */
    std::string ok = cb->freshVar();
    cb->emit(ok + " = and i1 " + inRange + ", xor i1 " + isNeg + ", true");

    /* conditional branch */
    cb->emit("br i1 " + ok + ", label " + okLabel + ", label " + errLabel);

    /* -------- error block -------- */
    cb->emitLabel(errLabel);
    std::string msgPtr = cb->freshVar();
    cb->emit(msgPtr +
        " = getelementptr [19 x i8], [20 x i8]* @.oob_str, i32 0, i32 0");
    cb->emit("call void @print(i8* " + msgPtr + ")");
    cb->emit("call void @exit(i32 1)");
    cb->emit("unreachable");

    /* -------- ok block opens here -------- */
    cb->emitLabel(okLabel);
    return okLabel;                 // caller continues right after this label
}


