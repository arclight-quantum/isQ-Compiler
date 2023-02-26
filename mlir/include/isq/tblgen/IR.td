include "mlir/Interfaces/CallInterfaces.td"
include "mlir/Interfaces/CastInterfaces.td"
include "mlir/Interfaces/SideEffectInterfaces.td"
include "mlir/IR/BuiltinAttributes.td"
include "mlir/IR/SymbolInterfaces.td"
include "mlir/IR/EnumAttr.td"
include "mlir/IR/BuiltinTypeInterfaces.td"
//include "mlir/Dialect/Shape/IR/ShapeBase.td"

////////////////////////////////////
// Dialect
////////////////////////////////////

def ISQ_Dialect: Dialect{
    let name = "isq";
    let cppNamespace = "::isq::ir";
    let summary = "isQ-IR Dialect.";
    let description = [{
        MLIR Dialect for expressing and optimization ranging from low-level quantum circuits, to high level quantum programs with complicated control structures.
    }];
    let dependentDialects = ["::mlir::memref::MemRefDialect", "::mlir::AffineDialect", "::mlir::arith::ArithmeticDialect", "::mlir::scf::SCFDialect", "::mlir::LLVM::LLVMDialect",
    "::mlir::func::FuncDialect", "::mlir::cf::ControlFlowDialect",
    "::mlir::complex::ComplexDialect"];
    let hasCanonicalizer = 1;
    let useDefaultTypePrinterParser = 0;
    let useDefaultAttributePrinterParser = 0;
    let extraClassDeclaration = [{
        ::mlir::Type parseType(::mlir::DialectAsmParser &parser) const override;
        void printType(::mlir::Type type,
                        ::mlir::DialectAsmPrinter &printer) const override;
        ::mlir::Attribute parseAttribute(::mlir::DialectAsmParser &parser,
                                                ::mlir::Type type) const override;
        void printAttribute(::mlir::Attribute attr,
                                        ::mlir::DialectAsmPrinter &os) const override;
    }];
}

////////////////////////////////////
// Types
////////////////////////////////////

class ISQ_Type<string name> : TypeDef<ISQ_Dialect, name>{}
class ISQ_Op<string mnemonic, list<Trait> traits = []> :
    Op<ISQ_Dialect, mnemonic, traits>{
    
}

