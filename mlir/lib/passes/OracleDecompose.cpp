#include "isq/GateDefTypes.h"
#include "isq/Operations.h"
#include "isq/QTypes.h"
#include "mlir/Dialect/Arithmetic/IR/Arithmetic.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/IR/Value.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Rewrite/FrozenRewritePatternSet.h"
#include "llvm/ADT/APFloat.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include "isq/passes/Passes.h"
#include "isq/oracle/QM.h"
#include <iostream>

namespace isq{
namespace ir{
namespace passes{

using namespace qm;

class OracleTableDef: public mlir::OpRewritePattern<DefgateOp>{
    mlir::ModuleOp rootModule;

public:
    OracleTableDef(mlir::MLIRContext* ctx, mlir::ModuleOp module): mlir::OpRewritePattern<DefgateOp>(ctx, 1), rootModule(module){
    }

    mlir::LogicalResult decomposeOracle(mlir::PatternRewriter& rewriter, ::mlir::func::FuncOp& fop, const std::vector<std::vector<int>>& value, int size) const{
        
        auto ctx = rewriter.getContext();
        

        mlir::PatternRewriter::InsertionGuard guard(rewriter);

        rewriter.updateRootInPlace(fop, [&]{
            mlir::SmallVector<mlir::Value> qubits;
            qubits.append(fop.getBody().front().args_begin(), fop.getBody().front().args_end());
            rewriter.setInsertionPointToStart(&fop.getBody().front());

            auto loc = fop.getLoc();
            auto idx = rewriter.create<mlir::arith::ConstantIndexOp>(loc, 0);
            auto qst = QStateType::get(rewriter.getContext());
            // get oracle's n and m
            int m = value.size();
            int n = size - m;
            // for each 0 ~ (m-1), decompose with every val row
            int midx = -1;
            for (auto &row: value){
                midx += 1;
                if (row.size() == 0) continue;
                // use QM algorithm
                auto qm = QM(n);
                std::set<int> A(row.begin(), row.end());
                auto nodes = qm.simplify(A);
                // optimize
                auto opt = qm.optimize(nodes);
                // deal result
                for (auto bit: opt){
                    // get control qbit idx
                    vector<int> qidx;
                    mlir::SmallVector<mlir::Attribute> ctrls;
                    for (int i = 0; i < n; i++){
                        if (bit[i] == '-') continue;
                        qidx.push_back(i);
                        ctrls.push_back(mlir::BoolAttr::get(ctx, bit[i] == '1'));
                    }
                    int num = qidx.size();
                    // get use gate
                    auto use = rewriter.create<isq::ir::UseGateOp>(loc, isq::ir::GateType::get(ctx, 1, GateTrait::General),::mlir::FlatSymbolRefAttr::get(ctx, "X"), ::mlir::ValueRange{});
                    mlir::Value res = use.getResult();
                    // get decorate
                    if (num > 0){
                        auto decorate = rewriter.create<isq::ir::DecorateOp>(loc, isq::ir::GateType::get(ctx, 1+num, GateTrait::General), use.getResult(), false, mlir::ArrayAttr::get(ctx, mlir::ArrayRef<mlir::Attribute>(ctrls)));
                        res = decorate.getResult();
                    }
                    // affine load 
                    mlir::SmallVector<mlir::Value> params;
                    mlir::SmallVector<mlir::Type> types;
                    for (auto q: qidx){
                        auto load_qubit = rewriter.create<mlir::AffineLoadOp>(loc, qubits[q], mlir::ArrayRef<mlir::Value>({idx}));
                        params.push_back(load_qubit.getResult());
                        types.push_back(qst);
                    }
                    auto target = rewriter.create<mlir::AffineLoadOp>(loc, qubits[n+midx], mlir::ArrayRef<mlir::Value>({idx}));
                    params.push_back(target.getResult());
                    types.push_back(qst);
                    // is apply
                    auto apply = rewriter.create<isq::ir::ApplyGateOp>(loc, mlir::ArrayRef<mlir::Type>(types), res, mlir::ArrayRef<mlir::Value>(params));
                    // affine store
                    int ridx = 0;
                    for (auto q: qidx){
                        rewriter.create<mlir::AffineStoreOp>(loc, apply.getResult(ridx), qubits[q], mlir::ArrayRef<mlir::Value>({idx}));
                        ridx += 1;
                    }
                    rewriter.create<mlir::AffineStoreOp>(loc, apply.getResult(ridx), qubits[n+midx], mlir::ArrayRef<mlir::Value>({idx}));
                }
            }
        });
        return mlir::success();
    }

    mlir::LogicalResult matchAndRewrite(isq::ir::DefgateOp defgate,  mlir::PatternRewriter &rewriter) const override{
        if (!defgate.definition()) return mlir::failure();
        if (defgate.definition()->size() != 2) return mlir::failure();
        
        int id = 0;
        mlir::func::FuncOp fop;
        std::vector<std::vector<int>> value;
        bool hasfunc = false, hasval = false;
        for (auto def: defgate.definition()->getAsRange<GateDefinition>()){
            auto d = AllGateDefs::parseGateDefinition(defgate, id, defgate.type(), def);
            if (d == std::nullopt) return mlir::failure();
            if (auto raw = llvm::dyn_cast_or_null<DecompositionRawDefinition>(&**d)){
                fop = raw->getDecomposedFunc();
                hasfunc = true;
            }
            if (auto oracle = llvm::dyn_cast_or_null<OracleTableDefinition>(&**d)){
                value = oracle->getValue();
                hasval = true;
            }
        }

        if (hasfunc && hasval){
            auto ctx = rewriter.getContext();
            if (mlir::failed(decomposeOracle(rewriter, fop, value, defgate.type().getSize()))){
                defgate->emitError() << "decompose oracle failed";
                return mlir::failure();
            }
            ::mlir::SmallVector<::mlir::Attribute> new_defs;
            new_defs.push_back(GateDefinition::get(
                ctx,
                ::mlir::StringAttr::get(ctx, "decomposition_raw"),
                ::mlir::FlatSymbolRefAttr::get(ctx, fop.getSymNameAttr())
            ));
            defgate->setAttr("definition", ::mlir::ArrayAttr::get(ctx, new_defs));
        }

        return mlir::success();
    }
};


struct OracleDecomposePass: public mlir::PassWrapper<OracleDecomposePass, mlir::OperationPass<mlir::ModuleOp>>{

    void runOnOperation() override{
        mlir::ModuleOp m = this->getOperation();
        auto ctx = m->getContext();
        
        mlir::RewritePatternSet rps(ctx);
        rps.add<OracleTableDef>(ctx, m);
        mlir::FrozenRewritePatternSet frps(std::move(rps));
        (void)mlir::applyPatternsAndFoldGreedily(m.getOperation(), frps);
    }

    mlir::StringRef getArgument() const final{
        return "isq-oracle-decompose";
    }

    mlir::StringRef getDescription() const final{
        return "Using Quinine-McCluskey Algorithm on truth table and optimize.";
    }
};

void registerOracleDecompose(){
    mlir::PassRegistration<OracleDecomposePass>();
}

}

}
}
