#include "isq/Dialect.h"
#include <iostream>
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/TypeName.h>
#include <llvm/Support/raw_ostream.h>
#include <mlir/Dialect/Affine/IR/AffineOps.h>
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/IR/AsmState.h>
#include <mlir/IR/Attributes.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/Location.h>
#include <mlir/IR/MLIRContext.h>
#include <mlir/IR/OpImplementation.h>
#include <mlir/IR/OperationSupport.h>
#include <mlir/IR/PatternMatch.h>
#include "isq/Enums.h"
#include "isq/Operations.h"
#include "isq/QAttrs.h"
#include <llvm/Support/Signals.h>
#include <mlir/IR/TypeRange.h>
using namespace isq::ir;

// An example app that prints sample isQ-IR.


int main(int argc, char **argv) {
    llvm::EnablePrettyStackTrace();
    mlir::registerAsmPrinterCLOptions();
    llvm::sys::PrintStackTraceOnErrorSignal(argv[0], false);
    mlir::DialectRegistry registry;
    isq::ir::ISQToolsInitialize(registry);
    mlir::MLIRContext context(registry);
    auto ctx = &context;
    ctx->loadAllAvailableDialects();
    
    auto loc = mlir::UnknownLoc::get(ctx);
    auto module = mlir::ModuleOp::create(loc);
    // declare qop.
    mlir::OpBuilder bld(module->getRegion(0));
    auto sig = mlir::FunctionType::get(ctx, {}, {});
    bld.create<DeclareQOpOp>(loc, "foo", "nested", 1, mlir::TypeAttr::get(sig));

    // define gate.
    auto ent = std::complex<double>(1.0 / (sqrt(2)));
    auto gatetype = GateType::get(ctx, 1, GateTrait::Hermitian);
    bld.create<DefgateOp>(loc, mlir::TypeAttr::get(gatetype), "hadamard", "public", 
    mlir::ArrayAttr(), 
    mlir::ArrayAttr::get(ctx, mlir::ArrayRef<mlir::Attribute>{
        GateDefinition::get(ctx, mlir::StringAttr::get(ctx, "unitary"), DenseComplexF64MatrixAttr::get(ctx, {{ent, ent},
        {ent, -ent}}))
    }), 
    mlir::ArrayAttr::get(ctx, {}));
    // Define function.
    auto qstate_type = QStateType::get(ctx);
    auto func_type = mlir::FunctionType::get(ctx, {qstate_type, qstate_type}, {qstate_type, qstate_type});
    auto func = bld.create<mlir::func::FuncOp>(loc, "bell", func_type, mlir::StringAttr::get(ctx, "public"));
    auto blk = func.addEntryBlock();
    mlir::Value q0 = blk->getArgument(0);
    mlir::Value q1 = blk->getArgument(1);
    bld.setInsertionPoint(blk, blk->begin());
    
    auto gate_h = bld.create<UseGateOp>(loc, gatetype, mlir::FlatSymbolRefAttr::get(ctx, "hadamard"),  mlir::ValueRange{});
    
    q0 = (bld.create<ApplyGateOp>(loc, mlir::ArrayRef<mlir::Type>{qstate_type}, gate_h.result(), mlir::ValueRange{q0}))->getResult(0);
    bld.create<mlir::func::ReturnOp>(loc, mlir::ValueRange{q0, q1});
    
    assert(mlir::succeeded(module.verify()));
    mlir::OpPrintingFlags flags;
    //flags.assumeVerified();
    // Print the IR.
    module->print(llvm::outs(), flags);

    return 0;
}