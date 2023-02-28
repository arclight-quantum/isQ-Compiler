{
module ISQ.Lang.ISQv2Parser where

import ISQ.Lang.ISQv2Tokenizer
import ISQ.Lang.ISQv2Grammar
import Data.Maybe (catMaybes)
import Control.Exception (throw, Exception)

}
%name isqv2
%tokentype { ISQv2Token }
%error { parseError }

%token 
    if { TokenReservedId $$ "if" }
    else { TokenReservedId $$ "else" }
    for { TokenReservedId $$ "for" }
    in { TokenReservedId $$ "in" }
    while { TokenReservedId $$ "while" }
    procedure { TokenReservedId $$ "procedure" }
    int { TokenReservedId $$ "int" }
    qbit { TokenReservedId $$ "qbit" }
    M { TokenReservedId $$ "M" }
    print { TokenReservedId $$ "print" }
    defgate { TokenReservedId $$ "defgate" }
    pass { TokenReservedId $$ "pass" }
    bp { TokenReservedId $$ "bp" }
    return { TokenReservedId $$ "return" }
    package { TokenReservedId $$ "package" }
    import { TokenReservedId $$ "import" }
    ctrl { TokenReservedId $$ "ctrl" }
    nctrl { TokenReservedId $$ "nctrl" }
    inv { TokenReservedId $$ "inv" }
    bool { TokenReservedId $$ "bool" }
    true { TokenReservedId $$ "true" }
    false { TokenReservedId $$ "false" }
    let { TokenReservedId $$ "let" }
    const { TokenReservedId $$ "const" }
    unit { TokenReservedId $$ "unit" }
    continue { TokenReservedId $$ "continue" }
    break { TokenReservedId $$ "break" }
    double { TokenReservedId $$ "double" }
    as { TokenReservedId $$ "as" }
    extern { TokenReservedId $$ "extern"}
    gate { TokenReservedId $$ "gate"}
    deriving { TokenReservedId $$ "deriving"}
    oracle { TokenReservedId $$ "oracle"}
    step { TokenReservedId $$ "step" }
    pi { TokenReservedId $$ "pi"}
    '|0>' { TokenReservedOp $$ "|0>" }
    '=' { TokenReservedOp $$ "=" }
    '==' { TokenReservedOp $$ "==" }
    '+' { TokenReservedOp $$ "+" }
    '-' { TokenReservedOp $$ "-" }
    '*' { TokenReservedOp $$ "*" }
    '/' { TokenReservedOp $$ "/" }
    '%' { TokenReservedOp $$ "%" }
    '**' { TokenReservedOp $$ "**" }
    '<' { TokenReservedOp $$ "<" }
    '>' { TokenReservedOp $$ ">" }
    '<=' { TokenReservedOp $$ "<=" }
    '>=' { TokenReservedOp $$ ">=" }
    '!=' { TokenReservedOp $$ "!=" }
    '&&' { TokenReservedOp $$ "&&" }
    and { TokenReservedOp $$ "and" }
    '||' { TokenReservedOp $$ "||" }
    or { TokenReservedOp $$ "or" }
    '!' { TokenReservedOp $$ "!" }
    not { TokenReservedOp $$ "not" }
    '&' { TokenReservedOp $$ "&" }
    '|' { TokenReservedOp $$ "|" }
    '^' { TokenReservedOp $$ "^" }
    '<<' { TokenReservedOp $$ "<<" }
    '>>' { TokenReservedOp $$ ">>" }
    ',' { TokenReservedOp $$ "," }
    ';' { TokenReservedOp $$ ";" }
    '(' { TokenReservedOp $$ "(" }
    ')' { TokenReservedOp $$ ")" }
    '[' { TokenReservedOp $$ "[" }
    ']' { TokenReservedOp $$ "]" }
    '{' { TokenReservedOp $$ "{" }
    '}' { TokenReservedOp $$ "}" }
    ':' { TokenReservedOp $$ ":" }
    '->' { TokenReservedOp $$ "->" }
    '.' { TokenReservedOp $$ "." }
    '..' { TokenReservedOp $$ ".." }
    NATURAL { TokenNatural _ _ }
    FLOAT { TokenFloat _ _ }
    IMAGPART { TokenImagPart _ _ }
    IDENTIFIER { TokenIdent _ _ }
    qualified { TokenQualified _ _}
    STRING { TokenStringLit _ _}

