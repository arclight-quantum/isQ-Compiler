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
    '||' { TokenReservedOp $$ "||" }
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
    NATURAL { TokenNatural _ _ }
    FLOAT { TokenFloat _ _ }
    IMAGPART { TokenImagPart _ _ }
    IDENTIFIERORACLE { TokenIdent _ ('@':_) }
    IDENTIFIER { TokenIdent _ _ }
    STRING { TokenStringLit _ _}

%left ':' -- Level 13
%left '==' '!=' -- Level 7
%nonassoc '>' '<' '>=' '<=' -- Level 6
%left '%' -- Level 5
%left '+' '-' -- Level 4
%left '*' '/' -- Level 3
%left '**'  -- Level 2
%right NEG POS -- Level 2
%left SUBSCRIPT CALL '[' '(' -- Level 1
%left ':'

%%

TopLevel :: {[LAST]}
TopLevel : {- empty -} { [] }
     | TopLevel TopDefMember {$1 ++ [$2]}

TopDefMember :: {LAST}
TopDefMember : ISQCore_GatedefStatement ';' {$1} 
             | TopLevelVar {$1}
             | ExternDefgate ';' { $1 }
             | Procedure { $1 }
             | OracleTruthTable { $1 }

StatementListMaybe :: {[Maybe LAST]}
StatementListMaybe : StatementListMaybe Statement { $1 ++ [$2] }
              | {- empty -} { [] }
StatementList :: {[LAST]}
StatementList : StatementListMaybe { catMaybes $1 }

Expr :: {LExpr}
Expr : Expr1 {$1} | Expr2 {$1}

ExprCallable :: {LExpr}
ExprCallable : '(' Expr ')' { $2 }
             | IDENTIFIER { EIdent (annotation $1) (tokenIdentV $1) }
             | IDENTIFIERORACLE { EIdent (annotation $1) (tokenIdentV $1)}
Expr1Left :: {LExpr}
Expr1Left : ExprCallable {$1}
          | Expr1Left '[' Expr ']' { ESubscript $2 $1 $3 }

Expr1 :: {LExpr}
Expr1 : Expr1Left { $1 }
     |  Expr1 '+' Expr1 { EBinary $2 Add $1 $3 }
     |  Expr1 '-' Expr1 { EBinary $2 Sub $1 $3 }
     |  Expr1 '*' Expr1 { EBinary $2 Mul $1 $3 }
     |  Expr1 '/' Expr1 { EBinary $2 Div $1 $3 }
     |  Expr1 '%' Expr1 { EBinary $2 Mod $1 $3 }
     |  Expr1 '**' Expr1 { EBinary $2 Pow $1 $3 }
     |  Expr1 '==' Expr1 { EBinary $2 (Cmp Equal) $1 $3 }
     |  Expr1 '!=' Expr1 { EBinary $2 (Cmp NEqual) $1 $3 }
     |  Expr1 '>' Expr1 { EBinary $2 (Cmp Greater) $1 $3 }
     |  Expr1 '<' Expr1 { EBinary $2 (Cmp Less) $1 $3 }
     |  Expr1 '>=' Expr1 { EBinary $2 (Cmp GreaterEq) $1 $3 }
     |  Expr1 '<=' Expr1 { EBinary $2 (Cmp LessEq) $1 $3 }
     | '-' Expr1 %prec NEG { EUnary $1 Neg $2 }
     | '+' Expr1 %prec POS { EUnary $1 Positive $2 }
     | NATURAL{ EIntLit (annotation $1) (tokenNaturalV $1) }
     | FLOAT { EFloatingLit (annotation $1) (tokenFloatV $1) }
     | pi { EFloatingLit $1 3.14159265358979323846264338327950288 }
     | IMAGPART { EImagLit (annotation $1) (tokenImagPartV $1) }
     | CallExpr { $1 }
     | '[' Expr1List ']' { EList $1 $2 }
     | true { EBoolLit $1 True }
     | false { EBoolLit $1 False }
     -- isQ Core (isQ v1) grammar.
     -- TODO: should they be allowed only in compatible mode?
     | ISQCore_MeasureExpr { $1 }

CallExpr :: {LExpr}
CallExpr : ExprCallable '(' Expr1List ')' %prec CALL { ECall (annotation $1) $1 $3}
MaybeExpr1 :: {Maybe LExpr}
MaybeExpr1 : Expr1 {Just $1}
          | {- empty -} {Nothing}
RangeExpr :: {LExpr}
RangeExpr : MaybeExpr1 ':' MaybeExpr1 ':' MaybeExpr1 { ERange $2 $1 $3 $5 }
          | MaybeExpr1 ':' MaybeExpr1 { ERange $2 $1 $3 Nothing }
Expr2 :: {LExpr}
Expr2 : RangeExpr { $1 }

Expr1List :: {[LExpr]}
Expr1List : Expr1 { [$1] } 
         | Expr1List ',' Expr1 { $1 ++ [$3] }
         | {- empty -} { [] }
