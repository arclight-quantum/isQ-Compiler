#include "isq/Operations.h"
#include "isq/QAttrs.h"
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
#include <mlir/IR/AffineExpr.h>
#include <optional>
#include "isq/Math.h"
#include "isq/GateDefTypes.h"
namespace isq{
namespace ir{

GateDefinition createMatrixDef(mlir::MLIRContext* ctx, const std::vector<std::vector<std::complex<double>>> & mat){
    mlir::SmallVector<mlir::SmallVector<std::complex<double>>> matrix_content;
    for(auto& row: mat){
        mlir::SmallVector<std::complex<double>> new_row;
        for(auto column: row){
            new_row.push_back(column);
        }
        matrix_content.push_back(std::move(new_row));
    }
    auto shape = mlir::RankedTensorType::get({2, 2}, mlir::ComplexType::get(mlir::Float64Type::get(ctx)));
    //auto shape = mlir::ShapedType::get
    return (GateDefinition::get(ctx, mlir::StringAttr::get(ctx, "unitary"), DenseComplexF64MatrixAttr::get(ctx, matrix_content)));
}


// Define by matrix.
MatrixDefinition::MatrixDefinition(::isq::ir::DefgateOp op, int id, ::isq::ir::GateType gateType, ::mlir::Attribute value): GateDefinitionAttribute(GD_MATRIX){
    auto arr = value.dyn_cast_or_null<DenseComplexF64MatrixAttr>();
    assert(arr);
    mat = arr.toMatrixVal();
}

const DenseComplexF64MatrixAttr::MatrixVal& MatrixDefinition::getMatrix() const{
    return this->mat;
}
::mlir::LogicalResult MatrixDefinition::verify(::isq::ir::DefgateOp op, int id, ::isq::ir::GateType ty, ::mlir::Attribute attribute) {
    if(op.shape()){
        op->emitError()
            << "Definition #" << id << " is a matrix definition and should not be used with gate arrays.";
        return mlir::failure();
    }
    // try to get as a matrix.
    auto arr = attribute.dyn_cast_or_null<DenseComplexF64MatrixAttr>();
    if (!arr) {
        op->emitError()
            << "Definition #" << id << " should use a matrix as value.";
        return mlir::failure();
    }
    auto mat = arr.toMatrixVal();
    math::InputSmallMatrix wrapper;
    wrapper.body = std::make_unique<math::InputSmallMatrix::Ty>(std::move(mat));
    auto dim = math::checkDimensionality(wrapper);
    if (!dim.hasValue()) {
        op->emitError()
            << "Definition #" << id << " input is not a square matrix.";
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
    this->decomposition = mlir::SymbolTable::lookupNearestSymbolFrom<mlir::func::FuncOp>(op, callee);
    assert(this->decomposition);
}
mlir::func::FuncOp DecompositionDefinition::getDecomposedFunc(){
    return this->decomposition;
}
::mlir::LogicalResult DecompositionDefinition::verify(::isq::ir::DefgateOp op, int id, ::isq::ir::GateType ty, ::mlir::Attribute attribute){
    return ::mlir::success();
}
::mlir::LogicalResult DecompositionDefinition::verifySymTable(::isq::ir::DefgateOp op, int id, ::isq::ir::GateType ty, ::mlir::Attribute value, ::mlir::SymbolTableCollection &symbolTable) {
    if(!value.isa<::mlir::SymbolRefAttr>()){
        value.dump();
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
    auto funcop = ::llvm::dyn_cast_or_null<::mlir::func::FuncOp>(sym);
    if(!funcop){
        op->emitError() << "Definition #" << id
                            << " does not refer to a valid symbol by `builtin.func`.";
        return mlir::failure();
    }
    // construct func signature.
    ::mlir::SmallVector<::mlir::Type> argtypes;
    ::mlir::SmallVector<::mlir::Type> returntypes;
    for(auto extra_arg: op.parameters()){
        argtypes.push_back(extra_arg.cast<mlir::TypeAttr>().getValue());
    }
    for(auto i=0; i<ty.getSize(); i++){
        argtypes.push_back(::isq::ir::QStateType::get(op.getContext()));
        returntypes.push_back(::isq::ir::QStateType::get(op.getContext()));
    }
    auto required_functype = ::mlir::FunctionType::get(op->getContext(), argtypes, returntypes);
    if(required_functype!=funcop.getFunctionType()){
            op->emitError() << "Definition #" << id
                            << " does not have signature "<<required_functype<<".";
        return mlir::failure();
    }
    return ::mlir::success();
}
// Define by composition-reference version.

// Define by decomposition.
DecompositionRawDefinition::DecompositionRawDefinition(::isq::ir::DefgateOp op, int id, ::isq::ir::GateType gateType, ::mlir::Attribute value): GateDefinitionAttribute(GD_DECOMPOSITION_RAW){
    auto callee = value.cast<::mlir::SymbolRefAttr>();
    this->decomposition = mlir::SymbolTable::lookupNearestSymbolFrom<mlir::func::FuncOp>(op, callee);
    assert(this->decomposition);
}
mlir::func::FuncOp DecompositionRawDefinition::getDecomposedFunc(){
    return this->decomposition;
}
::mlir::LogicalResult DecompositionRawDefinition::verify(::isq::ir::DefgateOp op, int id, ::isq::ir::GateType ty, ::mlir::Attribute attribute){
    return ::mlir::success();
}
::mlir::LogicalResult DecompositionRawDefinition::verifySymTable(::isq::ir::DefgateOp op, int id, ::isq::ir::GateType ty, ::mlir::Attribute value, ::mlir::SymbolTableCollection &symbolTable) {
    if(!value.isa<::mlir::SymbolRefAttr>()){
        value.dump();
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
    auto funcop = ::llvm::dyn_cast_or_null<::mlir::func::FuncOp>(sym);
    if(!funcop){
        op->emitError() << "Definition #" << id
                            << " does not refer to a valid symbol by `builtin.func`.";
        return mlir::failure();
    }
    // construct func signature.
    ::mlir::SmallVector<::mlir::Type> argtypes;
    ::mlir::SmallVector<::mlir::Type> returntypes;
    for(auto extra_arg: op.parameters()){
        argtypes.push_back(extra_arg.cast<mlir::TypeAttr>().getValue());
    }
    mlir::AffineExpr d0, s0;
    mlir::bindDims(op.getContext(), d0);
    mlir::bindSymbols(op.getContext(), s0);
    auto affine_map = mlir::AffineMap::get(1, 1, d0+s0);
    auto memref_1_qstate = mlir::MemRefType::get(mlir::ArrayRef<int64_t>{1},::isq::ir::QStateType::get(op.getContext()), affine_map);
    for(auto i=0; i<ty.getSize(); i++){
        argtypes.push_back(memref_1_qstate);
    }
    auto required_functype = ::mlir::FunctionType::get(op->getContext(), argtypes, returntypes);
    if(required_functype!=funcop.getFunctionType()){
            op->emitError() << "Definition #" << id
                            << " does not have signature "<<required_functype<<".";
        return mlir::failure();
    }
    return ::mlir::success();
}




QIRDefinition::QIRDefinition(::isq::ir::DefgateOp op, int id, ::isq::ir::GateType gateType, ::mlir::Attribute value): GateDefinitionAttribute(GD_QIR){
    auto qir_name = value.cast<::mlir::FlatSymbolRefAttr>();
    this->qir_name = qir_name;
}
::mlir::FlatSymbolRefAttr QIRDefinition::getQIRName(){
    return this->qir_name;
}
::mlir::LogicalResult QIRDefinition::verify(::isq::ir::DefgateOp op, int id, ::isq::ir::GateType ty, ::mlir::Attribute attribute){
    auto qir_name = attribute.dyn_cast_or_null<::mlir::FlatSymbolRefAttr>();
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




OracleTableDefinition::OracleTableDefinition(::isq::ir::DefgateOp op, int id, ::isq::ir::GateType gateType, ::mlir::Attribute value): GateDefinitionAttribute(GD_ORACLE_TABLE){
    auto arr = value.dyn_cast_or_null<::mlir::ArrayAttr>();
    assert(arr);
    for (auto row : arr) {
        auto row_arr = row.dyn_cast_or_null<::mlir::ArrayAttr>();
        std::vector<int> row_vec;
        assert(row_arr);
        for (auto element : row_arr) {
            auto element_attr = element.dyn_cast_or_null<mlir::IntegerAttr>();
            assert(element_attr);
            row_vec.push_back(element_attr.getInt());
        }
        this->value.push_back(std::move(row_vec));
    }
}

::mlir::LogicalResult OracleTableDefinition::verify(::isq::ir::DefgateOp op, int id, ::isq::ir::GateType ty, ::mlir::Attribute attribute){
    return ::mlir::success();
}

::mlir::LogicalResult OracleTableDefinition::verifySymTable(::isq::ir::DefgateOp op, int id, ::isq::ir::GateType ty, ::mlir::Attribute value, ::mlir::SymbolTableCollection &symbolTable) {
    return ::mlir::success();
}

const std::vector<std::vector<int>>& OracleTableDefinition::getValue() const{
    return this->value;
}

}
}