%left ':' -- Level 13
%left '||' and -- Level 12
%left '&&' or -- Level 11
%left '|' -- Level 10
%left '^' -- Level 9
%left '&' -- Level 8
%left '==' '!=' -- Level 7
%nonassoc '>' '<' '>=' '<=' -- Level 6
%left '>>' '<<' -- Level 5
%left '+' '-' -- Level 4
%left '*' '/' '%' -- Level 3
%left '**'  -- Level 2
%right NEG POS '!' not -- Level 2
%left SUBSCRIPT CALL '[' '(' -- Level 1
%left ':'

%%

TopLevel :: {LAST}
TopLevel : Package ImportList DefMemberList { NTopLevel $1 $2 $3 }

Package :: {Maybe LAST}
Package : {- empty -} { Nothing }
     | package IDENTIFIER ';' { Just $ NPackage $1 (tokenIdentV $2) }

ImportList :: {[LAST]}
ImportList : {- empty -} { [] }
     | ImportList Import { $1 ++ [$2] }

Import :: {LAST}
Import : import Qualified ';' { NImport $1 (tokenIdentV $2) }

Qualified :: {ISQv2Token}
Qualified : IDENTIFIER {$1}
     | qualified {$1}

DefMemberList :: {[LAST]}
DefMemberList : {- empty -} { [] }
     | DefMemberList TopDefMember { $1 ++ [$2] }

TopDefMember :: {LAST}
TopDefMember : ISQCore_GatedefStatement ';' { $1 } 
             | TopLevelVar { $1 }
             | ExternDefgate ';' { $1 }
             | Procedure { $1 }
             | OracleTruthTable { $1 }
             | OracleFunction { $1 }

StatementList :: {[LAST]}
StatementList : {- empty -} { [] }
               | StatementList Statement { $1 ++ [$2] }

