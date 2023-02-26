#include <isq/Operations.h>
#include <mlir/IR/PatternMatch.h>
#include <isq/passes/canonicalization/CanonicalizeDowngrade.h>
namespace isq {
namespace ir {
mlir::LogicalResult verify(DowngradeGateOp op) {
    auto result = op.getResult().getType().cast<GateType>();
    auto operand = op.getOperand().getType().cast<GateType>();
    auto vr = result.getHints();
    auto vo = operand.getHints();
    uint32_t bigset = static_cast<uint32_t>(vo);
    uint32_t smallset = static_cast<uint32_t>(vr);
    if (smallset & (~bigset)) {
        op.emitOpError("downgraded gate contains new trait(s) compared with "
                       "original input.");
        return mlir::failure();
    }

    return mlir::success();
}


void DowngradeGateOp::getCanonicalizationPatterns(mlir::RewritePatternSet &patterns,
                                       mlir::MLIRContext *context) {
    patterns.add<passes::canonicalize::EliminateUselessDowngrade>(context);
    patterns.add<passes::canonicalize::MergeDowngrade>(context);
}

::mlir::LogicalResult DowngradeGateOp::verify(){
    return ::isq::ir::verify(*this);
}

} // namespace ir
} // namespace isq