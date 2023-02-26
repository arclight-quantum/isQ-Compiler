#include "isq/GateDefTypes.h"
#include "isq/Operations.h"
#include "isq/QSynthesis.h"
#include "isq/QTypes.h"
#include "mlir/Dialect/Arithmetic/IR/Arithmetic.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/IR/Value.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Rewrite/FrozenRewritePatternSet.h"
#include "llvm/ADT/APFloat.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include "isq/passes/Passes.h"
namespace isq{
namespace ir{
namespace passes{

class DecomposeKnownGateDef : public mlir::OpRewritePattern<DefgateOp>{
    mlir::ModuleOp rootModule;
    bool ignore_sq;
public:
    DecomposeKnownGateDef(mlir::MLIRContext* ctx, mlir::ModuleOp module, bool ignore_sq): mlir::OpRewritePattern<DefgateOp>(ctx, 1), rootModule(module), ignore_sq(ignore_sq){

    }
    template<isq::ir::math::MatDouble Mat> 
    mlir::LogicalResult decomposeMatrix(mlir::PatternRewriter& rewriter, ::mlir::StringRef decomposed_name, const Mat& mat) const{
        auto rootModule = this->rootModule;
        auto n = (int) std::log2(mat.size());
        double eps = 1e-6;
        synthesis::UnitaryVector v;
        for(auto& row: mat){
            for(auto& elem: row){
                v.push_back(std::make_pair(elem.real(), elem.imag()));
            }
        }
        synthesis::QSynthesis A(n, v, eps);
        auto sim_gates = synthesis::simplify(A.gates);
        if(!synthesis::verify(n, v, sim_gates, A.phase)){
            return ::mlir::failure();
        }
        mlir::PatternRewriter::InsertionGuard guard(rewriter);
        rewriter.setInsertionPointToStart(rootModule.getBody());
        mlir::SmallVector<mlir::Type> qs;
        for(auto i=0; i<n; i++){
            qs.push_back(QStateType::get(rewriter.getContext()));
        }
        auto funcop = mlir::func::FuncOp::create(::mlir::UnknownLoc::get(rewriter.getContext()), decomposed_name, mlir::FunctionType::get(rewriter.getContext(), qs, qs));
        auto ctx = rewriter.getContext();
        funcop.setSymVisibilityAttr(mlir::StringAttr::get(ctx, "private"));
        rewriter.insert(funcop.getOperation());
        auto entry_block = funcop.addEntryBlock();
        rewriter.setInsertionPointToStart(entry_block);
        mlir::SmallVector<mlir::Value> qubits;
        qubits.append(entry_block->args_begin(), entry_block->args_end());
        for (int j=0; j< sim_gates.size(); j++) {
            auto type = std::get<0>(sim_gates[j]);
            auto pos = std::get<1>(sim_gates[j]);
            if (type == synthesis::GateType::CNOT){
                auto cnot_builtin = "$__isq__builtin__cnot";
                auto use_cnot_gate = rewriter.create<UseGateOp>(
                    ::mlir::UnknownLoc::get(ctx),
                    ir::GateType::get(ctx, 2, GateTrait::General),
                    mlir::FlatSymbolRefAttr::get(ctx, cnot_builtin),
                    ::mlir::ValueRange{}
                );
                auto qst = QStateType::get(rewriter.getContext());
                auto apply_cnot_gate = rewriter.create<ApplyGateOp>(
                    ::mlir::UnknownLoc::get(ctx),
                    ::mlir::ArrayRef<::mlir::Type>{qst, qst},
                    use_cnot_gate.result(),
                    ::mlir::ArrayRef{qubits[pos[0]], qubits[pos[1]]}
                );
                qubits[pos[0]]=apply_cnot_gate.getResult(0);
                qubits[pos[1]]=apply_cnot_gate.getResult(1);
            }else{
                double theta[3] = {std::get<2>(sim_gates[j]), std::get<3>(sim_gates[j]), std::get<4>(sim_gates[j])};
                auto u3_builtin = "$__isq__builtin__u3";
                ::mlir::SmallVector<mlir::Value> theta_v;
                for(auto i=0; i<3; i++){
                    auto v = rewriter.create<mlir::arith::ConstantFloatOp>(
                        ::mlir::UnknownLoc::get(ctx),
                        ::llvm::APFloat(theta[i]),
                        ::mlir::Float64Type::get(ctx)
                    );
                    theta_v.push_back(v.getResult());
                }
                auto use_u3_gate = rewriter.create<UseGateOp>(
                    ::mlir::UnknownLoc::get(ctx),
                    ir::GateType::get(ctx, 1, GateTrait::General),
                    mlir::FlatSymbolRefAttr::get(ctx, u3_builtin),
                    theta_v
                );
                auto qst = QStateType::get(rewriter.getContext());
                auto apply_u3_gate = rewriter.create<ApplyGateOp>(
                    ::mlir::UnknownLoc::get(ctx),
                    ::mlir::ArrayRef<::mlir::Type>{qst},
                    use_u3_gate.result(),
                    ::mlir::ArrayRef{qubits[pos[0]]}
                );
                qubits[pos[0]]=apply_u3_gate.getResult(0);

            }
        }
        rewriter.create<mlir::func::ReturnOp>(::mlir::UnknownLoc::get(rewriter.getContext()), qubits);
        return mlir::success();
    }
    mlir::LogicalResult matchAndRewrite(isq::ir::DefgateOp defgate,  mlir::PatternRewriter &rewriter) const override{
        if(this->ignore_sq && defgate.type().getSize()==1){
            return mlir::failure();
        }
        if(!defgate.definition()) return mlir::failure();
        int id = 0;
        bool added_new_definition = false;
        for(auto def: defgate.definition()->getAsRange<GateDefinition>()){
            auto d = AllGateDefs::parseGateDefinition(defgate, id, defgate.type(), def);
            if(d==std::nullopt) return mlir::failure();
            auto mat = llvm::dyn_cast_or_null<MatrixDefinition>(&**d);
            if(!mat){
                id++;
                continue;
            }
            auto qsd_decomp_name = std::string(defgate.sym_name())+"__qsd__decomposition";
            // construct new matrix name.
            auto qsd_decomp_sym = mlir::FlatSymbolRefAttr::get(mlir::StringAttr::get(rewriter.getContext(), qsd_decomp_name));
            auto qsd_decomp = mlir::SymbolTable::lookupNearestSymbolFrom<mlir::func::FuncOp>(defgate, qsd_decomp_sym);
            auto& mat_data = mat->getMatrix();
            auto n = (int) std::log2(mat_data.size());
            auto ctx = rewriter.getContext();
            if(!qsd_decomp){
                if(mlir::failed(decomposeMatrix(rewriter, qsd_decomp_name, mat_data))){
                    return mlir::failure();
                }
                rewriter.updateRootInPlace(defgate, [&]{
                    auto defs = *defgate.definition();
                    ::mlir::SmallVector<::mlir::Attribute> new_defs;
                    auto r = defs.getAsRange<::mlir::Attribute>();
                    new_defs.append(r.begin(), r.end());
                    new_defs.push_back(GateDefinition::get(
                        ctx,
                        ::mlir::StringAttr::get(ctx, "decomposition"),
                        qsd_decomp_sym
                    ));
                    defgate->setAttr("definition", ::mlir::ArrayAttr::get(ctx, new_defs));
                });
                added_new_definition=true;
            }
        }
        return mlir::success(added_new_definition);
    }
};

struct DecomposeKnownGatePass : public mlir::PassWrapper<DecomposeKnownGatePass, mlir::OperationPass<mlir::ModuleOp>>{
    DecomposeKnownGatePass() = default;
    DecomposeKnownGatePass(const DecomposeKnownGatePass& pass) {}
    void runOnOperation() override{
        mlir::ModuleOp m = this->getOperation();
        auto ctx = m->getContext();
        auto ignore_sq = ignore_sq_matrices.getValue();
        mlir::RewritePatternSet rps(ctx);
        rps.add<DecomposeKnownGateDef>(ctx, m, ignore_sq);
        isq::ir::passes::addLegalizeTraitsRules(rps);
        mlir::FrozenRewritePatternSet frps(std::move(rps));
        (void)mlir::applyPatternsAndFoldGreedily(m.getOperation(), frps);
    }
    Option<bool> ignore_sq_matrices{*this, "ignore-sq-matrices", llvm::cl::desc("Ignore single-qubit known matrices. Maybe useful for preserving optimization opportunities."), llvm::cl::init(false)};
    mlir::StringRef getArgument() const final {
        return "isq-decompose-known-gates-qsd";
    }
    mlir::StringRef getDescription() const final {
        return  "Using QSD decomposition on matrix-known gates.";
    }
};

void registerQSD(){
    mlir::PassRegistration<DecomposeKnownGatePass>();
}

}
}
}