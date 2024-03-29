#include "isq/GateDefTypes.h"
#include "isq/Operations.h"
#include "isq/QSynthesis.h"
#include "isq/passes/Passes.h"
#include <algorithm>
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
namespace isq::ir::passes{
const char* ISQ_INTERMEDIATE_RZ= "isq.intermediate.multi_rz";
const char* ISQ_INTERMEDIATE_RX= "isq.intermediate.multi_rx";
const char* ISQ_RZ_HAS_ANCILLA = "isq_rz_has_ancilla";
const char* ISQ_MULTIRX_CTRL = "isq_multirx_ctrl";

void emitDecomposedGateSequence(mlir::OpBuilder& builder, synthesis::DecomposedGates& sim_gates, mlir::MutableArrayRef<mlir::Value> qubits){
    auto ctx = builder.getContext();
    for(int i=0; i<sim_gates.size(); i++){
        auto type = std::get<0>(sim_gates[i]);
        auto pos = std::get<1>(sim_gates[i]);
        if(type==synthesis::GateType::H){
            emitBuiltinGate(builder, "H", mlir::ArrayRef<mlir::Value*>{&qubits[pos[0]]});
        }else if(type==synthesis::GateType::X){
            emitBuiltinGate(builder, "X", mlir::ArrayRef<mlir::Value*>{&qubits[pos[0]]});
        }else if(type==synthesis::GateType::TOFFOLI){
            if(pos.size()!=3){
                llvm::errs()<<"real size:"<<pos.size()<<"\n";
                assert(pos.size()==3);
            }
            
            emitBuiltinGate(builder, "Toffoli", mlir::ArrayRef<mlir::Value*>{&qubits[pos[0]], &qubits[pos[1]], &qubits[pos[2]]});
        }else if(type==synthesis::GateType::CNOT){
            emitBuiltinGate(builder, "CNOT", mlir::ArrayRef<mlir::Value*>{&qubits[pos[0]], &qubits[pos[1]]});
        }else if(type==synthesis::GateType::NONE){
            double theta[3] = {std::get<2>(sim_gates[i]), std::get<3>(sim_gates[i]), std::get<4>(sim_gates[i])};
            auto u3_builtin = "__isq__builtin__u3";
            ::mlir::SmallVector<mlir::Value> theta_v;
            for(auto i=0; i<3; i++){
                auto v = builder.create<mlir::arith::ConstantFloatOp>(
                    ::mlir::UnknownLoc::get(ctx),
                    ::llvm::APFloat(theta[i]),
                    ::mlir::Float64Type::get(ctx)
                );
                theta_v.push_back(v.getResult());
            }

            emitBuiltinGate(builder, "U3", mlir::ArrayRef<mlir::Value*>{&qubits[pos[0]]}, theta_v);
        }else if (type==synthesis::GateType::RX){
            double theta = std::get<2>(sim_gates[i]);
            auto v = builder.create<mlir::arith::ConstantFloatOp>(
                ::mlir::UnknownLoc::get(ctx),
                ::llvm::APFloat(theta),
                ::mlir::Float64Type::get(ctx)
            );
            emitBuiltinGate(builder, "RX", mlir::ArrayRef<mlir::Value*>{&qubits[pos[0]]}, mlir::ArrayRef<mlir::Value>{v});
        }else if (type==synthesis::GateType::RY){
            double theta = std::get<2>(sim_gates[i]);
            auto v = builder.create<mlir::arith::ConstantFloatOp>(
                ::mlir::UnknownLoc::get(ctx),
                ::llvm::APFloat(theta),
                ::mlir::Float64Type::get(ctx)
            );
            emitBuiltinGate(builder, "RY", mlir::ArrayRef<mlir::Value*>{&qubits[pos[0]]}, mlir::ArrayRef<mlir::Value>{v});
        }else if (type==synthesis::GateType::RZ){
            double theta = std::get<2>(sim_gates[i]);
            auto v = builder.create<mlir::arith::ConstantFloatOp>(
                ::mlir::UnknownLoc::get(ctx),
                ::llvm::APFloat(theta),
                ::mlir::Float64Type::get(ctx)
            );
            emitBuiltinGate(builder, "RZ", mlir::ArrayRef<mlir::Value*>{&qubits[pos[0]]}, mlir::ArrayRef<mlir::Value>{v});
        }else if (type==synthesis::GateType::CPHASE){
            double theta = std::get<2>(sim_gates[i]);
            auto v = builder.create<mlir::arith::ConstantFloatOp>(
                ::mlir::UnknownLoc::get(ctx),
                ::llvm::APFloat(theta),
                ::mlir::Float64Type::get(ctx)
            );
            emitBuiltinGate(builder, "RZ", mlir::ArrayRef<mlir::Value*>{&qubits[pos[0]]}, mlir::ArrayRef<mlir::Value>{v});
            auto p = builder.create<mlir::arith::ConstantFloatOp>(
                ::mlir::UnknownLoc::get(ctx),
                ::llvm::APFloat(theta/2),
                ::mlir::Float64Type::get(ctx)
            );
            emitBuiltinGate(builder, "GPHASE", {}, mlir::ArrayRef<mlir::Value>{p});
        }
        else{
            llvm_unreachable("Nope");
        }
    }
}

void addCtrlR(std::string gate, mlir::MutableArrayRef<mlir::Value> qubits, int ctrl, int target, mlir::Value theta, mlir::Value neg_theta, mlir::OpBuilder& builder){
    auto ctx = builder.getContext();
    if (gate == "RX"){
        auto pi_2 = builder.create<mlir::arith::ConstantFloatOp>(
            ::mlir::UnknownLoc::get(ctx),
            ::llvm::APFloat(M_PI / 2),
            ::mlir::Float64Type::get(ctx)
        );
        auto neg_pi_2 = builder.create<mlir::arith::ConstantFloatOp>(
            ::mlir::UnknownLoc::get(ctx),
            ::llvm::APFloat(-M_PI / 2),
            ::mlir::Float64Type::get(ctx)
        );
        auto pi_4 = builder.create<mlir::arith::ConstantFloatOp>(
            ::mlir::UnknownLoc::get(ctx),
            ::llvm::APFloat(M_PI / 4),
            ::mlir::Float64Type::get(ctx)
        );
        emitBuiltinGate(builder, "RZ", mlir::ArrayRef<mlir::Value*>{&qubits[target]}, mlir::ArrayRef<mlir::Value>{neg_pi_2});
        emitBuiltinGate(builder, "CNOT", mlir::ArrayRef<mlir::Value*>{&qubits[ctrl], &qubits[target]});
        emitBuiltinGate(builder, "RY", mlir::ArrayRef<mlir::Value*>{&qubits[target]}, mlir::ArrayRef<mlir::Value>{neg_theta});
        emitBuiltinGate(builder, "CNOT", mlir::ArrayRef<mlir::Value*>{&qubits[ctrl], &qubits[target]});
        emitBuiltinGate(builder, "RY", mlir::ArrayRef<mlir::Value*>{&qubits[target]}, mlir::ArrayRef<mlir::Value>{theta});
        emitBuiltinGate(builder, "RZ", mlir::ArrayRef<mlir::Value*>{&qubits[target]}, mlir::ArrayRef<mlir::Value>{pi_2});
    }else if (gate == "CPHASE") {
        emitBuiltinGate(builder, "CNOT", mlir::ArrayRef<mlir::Value*>{&qubits[ctrl], &qubits[target]});
        emitBuiltinGate(builder, "RZ", mlir::ArrayRef<mlir::Value*>{&qubits[target]}, mlir::ArrayRef<mlir::Value>{neg_theta});
        emitBuiltinGate(builder, "CNOT", mlir::ArrayRef<mlir::Value*>{&qubits[ctrl], &qubits[target]});
        emitBuiltinGate(builder, "RZ", mlir::ArrayRef<mlir::Value*>{&qubits[target]}, mlir::ArrayRef<mlir::Value>{theta});
        emitBuiltinGate(builder, "GPHASE", {}, mlir::ArrayRef<mlir::Value>{theta});
    }else{
        emitBuiltinGate(builder, "CNOT", mlir::ArrayRef<mlir::Value*>{&qubits[ctrl], &qubits[target]});
        emitBuiltinGate(builder, gate.c_str(), mlir::ArrayRef<mlir::Value*>{&qubits[target]}, mlir::ArrayRef<mlir::Value>{neg_theta});
        emitBuiltinGate(builder, "CNOT", mlir::ArrayRef<mlir::Value*>{&qubits[ctrl], &qubits[target]});
        emitBuiltinGate(builder, gate.c_str(), mlir::ArrayRef<mlir::Value*>{&qubits[target]}, mlir::ArrayRef<mlir::Value>{theta});
    }
}

void addMultiR(std::string gate, mlir::Value theta, mlir::MutableArrayRef<mlir::Value> qubits, mlir::OpBuilder& builder){
    auto ctx = builder.getContext();
    int n = qubits.size()-1;
    if (n > 0){
        double v = (1 << (n-1));
        auto divn = builder.create<mlir::arith::ConstantFloatOp>(
            ::mlir::UnknownLoc::get(ctx),
            ::llvm::APFloat(v),
            ::mlir::Float64Type::get(ctx)
        );
        theta = builder.create<mlir::arith::DivFOp>(::mlir::UnknownLoc::get(ctx), theta, divn);
    }

    auto half = builder.create<mlir::arith::ConstantFloatOp>(
        ::mlir::UnknownLoc::get(ctx),
        ::llvm::APFloat(0.5),
        ::mlir::Float64Type::get(ctx)
    );
    auto a_theta = builder.create<mlir::arith::MulFOp>(::mlir::UnknownLoc::get(ctx), theta, half);
    auto n_theta = builder.create<mlir::arith::NegFOp>(::mlir::UnknownLoc::get(ctx), a_theta);
    if (n == 0){
        if (gate == "CPHASE"){
            emitBuiltinGate(builder, "RZ", mlir::ArrayRef<mlir::Value*>{&qubits[0]}, mlir::ArrayRef<mlir::Value>{theta});
            emitBuiltinGate(builder, "GPHASE", {}, mlir::ArrayRef<mlir::Value>{a_theta});
        }else{
            emitBuiltinGate(builder, gate.c_str(), mlir::ArrayRef<mlir::Value*>{&qubits[0]}, mlir::ArrayRef<mlir::Value>{theta});
        }
        return;
    }

    auto gray_code = synthesis::generate_gray_code(n);
    int last_pattern = -1;

    for (auto i = 0; i < (1 << n); i++){
        if (i == 0) continue;
        int pattern = gray_code[i];

        if (last_pattern == -1) last_pattern = pattern;
        int lm_pos = synthesis::last_one_idx(pattern, n);
        int pos = synthesis::last_one_idx(last_pattern ^ pattern, n);
        if (pos > -1){
            if (lm_pos != pos){
                emitBuiltinGate(builder, "CNOT", mlir::ArrayRef<mlir::Value*>{&qubits[pos], &qubits[lm_pos]});
            }else{
                for (auto j=n-1; j>=0; j--){
                    if (((1 << j) & pattern) > 0){
                        if ((n-1-j) == lm_pos) continue;
                        emitBuiltinGate(builder, "CNOT", mlir::ArrayRef<mlir::Value*>{&qubits[n-1-j], &qubits[lm_pos]});
                    }
                }
            }
        }

        int cnt = synthesis::get_one_count(pattern, n);
        if (cnt % 2 == 0){
            addCtrlR(gate, qubits, lm_pos, n, n_theta, a_theta, builder);
        }else{
            addCtrlR(gate, qubits, lm_pos, n, a_theta, n_theta, builder);
        }

        last_pattern = pattern;
    }
}

void addMultiRx(mlir::ArrayAttr ctrl, mlir::MutableArrayRef<mlir::Value> qubits, mlir::PatternRewriter& rewriter){
    auto ctx = rewriter.getContext();
    mlir::SmallVector<mlir::Type> qubitTypes;
    qubitTypes.append(qubits.size(), QStateType::get(ctx));
    mlir::OperationState state(mlir::UnknownLoc::get(ctx), ISQ_INTERMEDIATE_RX, qubits, qubitTypes, mlir::ArrayRef<mlir::NamedAttribute>{mlir::NamedAttribute(rewriter.getStringAttr(ISQ_MULTIRX_CTRL), ctrl)});
    auto new_op = rewriter.create(state);
    for(auto i=0; i<qubits.size(); i++){
        qubits[i] = new_op->getResult(i);
    }
}
void addMultiRz(mlir::Value lambda_plus_phi, mlir::MutableArrayRef<mlir::Value> qubits, mlir::PatternRewriter& rewriter, bool with_ancilla = true){
    auto ctx = rewriter.getContext();
    mlir::SmallVector<mlir::Type> qubitTypes;
    qubitTypes.append(qubits.size(), QStateType::get(ctx));
    mlir::SmallVector<mlir::Value> operands;
    operands.push_back(lambda_plus_phi);
    operands.append(qubits.begin(), qubits.end());
    mlir::OperationState state(mlir::UnknownLoc::get(ctx), ISQ_INTERMEDIATE_RZ, operands, qubitTypes, mlir::ArrayRef<mlir::NamedAttribute>{});
    if(with_ancilla){
        state.addAttribute(ISQ_RZ_HAS_ANCILLA, mlir::UnitAttr::get(ctx));
    }
    auto new_op = rewriter.create(state);
    for(auto i=0; i<qubits.size(); i++){
        qubits[i] = new_op->getResult(i);
    }
}

// decompose multi control z
/*
A(n qbit array) is control qbit, T is target qbit
                                                  __             __
                                               --|  |-----------|  |--Z(θ/8)--
A   n--●--    n----●-----------●---Z(θ/2)-     --|+1|--Z(-θ/8)--|-1|--Z(θ/8)--
       |    =      |           |            =  --|  |--Z(-θ/4)--|  |--Z(θ/4)--
T    -Z(θ)-    ----⊕--Z(-θ/2)--⊕---Z(θ/2)-     --|__|--Z(-θ/2)--|__|--Z(θ/2)--

*/
struct DecomposeMultiRzRule : public mlir::RewritePattern{
    DecomposeMultiRzRule(mlir::MLIRContext *context)
        : RewritePattern(MatchAnyOpTypeTag(), 1, context){}
    void emitZtheta(mlir::PatternRewriter& rewriter, mlir::Value* q, mlir::Value angle) const{
        emitBuiltinGate(rewriter, "Rz", {q}, {angle}, {}, false);
        auto ctx = rewriter.getContext();
        auto half = rewriter.create<mlir::arith::ConstantFloatOp>(
            ::mlir::UnknownLoc::get(ctx),
            ::llvm::APFloat(0.5),
            ::mlir::Float64Type::get(ctx)
        );
        auto theta = rewriter.create<mlir::arith::MulFOp>(::mlir::UnknownLoc::get(ctx), angle, half);
        emitBuiltinGate(rewriter, "GPhase", {}, {theta});
    }
    mlir::LogicalResult
    matchAndRewrite(mlir::Operation * op,
                    mlir::PatternRewriter & rewriter) const override {
        auto ctx = op->getContext();
        if(op->getName().getStringRef() != ISQ_INTERMEDIATE_RZ) return mlir::failure();
        mlir::SmallVector<mlir::Value> operands;
        auto angle = op->getOperand(0);
        for(auto v: op->getOperands().drop_front()){
            operands.push_back(v);
        }
        auto use_ancilla = op->hasAttr(ISQ_RZ_HAS_ANCILLA);
        auto gate_size = operands.size();
        if(use_ancilla) gate_size--;
        if(gate_size==1){
            // replace with rz.
            emitZtheta(rewriter, &operands[0], angle);
        }else{
            auto zero = rewriter.create<mlir::arith::ConstantFloatOp>(
                ::mlir::UnknownLoc::get(ctx),
                ::llvm::APFloat(0.0),
                ::mlir::Float64Type::get(ctx)
            );
            auto half = rewriter.create<mlir::arith::ConstantFloatOp>(
                ::mlir::UnknownLoc::get(ctx),
                ::llvm::APFloat(0.5),
                ::mlir::Float64Type::get(ctx)
            );
            auto neghalf = rewriter.create<mlir::arith::ConstantFloatOp>(
                ::mlir::UnknownLoc::get(ctx),
                ::llvm::APFloat(-0.5),
                ::mlir::Float64Type::get(ctx)
            );
            auto negone = rewriter.create<mlir::arith::ConstantFloatOp>(
                ::mlir::UnknownLoc::get(ctx),
                ::llvm::APFloat(-1.0),
                ::mlir::Float64Type::get(ctx)
            );
            auto neg_z_angle = rewriter.create<mlir::arith::MulFOp>(::mlir::UnknownLoc::get(ctx), angle, neghalf);
            auto z_angle = rewriter.create<mlir::arith::MulFOp>(::mlir::UnknownLoc::get(ctx), angle, half);
            if(use_ancilla){
                auto total_size = operands.size();
                synthesis::DecomposedGates decomposed_gates = synthesis::mcdecompose_addone(total_size);
                // the last qubit is ancilla.
                emitDecomposedGateSequence(rewriter, decomposed_gates, operands);
                for (int i = operands.size()-2; i > 0; i--){
                    emitZtheta(rewriter, &operands[i], neg_z_angle);
                    neg_z_angle = rewriter.create<mlir::arith::MulFOp>(::mlir::UnknownLoc::get(ctx), neg_z_angle, half);
                }

                std::reverse(decomposed_gates.begin(), decomposed_gates.end());
                // reverse.
                emitDecomposedGateSequence(rewriter, decomposed_gates, operands);
                for (int i = operands.size()-2; i > 0; i--){
                    emitZtheta(rewriter, &operands[i], z_angle);
                    z_angle = rewriter.create<mlir::arith::MulFOp>(::mlir::UnknownLoc::get(ctx), z_angle, half);
                }

                // last angle.
                emitZtheta(rewriter, &operands[0], z_angle);
            }else{
                // This is in fact AXBXC decomposition:
                //n----●-----------●---Z(θ/2)-   
                //     |            |            
                // ----⊕--Z(-θ/2)---⊕---Z(θ/2)-   
                // TODO: borrowing one ancilla will make our life much easier!
                mlir::SmallVector<mlir::Attribute> true_ctrl;
                for(auto i=0; i<operands.size()-1; i++){
                    true_ctrl.push_back(mlir::BoolAttr::get(ctx, true));
                }
                addMultiRx(mlir::ArrayAttr::get(ctx, true_ctrl), operands, rewriter);
                auto last = operands.size()-1;
                emitZtheta(rewriter, &operands[last], neg_z_angle);
                addMultiRx(mlir::ArrayAttr::get(ctx, true_ctrl), operands, rewriter);
                emitZtheta(rewriter, &operands[last], z_angle);
                addMultiRz(z_angle, mlir::MutableArrayRef(operands.begin(), operands.end()-1), rewriter, false );
            }
            
            
        }
        rewriter.replaceOp(op, operands);
        return mlir::success();
    }
};
struct DecomposeMultiRxRule : public mlir::RewritePattern{
    DecomposeMultiRxRule(mlir::MLIRContext *context)
        : RewritePattern(MatchAnyOpTypeTag(), 1, context){}

