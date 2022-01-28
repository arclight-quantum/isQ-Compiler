#include "isq/Operations.h"
#include "isq/QAttrs.h"
#include "isq/QStructs.h"
#include "isq/QTypes.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/SymbolTable.h"
#include "mlir/IR/Types.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Support/LogicalResult.h"
#include "llvm/Support/Casting.h"
#include "llvm/ADT/StringRef.h"
#include <memory>
#include <optional>
#include "isq/Math.h"
#include "isq/GateDefTypes.h"
namespace isq{
namespace ir{
// Define by matrix.
MatrixDefinition::MatrixDefinition(::isq::ir::DefgateOp op, int id, ::isq::ir::GateType gateType, ::mlir::Attribute value): GateDefinitionAttribute(GD_MATRIX){
    auto arr = value.dyn_cast_or_null<::mlir::ArrayAttr>();
    assert(arr);
    for (auto row : arr) {
        auto row_arr = row.dyn_cast_or_null<::mlir::ArrayAttr>();
        std::vector<std::complex<double>> row_vec;
        assert(row_arr);
        for (auto element : row_arr) {
            auto element_attr = element.dyn_cast_or_null<ComplexF64Attr>();
            assert(element_attr);
            row_vec.push_back(element_attr.complexValue());
        }
        mat.push_back(std::move(row_vec));
    }
}
::mlir::LogicalResult MatrixDefinition::verify(::isq::ir::DefgateOp op, int id, ::isq::ir::GateType ty, ::mlir::Attribute attribute) {
    if(op.shape()){
        op->emitError()
            << "Definition #" << id << " is a matrix definition and should not be used with gate arrays.";
        return mlir::failure();
    }
    // try to get as a matrix.
    std::vector<std::vector<std::complex<double>>> mat;
    auto arr = attribute.dyn_cast_or_null<::mlir::ArrayAttr>();
    if (!arr) {
        op->emitError()
            << "Definition #" << id << " should use a matrix as value.";
        return mlir::failure();
    }
    for (auto row : arr) {
        auto row_arr = row.dyn_cast_or_null<::mlir::ArrayAttr>();
        std::vector<std::complex<double>> row_vec;
        if (!row_arr) {
            op->emitError()
                << "Definition #" << id << " should use a matrix as value.";
            return mlir::failure();
        }
        for (auto element : row_arr) {
            auto element_attr = element.dyn_cast_or_null<ComplexF64Attr>();
            if (!element_attr) {
                op->emitError()
                    << "Definition #" << id
                    << " matrix entries should be complex numbers.";
                return mlir::failure();
            }
            row_vec.push_back(element_attr.complexValue());
        }
        mat.push_back(std::move(row_vec));
    }
    math::InputMatrix wrapper;
    wrapper.body = std::make_unique<math::InputMatrix::Ty>(std::move(mat));
    auto dim = math::checkDimensionality(wrapper);
    if (!dim.hasValue()) {
        op->emitError()
            << "Definition #" << id << " input is not a matrix.";
        return mlir::failure();
    }
    auto dimension = dim.getValue();
    if (dimension != (1 << ty.getSize())) {
        op->emitError() << "Definition #" << id
                        << " matrix dimensionality and gate size mismatch.";
        return mlir::failure();
    }
    // check unitary.
    auto math_mat = math::toEigenMatrix(wrapper);
    if (!math_mat) {
        llvm_unreachable("nope");
    }
    if (!math::isUnitary(*math_mat)) {
        op->emitError()
            << "Definition #" << id << " matrix seems not unitary.";
        return mlir::failure();
    }
    auto hints = ty.getHints();
    if (bitEnumContains(hints, GateTrait::Hermitian)) {
        if (!math::isHermitian(*math_mat)) {
            op->emitError()
                << "Definition #" << id << " matrix seems not hermitian.";
            return mlir::failure();
        }
    }
    if (bitEnumContains(hints, GateTrait::Diagonal)) {
        if (!math::isDiagonal(*math_mat)) {
            op->emitError()
                << "Definition #" << id << " matrix seems not diagonal.";
            return mlir::failure();
        }
    }
    if (bitEnumContains(hints, GateTrait::Antidiagonal)) {
        if (!math::isAntiDiagonal(*math_mat)) {
            op->emitError() << "Definition #" << id
                            << " matrix seems not antidiagonal.";
            return mlir::failure();
        }
    }
    return ::mlir::success();
}
::mlir::LogicalResult MatrixDefinition::verifySymTable(::isq::ir::DefgateOp op, int id, ::isq::ir::GateType ty, ::mlir::Attribute attribute, ::mlir::SymbolTableCollection &symbolTable){
    return ::mlir::success();
}

// Define by decomposition.
DecompositionDefinition::DecompositionDefinition(::isq::ir::DefgateOp op, int id, ::isq::ir::GateType gateType, ::mlir::Attribute value): GateDefinitionAttribute(GD_DECOMPOSITION){
    auto callee = value.cast<::mlir::SymbolRefAttr>();
    this->decomposition = mlir::SymbolTable::lookupNearestSymbolFrom<mlir::FuncOp>(op, callee);
    assert(this->decomposition);
}
::mlir::LogicalResult DecompositionDefinition::verify(::isq::ir::DefgateOp op, int id, ::isq::ir::GateType ty, ::mlir::Attribute attribute){
    return ::mlir::success();
}
::mlir::LogicalResult DecompositionDefinition::verifySymTable(::isq::ir::DefgateOp op, int id, ::isq::ir::GateType ty, ::mlir::Attribute value, ::mlir::SymbolTableCollection &symbolTable) {
    if(!value.isa<::mlir::SymbolRefAttr>()){
        op->emitError() << "Definition #" << id
                            << " should refer to a decomposed function name.";
        return mlir::failure();
    }
    auto callee = value.cast<::mlir::SymbolRefAttr>();
    auto sym = mlir::SymbolTable::lookupNearestSymbolFrom(op, callee);
    if(!sym){
        op->emitError() << "Definition #" << id
                            << " does not refer to an existing symbol.";
        return mlir::failure();
    }
    auto funcop = ::llvm::dyn_cast_or_null<::mlir::FuncOp>(sym);
    if(!funcop){
        op->emitError() << "Definition #" << id
                            << " does not refer to a valid symbol by `builtin.func`.";
        return mlir::failure();
    }
    // construct func signature.
    ::mlir::SmallVector<::mlir::Type> types;
    for(auto i=0; i<ty.getSize(); i++){
        types.push_back(::isq::ir::QStateType::get(op.getContext()));
    }
    auto required_functype = ::mlir::FunctionType::get(op->getContext(), types, types);
    if(required_functype!=funcop.getType()){
            op->emitError() << "Definition #" << id
                            << " does not have signature "<<required_functype<<".";
        return mlir::failure();
    }
    return ::mlir::success();
}

QIRDefinition::QIRDefinition(::isq::ir::DefgateOp op, int id, ::isq::ir::GateType gateType, ::mlir::Attribute value): GateDefinitionAttribute(GD_QIR){
    auto qir_name = value.cast<::mlir::StringAttr>();
    this->qir_name = qir_name.getValue();
}
::mlir::LogicalResult QIRDefinition::verify(::isq::ir::DefgateOp op, int id, ::isq::ir::GateType ty, ::mlir::Attribute attribute){
    auto qir_name = attribute.dyn_cast_or_null<::mlir::StringAttr>();
    if(!qir_name){
        op->emitError() << "Definition #" << id
                            << " should specify a valid QIR gate name.";
        return mlir::failure();
    }
    return ::mlir::success();
}
::mlir::LogicalResult QIRDefinition::verifySymTable(::isq::ir::DefgateOp op, int id, ::isq::ir::GateType ty, ::mlir::Attribute value, ::mlir::SymbolTableCollection &symbolTable) {
    return ::mlir::success();
}


}
}