Expr :: {LExpr}
Expr : Expr1 { $1 } | Expr2 {$1}
Expr1 :: {LExpr}
Expr1 : IDENTIFIER { EIdent (annotation $1) (tokenIdentV $1) }
     | '(' Expr ')' { $2 }
     |  Expr1 '[' Expr ']' { ESubscript $2 $1 $3 }
     |  Expr1 '+' Expr1 { EBinary $2 Add $1 $3 }
     |  Expr1 '-' Expr1 { EBinary $2 Sub $1 $3 }
     |  Expr1 '*' Expr1 { EBinary $2 Mul $1 $3 }
     |  Expr1 '/' Expr1 { EBinary $2 Div $1 $3 }
     |  Expr1 '%' Expr1 { EBinary $2 Mod $1 $3 }
     |  Expr1 '**' Expr1 { EBinary $2 Pow $1 $3 }
     |  Expr1 '&&' Expr1 { EBinary $2 And $1 $3 }
     |  Expr1 and Expr1 { EBinary $2 And $1 $3 }
     |  Expr1 '||' Expr1 { EBinary $2 Or $1 $3 }
     |  Expr1 or Expr1 { EBinary $2 Or $1 $3 }
     |  Expr1 '&' Expr1 { EBinary $2 Andi $1 $3 }
     |  Expr1 '|' Expr1 { EBinary $2 Ori $1 $3 }
     |  Expr1 '^' Expr1 { EBinary $2 Xori $1 $3 }
     |  Expr1 '==' Expr1 { EBinary $2 (Cmp Equal) $1 $3 }
     |  Expr1 '!=' Expr1 { EBinary $2 (Cmp NEqual) $1 $3 }
     |  Expr1 '>' Expr1 { EBinary $2 (Cmp Greater) $1 $3 }
     |  Expr1 '<' Expr1 { EBinary $2 (Cmp Less) $1 $3 }
     |  Expr1 '>=' Expr1 { EBinary $2 (Cmp GreaterEq) $1 $3 }
     |  Expr1 '<=' Expr1 { EBinary $2 (Cmp LessEq) $1 $3 }
     |  Expr1 '<<' Expr1 { EBinary $2 Shl $1 $3 }
     |  Expr1 '>>' Expr1 { EBinary $2 Shr $1 $3 }
     | '-' Expr1 %prec NEG { EUnary $1 Neg $2 }
     | '+' Expr1 %prec POS { EUnary $1 Positive $2 }
     | '!' Expr1 { EUnary $1 Not $2 }
     | not Expr1 { EUnary $1 Not $2 }
     | NATURAL{ EIntLit (annotation $1) (tokenNaturalV $1) }
     | FLOAT { EFloatingLit (annotation $1) (tokenFloatV $1) }
     | pi { EFloatingLit $1 3.14159265358979323846264338327950288 }
     | IMAGPART { EImagLit (annotation $1) (tokenImagPartV $1) }
     | IDENTIFIER '(' ExprList ')' %prec CALL { ECall (annotation $1) $1 $3}
     | '(' Expr ')' '(' ExprList ')' %prec CALL { ECall (annotation $2) $2 $5}
     | '[' ExprList ']' { EList $1 $2 }
     | true { EBoolLit $1 True }
     | false { EBoolLit $1 False }

MaybeExpr1 :: {Maybe LExpr}
MaybeExpr1 : Expr1 {Just $1}
          | {- empty -} {Nothing}

RangeExpr :: {LExpr}
RangeExpr : MaybeExpr1 '..' MaybeExpr1 step MaybeExpr1 { ERange $2 $1 $3 $5 }
          | MaybeExpr1 '..' MaybeExpr1 { ERange $2 $1 $3 Nothing }
Expr2 :: {LExpr}
Expr2 : RangeExpr { $1 }

ExprList :: {[LExpr]}
ExprList : Expr { [$1] } 
         | ExprList ',' Expr { $1 ++ [$3] }
         | {- empty -} { [] }
ExprListNonEmpty :: {[LExpr]}
ExprListNonEmpty : Expr { [$1] }
                 | ExprListNonEmpty ',' Expr { $1 ++ [$3] }

IdentListNonEmpty :: {[ISQv2Token]}
IdentListNonEmpty : IDENTIFIER { [$1] }
                  | IdentListNonEmpty ',' IDENTIFIER { $1 ++ [$3] }

BlockStatement :: {LAST}
BlockStatement : '{' StatementList '}' { NBlock $1 $2 }
ForStatement :: {LAST}
ForStatement : for IDENTIFIER in RangeExpr BlockStatement { NFor $1 (tokenIdentV $2) $4 [$5] }
WhileStatement :: {LAST}
WhileStatement : while Expr BlockStatement { NWhile $1 $2 [$3] }
IfStatement :: {LAST}
IfStatement : if Expr BlockStatement { NIf $1 $2 [$3] [] }
            | if Expr BlockStatement else BlockStatement  { NIf $1 $2 [$3] [$5] }
PassStatement :: {LAST}
PassStatement : pass { NPass $1 }
BpStatement :: {LAST}
BpStatement : bp { NBp $1 }
DefvarStatement :: {LAST}
DefvarStatement : LetStyleDef { $1 }
                | ISQCore_CStyleVarDef { $1 }

LetStyleDef :: {LAST}
LetStyleDef : let IdentListNonEmpty ':' Type { NDefvar $1 (fmap (\x->($4, tokenIdentV x, Nothing)) $2) }