def ISQ_QState : TypeDef<ISQ_Dialect, "QState", [MemRefElementTypeInterface]>{
    let description = [{
        Represents an intermediate qubit SSA state. Written `!isq.qstate`.

        **Representation of qubit**: we use `memref<!isq.qstate>` to represent qubits.

        **Qubit state lifetime rule**: every `!isq.qstate` SSA value must be used **exactly once**. 
        The source of an `!isq.qstate` should be a memory-reading operation like `affine.load`, 
        and the drain should be a memory-writing operation like `affine.store`.
        The source `load` and drain `store` must refer to exactly the same memory location.
        An `!isq.qstate` SSA value is called **alive** between its definition and its usage.

        **Qubit state aliasing rule**: at any moment two alive `!isq.qstate` SSA values must refer to two different qubits.
        This is similar to the aliasing rule for `&mut` references in Rust or the functionality of `__restrict`(`restrict`) keyword in C.
        In this sense, all multi-qubit calls are naturally legal.

        This SSA representation is inspired by [David et al.](https://arxiv.org/abs/2101.11030).
    }];
    let summary = [{Qubit state}];
    let mnemonic = "qstate";
    let hasCustomAssemblyFormat = 1;
    /*
    let printer = [{
        QStateParsePrint p;
        p.printType(*this, $_printer);
    }];
    let parser = [{
        QStateParsePrint p;
        return p.parseType($_parser);
    }];
    */
}
def ISQ_Gate : TypeDef<ISQ_Dialect, "Gate", [MemRefElementTypeInterface]>{
    let description = [{
        Represents **reference** to a unitary (pure) quantum gate. This enables gate-level optimization of quantum programs.

        A gate type is parametrized with two parameters: **size** and **traits**.
        - **Size** indicates the size of the gate.
        - **Traits** indicates useful properties for optimization of the gate, for example, if the gate is diagonal a gate, or the gate is symmetric over its operands.

        A gate type can be written in the form `!isq.gate<size[, traits]>`. Currently supported traits include:
        - `hermitian`, indicating the gate is self-adjoint, i.e. \f$U^\dagger = U\f$.
        - `diagonal`, indicating the gate is diagonal (under computational basis).
        - `antidiagonal`, indicating the gate is antidiagonal (under computational basis). This enables switching between antidiagonal gates and \f$CZ\f$ gates, or `EjectPhasedPaulis` optimization.
        - `symmetric`, indicating the gate is symmetric over its **all** operands. Single qubit gates are intrinsically symmetric whether they are labeled or not.

        Note that we allow loading gates at runtime.
    }];
    let parameters = (ins "int64_t":$size,"GateTrait":$hints);
    let summary = [{Gate reference}];
    let mnemonic = "gate";
    let hasCustomAssemblyFormat = 1;
    /*
    let printer = [{
        GateParsePrint p;
        p.printType(*this, $_printer);
    }];
    let parser = [{
        GateParsePrint p;
        return p.parseType($_parser);
    }];
    */
    let extraClassDeclaration = [{
        ::mlir::SmallVector<mlir::Type> getApplyParameterType(GateType ty);
    }];
}
def ISQ_QIRQubit : TypeDef<ISQ_Dialect, "QIRQubit", [MemRefElementTypeInterface]>{
    let description = [{
        Represents an opaque value `%Qubit` in QIR(LLVM).
    }];
    let summary = [{QIR Opaque Qubit value.}];
    let mnemonic = "qir.qubit";
    let hasCustomAssemblyFormat = 1;
    /*
    let printer = [{
        QIRQubitParsePrint p;
        p.printType(*this, $_printer);
    }];
    let parser = [{
        QIRQubitParsePrint p;
        return p.parseType($_parser);
    }];
    */
}
def ISQ_QIRResult : TypeDef<ISQ_Dialect, "QIRResult", [MemRefElementTypeInterface]>{
    let description = [{
        Represents an opaque value `%Result` in QIR(LLVM).
    }];
    let summary = [{QIR Opaque measurement result value.}];
    let mnemonic = "qir.result";
    let hasCustomAssemblyFormat = 1;
    /*
    let printer = [{
        QIRResultParsePrint p;
        p.printType(*this, $_printer);
    }];
    let parser = [{
        QIRResultParsePrint p;
        return p.parseType($_parser);
    }];
    */
}
/*
def ISQ_QOp: DialectType<ISQ_Dialect, CPred<"$_self.isa<::isq::ir::QOpType>()">, "General quantum operation type">{
    let description = [{
        (Deprecated, since QOp will remain symbol and not materialized.)
        Represents a general quantum operation on several qubits, with certain classical input/output parameters.
        Think of this type as a black-box with the same number of input wires and output wires.

        A quantum operation type can be written in the form `!isq.qop<size, (operands)->results>`. Qstates and `operands` will be sent into this operation, and `results` come out.
    }];
}
*/

////////////////////////////////////
// Attributes
////////////////////////////////////

class ISQ_Attr<string name, list<Trait> traits = []> : AttrDef<ISQ_Dialect, name, traits>;


def GateTypeAttr: TypeAttrBase<"::isq::ir::GateType", "Gate type attribute">;
def FuncTypeAttr: TypeAttrBase<"::mlir::FunctionType", "Function type attribute, used as tuple of operands and results">;


// Gate traits

def GateTraitGeneral: I32BitEnumAttrCase<"General", 0, "general">;
def GateTraitDiagonal: I32BitEnumAttrCase<"Diagonal", 1, "diagonal">;
def GateTraitAntidiagonal: I32BitEnumAttrCase<"Antidiagonal", 2, "antidiagonal">;
def GateTraitSymmetric: I32BitEnumAttrCase<"Symmetric", 4, "symmetric">;
def GateTraitHermitian: I32BitEnumAttrCase<"Hermitian", 8, "hermitian">;
def GateTraitPhase: I32BitEnumAttrCase<"Phase", 16, "phase">;

def GateTrait: I32BitEnumAttr<"GateTrait", "Gate traits/hints", [GateTraitGeneral, GateTraitDiagonal, GateTraitAntidiagonal, GateTraitSymmetric, GateTraitHermitian, GateTraitPhase]>;
// Complex matrix.
def DenseComplexF64MatrixAttr : AttrDef<ISQ_Dialect, "DenseComplexF64Matrix">{
    let parameters = (ins
        "mlir::DenseElementsAttr": $body
    );
    let mnemonic = "matrix";
    let assemblyFormat = "`<` $body `>`";
    let builders = [
        AttrBuilder<(ins "const MatrixVal&": $matrix)>
    ];
    let extraClassDeclaration = [{
        using MatrixVal = llvm::SmallVector<llvm::SmallVector<std::complex<double>>>;
        MatrixVal toMatrixVal();
    }];
    let extraClassDefinition = [{
        template<isq::ir::math::MatDouble T> 
        static DenseComplexF64MatrixAttr fromMatrix(mlir::MLIRContext* ctx, const T& mat){
            return fromMatrixImpl(ctx, mat);
        }
    }];

}
// Gate definition
def ISQ_GateDefinitionAttr : AttrDef<ISQ_Dialect, "GateDefinition">{
    let parameters = (ins 
        "mlir::StringAttr": $type, 
        "mlir::Attribute": $value);
    let mnemonic = "gatedef";
    let assemblyFormat = "`<` struct(params) `>`";
}

def ISQ_GateDefinitionList : TypedArrayAttrBase<ISQ_GateDefinitionAttr, "Gate definition array attribute">;



def DenseComplexF64ArrayAttr : DenseArrayAttrBase<"DenseComplexF64ArrayAttr", "std::complex<double>", "complexF64">;

////////////////////////////////////
// Operations
////////////////////////////////////

def ISQ_DefgateOp : ISQ_Op<"defgate", [Symbol, DeclareOpInterfaceMethods<SymbolUserOpInterface>]>{
    let summary = "Defines or declares an array of gates.";
    let description = [{
        Syntax:
        ```
        operation ::= $sym_name attr-dict `:` $type
        ```
        Declares or defines an array of gate references.

        Defgate comes with an optional shape attribute.
        If the shape is present, the gate will be used as an array;
        otherwise the shape will be used as a single gate.

        Defgate allows multiple and overlapping definition of one gate.
        A defgate without shape and definition is seen as declaration.
        Which definition to choose while optimization and lowering depends on the optimization passes.



        Currently supported definitions:
        - `unitary`, unitary matrix definition. Not compatible with attribute `shape`.
        - `decomposition`, a `builtin.func` to represent the decomposition. The function should have the right signature and obeys correspondence between args and results.
        - `qir`, QIR call.

        (TODO: ways to define a gate are still not defined. We may want to support:
        - Matrix representation, where complex matrix is given out directly.
        - Gate decomposition, allowing using an entire decomposition function as a single gate.
        - Controlled-gate (Functors)
        - Classical oracle conversion
        - ...
        )

        ```mlir
        // Gate declaration
        isq.defgate @cnot : !isq.gate<2, hermitian>
        // Gate definition
        isq.defgate @hadamard {definition = [ /* possible gate definition */ ]} : !isq.gate<1, hermitian>
        // Gate array declaration
        isq.defgate @one_way_equivalent_gates [10, 10] : !isq.gate<1>
        // Gate family definition
        isq.defgate @rz(f64) { definitions = [ /* external call */ ]} : !isq.gate<1, diagonal>
        ```

        The symbol always uses `nested` visbility.
    }];
    let arguments = (ins
        GateTypeAttr: $type,
        SymbolNameAttr: $sym_name,
        StrAttr: $sym_visibility,
        OptionalAttr<I64ArrayAttr>: $shape,
        OptionalAttr<ISQ_GateDefinitionList>: $definition,
        TypeArrayAttr: $parameters
    );
    let results = (outs);
    let hasCustomAssemblyFormat = 1;
    let hasVerifier = 1;
    /*
    let assemblyFormat = [{
        custom<GateVisibility>($sym_visibility) $sym_name (`(` $parameters^ `)`)? ( $shape^ )? attr-dict `:` $type
    }];
    */
    /*
    let parser = [{return $cppClass::parseIR(parser, result);}];
    let printer = [{return this->printIR(p);}];
    
    let verifier = [{
        return this->verifyIR();
    }];
    */
    let extraClassDeclaration = [{
        bool isGateArray();
        bool isDeclaration();
        static ::mlir::LogicalResult parseIR(::mlir::OpAsmParser& parser, ::mlir::OperationState& result);
        void printIR(::mlir::OpAsmPrinter& p);
        static bool parseGateVisibility(::mlir::OpAsmParser& parser, ::mlir::StringAttr& v);
        void printGateVisibility(::mlir::OpAsmPrinter& p, DefgateOp op, ::mlir::StringAttr v);
        ::mlir::Type getTypeWhenUsed();
        ::mlir::LogicalResult verifyIR();
    }];


}

def ISQ_DeclareQOpOp : ISQ_Op<"declare_qop", [Symbol]>{
    let summary = "Declares general quantum operations";
    let description = [{
        Syntax:
        ```
        operation ::= $sym_name attr-dict `:` `[` $size `]` $signature
        ```

        Declares a general quantum operation that is most possibly not pure, i.e. a measurement.
        A QOp can be seen as a general operation accepting several qubits as fan-in and fan-out,
        while accepting possible (classical) inputs and outputs.


        Difference between QOp and general functions: QOp guarantees correspondence between input and output qstates.
        
        (TODO: recognizing general function as qop.)
        ```mlir
        // Computational basis measurement, accepting 1 qubit, no input parameters and outputting a boolean value.
        isq.declare_qop @measure : [1]()->i1
        // Try to measure a quantum money qubit, accepting 1 bit to specity basis and outputting 1 bit.
        isq.declare_qop @check_quantum_money_bit : [2](i1)->i1
        ```

        The symbol always uses `nested` visbility.
    }];
    let arguments = (ins
        SymbolNameAttr: $sym_name,
        StrAttr: $sym_visibility,
        UI64Attr: $size,
        FuncTypeAttr: $signature
    );
    let results = (outs);
    /*
    let assemblyFormat = [{
        $sym_name attr-dict `:` `[` $size `]` $signature
    }];
    */
    let hasCustomAssemblyFormat = 1;
    /*
    let parser = [{return $cppClass::parseIR(parser, result);}];
    let printer = [{return this->printIR(p);}];
    */
    let extraClassDeclaration = [{
        ::mlir::Type getTypeWhenUsed();
        bool isDeclaration();
        static ::mlir::LogicalResult parseIR(::mlir::OpAsmParser& parser, ::mlir::OperationState& result);
        void printIR(::mlir::OpAsmPrinter& p);
    }];

}
def ISQ_UseGateOp : ISQ_Op<"use", [NoSideEffect, DeclareOpInterfaceMethods<SymbolUserOpInterface>]>{
    let summary = "Use gate array";
    let description = [{
        Fetching a declared/defined global gate or gate array.
        Gate arrays are readonly and writing into gate array is currently not allowed.

        The return type depends on whether the `isq.defgate` declares a gate or a gate array,
        i.e. whether the `isq.defgate` contains the `shape` attribute.

        ```mlir
        isq.defgate @one_gate : !isq.gate<1, hermitian>
        isq.defgate @gates {shape=[10,20]}: !isq.gate<1>
        isq.defgate @u3(f64, f64, f64) : !isq.gate<1>
        func @use_gates()->(){
            %g1 = isq.use @one_gate : !isq.gate<1, hermitian>
            %g = isq.use @gates : memref<10x20x!isq.gate<1>>
            %zero = arith.constant 0.0 : f64
            %identity = isq.use @u3(%zero, %zero, %zero) : (f64, f64, f64)->!isq.gate<1>
            return
        }
        ```
    }];
    let arguments = (ins 
    SymbolRefAttr: $name,
    Variadic<AnyType>: $parameters
    );
    let results = (outs
    AnyType: $result);
    
    let assemblyFormat = [{
        $name attr-dict  ( `(` $parameters^ `)` `:` `(` qualified(type($parameters)) `)` `->`):(`:`)? qualified(type(results))
    }];
    
}

def ISQ_ContribUndef : ISQ_Op<"contrib.memref.undef", [NoSideEffect]>{
    let summary = "Undef value for a memref.";
    let description = [{
        The value that you will obtain by reading from an uninitialized memory.
    }];
    let arguments = (ins
        Variadic<AnyType>: $args
    );
    let results = (outs 
        AnyType: $result
    );
}

def ISQ_ApplyGPhase : ISQ_Op<"apply_gphase", []>{
    let summary = "Auxiliary operation that applies global-phase";
    let description = [{
        Auxiliary operation that side-effectfully applies a global-phase gate.
    }];
    let arguments = (ins
        ISQ_Gate: $gate
    );
    let assemblyFormat = [{
        $gate attr-dict `:` qualified(type($gate))
    }];
}

def ISQ_AccumulateGPhase : ISQ_Op<"accumulate_gphase", []>{
    let summary = "Auxiliary operation that accumulates global phase on a sub-system onto total global phase";
    let description = [{
        Auxiliary operation that prevents a procedure-local quantum subsystem from being optimized out.
        A sufficient condition that can prevent a procedure-local quantum subsystem from getting optimized out is creating entanglement between an external system. However, an isolated local system may introduce global phase onto the system as well, thus local phase for controlled-gates.
        This operation is inserted before every dealloc op. If the subsystem is linked to another subsystem, the semantics of the operation is "no-op"; otherwise, the semantics of the operation will be two `global-phase` gates: one global-phase zeroing out current subsystem and another global-phase on the other part of the system.
    }];
    let arguments = (ins
        AnyType: $subsystem
    );
    let assemblyFormat = [{
        $subsystem attr-dict `:` qualified(type($subsystem))
    }];
}

def ISQ_ApplyGateOp : ISQ_Op<"apply", [
    NoSideEffect,
    TypesMatchWith<
        "args equivalence", "gate", "args", "::isq::ir::getGateParameterType($_self.cast<::isq::ir::GateType>())"
    >,TypesMatchWith<
        "output equivalence", "gate", "r", "::isq::ir::getGateParameterType($_self.cast<::isq::ir::GateType>())"
    >]>{
    let summary = "Applies a quantum gate onto qstates";
    let description = [{
        Performing a gate onto qstates. Qstate outputs are in the same order as inputs.

        ```mlir
        isq.defgate @hadamard : !isq.gate<1, hermitian>
        isq.defgate @cnot : !isq.gate<2, hermitian>
        func @bell(%q1: !isq.qstate, %q2: !isq.qstate)->(!isq.qstate, !isq.qstate){
            %hadamard = isq.use @hadamard : !isq.gate<1, hermitian>
            %cnot = isq.use @cnot : !isq.gate<2, hermitian>
            %q3 = isq.apply %hadamard(%q1): !isq.gate<1, hermitian>
            %q4, %q5 = isq.apply %cnot(%q3, %q2): !isq.gate<2, hermitian>
            return %q4, %q5 : !isq.qstate, !isq.qstate
        }
        ```
    }];
    let arguments = (ins
        ISQ_Gate: $gate,
        Variadic<ISQ_QState>: $args
    );
    let results = (outs Variadic<ISQ_QState>: $r);
    let assemblyFormat = [{
        $gate `(` $args `)` attr-dict `:` qualified(type($gate))
    }];
    let hasVerifier = 1;
    /*
    let verifier = [{
        return this->verifyIR();
    }];
    */
    let extraClassDeclaration = [{
        ::mlir::LogicalResult verifyIR();
    }];
    let hasCanonicalizer = 1;
}

def ISQ_CallQOpOp: ISQ_Op<"call_qop", [DeclareOpInterfaceMethods<SymbolUserOpInterface>]>{
    let summary = "Applies a quantum operation onto qstates";
    let description = [{
        Syntax:
        ```
        operation ::= $callee `(` $args `)` attr-dict `:` `[` $size `]` $signature
        ```
        Performing a quantum operation onto qstates by given symbol and input, yielding outputs.
        Quantum operation arrays are currently disallowed.

        The syntax of `call_qop` resembles a function call.
        The list of required qstates are attached to the front of argument list,
        while the outputting qstates are attached to the front of result list.

        ```mlir
        // Measurement accepts one qubit, no inputs and yields one output.
        isq.declare_qop @measure : [1]()->i1

        func @measure_and_discard_result(%q: !isq.qstate)->!isq.qstate{
            // Here, [1]()->i1 corresponds to (!isq.qstate)->(!isq.qstate, i1).
            %q1, %c = isq.call_qop @measure(%q): (!isq.qstate)->(!isq.qstate, i1)
            return %q1: !isq.qstate
        }
        ```
    }];
    
    
    let arguments = (ins
        SymbolRefAttr: $callee,
        Variadic<AnyType>: $args,
        UI64Attr: $size,
        FuncTypeAttr: $signature
    );

    let results = (outs Variadic<AnyType>);
    let extraClassDeclaration = [{
        static ::mlir::ParseResult parseIR(::mlir::OpAsmParser& parser, ::mlir::OperationState& result);
        void printIR(::mlir::OpAsmPrinter& p);
    }];
    /*
    let parser = [{return $cppClass::parseIR(parser, result);}];
    let printer = [{return this->printIR(p);}];
    */
    let hasCustomAssemblyFormat = 1;

    /*
    let assemblyFormat = [{
        $callee `(` $args `)` attr-dict `:` `[` $size `]` $signature
    }];
    */

}

def ISQ_DowngradeGateOp: ISQ_Op<"downgrade", [NoSideEffect]>{
    let summary = "Downgrade a gate by removing its traits.`";
    let description = [{
        This operation is used to perform safe gate reference type casting.
        For example, if we want to pass the $CZ$ gate with type `!isq.gate<2, hermitian, symmetric>` to a function
        that accepts `!isq.gate<2>` only, this operation is needed for a downcast.
        ```mlir
        isq.defgate @hadamard : !isq.gate<1, hermitian>
        func @apply_twice(%g: !isq.gate<1>, %q: !isq.qstate)->!isq.qstate{
            %q1 = isq.apply %g(%q): !isq.gate<1>
            %q2 = isq.apply %g(%q1): !isq.gate<1>
            return %q2: !isq.qstate
        }
        func @apply_hadamard_twice(%q: !isq.qstate)->!isq.qstate{
            %hadamard = isq.use @hadamard : !isq.gate<1, hermitian>
            // Erasing the `hermitian` trait.
            %downgraded_hadamard = isq.downgrade (%hadamard: !isq.gate<1, hermitian>) : !isq.gate<1>
            %q2 = std.call @apply_twice(%downgraded_hadamard, %q) : (!isq.gate<1>, !isq.qstate)->!isq.qstate
            return %q2: !isq.qstate
        }
        ```
    }];
    let arguments = (ins 
        ISQ_Gate: $args
    );
    let results = (outs ISQ_Gate);
    let assemblyFormat = [{
        `(` $args `:` qualified(type($args)) `)` attr-dict `:`qualified(type(results))
    }];
    /*
    let verifier = [{
        return ::isq::ir::verify(*this);
    }];
    */
    let hasVerifier = 1;
    let hasCanonicalizer = 1;
}

def ISQ_DecorateOp: ISQ_Op<"decorate", [NoSideEffect]>{
    let summary = "Decorate a gate by adding controlled and adjoint markers.";
    let description = [{
        This operations is used to derive a controlled / inverse gate from a used gate.
    }];
    let arguments = (
        ins ISQ_Gate: $args,
        BoolAttr: $adjoint,
        BoolArrayAttr : $ctrl
    );
    let results = (outs ISQ_Gate);
    let assemblyFormat = [{
        `(` $args `:` qualified(type($args)) `)` attr-dict `:` qualified(type(results))
    }];
    let extraClassDeclaration = [{
    public:
        static GateTrait computePostDecorateTrait(GateTrait attr, int ctrl, bool adj, bool ctrl_all_one);
    }];
    /*
    let verifier = [{
        return ::isq::ir::verify(*this);
    }];*/
    let hasVerifier = 1;
    let hasCanonicalizer = 1;
}


def ISQ_PrintOp : ISQ_Op<"print"> {
    let arguments = (ins AnyType:$input);

    let assemblyFormat = [{
        $input `:` qualified(type($input))  attr-dict
    }];
}

def ISQ_PassOp :ISQ_Op<"pass"> {
    let assemblyFormat = [{
        attr-dict
    }];
}