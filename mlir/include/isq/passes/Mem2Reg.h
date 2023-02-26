#ifndef _ISQ_PASSES_MEM2REG_H
#define _ISQ_PASSES_MEM2REG_H
#include <mlir/IR/Operation.h>
#include <mlir/IR/PatternMatch.h>
#include <mlir/Dialect/SCF/IR/SCF.h>
namespace isq{
namespace ir{
namespace passes{
class Mem2RegRewrite{
protected:
    virtual bool isLoad(mlir::Operation* op) const = 0;
    virtual int loadId(mlir::Operation* op) const = 0;
    virtual bool isStore(mlir::Operation* op) const = 0;
    virtual int storeId(mlir::Operation* op) const = 0;
    virtual int storeValue(mlir::Operation* op) const = 0;
public:
    // Perform mem2reg on given block, appending loaded types to block parameters.
    // This is used on non-entry blocks.
    void mem2regAlterBlockParam(mlir::TypeRange types, mlir::Block* block, mlir::PatternRewriter& rewriter);
    // Perform mem2reg on given block, using the block-args as-is.
    // This is used on entry blocks where new parameters are inserted by caller in advance.
    // Or this can be used on two blocks of if-operation, where no parameters are attached to block parameters.
    void mem2regKeepBlockParam(mlir::Block* block, mlir::PatternRewriter& rewriter, mlir::SmallVector<mlir::Value> blockArgs);
private:
    void mem2regOp(mlir::SmallVector<mlir::Value>& values, mlir::PatternRewriter& rewriter, mlir::scf::IfOp op);
    void mem2regBlock(mlir::TypeRange types, mlir::Block* block, mlir::PatternRewriter& rewriter, mlir::SmallVector<mlir::Value> blockArgs);
};
}
}
}
#endif