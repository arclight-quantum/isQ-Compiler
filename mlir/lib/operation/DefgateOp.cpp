#include "isq/GateDefTypes.h"
#include "isq/Math.h"
#include "isq/QTypes.h"
#include "mlir/IR/Attributes.h"
#include "llvm/ADT/SmallVector.h"
#include <isq/IR.h>
#include <llvm/ADT/StringSwitch.h>
#include <llvm/Support/raw_ostream.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/Types.h>
#include <mlir/Support/LLVM.h>
#include <mlir/Support/LogicalResult.h>
#include <optional>

namespace isq {
namespace ir {
bool DefgateOp::isDeclaration() {
    auto defs = this->definition();
    return !defs.hasValue();
}
bool DefgateOp::isGateArray() {
    auto shape = this->shape();
    return shape.hasValue();
}
::mlir::Type DefgateOp::getTypeWhenUsed() {
    auto shape = this->shape();
    if (shape.hasValue()) {
        auto arr = shape->getValue();
        ::mlir::SmallVector<int64_t> tmp;
        for (auto iter = arr.begin(); iter != arr.end(); iter++) {
            auto i = iter->dyn_cast<::mlir::IntegerAttr>();
            tmp.push_back(i.getInt());
        }
        return ::mlir::MemRefType::get(tmp, this->type());
    } else {
        return this->type();
    }
}

::mlir::LogicalResult DefgateOp::parseIR(::mlir::OpAsmParser &parser,
                                         ::mlir::OperationState &result) {
    ::mlir::StringAttr sym_nameAttr;
    ::mlir::TypeAttr typeAttr;

    if (parser.parseSymbolName(sym_nameAttr, "sym_name", result.attributes))
        return ::mlir::failure();
    ::mlir::NamedAttrList parsedAttributes;
    auto parametersLocation = parser.getCurrentLocation();
    llvm::SmallVector<mlir::Attribute> partypeattrs;
    ::mlir::ArrayAttr shapeAttr;
    if (mlir::succeeded(parser.parseOptionalLParen())){
        llvm::SmallVector<mlir::Type> partypes;
        if(parser.parseTypeList(partypes)){
            return ::mlir::failure();
        }
        for(auto& ty: partypes){
            partypeattrs.push_back(mlir::TypeAttr::get(ty));
        }
        if(parser.parseRParen()){
            return ::mlir::failure();
        }
    } else {
        ::mlir::OptionalParseResult parseResult = parser.parseOptionalAttribute(shapeAttr, parser.getBuilder().getType<::mlir::NoneType>(), "shape", result.attributes);
        if(parseResult.hasValue() && mlir::failed(*parseResult)){
            return ::mlir::failure();
        }
    }
    result.attributes.push_back(mlir::NamedAttribute(mlir::StringAttr::get(parser.getContext(), "parameters"), mlir::ArrayAttr::get(parser.getContext(), partypeattrs)));
    auto attributeDictLocation = parser.getCurrentLocation();
    if (parser.parseOptionalAttrDict(parsedAttributes)) {
        return ::mlir::failure();
    }
    for (::mlir::StringRef disallowed :
         {::mlir::SymbolTable::getVisibilityAttrName(),
          ::mlir::SymbolTable::getSymbolAttrName(),
          ::mlir::StringRef("type"),
          ::mlir::StringRef("parameters"),
          ::mlir::StringRef("shape")}) {
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

    if (parser.parseAttribute(typeAttr,
                              parser.getBuilder().getType<::mlir::NoneType>(),
                              "type", result.attributes))
        return ::mlir::failure();
    return ::mlir::success();
}

void DefgateOp::printIR(::mlir::OpAsmPrinter &p) {
    //p << "isq.defgate";
    p << ' ';
    p.printSymbolName(sym_nameAttr().getValue());
    if(parameters().size()){
        p<<'(';
        bool flag=false;
        for(auto& pa: parameters()){
            if(flag) p<<", ";
            p.printType(pa.cast<mlir::TypeAttr>().getValue());
            flag=true;
        }
        p<<')';
    }
    if(shape()){
        p.printAttributeWithoutType(*shape());
    }
    p.printOptionalAttrDict(
        (*this)->getAttrs(),
        /*elidedAttrs=*/{"sym_name", "type", "sym_visibility", "shape", "parameters"});
    p << ' ' << ":";
    p << ' ';
    p.printAttributeWithoutType(typeAttr());
}
mlir::LogicalResult
DefgateOp::verifySymbolUses(::mlir::SymbolTableCollection &symbolTable) {
    if (this->definition().hasValue()) {
        auto defs = this->definition()->getValue();
        for (auto i = 0; i < defs.size(); i++) {
            auto id = i;
            auto def = defs[i].dyn_cast<GateDefinition>();
            if (!def) {
                this->emitError()
                    << "Definition #" << id << " should be GateDefinition";
                return mlir::failure();
            }
            auto result = AllGateDefs::verifySymTable(*this, i, this->type(), def, symbolTable);
            if(::mlir::failed(result)) {
                return ::mlir::failure();
            }
        }
    }
    return mlir::success();
}

/*
bool DefgateOp::parseGateVisibility(::mlir::OpAsmParser& parser, ::mlir::StringAttr& v){
    ::mlir::StringRef visibility;
    auto kwLoc = parser.getCurrentLocation();
    if(succeeded(parser.parseOptionalKeyword(&visibility))){
        if(visibility=="nested" || visibility=="public" || visibility=="private"){
            v=parser.getBuilder().getStringAttr(visibility);
            return false;
        }else{
            parser.emitError(kwLoc, "Bad visibility! Must be one of nested/public/private.");
            return true;
        }
    }else{
        v=parser.getBuilder().getStringAttr("nested");
        return false;
    }


}
void DefgateOp::printGateVisibility(::mlir::OpAsmPrinter& p, DefgateOp op, ::mlir::StringAttr v){
    if(v.getValue()!="nested"){
        p.printKeywordOrString(v.getValue());
    }
}
*/

mlir::LogicalResult verifyGateDefinition(DefgateOp op, int id,
                                         GateDefinition def, GateType ty) {
    if (!def) {
        llvm_unreachable("Null GateDefinition.");
    }
    return mlir::success(AllGateDefs::parseGateDefinition(op, id, ty, def) != std::nullopt);
}
::mlir::LogicalResult DefgateOp::verifyIR() {
    if (this->definition().hasValue()) {
        auto defs = this->definition()->getValue();
        for (auto i = 0; i < defs.size(); i++) {
            auto def = defs[i];
            if (mlir::failed(verifyGateDefinition(
                    *this, i, def.dyn_cast<GateDefinition>(), this->type()))) {
                return mlir::failure();
            }
        }
    }
    return mlir::success();
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

::mlir::ParseResult DefgateOp::parse(::mlir::OpAsmParser &parser, ::mlir::OperationState &result){
        return DefgateOp::parseIR(parser, result);
}
void DefgateOp::print(::mlir::OpAsmPrinter & p){
    return this->printIR(p);
}
::mlir::LogicalResult DefgateOp::verify(){
    return this->verifyIR();
}

} // namespace ir
} // namespace isq