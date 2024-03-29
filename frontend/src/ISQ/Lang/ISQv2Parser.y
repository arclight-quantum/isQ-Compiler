{
module ISQ.Lang.ISQv2Parser where

import ISQ.Lang.ISQv2Tokenizer
import ISQ.Lang.ISQv2Grammar
import Data.Maybe (catMaybes)
import Control.Exception (throw, Exception)
import Control.Monad (void)

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
    param { TokenReservedId $$ "param" }
    M { TokenReservedId $$ "M" }
    print { TokenReservedId $$ "print" }
    defgate { TokenReservedId $$ "defgate" }
    pass { TokenReservedId $$ "pass" }
    bp { TokenReservedId $$ "bp" }
    return { TokenReservedId $$ "return" }
    package { TokenReservedId $$ "package" }
    import { TokenReservedId $$ "import" }
    assert { TokenReservedId $$ "assert" }
    switch { TokenReservedId $$ "switch" }
    default { TokenReservedId $$ "default" }
    case { TokenReservedId $$ "case" }
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
    pi { TokenReservedId $$ "pi"}
    span { TokenReservedId $$ "span" }
    '=' { TokenReservedOp $$ "=" }
    '==' { TokenReservedOp $$ "==" }
    '+' { TokenReservedOp $$ "+" }
    '+=' { TokenReservedOp $$ "+=" }
    '-' { TokenReservedOp $$ "-" }
    '-=' { TokenReservedOp $$ "-=" }
    '*' { TokenReservedOp $$ "*" }
    '*=' { TokenReservedOp $$ "*=" }
    '/' { TokenReservedOp $$ "/" }
    '/=' { TokenReservedOp $$ "/=" }
    '%' { TokenReservedOp $$ "%" }
    '%=' { TokenReservedOp $$ "%=" }
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
    '|||' { TokenReservedOp $$ "|||" }
    '^' { TokenReservedOp $$ "^" }
    '~' { TokenReservedOp $$ "~" }
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
    '.length' { TokenReservedOp $$ ".length" }
    '.' { TokenReservedOp $$ "." }
    NATURAL { TokenNatural _ _ }
    FLOAT { TokenFloat _ _ }
    IMAGPART { TokenImagPart _ _ }
    IDENTIFIERORACLE { TokenIdent _ ('@':_) }
    IDENTIFIER { TokenIdent _ _ }
    QUALIFIED { TokenQualified _ _}
    STRING { TokenStringLit _ _}

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
Import : import Qualified ';' { NImport $1 (tokenIdentV $2) Nothing }
       | import Qualified as Qualified ';' { NImport $1 (tokenIdentV $2) $ Just (tokenIdentV $4) }

Qualified :: {ISQv2Token}
Qualified : IDENTIFIER {$1}
     | QUALIFIED {$1}

Paramt :: {LAST}
Paramt :  param IdentListNonEmpty ';' { NDefParam $1 $2 }

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
             | OracleLogic { $1 }
             | Paramt { $1 }

StatementList :: {[LAST]}
StatementList : {- empty -} { [] }
               | StatementList Statement { $1 ++ [$2] }

Expr :: {LExpr}
Expr : Expr12 {$1} | Expr2p {$1}

ExprCallable :: {LExpr}
ExprCallable : Qualified { EIdent (annotation $1) (tokenIdentV $1) }
             | IDENTIFIERORACLE { EIdent (annotation $1) (tokenIdentV $1)}
Expr1Left :: {LExpr}
Expr1Left : ExprCallable {$1}
          | Expr1Left '[' Expr ']' { ESubscript $2 $1 $3 }
          | '[' Expr1ListNonEmpty ']' { EList $1 $2 }

Expr12 :: {LExpr}
Expr12 : Expr11 Expr11s { $2 $1 }

Expr11s :: { LExpr -> LExpr }
Expr11s : {- empty -} {\t -> t}
        | '||' Expr11 { \t -> EBinary $1 Or t $2 }
        | or Expr11 { \t -> EBinary $1 Or t $2 }
        | '||' Expr11 Expr11s { \t -> $3 $ EBinary $1 Or t $2 }
        | or Expr11 Expr11s { \t -> $3 $ EBinary $1 Or t $2 }

Expr11 :: {LExpr}
Expr11 : Expr10 Expr10s { $2 $1 }

Expr10s :: { LExpr -> LExpr }
Expr10s : {- empty -} {\t -> t}
        | '&&' Expr10 { \t -> EBinary $1 And t $2 }
        | and Expr10 { \t -> EBinary $1 And t $2 }
        | '&&' Expr10 Expr10s { \t -> $3 $ EBinary $1 And t $2 }
        | and Expr10 Expr10s { \t -> $3 $ EBinary $1 And t $2 }

Expr10 :: {LExpr}
Expr10 : Expr9 Expr9s { $2 $1 }

Expr9s :: { LExpr -> LExpr }
Expr9s : {- empty -} {\t -> t}
       -- Not use '|' to avoid conflit with |0>
       | '|||' Expr9 { \t -> EBinary $1 Ori t $2 }
       | '|||' Expr9 Expr9s { \t -> $3 $ EBinary $1 Ori t $2 }

Expr9 :: {LExpr}
Expr9 : Expr8 Expr8s { $2 $1 }

Expr8s :: {LExpr -> LExpr}
Expr8s : {- empty -} {\t -> t}
       | '^' Expr8 { \t -> EBinary $1 Xori t $2 }
       | '^' Expr8 Expr8s { \t -> $3 $ EBinary $1 Xori t $2 }

Expr8 :: {LExpr}
Expr8 : Expr7 Expr7s { $2 $1 }

Expr7s :: {LExpr -> LExpr}
Expr7s : {- empty -} {\t -> t}
       | '&' Expr7 { \t -> EBinary $1 Andi t $2 }
       | '&' Expr7 Expr7s { \t -> $3 $ EBinary $1 Andi t $2 }

Expr7 :: {LExpr}
Expr7 : Expr6 Expr6s { $2 $1 }

Expr6s :: {LExpr -> LExpr}
Expr6s : {- empty -} {\t -> t}
       | '==' Expr6 { \t -> EBinary $1 (Cmp Equal) t $2 }
       | '!=' Expr6 { \t -> EBinary $1 (Cmp NEqual) t $2 }
       | '==' Expr6 Expr6s { \t -> $3 $ EBinary $1 (Cmp Equal) t $2 }
       | '!=' Expr6 Expr6s { \t -> $3 $ EBinary $1 (Cmp NEqual) t $2 }

Expr6 :: {LExpr}
Expr6 : Expr5 Expr5s { $2 $1 }

Expr5s :: {LExpr -> LExpr}
Expr5s : {- empty -} {\t -> t}
      | '>' Expr5 { \t -> EBinary $1 (Cmp Greater) t $2 }
      | '>=' Expr5 { \t -> EBinary $1 (Cmp GreaterEq) t $2 }
      | '<' Expr5 { \t -> EBinary $1 (Cmp Less) t $2 }
      | '<=' Expr5 { \t -> EBinary $1 (Cmp LessEq) t $2 }

Expr5 :: {LExpr}
Expr5 : Expr4 Expr4s { $2 $1 }

Expr4s :: {LExpr -> LExpr}
Expr4s : {- empty -} {\t -> t}
      | '>>' Expr4 { \t -> EBinary $1 Shr t $2 }
      | '<<' Expr4 { \t -> EBinary $1 Shl t $2 }
      | '>>' Expr4 Expr4s { \t -> $3 $ EBinary $1 Shr t $2 }
      | '<<' Expr4 Expr4s { \t -> $3 $ EBinary $1 Shl t $2 }

Expr4 :: {LExpr}
Expr4 : Expr3 Expr3s { $2 $1 }

Expr3s :: {LExpr -> LExpr}
Expr3s : {- empty -} {\t -> t}
       | '+' Expr3 { \t -> EBinary $1 Add t $2 }
       | '-' Expr3 { \t -> EBinary $1 Sub t $2 }
       | '+' Expr3 Expr3s { \t -> $3 $ EBinary $1 Add t $2 }
       | '-' Expr3 Expr3s { \t -> $3 $ EBinary $1 Sub t $2 }

Expr3 :: {LExpr}
Expr3 : Expr25 Expr25s { $2 $1 }

-- Set the precedence of |ket> between + and *
-- So that expressions like |0> + 1.0/3|1> can be correctly parsed
Expr25s :: {LExpr -> LExpr}
Expr25s : {- empty -} {\t -> t}
        | '|' NATURAL '>' { \t -> EKet $1 t (tokenNaturalV $2) }

Expr25 :: {LExpr}
Expr25 : Expr2 Expr2s { $2 $1 }

Expr2s :: {LExpr -> LExpr}
Expr2s : {- empty -} {\t -> t}
       | '*' Expr2 { \t -> EBinary $1 Mul t $2 }
       | '/' Expr2 { \t -> EBinary $1 Div t $2 }
       | '%' Expr2 { \t -> EBinary $1 Mod t $2 }
       | '*' Expr2 Expr2s { \t -> $3 $ EBinary $1 Mul t $2 }
       | '/' Expr2 Expr2s { \t -> $3 $ EBinary $1 Div t $2 }
       | '%' Expr2 Expr2s { \t -> $3 $ EBinary $1 Mod t $2 }

Expr2 :: {LExpr}
Expr2 : Expr15 {$1}
      | '-' Expr15 { EUnary $1 Neg $2 }
      | '+' Expr15 { EUnary $1 Positive $2 }
      | '!' Expr15 { EUnary $1 Not $2 }
      | not Expr15 { EUnary $1 Not $2 }
      | '~' Expr15 { EUnary $1 Noti $2 }

Expr15 :: {LExpr}
Expr15 : Expr1 Expr1s {$2 $1}

-- Right associative, i.e., 2**3**4=2**(3**4)
Expr1s :: {LExpr -> LExpr}
Expr1s : {- empty -} {\t -> t}
       | '**' Expr1 { \t -> EBinary $1 Pow t $2 }
       | '**' Expr1 Expr1s { \t -> EBinary $1 Pow t $ $3 $2 }

Expr1 :: {LExpr}
Expr1 : Primary Expr0s { $2 $1 }

Expr0s :: {LExpr -> LExpr}
Expr0s : {- empty -} {\t -> t}
       | as Type { \t -> ECast $1 t $2 }

Ket :: {LExpr}
Ket : '|' NATURAL '>' { EKet $1 (EIntLit $1 1) (tokenNaturalV $2) }

Primary :: {LExpr}
Primary : Expr1Left '.length' { EArrayLen $2 $1 }
        | NATURAL{ EIntLit (annotation $1) (tokenNaturalV $1) }
        | FLOAT { EFloatingLit (annotation $1) (tokenFloatV $1) }
        | pi { EFloatingLit $1 3.14159265358979323846264338327950288 }
        | IMAGPART { EImagLit (annotation $1) (tokenImagPartV $1) }
        | CallExpr { $1 }
        | true { EBoolLit $1 True }
        | false { EBoolLit $1 False }
        -- isQ Core (isQ v1) grammar.
        -- TODO: should they be allowed only in compatible mode?
        | ISQCore_MeasureExpr { $1 }
        | '(' Expr ')' { $2 }
        | Expr1Left { $1 }
        | Ket { $1 }

CallExpr :: {LExpr}
CallExpr : ExprCallable '(' Expr1List ')' { ECall (annotation $1) $1 $3}

MaybeExpr1 :: {Maybe LExpr}
MaybeExpr1 : Expr12 {Just $1}
          | {- empty -} {Nothing}
RangeExpr :: {LExpr}
RangeExpr : MaybeExpr1 ':' MaybeExpr1 ':' MaybeExpr1 { ERange $2 $1 $3 $5 }
          | MaybeExpr1 ':' MaybeExpr1 { ERange $2 $1 $3 Nothing }

Expr2p :: {LExpr}
Expr2p : RangeExpr { $1 }

Expr1List :: {[LExpr]}
Expr1List : Expr12 { [$1] } 
         | Expr1List ',' Expr12 { $1 ++ [$3] }
         | {- empty -} { [] }
Expr1ListNonEmpty :: {[LExpr]}
Expr1ListNonEmpty : Expr12 { [$1] }
                 | Expr1ListNonEmpty ',' Expr12 { $1 ++ [$3] }
Expr1LeftListNonEmpty :: {[LExpr]}
Expr1LeftListNonEmpty : Expr1Left { [$1] }
                      | Expr1LeftListNonEmpty ',' Expr1Left { $1 ++ [$3] }
IdentListNonEmpty :: {[(Ident, Maybe Pos)]}
IdentListNonEmpty : IDENTIFIER { [(tokenIdentV $1, Nothing)] }
                  | IDENTIFIER '[' ']' { [(tokenIdentV $1, Just $2)] }
                  | IdentListNonEmpty ',' IdentListNonEmpty { $1 ++ $3 }

BlockStatement :: {LAST}
BlockStatement : '{' StatementList '}' { NBlock $1 $2 }
ForStatement :: {LAST}
ForStatement : for IDENTIFIER in Expr Statement { NFor $1 (tokenIdentV $2) $4 [$5] }
WhileStatement :: {LAST}
WhileStatement : while Expr Statement { NWhile $1 $2 [$3] }

{-
Example:
switch q {
   case |2> : X(p);
   case |0> :
   default: { Z(p); }
}
Constraints:
   - 'default' must be the last item.
   - The outer braces are essential while internal ones are optional.
-}
CaseStatement :: {LAST}
CaseStatement : case NATURAL ':' StatementList { NCase (annotation $2) (tokenNaturalV $2) $4 False }
              | case '|' NATURAL '>' ':' StatementList { NCase (annotation $3) (tokenNaturalV $3) $6 True }
CaseStatements :: {[LAST]}
CaseStatements : {- empty -} {[]}
               | CaseStatements CaseStatement { $1 ++ [$2] }
SwitchStatement :: {LAST}
SwitchStatement : switch Expr '{' CaseStatements default ':' StatementList '}' { NSwitch $1 $2 $4 $7 }
                | switch Expr '{' CaseStatements '}' { NSwitch $1 $2 $4 [] }

IfStatement :: {LAST}
IfStatement : if Expr Statement { NIf $1 $2 [$3] [] }
            | if Expr Statement else Statement  { NIf $1 $2 [$3] [$5] }
PassStatement :: {LAST}
PassStatement : pass { NPass $1 }
Vec :: {[LExpr]}
Vec : ISQCore_GatedefMatrix { head $1 }
    | Ket { [$1] }
VecList :: {[[LExpr]]}
VecList : Vec { [$1] }
        | Vec ',' VecList { $1 : $3 }
AssertStatement :: {LAST}
AssertStatement : assert Expr { NAssert $1 $2 Nothing }
                | assert Expr in ISQCore_GatedefMatrix { NAssert $1 $2 $ Just $4 }
                | assert Expr in span '(' VecList ')' { NAssertSpan $1 $2 $6 }
BpStatement :: {LAST}
BpStatement : bp { NBp $1 }

CallStatement :: {LAST}
CallStatement : CallExpr { NCall (annotation $1) $1 }
AssignStatement :: {LAST}
AssignStatement : Expr1Left '=' Expr { NAssign $2 $1 $3 AssignEq }
                | Expr1Left '+=' Expr { NAssign $2 $1 $3 AddEq }
                | Expr1Left '-=' Expr { NAssign $2 $1 $3 SubEq }
                | Expr1Left '*=' Expr { NAssign $2 $1 (EBinary $2 Mul $1 $3) AssignEq }
                | Expr1Left '/=' Expr { NAssign $2 $1 (EBinary $2 Div $1 $3) AssignEq }
                | Expr1Left '%=' Expr { NAssign $2 $1 (EBinary $2 Mod $1 $3) AssignEq }
                | Expr1Left '=' ISQCore_GatedefMatrix { NCoreInit $2 $1 $3 }

ReturnStatement :: {LAST}
ReturnStatement : return Expr {NReturn $1 $2}
                | return {NReturn $1 (EUnitLit $1)}

ISQCore_GatedefMatrix :: {[[LExpr]]}
ISQCore_GatedefMatrix : '[' ISQCore_GatedefMatrixContent ']' { $2 }
ISQCore_GatedefMatrixContent :: {[[LExpr]]}
ISQCore_GatedefMatrixContent : ISQCore_GatedefMatrixRow { [$1] }
                             | ISQCore_GatedefMatrixContent ';' ISQCore_GatedefMatrixRow { $1 ++ [$3] }
ISQCore_GatedefMatrixRow :: {[LExpr]}
ISQCore_GatedefMatrixRow : Expr12 { [$1] }
                         | ISQCore_GatedefMatrixRow ',' Expr12 { $1 ++ [$3] }
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

ISQCore_UnitaryStatement :: {LAST}
ISQCore_UnitaryStatement : GateModifierListNonEmpty ExprCallable '(' Expr1List ')' { NCoreUnitary (annotation $2) $2 $4 $1}

ISQCore_MeasureExpr :: {LExpr}
ISQCore_MeasureExpr : M '(' Expr1Left ')' { ECoreMeasure $1 $3 }
ISQCore_MeasureStatement :: {LAST}
ISQCore_MeasureStatement : ISQCore_MeasureExpr { NCoreMeasure (annotation $1) $1}
ISQCore_PrintStatement :: {LAST}
ISQCore_PrintStatement : print Expr { NCorePrint $1 $2 }

ContinueStatement :: {LAST}
ContinueStatement : continue { NContinue $1 }
BreakStatement :: {LAST}
BreakStatement : break { NBreak $1 }

Statement :: {LAST}
Statement : ';' { NEmpty $1 }
          | PassStatement ';' { $1 }
          | AssertStatement ';' { $1 }
          | BpStatement ';' { $1 }
          | BlockStatement { $1 }
          | IfStatement { $1 }
          | ForStatement { $1 }
          | WhileStatement { $1 }
          | SwitchStatement { $1 }
          | DefvarStatement ';' { $1 }
          | CallStatement ';' { $1 }
          | AssignStatement ';' { $1 }
          | ReturnStatement ';' { $1 }
          | ContinueStatement ';' { $1 }
          | BreakStatement ';' { $1 }
          | ISQCore_UnitaryStatement ';' { $1 }
          | ISQCore_MeasureStatement ';' { $1 }
          | ISQCore_PrintStatement ';' { $1 }

ArrayTypeDecorator :: {BuiltinType}
ArrayTypeDecorator : '[' ']' { (Array 0) }
                   | '[' NATURAL ']' { Array (tokenNaturalV $2)}

Type :: {LType}
Type : SimpleType { $1 }
     | CompositeType { $1 }
     | FuncType { $1 }

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

FuncType :: {LType}
FuncType :  '(' TypeList ')' '->' SimpleType { funcType $1 ([$5] ++ $2) }

ISQCore_CStyleVarDefTerm :: {LType -> (LType, ISQv2Token, Maybe LExpr, Maybe LExpr)}
ISQCore_CStyleVarDefTerm : IDENTIFIER { \t->(t, $1, Nothing, Nothing) }
                         | IDENTIFIER '[' Expr12 ']' { \t->(Type (annotation $1) (Array 0) [t], $1, Nothing, Just $3)}
                         | IDENTIFIER '=' Expr { \t->(t, $1, Just $3, Nothing) }
                         | IDENTIFIER '[' ']' '=' Expr { \t->(Type (annotation $1) (Array 0) [t], $1, Just $5, Nothing)}
ISQCore_CStyleVarDefList :: {[LType -> (LType, ISQv2Token, Maybe LExpr, Maybe LExpr)]}
ISQCore_CStyleVarDefList : ISQCore_CStyleVarDefTerm {[$1]}
                         | ISQCore_CStyleVarDefList ',' ISQCore_CStyleVarDefTerm {$1 ++ [$3]}
DefvarStatement :: {LAST}
DefvarStatement: SimpleType ISQCore_CStyleVarDefList { let args = fmap (\f->let (a, b, c, d) = f $1 in (a, tokenIdentV b, c, d)) $2 in NDefvar (annotation $1) args}

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
             | SimpleType IDENTIFIER '(' TypeList ')' { (funcType $3 ([$1] ++ $4), tokenIdentV $2) }
             
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

OracleLogic :: {LAST}
OracleLogic : oracle Type IDENTIFIER '(' ProcedureArgList ')' '{' StatementList '}' { NOracleLogic $1 (void $2) (tokenIdentV $3) (fmap (\(ty, ident) -> (void ty, ident)) $5) $8 }

{
parseError :: [ISQv2Token] -> a
parseError xs = throw xs
}