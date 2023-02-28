include "mlir/Interfaces/CallInterfaces.td"
include "mlir/Interfaces/CastInterfaces.td"
include "mlir/Interfaces/SideEffectInterfaces.td"
include "mlir/IR/BuiltinAttributes.td"
include "mlir/IR/SymbolInterfaces.td"
include "mlir/IR/BuiltinTypeInterfaces.td"
include "mlir/IR/OpBase.td"
include "mlir/IR/EnumAttr.td"
//include "mlir/Dialect/Shape/IR/ShapeBase.td"


def ISQAST_Dialect : Dialect{
    let name = "isqast";
    let cppNamespace = "::isq::ast";
    let summary = "isQ frontend AST Dialect.";
    let description = [{
        MLIR Dialect for representing AST-level constructs from isQ language.
        Front-end passes (e.g. typechecking) are done here.
        This dialect will eventually be lowered to isQ-IR.
    }];
}

class ISQAST_Type<string name> : TypeDef<ISQAST_Dialect, name>{}
class ISQAST_Op<string mnemonic, list<Trait> traits = []>:
    Op<ISQAST_Dialect, mnemonic, traits>{

}


def ISQAST_Basic : ISQAST_Type<"Basic"> {
    let description = [{
        Basic opaque type for isQ AST values.
    }];
    let summary = [{isQ basic opaque type.}];
    let parameters = (ins "llvm::StringAttr" : $opaque);
}

def ISQAST_Ref : ISQAST_Type<"Ref">{
    let description = [{
        Non-owned reference type.
    }];
    let summary = [{Ref type.}];
    let parameters = (ins "mlir::Type" : $inner);
}
def ISQAST_Owned : ISQAST_Type<"Owned">{
    let description = [{
        Owned reference type.
    }];
    let summary = [{Owned type.}];
    let parameters = (ins "mlir::Type" : $inner);
}
def ISQAST_Unknown : ISQAST_Type<"Unknown">{
    let description = [{ Type inference placeHolder.}];
    let summary = [{Unknown type.}];
}
def ISQAST_Unit : ISQAST_Type<"Unit">{
    let description = [{ Unit type.}];
    let summary = [{ Unit type.}];
}
// TODO: Use Generic or Generic1 to hold the higher-order types?

def ISQAST_Array : ISQAST_Type<"Array">{
    let parameters = (ins "mlir::Type" : $inner);
}
def ISQAST_Slice : ISQAST_Type<"Slice">{
    let parameters = (ins "mlir::Type" : $inner);
}


def ISQAST_Func : ISQAST_Op<"func", [Symbol]>{
    let arguments = (ins
        SymbolNameAttr: $sym_name,
        TypeAttrOf<FunctionType>:$function_type,
        OptionalAttr<StrAttr>: $sym_visibility
    );
    let regions = (region AnyRegion:$body);
    let builders = [OpBuilder<(ins
        "StringRef":$name, "FunctionType":$type,
        CArg<"ArrayRef<NamedAttribute>", "{}">:$attrs,
        CArg<"ArrayRef<DictionaryAttr>", "{}">:$argAttrs)
    >];

}

def ISQAST_Call : ISQAST_Op<"call", [DeclareOpInterfaceMethods<SymbolUserOpInterface>]>{
    let arguments = (ins
        SymbolNameAttr: $callee,
        Variadic<AnyType>: $parameters
    );
    let results = (outs
        Variadic<AnyType>: $results        
    );
}

class ISQAST_Arith1<string name> : ISQAST_Op<name>{
    let arguments = (ins
        AnyType: $arg
    );
    let results = (outs
    AnyType: $result);
}
class ISQAST_Arith2<string name> : ISQAST_Op<name>{
    let arguments = (ins
        AnyType: $lhs,
        AnyType: $rhs
    );
    let results = (outs
    AnyType: $result);
}

def ISQAST_Add:     ISQAST_Arith2<"add">;
def ISQAST_Sub:     ISQAST_Arith2<"sub">;
def ISQAST_Mul:     ISQAST_Arith2<"mul">;
def ISQAST_Div:     ISQAST_Arith2<"div">;
def ISQAST_Pow:     ISQAST_Arith2<"pow">;
def ISQAST_Mod:     ISQAST_Arith2<"mod">;
def ISQAST_And:     ISQAST_Arith2<"and">;
def ISQAST_Or:      ISQAST_Arith2<"or">;
def ISQAST_BitAnd : ISQAST_Arith2<"bitand">;
def ISQAST_BitOr  : ISQAST_Arith2<"bitor">;
def ISQAST_BitXor : ISQAST_Arith2<"bitxor">;
def ISQAST_Shl : ISQAST_Arith2<"shl">;
def ISQAST_Shr : ISQAST_Arith2<"shr">;

def ISQAST_CmpTypeAttr : I64EnumAttr<
    "CmpType", "",
    [
      I64EnumAttrCase<"eq", 0>,
      I64EnumAttrCase<"ne", 1>,
      I64EnumAttrCase<"slt", 2>,
      I64EnumAttrCase<"sle", 3>,
      I64EnumAttrCase<"sgt", 4>,
      I64EnumAttrCase<"sge", 5>,
      I64EnumAttrCase<"ult", 6>,
      I64EnumAttrCase<"ule", 7>,
      I64EnumAttrCase<"ugt", 8>,
      I64EnumAttrCase<"uge", 9>,
    ]> {
  let cppNamespace = "::isq::ast";
}


def ISQAST_Cmp : ISQAST_Op<"cmp">{
    let arguments = (ins
        ISQAST_CmpTypeAttr: $cmpType,
        AnyType: $lhs,
        AnyType: $rhs
    );
    let results = (outs
    AnyType: $result);
}

def ISQAST_Subscript : ISQAST_Arith2<"subscript">;

def ISQAST_Neg : ISQAST_Arith1<"neg">;
def ISQAST_Pos : ISQAST_Arith1<"pos">;
def ISQAST_Not : ISQAST_Arith1<"not">;
def ISQAST_Borrow : ISQAST_Arith1<"borrow">;
def ISQAST_Deref : ISQAST_Arith1<"deref">;
def ISQAST_Paren : ISQAST_Arith1<"paren">;

def ISQAST_Literal : ISQAST_Op<"literal">{
    let arguments = (ins AnyAttr:$value);
    let results = (outs AnyType:$result);
}

def ISQAST_LitList : ISQAST_Op<"literal">{
    let arguments = (ins AnyType : $values);
    let results = (outs AnyType: $result);
}

def ISQAST_VarDef : ISQAST_Op<"defvar">{
    let arguments = (ins AnyType : $default);
    let results = (outs AnyType: $result);
}

def ISQAST_Assign : ISQAST_Op<"assign"> {
    let arguments = (ins
        AnyType : $ref,
        AnyType : $value
    );
}
def ISQAST_OwnToVal : ISQAST_Arith1<"owntoval">;
def ISQAST_OwnToRef : ISQAST_Arith1<"owntoref">;
def ISQAST_Cast : ISQAST_Arith1<"cast">;


// We don't re-invent control flow wheels by ourselves.
// Instead, we leverage existing dialect.


def ISQAST_RangeExtract : ISQAST_Op<"rangeextract">{
    let arguments = (ins
        AnyType: $arg
    );
    let results = (outs
    AnyType: $lo,
    AnyType: $hi,
    AnyType: $step);
}
