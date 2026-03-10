 %{

#include "ast/nodes.hpp"
#include "codegen/output.hpp"
#include <iostream>
// bison declarations
extern int yylineno;
extern int yylex();

void yyerror(const char*);

// root of the AST, set by the parser and used by other parts of the compiler
std::shared_ptr<ast::Node> program;

using namespace std;

// TODO: Place any additional declarations here
%}

// TODO: Define tokens here
%token VOID INT BYTE BOOL TRUE FALSE RETURN WHILE BREAK CONTINUE SC
%token COMMA  RPAREN LBRACE RBRACE RBRACK ASSIGN 
%token ID NUM NUM_B STRING
// TODO: Define precedence and associativity here


%left OR
%left AND
%left LT GT LET GET
%left EQEQ NOTEQ
%left ADD SUB
%left MUL DIV
%right NOT
%left LPAREN LBRACK
%right IF
%right ELSE


%%

// While reducing the start variable, set the root of the AST
Program:  Funcs { program = dynamic_pointer_cast<ast::Funcs>($1); }
;

// TODO: Define grammar here
Funcs: {$$=std::make_shared<ast::Funcs>();}
|FuncDecl Funcs { auto allfuncs = std::dynamic_pointer_cast<ast::Funcs>($2);
         allfuncs->push_front(std::dynamic_pointer_cast<ast::FuncDecl>($1));
         $$ = allfuncs; };

FuncDecl: RetType ID LPAREN Formals RPAREN LBRACE Statements RBRACE
{
     $$ = std::make_shared<ast::FuncDecl>(
        std::dynamic_pointer_cast<ast::ID>($2),
        std::dynamic_pointer_cast<ast::PrimitiveType>($1),
        std::dynamic_pointer_cast<ast::Formals>($4),
        std::dynamic_pointer_cast<ast::Statements>($7));
};
RetType:
       Type {  std::dynamic_pointer_cast<ast::PrimitiveType>($1);}
       | VOID{ $$ = std::make_shared<ast::PrimitiveType>(ast::BuiltInType::VOID);}
       ;
Formals: { $$ = std::make_shared<ast::Formals>(); }
       | FormalsList { $$ = $1; }
       ;
FormalsList:  FormalDecl { $$ = std::make_shared<ast::Formals>(std::dynamic_pointer_cast<ast::Formal>($1)); }
           | FormalDecl COMMA FormalsList { 
               auto allFor = std::dynamic_pointer_cast<ast::Formals>($3);
               allFor->push_front(std::dynamic_pointer_cast<ast::Formal>($1));
               $$ = allFor;
           }
           ; 
 FormalDecl: Type ID { $$ = std::make_shared<ast::Formal>(
    std::dynamic_pointer_cast<ast::ID>($2),
    std::dynamic_pointer_cast<ast::PrimitiveType>($1)
); }     
Statements: Statement { $$ = std::make_shared<ast::Statements>(std::dynamic_pointer_cast<ast::Statement>($1)); }
          | Statements Statement { 
              auto allSt = std::dynamic_pointer_cast<ast::Statements>($1);
              allSt->push_back(std::dynamic_pointer_cast<ast::Statement>($2));
              $$ = allSt;
          }
          ;
Statement: LBRACE Statements RBRACE { $$ = $2; }
         | Type ID SC { $$ = std::make_shared<ast::VarDecl>(
             std::dynamic_pointer_cast<ast::ID>($2),
             std::dynamic_pointer_cast<ast::PrimitiveType>($1)
         ); }
         | Type ID ASSIGN Exp SC { $$ = std::make_shared<ast::VarDecl>(
             std::dynamic_pointer_cast<ast::ID>($2),
             std::dynamic_pointer_cast<ast::PrimitiveType>($1),
             std::dynamic_pointer_cast<ast::Exp>($4)
         ); }
         | ID ASSIGN Exp SC { $$ = std::make_shared<ast::Assign>(
             std::dynamic_pointer_cast<ast::ID>($1),
             std::dynamic_pointer_cast<ast::Exp>($3)
         ); }
         | ID LBRACK Exp RBRACK ASSIGN Exp SC {$$=std::make_shared<ast::ArrayAssign>(
            std::dynamic_pointer_cast<ast::ID>($1),
            std::dynamic_pointer_cast<ast::Exp>($6),
            std::dynamic_pointer_cast<ast::Exp>($3));} // dont know if here should be id like the test 
         |  Type ID LBRACK Exp RBRACK SC {
           ast::BuiltInType bit=std::dynamic_pointer_cast<ast::PrimitiveType>($1)->type;
           auto arraytoype=std::make_shared<ast::ArrayType>(bit,std::dynamic_pointer_cast<ast::Exp>($4));
           $$=std::make_shared<ast::VarDecl>(std::dynamic_pointer_cast<ast::ID>($2),arraytoype);

         }
         | Call SC { $$ = $1; }
         | RETURN SC { $$ = std::make_shared<ast::Return>(); }
         | RETURN Exp SC { $$ = std::make_shared<ast::Return>(
             std::dynamic_pointer_cast<ast::Exp>($2)
         ); }
         | IF LPAREN Exp RPAREN Statement %prec IF{ $$ = std::make_shared<ast::If>(
             std::dynamic_pointer_cast<ast::Exp>($3),
             std::dynamic_pointer_cast<ast::Statement>($5)
         ); }
         | IF LPAREN Exp RPAREN Statement ELSE Statement { $$ = std::make_shared<ast::If>(
             std::dynamic_pointer_cast<ast::Exp>($3),
             std::dynamic_pointer_cast<ast::Statement>($5),
             std::dynamic_pointer_cast<ast::Statement>($7)
         ); }
         | WHILE LPAREN Exp RPAREN Statement { $$ = std::make_shared<ast::While>(
             std::dynamic_pointer_cast<ast::Exp>($3),
             std::dynamic_pointer_cast<ast::Statement>($5)
         ); }
         | BREAK SC { $$ = std::make_shared<ast::Break>(); }
         | CONTINUE SC { $$ = std::make_shared<ast::Continue>(); }
         ;