AssignStatement :: {LAST}
AssignStatement : Expr '=' Expr { NAssign $2 $1 $3 }

ReturnStatement :: {LAST}
ReturnStatement : return Expr {NReturn $1 $2}
                | return {NReturn $1 (EUnitLit $1)}

ISQCore_GatedefMatrix :: {[[LExpr]]}
ISQCore_GatedefMatrix : '[' ISQCore_GatedefMatrixContent ']' { $2 }
ISQCore_GatedefMatrixContent :: {[[LExpr]]}
ISQCore_GatedefMatrixContent : ISQCore_GatedefMatrixRow { [$1] }
                             | ISQCore_GatedefMatrixContent ';' ISQCore_GatedefMatrixRow { $1 ++ [$3] }
ISQCore_GatedefMatrixRow :: {[LExpr]}
ISQCore_GatedefMatrixRow : Expr { [$1] }
                         | ISQCore_GatedefMatrixRow ',' Expr { $1 ++ [$3] }
ISQCore_GatedefStatement :: {LAST}
ISQCore_GatedefStatement : defgate IDENTIFIER '=' ISQCore_GatedefMatrix GatedefMaybeExtern { NGatedef $1 (tokenIdentV $2) $4 $5}

GatedefMaybeExtern :: {Maybe String}
GatedefMaybeExtern : extern STRING { Just (tokenStringLitV $2) }
                   | {- empty -} { Nothing }

GateModifier :: {GateModifier}
GateModifier : inv { Inv }
             | ctrl '<' NATURAL '>' {Ctrl True (tokenNaturalV $3)}
             | nctrl '<' NATURAL '>' {Ctrl False (tokenNaturalV $3)}
             | ctrl {Ctrl True 1}
             | nctrl {Ctrl False 1}
GateModifierListNonEmpty :: {[GateModifier]}
GateModifierListNonEmpty : GateModifierListNonEmpty GateModifier { $1 ++ [$2] }
                         | GateModifier {[$1]}
                         
ExternDefgate :: {LAST}
ExternDefgate : extern defgate IDENTIFIER '(' TypeList ')' ':' gate '(' NATURAL ')' '=' STRING { NExternGate $1 (tokenIdentV $3) $5 (tokenNaturalV $10) (tokenStringLitV $13) }


ContinueStatement :: {LAST}
ContinueStatement : continue { NContinue $1 }
BreakStatement :: {LAST}
BreakStatement : break { NBreak $1 }

Statement :: {LAST}
Statement : ';' { NEmpty $1 }
          | PassStatement ';' { $1 }
          | BpStatement ';' { $1 }
          | BlockStatement { $1 }
          | IfStatement { $1 }
          | ForStatement { $1 }
          | WhileStatement { $1 }
          | DefvarStatement ';' { $1 }
          | AssignStatement ';' { $1 }
          | ReturnStatement ';' { $1 }
          | ContinueStatement ';' { $1 }
          | BreakStatement ';' { $1 }

ArrayTypeDecorator :: {BuiltinType}
ArrayTypeDecorator : '[' ']' { UnknownArray }
                   | '[' NATURAL ']' { FixedArray (tokenNaturalV $2)}

Type :: {LType}
Type : SimpleType { $1 }
     | CompositeType { $1 }
SimpleType :: {LType}
SimpleType : int { intType $1 }
           | qbit { qbitType $1 }
           | bool { boolType $1 }
           | unit { unitType $1 }
           | double { doubleType $1 }
CompositeType :: {LType}
CompositeType : Type ArrayTypeDecorator { Type (annotation $1) $2 [$1] }

TypeList :: {[LType]}
TypeList : TypeList ',' Type { $1 ++ [$3] }
         | Type { [$1] }
         | {- empty -} {[]}

