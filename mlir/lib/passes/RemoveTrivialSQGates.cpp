#include "isq/GateDefTypes.h"
#include "isq/Operations.h"
#include "isq/QSynthesis.h"
#include "isq/passes/Passes.h"
#include <llvm/Support/Casting.h>
#include <mlir/Dialect/Arith/IR/Arith.h>
#include <mlir/IR/Attributes.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/Location.h>
#include <mlir/IR/PatternMatch.h>
#include <mlir/IR/SymbolTable.h>
#include <mlir/Pass/Pass.h>
#include <mlir/Pass/PassRegistry.h>
#include <mlir/Rewrite/FrozenRewritePatternSet.h>
#include <mlir/Support/LLVM.h>
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#define EPS (1e-6)
namespace isq::ir::passes{
struct RemoveTrivialConstantU3 : public mlir::OpRewritePattern<ApplyGateOp>{
    RemoveTrivialConstantU3(mlir::MLIRContext* ctx): mlir::OpRewritePattern<ApplyGateOp>(ctx, 1){}
    mlir::LogicalResult matchAndRewrite(ApplyGateOp op, mlir::PatternRewriter& rewriter) const override{
        auto ctx = op->getContext();
        auto decorate_op = llvm::dyn_cast<DecorateOp>(op.getGate().getDefiningOp());
        UseGateOp usegate_op;
        if(decorate_op){
            usegate_op = llvm::dyn_cast<UseGateOp>(decorate_op.getArgs().getDefiningOp());
        }else{
            usegate_op = llvm::dyn_cast<UseGateOp>(op.getGate().getDefiningOp());
        } 
        if(!usegate_op) return mlir::failure();
        auto gatedef = llvm::dyn_cast_or_null<DefgateOp>(mlir::SymbolTable::lookupNearestSymbolFrom(usegate_op, usegate_op.getName()));
        if(!gatedef) return mlir::failure();
        if(!isFamousGate(gatedef, "U3")) return mlir::failure();
        mlir::SmallVector<mlir::Value> operands;
        for(auto v: op.getArgs()){
            operands.push_back(v);
        }
        auto theta = usegate_op.getParameters()[0];
        auto phi = usegate_op.getParameters()[1];
        auto lam = usegate_op.getParameters()[2];
        double t, p, l;
        if(auto cop = mlir::dyn_cast_or_null<mlir::arith::ConstantFloatOp>(theta.getDefiningOp())){
            t=cop.value().convertToDouble();
        }else{
            return mlir::failure();
        }
        if(auto cop = mlir::dyn_cast_or_null<mlir::arith::ConstantFloatOp>(phi.getDefiningOp())){
            p=cop.value().convertToDouble();
        }else{
            return mlir::failure();
        }
        if(auto cop = mlir::dyn_cast_or_null<mlir::arith::ConstantFloatOp>(lam.getDefiningOp())){
            l=cop.value().convertToDouble();
        }else{
            return mlir::failure();
        }
        if(abs(t)>EPS || abs(p)>EPS || abs(l)>EPS){
            return mlir::failure();
        }
        do{
            auto lambda = std::complex<double>(l);
            auto phi = std::complex<double>(p);
            auto theta = std::complex<double>(t);
            auto i = std::complex<double>(0, 1.0);
            ::mlir::SmallVector<::mlir::SmallVector<std::complex<double>>> m = {
                {
                    std::exp((-i*(phi+lambda)/2.0)) * std::cos(theta / 2.0),
                    -std::exp((i * (lambda-phi)/2.0)) * std::sin(theta / 2.0),
                },
                {
                    std::exp((i * (phi-lambda)/2.0)) * std::sin(theta / 2.0),
                    std::exp((i * (phi + lambda))) * std::cos(theta / 2.0),
                }
            };
            auto first = m[0][0];
            // TODO: what about converting nearly-e^{i\theta} SQ gates into gphase?
            for(auto i=0; i<m.size(); i++){
                for(auto j=0; j<m.size(); j++){
                    auto expected = i==j?first:0.0;
                    if(std::norm(m[i][j]-expected)>EPS){
                        return mlir::failure();
                    }
                }
            }
        }while(0);
        // otherwise
        rewriter.replaceOp(op, op.getArgs());
        return mlir::success();
    }
};

struct RemoveTrivialKnownGate : public mlir::OpRewritePattern<ApplyGateOp>{
    RemoveTrivialKnownGate(mlir::MLIRContext* ctx): mlir::OpRewritePattern<ApplyGateOp>(ctx, 1){}
    mlir::LogicalResult matchAndRewrite(ApplyGateOp op, mlir::PatternRewriter& rewriter) const override{
        auto ctx = op->getContext();
        auto decorate_op = llvm::dyn_cast<DecorateOp>(op.getGate().getDefiningOp());
        if(!decorate_op) return mlir::failure();
        auto usegate_op = llvm::dyn_cast<UseGateOp>(decorate_op.getArgs().getDefiningOp());
        if(!usegate_op) return mlir::failure();
        auto defgate = llvm::dyn_cast_or_null<DefgateOp>(mlir::SymbolTable::lookupNearestSymbolFrom(usegate_op, usegate_op.getName()));
        if(!defgate) return mlir::failure();
        auto id=0;
        for(auto def: defgate.getDefinition()->getAsRange<GateDefinition>()){
            auto d = AllGateDefs::parseGateDefinition(defgate, id, defgate.getType(), def);
            if(d==std::nullopt) return mlir::failure();
            if(auto mat = llvm::dyn_cast_or_null<MatrixDefinition>(&**d)){
                auto m = mat->getMatrix();
                // TODO: what about converting nearly-e^{i\theta} SQ gates into gphase?
                for(auto i=0; i<m.size(); i++){
                    for(auto j=0; j<m.size(); j++){
                        auto expected = i==j?1.0:0.0;
                        if(std::norm(m[i][j]-expected)>EPS){
                            return mlir::failure();
                        }
                    }
                }
                // otherwise
                rewriter.replaceOp(op, op.getArgs());
                return mlir::success();
            }
        }
        return mlir::failure();

    }
};
struct RemoveTrivialSQGatesPass : public mlir::PassWrapper<RemoveTrivialSQGatesPass, mlir::OperationPass<mlir::ModuleOp>>{
    void runOnOperation() override {
        mlir::ModuleOp m = this->getOperation();
        auto ctx = m->getContext();
        do{
            mlir::RewritePatternSet rps(ctx);
            rps.add<RemoveTrivialConstantU3>(ctx);
            rps.add<RemoveTrivialKnownGate>(ctx);
            mlir::FrozenRewritePatternSet frps(std::move(rps));
            (void)mlir::applyPatternsAndFoldGreedily(m.getOperation(), frps);
        }while(0);
        
    }
    mlir::StringRef getArgument() const final {
        return "isq-remove-trivial-sq-gates";
    }
    mlir::StringRef getDescription() const final {
        return  "Remove trivial single-qubit gates.";
    }
};

void registerRemoveTrivialSQGates(){
    mlir::PassRegistration<RemoveTrivialSQGatesPass>();
}


}
