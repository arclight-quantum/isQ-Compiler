#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/raw_ostream.h>
#include <mlir/Dialect/Affine/IR/AffineOps.h>
#include <mlir/Dialect/MemRef/IR/MemRef.h>
#include <mlir/Dialect/SCF/IR/SCF.h>
#include <mlir/IR/Attributes.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/Location.h>
#include <mlir/IR/Value.h>
#include <mlir/Pass/PassRegistry.h>
#include <mlir/Support/LLVM.h>
#include "isq/Operations.h"
#include "isq/QTypes.h"
#include "isq/passes/Passes.h"
#include "mlir/IR/OperationSupport.h"
#include "mlir/IR/PatternMatch.h"
#include <unordered_set>
#include "isq/GateDefTypes.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Rewrite/FrozenRewritePatternSet.h"
#include "isq/passes/Mem2Reg.h"
namespace isq{
namespace ir{
namespace passes{

const char* ISQ_PURE_GATE = "isq_pure_gate";
const char* ISQ_PURE_LOAD = "isq_load";
const char* ISQ_PURE_STORE = "isq_store";
const char* ISQ_PURE_STORE_OPERAND = "isq_store_operand";
using StringSmallPtrSet = llvm::SmallPtrSet<::mlir::StringAttr, 16>;

struct PureGateMem2Reg : public Mem2RegRewrite{
    bool isLoad(mlir::Operation* op) const override{
        return op->hasAttr(ISQ_PURE_LOAD);
    }
    int loadId(mlir::Operation* op) const override{
        return op->getAttrOfType<mlir::IntegerAttr>(ISQ_PURE_LOAD).getInt();
    }
    bool isStore(mlir::Operation* op) const override{
        return op->hasAttr(ISQ_PURE_STORE);
    }
    int storeId(mlir::Operation* op) const override{
        return op->getAttrOfType<mlir::IntegerAttr>(ISQ_PURE_STORE).getInt();
    }
    int storeValue(mlir::Operation* op) const override{
        return op->getAttrOfType<mlir::IntegerAttr>(ISQ_PURE_STORE_OPERAND).getInt();
    }
};

class DetectPureGates : public mlir::OpRewritePattern<DefgateOp>{
    mlir::ModuleOp rootModule;
    StringSmallPtrSet& pureSymbols;
public:
    DetectPureGates (mlir::MLIRContext* ctx, mlir::ModuleOp module, StringSmallPtrSet& pureSymbols): mlir::OpRewritePattern<DefgateOp>(ctx, 1), rootModule(module), pureSymbols(pureSymbols){
        
    }
    mlir::LogicalResult matchAndRewrite(DefgateOp op, mlir::PatternRewriter& rewriter) const override{
        auto ctx = op->getContext();
        if(op->hasAttr(ISQ_PURE_GATE)){
            return mlir::failure();
        }
        rewriter.startRootUpdate(op);
        op->setAttr(mlir::StringAttr::get(ctx, ISQ_PURE_GATE), mlir::UnitAttr::get(ctx));
        rewriter.finalizeRootUpdate(op);
        int id = 0;
        for(auto def: op.definition()->getAsRange<GateDefinition>()){
            auto d = AllGateDefs::parseGateDefinition(op, id, op.type(), def);
            if(d==std::nullopt) return mlir::failure();
            if(auto def = llvm::dyn_cast_or_null<DecompositionRawDefinition>(&**d)){
                pureSymbols.insert(def->getDecomposedFunc().getSymNameAttr());
            }
            id++;
        }
        
        return mlir::success();
    }
};

class TagPureGates : public mlir::OpRewritePattern<mlir::func::FuncOp>{
    mlir::ModuleOp rootModule;
    StringSmallPtrSet& pureSymbols;
public:
    TagPureGates (mlir::MLIRContext* ctx, mlir::ModuleOp module, StringSmallPtrSet& pureSymbols): mlir::OpRewritePattern<mlir::func::FuncOp>(ctx, 1), rootModule(module), pureSymbols(pureSymbols){

    }
    mlir::LogicalResult matchAndRewrite(mlir::func::FuncOp op, mlir::PatternRewriter& rewriter) const override{
        auto ctx = op->getContext();
        if(op->hasAttr(ISQ_PURE_GATE)){
            return mlir::failure();
        }
        if(pureSymbols.find(op.getSymNameAttr())!=pureSymbols.end()){
            rewriter.startRootUpdate(op);
            op->setAttr(mlir::StringAttr::get(ctx, ISQ_PURE_GATE), mlir::UnitAttr::get(ctx));
            rewriter.finalizeRootUpdate(op);
        }

        return mlir::success();
    }
};

class PureGatedefCleanup : public mlir::OpRewritePattern<DefgateOp>{
    mlir::ModuleOp rootModule;
public:
    PureGatedefCleanup(mlir::MLIRContext* ctx, mlir::ModuleOp module): mlir::OpRewritePattern<DefgateOp>(ctx, 1), rootModule(module){

    }
    mlir::LogicalResult matchAndRewrite(DefgateOp op, mlir::PatternRewriter& rewriter) const override{
        auto ctx = op->getContext();
        // Check for pure-gate notation.
        if(!op->hasAttr(ISQ_PURE_GATE)){
            return mlir::failure();
        }
        rewriter.startRootUpdate(op);
        if(op.definition()){
            mlir::SmallVector<mlir::Attribute> attrs; 
            auto defs = *op.definition();
            for(auto def: defs){
                
                auto gatedef = def.cast<GateDefinition>();
                if(gatedef.getType()=="decomposition_raw"){
                    attrs.push_back(GateDefinition::get(ctx, mlir::StringAttr::get(ctx, "decomposition"), gatedef.getValue()));
                }else{
                    attrs.push_back(gatedef);
                }
            }
            op.definitionAttr(mlir::ArrayAttr::get(ctx, attrs));
        }
        op->removeAttr(ISQ_PURE_GATE);
        rewriter.finalizeRootUpdate(op);
        return mlir::success();
    }
};

class PureGateRewrite : public mlir::OpRewritePattern<mlir::func::FuncOp>{
    mlir::ModuleOp rootModule;
public:
    PureGateRewrite(mlir::MLIRContext* ctx, mlir::ModuleOp module): mlir::OpRewritePattern<mlir::func::FuncOp>(ctx, 1), rootModule(module){

    }

    

    
    