ISQCore_CStyleVarDefTerm :: {LType->(LType, ISQv2Token, Maybe LExpr)}
ISQCore_CStyleVarDefTerm : IDENTIFIER { \t->(t, $1, Nothing) }
                         | IDENTIFIER ArrayTypeDecorator { \t->(Type (annotation $1) $2 [t], $1, Nothing)}
                         | IDENTIFIER '=' Expr { \t->(t, $1, Just $3) }
                         | IDENTIFIER ArrayTypeDecorator '=' Expr { \t->(Type (annotation $1) $2 [t], $1, Just $4)}
ISQCore_CStyleVarDefList :: {[LType->(LType, ISQv2Token, Maybe LExpr)]}
ISQCore_CStyleVarDefList : ISQCore_CStyleVarDefTerm {[$1]}
                         | ISQCore_CStyleVarDefList ',' ISQCore_CStyleVarDefTerm {$1 ++ [$3]}
ISQCore_CStyleVarDef :: {LAST}
ISQCore_CStyleVarDef: SimpleType ISQCore_CStyleVarDefList { let args = fmap (\f->let (a, b, c) = f $1 in (a, tokenIdentV b, c)) $2 in NDefvar (annotation $1) args}

ProcedureArgListNonEmpty :: {[(LType, Ident)]}
ProcedureArgListNonEmpty : ProcedureArg { [$1] }
                         | ProcedureArgListNonEmpty ',' ProcedureArg { $1 ++ [$3]}
ProcedureArgList :: {[(LType, Ident)]}
ProcedureArgList : ProcedureArgListNonEmpty { $1 }
                 | {- empty -} {[]}
ProcedureArg :: {(LType, Ident)}
ProcedureArg : SimpleType IDENTIFIER { ($1, tokenIdentV $2) }
             | ISQCore_CStyleArrayArg {$1}
             | IDENTIFIER ':' Type { ($3, tokenIdentV $1) }
ISQCore_CStyleArrayArg :: {(LType, Ident)}
ISQCore_CStyleArrayArg : SimpleType IDENTIFIER ArrayTypeDecorator { (Type (annotation $1) $3 [$1], tokenIdentV $2) }

ProcedureDeriving :: {Maybe DerivingType}
ProcedureDeriving : {- empty -} {Nothing}
                  | deriving gate {Just DeriveGate}
                  | deriving oracle '(' NATURAL ')' {Just (DeriveOracle (tokenNaturalV $4))}
Procedure :: {LAST}
Procedure : procedure IDENTIFIER '(' ProcedureArgList ')'  '{' StatementList '}' ProcedureDeriving { NProcedureWithDerive $1 (unitType $1) (tokenIdentV $2) $4 $7 $9}
          | procedure IDENTIFIER '(' ProcedureArgList ')' '->' Type '{' StatementList '}' ProcedureDeriving {NProcedureWithDerive $1 $7 (tokenIdentV $2) $4 $9 $11}
          | SimpleType IDENTIFIER '(' ProcedureArgList ')' '{' StatementList '}' ProcedureDeriving { NProcedureWithDerive (annotation $1) $1 (tokenIdentV $2) $4 $7 $9} 

TopLevelVar :: {LAST}
TopLevelVar : DefvarStatement ';' { $1 }

OracleTruthTable :: {LAST}
OracleTruthTable : oracle IDENTIFIER '(' NATURAL ',' NATURAL ')' '=' '[' ISQCore_GatedefMatrixRow ']' ';' { NOracle $1 (tokenIdentV $2) (tokenNaturalV $4) (tokenNaturalV $6) $10}

OracleFunction :: {LAST}
OracleFunction : oracle IDENTIFIER '(' NATURAL ',' NATURAL ')' ':' IDENTIFIER '{' StatementList '}' { NOracleFunc $1 (tokenIdentV $2) (tokenNaturalV $4) (tokenNaturalV $6) (tokenIdentV $9) $11 }

{
parseError :: [ISQv2Token] -> a
parseError xs = throw xs
}