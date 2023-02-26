#include "isq/Operations.h"
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/Pass/PassManager.h>
#include <mlir/Rewrite/FrozenRewritePatternSet.h>
#include <mlir/Pass/Pass.h>
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
namespace isq::ir::passes{
namespace{
// Remove gphase.
class RuleRemoveAccumulateGPhaseAux : public mlir::OpRewritePattern<AccumulateGPhase>{
public:
    RuleRemoveAccumulateGPhaseAux(mlir::MLIRContext* ctx): mlir::OpRewritePattern<AccumulateGPhase>(ctx, 1){}
    mlir::LogicalResult matchAndRewrite(AccumulateGPhase op,  mlir::PatternRewriter &rewriter) const override{
        rewriter.eraseOp(op);
        return mlir::success();
    }
};
class RuleRemoveApplyGPhaseAux : public mlir::OpRewritePattern<ApplyGPhase>{
public:
    RuleRemoveApplyGPhaseAux(mlir::MLIRContext* ctx): mlir::OpRewritePattern<ApplyGPhase>(ctx, 1){}
    mlir::LogicalResult matchAndRewrite(ApplyGPhase op,  mlir::PatternRewriter &rewriter) const override{
        rewriter.eraseOp(op);
        return mlir::success();
    }
};
}

const char* ISQ_GPHASE_REMOVED = "ISQ_GPHASE_REMOVED";

struct RemoveGPhaseFuncPass : public mlir::PassWrapper<RemoveGPhaseFuncPass, mlir::OperationPass<mlir::func::FuncOp>>{
    void runOnOperation() override {
        mlir::func::FuncOp m = this->getOperation();
        auto ctx = m->getContext();
        // Mark the function as gphase-unreliable.
        bool has_gphase = false;
        m->walk([&](mlir::Operation* op){
            if(llvm::isa<ApplyGPhase>(op) || llvm::isa<AccumulateGPhase>(op)){
                has_gphase=true;
            }
        });
        if(has_gphase){
            m->setAttr(ISQ_GPHASE_REMOVED, mlir::UnitAttr::get(ctx));
        }
        do{
            mlir::RewritePatternSet rps(ctx);
            rps.add<RuleRemoveAccumulateGPhaseAux>(ctx);
            rps.add<RuleRemoveApplyGPhaseAux>(ctx);
            mlir::FrozenRewritePatternSet frps(std::move(rps));
            (void)mlir::applyPatternsAndFoldGreedily(m.getOperation(), frps);
        }while(0);
    }

};

struct RemoveGPhasePass : public mlir::PassWrapper<RemoveGPhasePass, mlir::OperationPass<mlir::ModuleOp>>{
    void runOnOperation() override {
        mlir::ModuleOp module = getOperation();
        mlir::PassManager pm(module->getContext());
        pm.addNestedPass<mlir::func::FuncOp>(std::make_unique<RemoveGPhaseFuncPass>());
        if(failed(pm.run(module))){
            return signalPassFailure();
        }
    }
    mlir::StringRef getArgument() const final {
        return "isq-remove-gphase";
    }
    mlir::StringRef getDescription() const final {
        return  "Remove global phase. Mark the decomposition as unusable for decorate-folding.";
    }
};

void registerRemoveGPhase(){
    mlir::PassRegistration<RemoveGPhasePass>();
}

}