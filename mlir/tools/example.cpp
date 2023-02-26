#include "isq/Dialect.h"
#include <iostream>
#include <llvm/Support/TypeName.h>
#include <llvm/Support/raw_ostream.h>
#include <mlir/Dialect/Affine/IR/AffineOps.h>
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/IR/Attributes.h>
#include <mlir/IR/Builders.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/Location.h>
#include <mlir/IR/MLIRContext.h>
#include <mlir/IR/OperationSupport.h>
#include <mlir/IR/PatternMatch.h>
#include "isq/Enums.h"
#include "isq/Operations.h"
#include "isq/QAttrs.h"
using namespace isq::ir;

// An example app that prints sample isQ-IR.


int main(int argc, char **argv) {
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
    bld.create<DefgateOp>(loc, mlir::TypeAttr::get(GateType::get(ctx, 1, GateTrait::Hermitian)), "hadamard", "public", 
    mlir::ArrayAttr(), 
    mlir::ArrayAttr::get(ctx, mlir::ArrayRef<mlir::Attribute>{
        GateDefinition::get(ctx, mlir::StringAttr::get(ctx, "unitary"), DenseComplexF64MatrixAttr::get(ctx, {{(std::complex<double>)0.707, (std::complex<double>)0.707},
        {(std::complex<double>)0.707, (std::complex<double>)-0.707}}))
    }), 
    mlir::ArrayAttr::get(ctx, {}));
    auto func = bld.create<mlir::func::FuncOp>(loc, "bar", sig, mlir::StringAttr::get(ctx, "public"));
    auto blk = func.addEntryBlock();
    bld.setInsertionPoint(blk, blk->begin());
    bld.create<mlir::func::ReturnOp>(loc);
    
    

    // Print the IR.
    module->print(llvm::outs());

    return 0;
}