#include "isq/Dialect.h"
#include "isq/Lower.h"
#include "isq/Operations.h"
#include "isq/QTypes.h"
#include "isq/GateDefTypes.h"
#include "mlir/Conversion/AffineToStandard/AffineToStandard.h"
#include "mlir/Conversion/ArithmeticToLLVM/ArithmeticToLLVM.h"
#include "mlir/Conversion/MemRefToLLVM/MemRefToLLVM.h"
#include "mlir/Conversion/LLVMCommon/TypeConverter.h"
#include "mlir/Conversion/ReconcileUnrealizedCasts/ReconcileUnrealizedCasts.h"
#include "mlir/Conversion/SCFToControlFlow/SCFToControlFlow.h"
#include "mlir/Conversion/ControlFlowToLLVM/ControlFlowToLLVM.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arithmetic/IR/Arithmetic.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinDialect.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/OperationSupport.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Rewrite/FrozenRewritePatternSet.h"
#include "mlir/Transforms/DialectConversion.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include "mlir/Dialect/Func/Transforms/FuncConversions.h"
#include "llvm/Support/raw_ostream.h"
#include <mlir/Dialect/Arithmetic/Transforms/Passes.h>
namespace isq{
namespace ir{
namespace passes{

namespace qir_rep_to_llvm{
using namespace mlir;
struct QIRRepToLLVMPass : public mlir::PassWrapper<QIRRepToLLVMPass, mlir::OperationPass<mlir::ModuleOp>>{

    void runOnOperation() override {
        ConversionTarget target(getContext());
        //target.addLegalDialect<LLVM::LLVMDialect, mlir::scf::SCFDialect, mlir::arith::ArithmeticDialect>();
        target.addLegalDialect<LLVM::LLVMDialect>();
        target.addLegalOp<mlir::ModuleOp>();
        LLVMTypeConverter typeConverter(&getContext());
        //llvm::errs() << "Host index bit width: " <<typeConverter.getIndexTypeBitwidth() << "\n";
        

        typeConverter.addConversion([&](isq::ir::QIRQubitType type) {
            return LLVM::LLVMPointerType::get(LLVM::LLVMStructType::getOpaque(StringRef("Qubit"), &getContext()));
        });
        typeConverter.addConversion([&](isq::ir::QIRResultType type) {
            return LLVM::LLVMPointerType::get(LLVM::LLVMStructType::getOpaque(StringRef("Result"), &getContext()));
        });

        RewritePatternSet patterns(&getContext());
        populateAffineToStdConversionPatterns(patterns);
        populateSCFToControlFlowConversionPatterns(patterns);
        populateMemRefToLLVMConversionPatterns(typeConverter, patterns);
        arith::populateArithmeticExpandOpsPatterns(patterns);
        arith::populateArithmeticToLLVMConversionPatterns(typeConverter, patterns);
        cf::populateControlFlowToLLVMConversionPatterns(typeConverter, patterns);
        //populateStdToLLVMConversionPatterns(typeConverter, patterns);

        auto module = getOperation();

        if (failed(applyFullConversion(module, target, std::move(patterns))))
            signalPassFailure();
    }
  mlir::StringRef getArgument() const final {
    return "isq-lower-qir-rep-to-llvm";
  }
  mlir::StringRef getDescription() const final {
    return  "Lower QIR subdialect to standard LLVM.";
  }
};


}

void registerQIR2LLVM(){
    using namespace qir_rep_to_llvm;
    mlir::PassRegistration<QIRRepToLLVMPass>();
}

}
}
}