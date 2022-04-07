#include "isq/Enums.h"
#include "isq/passes/canonicalization/CanonicalizeDecorateGates.h"
#include "mlir/IR/PatternMatch.h"
#include <isq/Operations.h>
namespace isq {
namespace ir {
GateTrait DecorateOp::computePostDecorateTrait(GateTrait attr, int ctrl, bool adj){
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
    return static_cast<GateTrait>(new_trait);
}
mlir::LogicalResult verify(DecorateOp op) {
    auto result = op.getResult().getType().cast<GateType>();
    auto operand = op.getOperand().getType().cast<GateType>();
    auto vr = result.getHints();
    auto vo = operand.getHints();
    
    auto ctrl = op.ctrl();
    auto adjoint = op.adjoint();
    auto expected_vo = DecorateOp::computePostDecorateTrait(result.getHints(), ctrl.size(), adjoint);
    auto size = result.getSize();
    auto expected_size = operand.getSize() + ctrl.size();
    if (expected_vo != vo) {
        op.emitOpError("decorate trait mismatch");
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


} // namespace ir
} // namespace isq