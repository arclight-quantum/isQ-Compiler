#include <isq/Operations.h>
#include <mlir/IR/PatternMatch.h>
#include <mlir/Support/LogicalResult.h>
#include <isq/OpVerifier.h>
namespace isq {
namespace ir {
::mlir::LogicalResult
CallQOpOp::verifySymbolUses(::mlir::SymbolTableCollection &symbolTable) {
    auto symbol_def =
        symbolTable.lookupNearestSymbolFrom(*this, this->callee());
    if (auto qop = llvm::dyn_cast_or_null<DeclareQOpOp>(symbol_def)) {

        auto fn =
            mlir::FunctionType::get(this->getContext(), this->args().getTypes(),
                                    this->getResults().getTypes());
        if (fn == qop.getTypeWhenUsed()) {
            return mlir::success();
        } else {
            this->emitOpError()
                << "type mismatch, expected " << qop.getTypeWhenUsed();
            return mlir::failure();
        }
    }
    this->emitOpError() << "symbol `" << this->callee()
                        << "` not found or has wrong type";
    return mlir::failure();
}

::mlir::ParseResult CallQOpOp::parseIR(::mlir::OpAsmParser &parser,
                                       ::mlir::OperationState &result) {
    ::mlir::SymbolRefAttr calleeAttr;
    ::mlir::SmallVector<::mlir::OpAsmParser::UnresolvedOperand, 4> argsOperands;
    ::llvm::SMLoc argsOperandsLoc;
    (void)argsOperandsLoc;
    ::llvm::ArrayRef<::mlir::Type> argsTypes;
    ::llvm::ArrayRef<::mlir::Type> allResultTypes;
    ::mlir::IntegerAttr sizeAttr;
    ::mlir::TypeAttr signatureAttr;

    if (parser.parseAttribute(calleeAttr,
                              parser.getBuilder().getType<::mlir::NoneType>(),
                              "callee", result.attributes))
        return ::mlir::failure();
    if (parser.parseLParen())
        return ::mlir::failure();

    argsOperandsLoc = parser.getCurrentLocation();
    if (parser.parseOperandList(argsOperands))
        return ::mlir::failure();
    if (parser.parseRParen())
        return ::mlir::failure();
    if (parser.parseOptionalAttrDict(result.attributes))
        return ::mlir::failure();
    if (parser.parseColon())
        return ::mlir::failure();

    if (parser.parseLSquare())
        return ::mlir::failure();

    if (parser.parseAttribute(
            sizeAttr,
            parser.getBuilder().getIntegerType(64, /*isSigned=*/false), "size",
            result.attributes))
        return ::mlir::failure();
    if (parser.parseRSquare())
        return ::mlir::failure();
    auto signature_loc = parser.getCurrentLocation();
    if (parser.parseAttribute(signatureAttr,
                              parser.getBuilder().getType<::mlir::NoneType>(),
                              "signature", result.attributes))
        return ::mlir::failure();
    auto expanded_fn =
        signatureAttr.getValue().dyn_cast_or_null<::mlir::FunctionType>();
    if (!expanded_fn) {
        parser.emitError(signature_loc, "expecting FunctionType here.");
        return ::mlir::failure();
    }
    ::mlir::FunctionType args__allResult_functionType = getExpandedFunctionType(
        parser.getBuilder().getContext(), sizeAttr.getUInt(), expanded_fn);
    argsTypes = args__allResult_functionType.getInputs();
    allResultTypes = args__allResult_functionType.getResults();
    result.addTypes(allResultTypes);
    if (parser.resolveOperands(argsOperands, argsTypes, argsOperandsLoc,
                               result.operands))
        return ::mlir::failure();
    return ::mlir::success();
}

void CallQOpOp::printIR(::mlir::OpAsmPrinter &p) {
    //p << "isq.call_qop";
    //p << ' ';
    p.printAttributeWithoutType(calleeAttr());
    p << "(";
    p << args();
    p << ")";
    p.printOptionalAttrDict((*this)->getAttrs(), /*elidedAttrs=*/{"callee", "size", "signature"});
    p << ' ' << ":";
    p << ' ' << "[";
    p.printAttributeWithoutType(sizeAttr());
    p << "]";
    p << ' ';
    p.printAttributeWithoutType(signatureAttr());
}


::mlir::ParseResult CallQOpOp::parse(::mlir::OpAsmParser &parser, ::mlir::OperationState &result){
        return CallQOpOp::parseIR(parser, result);
}
void CallQOpOp::print(::mlir::OpAsmPrinter & p){
    return this->printIR(p);
}


/*
void ApplyOp::getCanonicalizationPatterns(mlir::RewritePatternSet &results,
                                      mlir::MLIRContext *context) {
results.add<EliminateHermitianPairs>(context);
}
*/
} // namespace ir
} // namespace isq