    mlir::LogicalResult matchAndRewrite(mlir::func::FuncOp op, mlir::PatternRewriter& rewriter) const override{
        auto ctx = op->getContext();
        // Check for pure-gate notation.
        if(!op->hasAttr(ISQ_PURE_GATE)){
            return mlir::failure();
        }
        
        
        rewriter.startRootUpdate(op);
        op->removeAttr(ISQ_PURE_GATE);
        // Transform all parameters.
        auto func_type = op.getFunctionType();
        ::mlir::SmallVector<::mlir::Type> args;
        ::mlir::SmallVector<::mlir::Type> results;
        ::mlir::SmallVector<::mlir::Type> extra_args;
        assert(func_type.getResults().size()==0);
        ::mlir::SmallVector<int> arg_rewrite;
        for(auto argi=0; argi<func_type.getNumInputs(); argi++){
            auto arg = func_type.getInput(argi);
            auto memref = arg.dyn_cast<mlir::MemRefType>();
            if(memref && memref.getElementType().isa<QStateType>()){
                // assert they are single qubits.
                // Thus load/store/casting/subviewing can be seen as nop.
                assert(memref.getShape().size()==1);
                assert(memref.getDimSize(0)==1);
                args.push_back(QStateType::get(ctx));
                results.push_back(QStateType::get(ctx));
                extra_args.push_back(QStateType::get(ctx));
                arg_rewrite.push_back(argi);
            }else{
                args.push_back(arg);
            }
        }
        op.setType(mlir::FunctionType::get(ctx, args, results));
        ::mlir::SmallVector<::mlir::Value> arg_rewrite_args;
        for(auto index: arg_rewrite){
            auto arg = op.getArgument(index);
            arg.setType(QStateType::get(ctx));
            arg_rewrite_args.push_back(arg);
        }
        auto i=0;
        for(auto index: arg_rewrite){
            auto arg = op.getArgument(index);
            for(auto op: arg.getUsers()){
                if(llvm::isa<mlir::memref::LoadOp>(op) || llvm::isa<mlir::AffineLoadOp>(op)  || llvm::isa<mlir::memref::CastOp>(op) || llvm::isa<mlir::memref::SubViewOp>(op)){
                    op->setAttr(ISQ_PURE_LOAD, mlir::IntegerAttr::get(rewriter.getI64Type(), i));
                }else if(llvm::isa<mlir::memref::StoreOp>(op)){
                    op->setAttr(ISQ_PURE_STORE, mlir::IntegerAttr::get(rewriter.getI64Type(), i));
                    op->setAttr(ISQ_PURE_STORE_OPERAND, mlir::IntegerAttr::get(rewriter.getI64Type(), 0));
                }else if(llvm::isa<mlir::AffineStoreOp>(op)){
                    op->setAttr(ISQ_PURE_STORE, mlir::IntegerAttr::get(rewriter.getI64Type(), i));
                    op->setAttr(ISQ_PURE_STORE_OPERAND, mlir::IntegerAttr::get(rewriter.getI64Type(), 0));
                }
                
            }
            i++;
        }
        PureGateMem2Reg mem2reg;
        for(auto& block: op.getBlocks()){
            if(block.isEntryBlock()){
                mem2reg.mem2regKeepBlockParam(&block, rewriter, arg_rewrite_args);
            }else{
                mem2reg.mem2regAlterBlockParam(extra_args, &block, rewriter);
            }
        }

        rewriter.finalizeRootUpdate(op);
        return mlir::success();
    }
};
/*
namespace{
    const char* ISQ_ATTR_GATE_SIZE = "ISQ_ATTR_GATE_SIZE";
}
*/

struct PureGateDetectPass : public mlir::PassWrapper<PureGateDetectPass, mlir::OperationPass<mlir::ModuleOp>>{
    void runOnOperation() override{
        StringSmallPtrSet pureFuncs;
        mlir::ModuleOp m = this->getOperation();
        auto ctx = m->getContext();
        // Find all pure gate functions.
        do{
            mlir::RewritePatternSet rps(ctx);
            rps.add<DetectPureGates>(ctx, m, pureFuncs);
            mlir::FrozenRewritePatternSet frps(std::move(rps));
            (void)mlir::applyPatternsAndFoldGreedily(m.getOperation(), frps);
        }while(0);
        // Tag the pure gate functions.
        do{
            mlir::RewritePatternSet rps(ctx);
            rps.add<TagPureGates>(ctx, m, pureFuncs);
            mlir::FrozenRewritePatternSet frps(std::move(rps));
            (void)mlir::applyPatternsAndFoldGreedily(m.getOperation(), frps);
        }while(0);
        // Perform rewrite on functions and rewrite gatedefs.
        do{
            mlir::RewritePatternSet rps(ctx);
            rps.add<PureGateRewrite>(ctx, m);
            rps.add<PureGatedefCleanup>(ctx, m);
            mlir::FrozenRewritePatternSet frps(std::move(rps));
            (void)mlir::applyPatternsAndFoldGreedily(m.getOperation(), frps);
        }while(0);

    }
    mlir::StringRef getArgument() const final{
        return "isq-pure-gate-detection";
    }
    mlir::StringRef getDescription() const final{
        return "Transform qubits in pure gates into SSA form.";
    }
};

void registerPureGateDetect(){
    mlir::PassRegistration<PureGateDetectPass>();
}

}
}
}