Call: ID LPAREN ExpList RPAREN { $$ = std::make_shared<ast::Call>(
    std::dynamic_pointer_cast<ast::ID>($1),
    std::dynamic_pointer_cast<ast::ExpList>($3)
); }
    | ID LPAREN RPAREN { $$ = std::make_shared<ast::Call>(
    std::dynamic_pointer_cast<ast::ID>($1)
); }
    ;

ExpList: Exp { $$ = std::make_shared<ast::ExpList>(std::dynamic_pointer_cast<ast::Exp>($1)); }
       | Exp COMMA ExpList { 
           auto allExps = std::dynamic_pointer_cast<ast::ExpList>($3);
           allExps->push_front(std::dynamic_pointer_cast<ast::Exp>($1));
           $$ = allExps;
       }
       ;
Type: INT { $$ = std::make_shared<ast::PrimitiveType>(ast::BuiltInType::INT);}
    | BYTE { $$ = std::make_shared<ast::PrimitiveType>(ast::BuiltInType::BYTE); }
    | BOOL { $$ = std::make_shared<ast::PrimitiveType>(ast::BuiltInType::BOOL); }
    ;
Exp: LPAREN Exp RPAREN { $$ = $2; }
    | ID LBRACK Exp RBRACK{ $$ = std::make_shared<ast::ArrayDereference>(
        std::dynamic_pointer_cast<ast::ID>($1),
        std::dynamic_pointer_cast<ast::Exp>($3)
    );}
    | Exp DIV Exp{$$= std::make_shared<ast::BinOp>(
        std::dynamic_pointer_cast<ast::Exp>($1),
        std::dynamic_pointer_cast<ast::Exp>($3),
        ast::BinOpType::DIV
    );}
     | Exp MUL Exp{$$= std::make_shared<ast::BinOp>(
        std::dynamic_pointer_cast<ast::Exp>($1),
        std::dynamic_pointer_cast<ast::Exp>($3),
        ast::BinOpType::MUL
    );}
     | Exp SUB Exp{$$= std::make_shared<ast::BinOp>(
        std::dynamic_pointer_cast<ast::Exp>($1),
        std::dynamic_pointer_cast<ast::Exp>($3),
        ast::BinOpType::SUB
    );}
     | Exp ADD Exp{$$= std::make_shared<ast::BinOp>(
        std::dynamic_pointer_cast<ast::Exp>($1),
        std::dynamic_pointer_cast<ast::Exp>($3),
        ast::BinOpType::ADD
    );} 
    | ID { $$ = $1; }
    | Call { $$ = $1; }
    | NUM { $$ = $1; }
    | NUM_B { $$ = $1; }
    | STRING { $$ = $1; }
    | TRUE { $$ = std::make_shared<ast::Bool>(true); }
    | FALSE { $$ = std::make_shared<ast::Bool>(false); }

| NOT Exp { $$ = std::make_shared<ast::Not>(
        std::dynamic_pointer_cast<ast::Exp>($2)
    ); }
    | Exp AND Exp { $$ = std::make_shared<ast::And>(
        std::dynamic_pointer_cast<ast::Exp>($1),
        std::dynamic_pointer_cast<ast::Exp>($3)
    ); }
      | Exp OR Exp { $$ = std::make_shared<ast::Or>(
        std::dynamic_pointer_cast<ast::Exp>($1),
        std::dynamic_pointer_cast<ast::Exp>($3)
    ); }
     
    | Exp EQEQ Exp { $$ = std::make_shared<ast::RelOp>(
        std::dynamic_pointer_cast<ast::Exp>($1),
        std::dynamic_pointer_cast<ast::Exp>($3),
        ast::RelOpType::EQ
    ); }
     | Exp NOTEQ Exp { $$ = std::make_shared<ast::RelOp>(
        std::dynamic_pointer_cast<ast::Exp>($1),
        std::dynamic_pointer_cast<ast::Exp>($3),
        ast::RelOpType::NE
    ); }
     | Exp GET Exp { $$ = std::make_shared<ast::RelOp>(
        std::dynamic_pointer_cast<ast::Exp>($1),
        std::dynamic_pointer_cast<ast::Exp>($3),
        ast::RelOpType::GE
    ); }
     | Exp GT Exp { $$ = std::make_shared<ast::RelOp>(
        std::dynamic_pointer_cast<ast::Exp>($1),
        std::dynamic_pointer_cast<ast::Exp>($3),
        ast::RelOpType::GT
    ); }
    | Exp LET Exp { $$ = std::make_shared<ast::RelOp>(
        std::dynamic_pointer_cast<ast::Exp>($1),
        std::dynamic_pointer_cast<ast::Exp>($3),
        ast::RelOpType::LE
    ); }
     | Exp LT Exp { $$ = std::make_shared<ast::RelOp>(
        std::dynamic_pointer_cast<ast::Exp>($1),
        std::dynamic_pointer_cast<ast::Exp>($3),
        ast::RelOpType::LT
    ); }
    | LPAREN Type RPAREN Exp %prec LPAREN { $$ = std::make_shared<ast::Cast>(
        std::dynamic_pointer_cast<ast::Exp>($4),
        std::dynamic_pointer_cast<ast::PrimitiveType>($2)
    );  };
//tzbeet el trteeb el klaleem
// + el error tzbeet






     

%%

// TODO: Place any additional code here