Expr1ListNonEmpty :: {[LExpr]}
Expr1ListNonEmpty : Expr1 { [$1] }
                 | Expr1ListNonEmpty ',' Expr1 { $1 ++ [$3] }
Expr1LeftListNonEmpty :: {[LExpr]}
Expr1LeftListNonEmpty : Expr1Left { [$1] }
                      | Expr1LeftListNonEmpty ',' Expr1Left { $1 ++ [$3] }
IdentListNonEmpty :: {[ISQv2Token]}
IdentListNonEmpty : IDENTIFIER { [$1] }
                  | IdentListNonEmpty ',' IDENTIFIER { $1 ++ [$3] }
ForStatement :: {LAST}
ForStatement : for IDENTIFIER in RangeExpr '{' StatementList '}' {NFor $1 (tokenIdentV $2) $4 $6 }
WhileStatement :: {LAST}
WhileStatement : while Expr '{' StatementList '}' {NWhile $1 $2 $4}
IfStatement :: {LAST}
IfStatement : if Expr '{' StatementList '}' {NIf $1 $2 $4 []}
            | if Expr '{' StatementList '}' else '{' StatementList '}'  {NIf $1 $2 $4 $8}
PassStatement :: {LAST}
PassStatement : pass { NPass $1 }
BpStatement :: {LAST}
BpStatement : bp { NBp $1 }
DefvarStatement :: {LAST}
DefvarStatement : LetStyleDef { $1 }
                | ISQCore_CStyleVarDef { $1 }

LetStyleDef :: {LAST}
LetStyleDef : let IdentListNonEmpty ':' Type { NDefvar $1 (fmap (\x->($4, tokenIdentV x, Nothing)) $2) }

CallStatement :: {LAST}
CallStatement : CallExpr { NCall (annotation $1) $1 }
AssignStatement :: {LAST}
AssignStatement : Expr1Left '=' Expr { NAssign $2 $1 $3 }

ReturnStatement :: {LAST}
ReturnStatement : return Expr {NReturn $1 $2}
                | return {NReturn $1 (EUnitLit $1)}

ISQCore_GatedefMatrix :: {[[LExpr]]}
ISQCore_GatedefMatrix : '[' ISQCore_GatedefMatrixContent ']' { $2 }
ISQCore_GatedefMatrixContent :: {[[LExpr]]}
ISQCore_GatedefMatrixContent : ISQCore_GatedefMatrixRow { [$1] }
                             | ISQCore_GatedefMatrixContent ';' ISQCore_GatedefMatrixRow { $1 ++ [$3] }
ISQCore_GatedefMatrixRow :: {[LExpr]}
ISQCore_GatedefMatrixRow : Expr1 { [$1] }
                         | ISQCore_GatedefMatrixRow ',' Expr1 { $1 ++ [$3] }
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
ISQCore_UnitaryStatement : ExprCallable '<' Expr1LeftListNonEmpty '>' { NCoreUnitary (annotation $1) $1 $3 []}
                         | GateModifierListNonEmpty ExprCallable '<' Expr1LeftListNonEmpty '>' { NCoreUnitary (annotation $2) $2 $4 $1}
                         | GateModifierListNonEmpty ExprCallable '(' Expr1List ')' { NCoreUnitary (annotation $2) $2 $4 $1}

ISQCore_MeasureExpr :: {LExpr}
ISQCore_MeasureExpr : M '<' Expr1Left '>' { ECoreMeasure $1 $3 }
                | M '(' Expr1Left ')' { ECoreMeasure $1 $3 }
ISQCore_MeasureStatement :: {LAST}
ISQCore_MeasureStatement : ISQCore_MeasureExpr { NCoreMeasure (annotation $1) $1}
ISQCore_ResetStatement :: {LAST}
ISQCore_ResetStatement : Expr1Left '=' '|0>' { NCoreReset (annotation $1) $1 }
ISQCore_PrintStatement :: {LAST}
ISQCore_PrintStatement : print Expr { NCorePrint $1 $2 }

ContinueStatement :: {LAST}
ContinueStatement : continue { NContinue $1 }
BreakStatement :: {LAST}
BreakStatement : break { NBreak $1 }

StatementNonEmpty :: {LAST}
StatementNonEmpty : PassStatement ';' { $1 }
          | BpStatement ';' { $1 }
          | IfStatement { $1 }
          | ForStatement { $1 }
          | WhileStatement { $1 }
          | DefvarStatement ';' { $1 }
          | CallStatement ';' { $1 }
          | AssignStatement ';' { $1 }
          | ReturnStatement ';' { $1 }
          | ContinueStatement ';' { $1 }
          | BreakStatement ';' { $1 }
          | ISQCore_UnitaryStatement ';' { $1 }
          | ISQCore_MeasureStatement ';' { $1 }
          | ISQCore_ResetStatement ';' { $1 }
          | ISQCore_PrintStatement ';' { $1 }

Statement :: {Maybe LAST}
Statement : StatementNonEmpty {Just $1}
          | ';' {Nothing}

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

           

{
parseError :: [ISQv2Token] -> a
parseError xs = throw xs
     
}