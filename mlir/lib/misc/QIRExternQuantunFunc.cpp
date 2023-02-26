#include "isq/Lower.h"
#include "isq/QTypes.h"
#include "mlir/Dialect/Arithmetic/IR/Arithmetic.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "llvm/Support/raw_ostream.h"
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <set>
#include <math.h>
#include <iostream>
#include "isq/QAttrs.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/PatternMatch.h"

using namespace isq::ir;
using namespace isq::ir::lower;
using namespace Eigen;
using synthesis::ComplexPair;

using namespace mlir;
using namespace std;

namespace {
const std::string main_func = "__isq_main";
const std::string global_initializer = "__isq__global_initialize";

const std::string qir_printf = "printf";
const std::string qir_alloc_qubit_array = "__quantum__rt__qubit_allocate_array";
const std::string qir_alloc_qubit = "__quantum__rt__qubit_allocate";
const std::string qir_release_qubit = "__quantum__rt__qubit_release";
const std::string qir_measure = "__quantum__qis__measure";
const std::string qir_result_get_one = "__quantum__rt__result_get_one";
const std::string qir_result_equal = "__quantum__rt__result_equal";
const std::string qir_reset = "__quantum__qis__reset";
const std::string qir_gate_head = "__quantum__qis__";
const std::string isq_decomposed_head = "__isq__decomposed__";
const std::string qir_bp = "__quantum__qis__bp";
const std::string qir_print_i64 = "__quantum__qis__isq_print_i64";
const std::string qir_print_f64 = "__quantum__qis__isq_print_f64";
using mlir::func::FuncOp;
using mlir::func::CallOp;
using mlir::func::ReturnOp;
void createExternOp(::mlir::PatternRewriter& rewriter, ::mlir::MLIRContext* ctx, ::mlir::StringRef externOpName, ::mlir::ArrayRef<::mlir::Type> argsType, ::mlir::ArrayRef<::mlir::Type> returnType){
    auto fntype = FunctionType::get(ctx, argsType, returnType);
    rewriter.create<FuncOp>(UnknownLoc::get(ctx), externOpName, fntype, ::mlir::StringAttr::get(ctx, "private"));
}

CallOp callExternOp(::mlir::PatternRewriter& rewriter, ::mlir::Location loc, ::mlir::StringRef externOpName, ::mlir::ArrayRef<::mlir::Value> args, ::mlir::ArrayRef<::mlir::Type> returnType){
    return rewriter.create<CallOp>(loc, ::mlir::FlatSymbolRefAttr::get(rewriter.getContext(), externOpName), returnType, args);
}

CallOp safeCallExternOp(::mlir::ModuleOp modul, ::mlir::PatternRewriter& rewriter, ::mlir::Location loc, ::mlir::StringRef externOpName, ::mlir::ArrayRef<::mlir::Value> args, ::mlir::ArrayRef<::mlir::Type> returnType){
    auto ctx = modul->getContext();
    ::llvm::SmallVector<::mlir::Type> argTypes;
    for(auto& val: args){
        argTypes.push_back(val.getType());
    }
    if(auto defined_func = modul.lookupSymbol<FuncOp>(externOpName)){
        assert(defined_func.getFunctionType() == FunctionType::get(ctx, argTypes, returnType));
    }else{
        PatternRewriter::InsertionGuard insertGuard(rewriter);
        rewriter.setInsertionPointToStart(modul.getBody());
        createExternOp(rewriter, ctx, externOpName, argTypes, returnType);
    }
    return callExternOp(rewriter, loc, externOpName, args, returnType);
}

}
string QIRExternQuantumFunc::getMainFuncName(){
    return main_func;
}

