#include "isq/GateDefTypes.h"
#include "isq/Operations.h"
#include "isq/QAttrs.h"
#include "isq/QTypes.h"
#include "isq/passes/Passes.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/IR/SymbolTable.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Rewrite/FrozenRewritePatternSet.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Support/LogicalResult.h"
#include "llvm/Support/Casting.h"
#include <optional>
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
namespace isq{
namespace ir{
namespace passes{
// Inline apply-ops into decomposition.
class ExpandDecompositionRewrite : public mlir::OpRewritePattern<isq::ir::ApplyGateOp>{
    mlir::ModuleOp rootModule;
public:
    ExpandDecompositionRewrite(mlir::MLIRContext* ctx, mlir::ModuleOp module): mlir::OpRewritePattern<isq::ir::ApplyGateOp>(ctx, 1), rootModule(module){

    }
    bool hasDecomposition(DefgateOp op) const{
        auto defs = *op.definition();
        auto id=0;
        for(auto def: defs.getAsRange<GateDefinition>()){
            auto d = AllGateDefs::parseGateDefinition(op, id, op.type(), def);
            if(d==std::nullopt) {
                llvm_unreachable("bad");
            }
            if(auto decomp = llvm::dyn_cast_or_null<DecompositionDefinition>(&**d)){
                return true;
            }
            id++;
        }
        return false;
    }
    mlir::LogicalResult matchAndRewrite(isq::ir::ApplyGateOp op,  mlir::PatternRewriter &rewriter) const override{
        auto use_op = mlir::dyn_cast_or_null<UseGateOp>(op.gate().getDefiningOp());
        if(!use_op) return mlir::failure();
        auto defgate = mlir::SymbolTable::lookupNearestSymbolFrom<DefgateOp>(use_op.getOperation(), use_op.name());
        assert(defgate);
        if(!defgate.definition()) return mlir::failure();
        /*
        auto has_decomp = this->hasDecomposition(defgate);
        if(!has_decomp && use_op.parameters().size()>0){
            return mlir::failure(); // Only matrix-gates are supported.
        }
        */
        
        int id = 0;
        for(auto def: defgate.definition()->getAsRange<GateDefinition>()){
            auto d = AllGateDefs::parseGateDefinition(defgate, id, defgate.type(), def);
            if(d==std::nullopt) return mlir::failure();
            if(llvm::dyn_cast_or_null<QIRDefinition>(&**d)){
                // A QIR implementation is good enough.
                if(!isFamousGate(defgate, "Toffoli")) // However we decompose toffoli.
                    return mlir::failure();
            }
            id++;
        }
        id=0;
        for(auto def: defgate.definition()->getAsRange<GateDefinition>()){
            auto d = AllGateDefs::parseGateDefinition(defgate, id, defgate.type(), def);
            if(d==std::nullopt) return mlir::failure();
            auto decomp = llvm::dyn_cast_or_null<DecompositionDefinition>(&**d);
            if(!decomp){
                id++;
                continue;
            }
            auto decomp_f = decomp->getDecomposedFunc();
            mlir::SmallVector<mlir::Value> calling_args;
            calling_args.append(use_op.parameters().begin(), use_op.parameters().end());
            calling_args.append(op.args().begin(), op.args().end());
            rewriter.replaceOpWithNewOp<mlir::func::CallOp>(op.getOperation(), decomp_f, calling_args);
            return mlir::success();

        }
        return mlir::failure();
    }
};

struct ExpandDecompositionPass : public mlir::PassWrapper<ExpandDecompositionPass, mlir::OperationPass<mlir::ModuleOp>>{
    void runOnOperation() override {
        mlir::ModuleOp m = this->getOperation();
        auto ctx = m->getContext();
        mlir::RewritePatternSet rps(ctx);
        rps.add<ExpandDecompositionRewrite>(ctx, m);
        mlir::FrozenRewritePatternSet frps(std::move(rps));
        (void)mlir::applyPatternsAndFoldGreedily(m.getOperation(), frps);
    }
  mlir::StringRef getArgument() const final {
    return "isq-expand-decomposition";
  }
  mlir::StringRef getDescription() const final {
    return  "Lower isq.apply into its decomposition form.";
  }
};

void registerExpandDecomposition(){
    mlir::PassRegistration<ExpandDecompositionPass>();
}



}
}
}