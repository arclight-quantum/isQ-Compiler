// convert control-famous gates into control-u3.
// control-u3 can be then decomposed using mcdecompose.

#include "isq/Operations.h"
#include "isq/QTypes.h"
#include "isq/passes/Passes.h"
#include <llvm/Support/Casting.h>
#include <mlir/Dialect/Arithmetic/IR/Arithmetic.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/MLIRContext.h>
#include <mlir/IR/PatternMatch.h>
#include <mlir/IR/SymbolTable.h>
#include <mlir/Pass/PassRegistry.h>
#include <mlir/Rewrite/FrozenRewritePatternSet.h>
#include <mlir/Support/LogicalResult.h>
#include <mlir/Pass/Pass.h>
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include <llvm/ADT/SmallString.h>
namespace isq::ir::passes{
// Convert famous parametric sq gates into controlled U3.
// Rx, Ry and Rz.
class ConvertFamousParamSQIntoU3Rule : public mlir::OpRewritePattern<UseGateOp>{
public:
    mlir::ModuleOp rootModule;
    ConvertFamousParamSQIntoU3Rule(mlir::MLIRContext* ctx, mlir::ModuleOp rootModule): mlir::OpRewritePattern<UseGateOp>(ctx, 1), rootModule(rootModule){
        
    }


    mlir::LogicalResult matchAndRewrite(UseGateOp use, mlir::PatternRewriter& rewriter) const override{
        auto defgate = llvm::dyn_cast_or_null<DefgateOp>(mlir::SymbolTable::lookupNearestSymbolFrom(use, use.name()));
        if(!defgate) return mlir::failure();
        auto ctx = rewriter.getContext();
        
        
        if(isFamousGate(defgate, "Rx")){
            auto theta = use.parameters()[0];
            auto pi_2 = rewriter.create<mlir::arith::ConstantFloatOp>(
                    ::mlir::UnknownLoc::get(ctx),
                    ::llvm::APFloat(M_PI / 2),
                    ::mlir::Float64Type::get(ctx)
            );
            auto neg_pi_2 = rewriter.create<mlir::arith::ConstantFloatOp>(
                ::mlir::UnknownLoc::get(ctx),
                ::llvm::APFloat(-M_PI / 2),
                ::mlir::Float64Type::get(ctx)
            );
            ::mlir::SmallVector<mlir::Value> theta_v = {theta, neg_pi_2, pi_2};
            rewriter.replaceOpWithNewOp<UseGateOp>(
                use,
                use.getType(),
                mlir::FlatSymbolRefAttr::get(ctx, getFamousName("U3")),
                theta_v
            );
            return mlir::success();
        }
        if(isFamousGate(defgate, "Ry")){
            auto theta = use.parameters()[0];
            auto zero = rewriter.create<mlir::arith::ConstantFloatOp>(
                ::mlir::UnknownLoc::get(ctx),
                ::llvm::APFloat(0.0),
                ::mlir::Float64Type::get(ctx)
            );
            ::mlir::SmallVector<mlir::Value> theta_v = {theta, zero, zero};
            rewriter.replaceOpWithNewOp<UseGateOp>(
                use,
                use.getType(),
                mlir::FlatSymbolRefAttr::get(ctx, getFamousName("U3")),
                theta_v
            );
            return mlir::success();
        }
        if(isFamousGate(defgate, "Rz")){
            auto theta = use.parameters()[0];
            auto zero = rewriter.create<mlir::arith::ConstantFloatOp>(
                ::mlir::UnknownLoc::get(ctx),
                ::llvm::APFloat(0.0),
                ::mlir::Float64Type::get(ctx)
            );
            ::mlir::SmallVector<mlir::Value> theta_v ={zero, zero, theta};
            rewriter.replaceOpWithNewOp<UseGateOp>(
                use,
                use.getType(),
                mlir::FlatSymbolRefAttr::get(ctx, getFamousName("U3")),
                theta_v
            );
            return mlir::success();
        }
        return mlir::failure();
    }
};

class ConvertFamousParamSQIntoU3Pass : public mlir::PassWrapper<ConvertFamousParamSQIntoU3Pass, mlir::OperationPass<mlir::ModuleOp>>{
    void runOnOperation() override{
        mlir::ModuleOp m = this->getOperation();
        auto ctx = m->getContext();
        do{
            mlir::RewritePatternSet rps(ctx);
            rps.add<ConvertFamousParamSQIntoU3Rule>(ctx, m);
            addLegalizeTraitsRules(rps);
            mlir::FrozenRewritePatternSet frps(std::move(rps));
            (void)mlir::applyPatternsAndFoldGreedily(m.getOperation(), frps);
        }while(0);
    }
    mlir::StringRef getArgument() const final {
        return "isq-convert-famous-rot";
    }
    mlir::StringRef getDescription() const final {
        return  "Converting famous single qubit rotations (Rx, Ry, Rz) into rotated U3.";
    }
};

void registerSQRot2U3(){
    mlir::PassRegistration<ConvertFamousParamSQIntoU3Pass>();
}

}
