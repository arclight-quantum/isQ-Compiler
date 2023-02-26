include "mlir/Interfaces/CallInterfaces.td"
include "mlir/Interfaces/CastInterfaces.td"
include "mlir/Interfaces/SideEffectInterfaces.td"
include "mlir/IR/BuiltinAttributes.td"
include "mlir/IR/SymbolInterfaces.td"
include "mlir/IR/BuiltinTypeInterfaces.td"
include "mlir/IR/OpBase.td"
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


def ISQAST_Opaque : TypeDef<ISQAST_Dialect, "ASTType", []> {
    let description = [{
        Opaque type for isQ AST values.
        Typing rules and type checkers are opaque to MLIR.
    }];
    let summary = [{isQ value type}];
    let mnemonic = "type";
    let printer = [{
        ISQTypeParsePrint p;
        p.printType(*this, $_printer);
    }];
    let parser = [{
        ISQTypeParsePrint p;
        return p.parseType($_parser);
    }];
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