#include "isq/Enums.h"
#include "isq/passes/canonicalization/CanonicalizeDecorateGates.h"
#include "mlir/IR/PatternMatch.h"
#include <isq/Operations.h>
namespace isq {
namespace ir {
GateTrait DecorateOp::computePostDecorateTrait(GateTrait attr, int ctrl, bool adj, bool ctrl_all_one){
    auto trait = static_cast<uint32_t>(attr);
    auto new_trait = 0;
    if(trait & static_cast<uint32_t>(GateTrait::Diagonal)){
        new_trait |= static_cast<uint32_t>(GateTrait::Diagonal);
    }
    if(ctrl==0 && (trait & static_cast<uint32_t>(GateTrait::Antidiagonal))){
        new_trait |= static_cast<uint32_t>(GateTrait::Antidiagonal);
    }
    if(ctrl==0 && (trait & static_cast<uint32_t>(GateTrait::Symmetric))){
        new_trait |= static_cast<uint32_t>(GateTrait::Symmetric);
    }
    if(trait & static_cast<uint32_t>(GateTrait::Hermitian)){
        new_trait |= static_cast<uint32_t>(GateTrait::Hermitian);
    }
    if(ctrl_all_one && (trait & static_cast<uint32_t>(GateTrait::Phase))){
        //assert(trait & static_cast<uint32_t>(GateTrait::Symmetric));
        new_trait |= (static_cast<uint32_t>(GateTrait::Phase) | static_cast<uint32_t>(GateTrait::Symmetric));
    }
    return static_cast<GateTrait>(new_trait);
}
mlir::LogicalResult verify(DecorateOp op) {
    auto result = op.getResult().getType().cast<GateType>();
    auto operand = op.getOperand().getType().cast<GateType>();
    auto vr = result.getHints();
    auto vo = operand.getHints();
    
    auto ctrl = op.ctrl();
    auto adjoint = op.adjoint();
    auto ctrls = ctrl.getAsValueRange<mlir::BoolAttr>();
    auto all_one = std::all_of(ctrls.begin(), ctrls.end(), [](auto x){return x;});
    auto expected_vr = DecorateOp::computePostDecorateTrait(result.getHints(), ctrl.size(), adjoint, all_one);
    auto size = result.getSize();
    auto expected_size = operand.getSize() + ctrl.size();
    if (expected_vr != vr) {
        std::stringstream ss;
        ss<<"decorate trait mismatch, expected "<<(uint32_t)expected_vr<<", actual "<<(uint32_t)vr<<"\n";
	auto err = ss.str();
        op.emitOpError(err.c_str());
        return mlir::failure();
    }
    if (size != expected_size){
        op.emitOpError("decorate size mismatch");
        return mlir::failure();
    }

    return mlir::success();
}

void DecorateOp::getCanonicalizationPatterns(mlir::RewritePatternSet &patterns,
                                       mlir::MLIRContext *context) {
    patterns.add<passes::canonicalize::MergeDecorate>(context);
    patterns.add<passes::canonicalize::EliminateUselessDecorate>(context);
    patterns.add<passes::canonicalize::AdjointHermitian>(context);
}

::mlir::LogicalResult DecorateOp::verify(){
    return ::isq::ir::verify(*this);
}

} // namespace ir
} // namespace isq