    mlir::LogicalResult
    matchAndRewrite(mlir::Operation * op,
                    mlir::PatternRewriter & rewriter) const override {
        auto ctx = op->getContext();
        if(op->getName().getStringRef() != ISQ_INTERMEDIATE_RX) return mlir::failure();
        mlir::SmallVector<mlir::Value> operands;
        for(auto v: op->getOperands()){
            operands.push_back(v);
        }
        assert(operands.size()!=1);
        if(operands.size()==2){
            // replace with cnot.
            emitBuiltinGate(rewriter, "CNOT", {&operands[0], &operands[1]});
        }else{
            synthesis::UnitaryVector v;
            // This is X matrix.
            for(int i = 0; i < 2; i++){
                for(int j = 0; j < 2; j++){
                    v.push_back(std::make_pair((double)(i!=j),0.0));
                }
            }
            std::string ctrl="";
            for(auto c: op->getAttrOfType<mlir::ArrayAttr>(ISQ_MULTIRX_CTRL).getAsValueRange<mlir::BoolAttr>()){
                ctrl+=c?"t":"f";
            }
            auto multi_ctrl_x = synthesis::mcdecompose_u(v, ctrl);
            emitDecomposedGateSequence(rewriter, multi_ctrl_x, operands);
        }
        rewriter.replaceOp(op, operands);
        return mlir::success();
    }
};
struct MergeAdjointIntoU3Rule : public mlir::OpRewritePattern<DecorateOp>{
    MergeAdjointIntoU3Rule(mlir::MLIRContext* ctx): mlir::OpRewritePattern<DecorateOp>(ctx, 1){}
    mlir::LogicalResult matchAndRewrite(DecorateOp op, mlir::PatternRewriter& rewriter) const override{
        auto ctx = op->getContext();
        auto usegate_op = llvm::dyn_cast<UseGateOp>(op.getArgs().getDefiningOp());
        if(!usegate_op) return mlir::failure();
        auto gatedef = llvm::dyn_cast_or_null<DefgateOp>(mlir::SymbolTable::lookupNearestSymbolFrom(usegate_op, usegate_op.getName()));
        if(!gatedef) return mlir::failure();
        //if(!isFamousGate(gatedef, "U3")) return mlir::failure();
        if(!op.getAdjoint()){
            return mlir::failure();
        }
        if (isFamousGate(gatedef, "U3")){
            // Pull adjoint into u3.
            auto theta = usegate_op.getParameters()[0];
            auto phi = usegate_op.getParameters()[1];
            auto lam = usegate_op.getParameters()[2];
            auto new_theta = rewriter.create<mlir::arith::NegFOp>(::mlir::UnknownLoc::get(ctx), theta);
            auto new_phi = rewriter.create<mlir::arith::NegFOp>(::mlir::UnknownLoc::get(ctx), lam);
            auto new_lam = rewriter.create<mlir::arith::NegFOp>(::mlir::UnknownLoc::get(ctx), phi);
            auto new_used_gate = emitUseBuiltinGate(rewriter, 1, "U3", {new_theta, new_phi, new_lam}, op.getCtrl(), false);
            rewriter.replaceOp(op, {new_used_gate});
            return mlir::success();
        }else if (isFamousGate(gatedef, "Rx")){
            auto theta = usegate_op.getParameters()[0];
            auto new_theta = rewriter.create<mlir::arith::NegFOp>(::mlir::UnknownLoc::get(ctx), theta);
            auto new_used_gate = emitUseBuiltinGate(rewriter, 1, "Rx", {new_theta}, op.getCtrl(), false);
            rewriter.replaceOp(op, {new_used_gate});
            return mlir::success();
        }else if (isFamousGate(gatedef, "Ry")){
            auto theta = usegate_op.getParameters()[0];
            auto new_theta = rewriter.create<mlir::arith::NegFOp>(::mlir::UnknownLoc::get(ctx), theta);
            auto new_used_gate = emitUseBuiltinGate(rewriter, 1, "Ry", {new_theta}, op.getCtrl(), false);
            rewriter.replaceOp(op, {new_used_gate});
            return mlir::success();
        }else if (isFamousGate(gatedef, "Rz")){
            auto theta = usegate_op.getParameters()[0];
            auto new_theta = rewriter.create<mlir::arith::NegFOp>(::mlir::UnknownLoc::get(ctx), theta);
            auto new_used_gate = emitUseBuiltinGate(rewriter, 1, "Rz", {new_theta}, op.getCtrl(), false);
            rewriter.replaceOp(op, {new_used_gate});
            return mlir::success();
        }
        
        return mlir::failure();
    }
};
struct MergeAdjointIntoGPhaseRule : public mlir::OpRewritePattern<DecorateOp>{
    MergeAdjointIntoGPhaseRule(mlir::MLIRContext* ctx): mlir::OpRewritePattern<DecorateOp>(ctx, 1){}
    mlir::LogicalResult matchAndRewrite(DecorateOp op, mlir::PatternRewriter& rewriter) const override{
        auto ctx = op->getContext();
        auto usegate_op = llvm::dyn_cast<UseGateOp>(op.getArgs().getDefiningOp());
        if(!usegate_op) return mlir::failure();
        auto gatedef = llvm::dyn_cast_or_null<DefgateOp>(mlir::SymbolTable::lookupNearestSymbolFrom(usegate_op, usegate_op.getName()));
        if(!gatedef) return mlir::failure();
        if(!isFamousGate(gatedef, "GPhase")) return mlir::failure();
        if(!op.getAdjoint()){
            return mlir::failure();
        }
        // Pull adjoint into gphase.
        auto theta = usegate_op.getParameters()[0];
        auto new_theta = rewriter.create<mlir::arith::NegFOp>(::mlir::UnknownLoc::get(ctx), theta);
        auto new_used_gate = emitUseBuiltinGate(rewriter, 0, "GPhase", {new_theta}, op.getCtrl(), false);
        rewriter.replaceOp(op, {new_used_gate});
        return mlir::success();
    }
};

struct DecomposeCtrlKnownSQRule : public mlir::OpRewritePattern<ApplyGateOp>{
    DecomposeCtrlKnownSQRule(mlir::MLIRContext* ctx): mlir::OpRewritePattern<ApplyGateOp>(ctx, 1){}
    mlir::LogicalResult matchAndRewrite(ApplyGateOp op, mlir::PatternRewriter& rewriter) const override{
        auto ctx = op->getContext();
        auto decorate_op = llvm::dyn_cast<DecorateOp>(op.getGate().getDefiningOp());
        if(!decorate_op) return mlir::failure();
        auto usegate_op = llvm::dyn_cast<UseGateOp>(decorate_op.getArgs().getDefiningOp());
        if(!usegate_op) return mlir::failure();
        if(usegate_op.getType().cast<GateType>().getSize()!=1) return mlir::failure();
        auto gatedef = llvm::dyn_cast_or_null<DefgateOp>(mlir::SymbolTable::lookupNearestSymbolFrom(usegate_op, usegate_op.getName()));
        if(!gatedef) return mlir::failure();
        if(gatedef.getType().getSize()!=1) return mlir::failure();
        if(decorate_op.getCtrl().size()==0) return mlir::failure();
        std::optional<synthesis::UnitaryVector> requiredMatrix;
        auto id=0;
        for(auto def: gatedef.getDefinition()->getAsRange<GateDefinition>()){
            auto d = AllGateDefs::parseGateDefinition(gatedef, id, gatedef.getType(), def);
            if(d==std::nullopt) return mlir::failure();
            if(auto mat = llvm::dyn_cast_or_null<MatrixDefinition>(&**d)){
                auto& matrix = mat->getMatrix();
                requiredMatrix = synthesis::UnitaryVector();
                for(auto i=0; i<2; i++){
                    for(auto j=0; j<2; j++){
                        requiredMatrix->push_back(std::make_pair(matrix[i][j].real(), (decorate_op.getAdjoint()?-1.0:1.0)*matrix[i][j].imag()));
                    }
                }
                if(decorate_op.getAdjoint()){
                    std::swap((*requiredMatrix)[1], (*requiredMatrix)[2]);
                }
            }
            id++;
        }
        if(!requiredMatrix) return mlir::failure();
        mlir::SmallVector<mlir::Value> operands;
        for(auto v: op.getArgs()){
            operands.push_back(v);
        }
        std::string ctrl="";
        for(auto c: decorate_op.getCtrl().getAsValueRange<mlir::BoolAttr>()){
            ctrl+=c?"t":"f";
        }
        auto multi_ctrl_x = synthesis::mcdecompose_u(*requiredMatrix, ctrl);
        emitDecomposedGateSequence(rewriter, multi_ctrl_x, operands);
        rewriter.replaceOp(op, operands);
        return mlir::success();
    }
};
struct DecomposeCtrlU3Rule : public mlir::OpRewritePattern<ApplyGateOp>{
    DecomposeCtrlU3Rule(mlir::MLIRContext* ctx): mlir::OpRewritePattern<ApplyGateOp>(ctx, 1){}
    mlir::LogicalResult matchAndRewrite(ApplyGateOp op, mlir::PatternRewriter& rewriter) const override{
        auto ctx = op->getContext();
        auto decorate_op = llvm::dyn_cast<DecorateOp>(op.getGate().getDefiningOp());
        if(!decorate_op) return mlir::failure();
        auto usegate_op = llvm::dyn_cast<UseGateOp>(decorate_op.getArgs().getDefiningOp());
        if(!usegate_op) return mlir::failure();
        auto gatedef = llvm::dyn_cast_or_null<DefgateOp>(mlir::SymbolTable::lookupNearestSymbolFrom(usegate_op, usegate_op.getName()));
        if(!gatedef) return mlir::failure();
        mlir::SmallVector<mlir::Value> operands;
        for(auto v: op.getArgs()){
            operands.push_back(v);
        }
        if (decorate_op.getAdjoint()){
            return mlir::failure();
        }
        if (decorate_op.getCtrl().size()==0){
            return mlir::failure();
        }
        if (!llvm::all_of(decorate_op.getCtrl().getAsValueRange<mlir::BoolAttr>(), [](bool x){
            return x;
        })){
            return mlir::failure();
        }
        if(isFamousGate(gatedef, "U3")){
            auto theta = usegate_op.getParameters()[0];
            auto phi = usegate_op.getParameters()[1];
            auto lam = usegate_op.getParameters()[2];

            
            auto zero = rewriter.create<mlir::arith::ConstantFloatOp>(
                ::mlir::UnknownLoc::get(ctx),
                ::llvm::APFloat(0.0),
                ::mlir::Float64Type::get(ctx)
            );
            auto half = rewriter.create<mlir::arith::ConstantFloatOp>(
                ::mlir::UnknownLoc::get(ctx),
                ::llvm::APFloat(0.5),
                ::mlir::Float64Type::get(ctx)
            );
            auto neghalf = rewriter.create<mlir::arith::ConstantFloatOp>(
                ::mlir::UnknownLoc::get(ctx),
                ::llvm::APFloat(-0.5),
                ::mlir::Float64Type::get(ctx)
            );
            auto negone = rewriter.create<mlir::arith::ConstantFloatOp>(
                ::mlir::UnknownLoc::get(ctx),
                ::llvm::APFloat(-1.0),
                ::mlir::Float64Type::get(ctx)
            );

            // decompose U3 = AXBXC
            // C = Rz((λ-φ) / 2) = u3(0, (λ-φ) / 2, 0)
            // B = Ry(-θ/2)Rz(-(λ+φ) / 2) = u3(-θ/2, 0, -(λ+φ) / 2)
            // A = Rz(φ)Ry(θ/2) = u3(θ/2, φ, 0)
            // ctrl(n-2) Z((λ+φ) / 2)
            auto c_sub = rewriter.create<mlir::arith::SubFOp>(::mlir::UnknownLoc::get(ctx), lam, phi);
            auto c_angle = rewriter.create<mlir::arith::MulFOp>(::mlir::UnknownLoc::get(ctx), c_sub, half);
            emitBuiltinGate(rewriter, "Rz", {&operands[operands.size()-1]}, {c_angle});
            addMultiRx(decorate_op.getCtrl(), operands, rewriter);
            auto b_theta = rewriter.create<mlir::arith::MulFOp>(::mlir::UnknownLoc::get(ctx), theta, neghalf);
            auto b_add = rewriter.create<mlir::arith::AddFOp>(::mlir::UnknownLoc::get(ctx), lam, phi);
            auto b_angle = rewriter.create<mlir::arith::MulFOp>(::mlir::UnknownLoc::get(ctx), b_add, neghalf);
            emitBuiltinGate(rewriter, "Rz", {&operands[operands.size()-1]}, {b_angle});
            emitBuiltinGate(rewriter, "Ry", {&operands[operands.size()-1]}, {b_theta});
            addMultiRx(decorate_op.getCtrl(), operands, rewriter);
            auto a_theta = rewriter.create<mlir::arith::MulFOp>(::mlir::UnknownLoc::get(ctx), theta, half);
            emitBuiltinGate(rewriter, "Ry", {&operands[operands.size()-1]}, {a_theta});
            emitBuiltinGate(rewriter, "Rz", {&operands[operands.size()-1]}, {phi});
            auto lambda_plus_phi = b_add;
            addMultiRz(lambda_plus_phi, operands, rewriter);
            rewriter.replaceOp(op, operands);
        }else if(isFamousGate(gatedef, "GPhase")){
            addMultiRz(usegate_op.getParameters()[0], operands, rewriter, false);
            rewriter.replaceOp(op, operands);
        }else if (isFamousGate(gatedef, "Rx")){
            addMultiR("RX", usegate_op.getParameters()[0], operands, rewriter);
            rewriter.replaceOp(op, operands);
        }else if (isFamousGate(gatedef, "Ry")){
            addMultiR("RY", usegate_op.getParameters()[0], operands, rewriter);
            rewriter.replaceOp(op, operands);
        }else if (isFamousGate(gatedef, "Rz")){
            addMultiR("RZ", usegate_op.getParameters()[0], operands, rewriter);
            rewriter.replaceOp(op, operands);
        }else{
            return mlir::failure();
        }

        return mlir::success();
    }
};
class DecomposeCtrlU3Pass : public mlir::PassWrapper<DecomposeCtrlU3Pass, mlir::OperationPass<mlir::ModuleOp>>{
    void runOnOperation() override{
        mlir::ModuleOp m = this->getOperation();
        auto ctx = m->getContext();
        do{
            mlir::RewritePatternSet rps(ctx);
            rps.add<DecomposeMultiRzRule>(ctx);
            rps.add<DecomposeMultiRxRule>(ctx);
            rps.add<MergeAdjointIntoU3Rule>(ctx);
            rps.add<MergeAdjointIntoGPhaseRule>(ctx);
            rps.add<DecomposeCtrlU3Rule>(ctx);
            rps.add<DecomposeCtrlKnownSQRule>(ctx);
            mlir::FrozenRewritePatternSet frps(std::move(rps));
            (void)mlir::applyPatternsAndFoldGreedily(m.getOperation(), frps);
        }while(0);
    }
    mlir::StringRef getArgument() const final {
        return "isq-decompose-ctrl-u3";
    }
    mlir::StringRef getDescription() const final {
        return  "Decompose controlled single-qubit-gates and U3.";
    }
};

void registerDecomposeCtrlU3(){
    mlir::PassRegistration<DecomposeCtrlU3Pass>();
}
}
