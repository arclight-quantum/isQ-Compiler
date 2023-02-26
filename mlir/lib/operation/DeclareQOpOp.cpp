#include "isq/QTypes.h"
#include <isq/Operations.h>
#include <mlir/IR/Attributes.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/SymbolTable.h>
#include <mlir/Support/LogicalResult.h>
namespace isq {
namespace ir {
bool DeclareQOpOp::isDeclaration() { return true; }
/*
::mlir::SymbolTable::Visibility DeclareQOpOp::getVisibility(){
    return
}
*/
::mlir::FunctionType getExpandedFunctionType(::mlir::MLIRContext *ctx,
                                             uint64_t size,
                                             ::mlir::FunctionType signature) {
    ::mlir::SmallVector<::mlir::Type> inputs, outputs;
    ::mlir::SmallVector<::mlir::Type> tup_elements;
    for (auto i = 0; i < size; i++) {
        auto q = QStateType::get(ctx);
        tup_elements.push_back(q);
    }
    auto tup = tup_elements;
    // auto tup = ::mlir::TupleType::get(this->getContext(), tup_elements);
    inputs.append(tup.begin(), tup.end());
    outputs.append(tup.begin(), tup.end());
    auto in = signature.getInputs();
    inputs.append(in.begin(), in.end());
    auto out = signature.getResults();
    outputs.append(out.begin(), out.end());
    return ::mlir::FunctionType::get(ctx, inputs, outputs);
}
::mlir::Type DeclareQOpOp::getTypeWhenUsed() {
    return getExpandedFunctionType(this->getContext(), this->size(),
                                   this->signature());
}
mlir::LogicalResult DeclareQOpOp::parseIR(::mlir::OpAsmParser &parser,
                                          ::mlir::OperationState &result) {
    ::mlir::StringAttr sym_nameAttr;
    ::mlir::IntegerAttr sizeAttr;
    ::mlir::TypeAttr signatureAttr;
    if (parser.parseSymbolName(sym_nameAttr, "sym_name", result.attributes))
        return ::mlir::failure();
    ::mlir::NamedAttrList parsedAttributes;
    auto attributeDictLocation = parser.getCurrentLocation();
    if (parser.parseOptionalAttrDict(parsedAttributes)) {
        return ::mlir::failure();
    }
    for (::mlir::StringRef disallowed :
         {::mlir::SymbolTable::getVisibilityAttrName(),
          ::mlir::SymbolTable::getSymbolAttrName(), ::mlir::StringRef("size"),
          ::mlir::StringRef("signature")}) {
        if (parsedAttributes.get(disallowed))
            return parser.emitError(attributeDictLocation, "'")
                   << disallowed
                   << "' is an inferred attribute and should not be specified "
                      "in the "
                      "explicit attribute dictionary";
    }
    auto ctx = parser.getBuilder().getContext();
    parsedAttributes.push_back(
        mlir::NamedAttribute(::mlir::StringAttr::get(ctx, "sym_visibility"),
                         ::mlir::StringAttr::get(ctx, "nested")));
    result.attributes.append(parsedAttributes);
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

    if (parser.parseAttribute(signatureAttr,
                              parser.getBuilder().getType<::mlir::NoneType>(),
                              "signature", result.attributes))
        return ::mlir::failure();
    return ::mlir::success();
}
void DeclareQOpOp::printIR(::mlir::OpAsmPrinter &p) {
    //p << "isq.declare_qop";
    //p << ' ';
    p.printSymbolName(sym_nameAttr().getValue());
    p.printOptionalAttrDict(
        (*this)->getAttrs(),
        /*elidedAttrs=*/{"sym_name", "size", "signature", "sym_visibility"});
    p << ' ' << ":";
    p << ' ' << "[";
    p.printAttributeWithoutType(sizeAttr());
    p << "]";
    p << ' ';
    p.printAttributeWithoutType(signatureAttr());
}
/*
mlir::LogicalResult verify(DeclareOp op) {
if (op.op_type() != op.getResult().getType()) {
    op.emitOpError("operation signature mismatch.");
    return mlir::failure();
}
return mlir::success();
}
*/


::mlir::ParseResult DeclareQOpOp::parse(::mlir::OpAsmParser &parser, ::mlir::OperationState &result){
        return DeclareQOpOp::parseIR(parser, result);
}
void DeclareQOpOp::print(::mlir::OpAsmPrinter & p){
    return this->printIR(p);
}


} // namespace ir
} // namespace isq