QStateType QIRExternQuantumFunc::getQStateType(::mlir::MLIRContext *ctx){
    return QStateType::get(ctx);
}
QIRQubitType QIRExternQuantumFunc::getQIRQubitType(::mlir::MLIRContext *ctx){
    return QIRQubitType::get(ctx);
}
QIRResultType QIRExternQuantumFunc::getQIRResultType(::mlir::MLIRContext *ctx){
    return QIRResultType::get(ctx);
}
mlir::IndexType QIRExternQuantumFunc::getIndexType(::mlir::MLIRContext *ctx){
    return IndexType::get(ctx);
}
mlir::IntegerType QIRExternQuantumFunc::getI64Type(::mlir::MLIRContext *ctx){
    return IntegerType::get(ctx, 64);
}
mlir::IntegerType QIRExternQuantumFunc::getI1Type(::mlir::MLIRContext *ctx){
    return IntegerType::get(ctx, 1);
}
mlir::Float64Type QIRExternQuantumFunc::getF64Type(::mlir::MLIRContext *ctx){
    return Float64Type::get(ctx);
}


void QIRExternQuantumFunc::breakPoint(::mlir::Location loc, PatternRewriter& rewriter, ModuleOp module, Value i){
    assert(i.getType() == getIndexType(module->getContext()));
    auto indexcast = rewriter.create<arith::IndexCastOp>(loc, getI64Type(module.getContext()),i);
    safeCallExternOp(module, rewriter, loc, qir_bp, ArrayRef<Value>{indexcast.getOut()}, ArrayRef<Type>{});
}
void QIRExternQuantumFunc::printInt(::mlir::Location loc, PatternRewriter& rewriter, ModuleOp module, Value i){
    assert(i.getType() == getIndexType(module->getContext()));
    auto indexcast = rewriter.create<arith::IndexCastOp>(loc, getI64Type(module.getContext()), i);
    safeCallExternOp(module, rewriter, loc, qir_print_i64, ArrayRef<Value>{indexcast.getOut()}, ArrayRef<Type>{});
}
void QIRExternQuantumFunc::printFloat(::mlir::Location loc, PatternRewriter& rewriter, ModuleOp module, Value f){
    assert(f.getType() == getF64Type(module->getContext()));
    safeCallExternOp(module, rewriter, loc, qir_print_f64, ArrayRef<Value>{f}, ArrayRef<Type>{});
}
Value QIRExternQuantumFunc::allocQubit(::mlir::Location loc, PatternRewriter& rewriter, ModuleOp module){
    auto op = safeCallExternOp(module, rewriter, loc, qir_alloc_qubit, ArrayRef<Value>{}, ArrayRef<Type>{getQIRQubitType(module->getContext())});
    return op->getResult(0);
}
void QIRExternQuantumFunc::releaseQubit(::mlir::Location loc, PatternRewriter &rewriter, ModuleOp module, Value q){
    assert(q.getType().isa<QIRQubitType>());
    safeCallExternOp(module, rewriter, loc, qir_release_qubit, ArrayRef<Value>{q}, ArrayRef<Type>{});
}
Value QIRExternQuantumFunc::measureQubit(::mlir::Location loc, PatternRewriter &rewriter, ModuleOp module, Value q){
    assert(q.getType().isa<QIRQubitType>());
    auto i1 = getI1Type(module.getContext());
    auto result = getQIRResultType(module.getContext());
    auto meas_result = safeCallExternOp(module, rewriter, loc, qir_measure, ArrayRef<Value>{q}, ArrayRef<Type>{result}).getResult(0);
    auto meas_one = safeCallExternOp(module, rewriter, loc, qir_result_get_one, ArrayRef<Value>{}, ArrayRef<Type>{result}).getResult(0);
    return safeCallExternOp(module, rewriter, loc, qir_result_equal, ArrayRef<Value>{meas_result, meas_one}, ArrayRef<Type>{i1}).getResult(0);
}
void QIRExternQuantumFunc::reset(::mlir::Location loc, PatternRewriter &rewriter, ModuleOp module, Value q){
    assert(q.getType().isa<QIRQubitType>());
    safeCallExternOp(module, rewriter, loc, qir_reset, ArrayRef<Value>{q}, ArrayRef<Type>{});
}
void QIRExternQuantumFunc::callQIRGate(::mlir::Location loc, PatternRewriter &rewriter, ModuleOp module, ::mlir::StringRef gateName, ::mlir::ArrayRef<Value> arguments){
    safeCallExternOp(module, rewriter, loc, gateName, arguments, ArrayRef<Type>{});
}