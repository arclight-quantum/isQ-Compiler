#ifndef _ISQ_OPVERIFIER_H
#define _ISQ_OPVERIFIER_H
#include "./Operations.h"
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/Support/LLVM.h>
#include "./QTypes.h"
#include "./QAttrs.h"
namespace isq {
namespace ir {
mlir::SmallVector<mlir::Type> getGateParameterType(GateType ty);
::mlir::FunctionType getExpandedFunctionType(::mlir::MLIRContext *ctx,
                                             uint64_t size,
                                             ::mlir::FunctionType signature);
// mlir::LogicalResult verify(DeclareOp op);
mlir::LogicalResult verify(DowngradeGateOp op);
mlir::LogicalResult verify(DecorateOp op);
// mlir::LogicalResult verify(GateOp op);
} // namespace ir
} // namespace isq
#endif