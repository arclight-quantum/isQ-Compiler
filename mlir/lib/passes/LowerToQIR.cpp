#include "isq/Dialect.h"
#include "isq/Lower.h"
#include "isq/Operations.h"
#include "isq/QTypes.h"
#include "isq/GateDefTypes.h"
#include "mlir/Conversion/ReconcileUnrealizedCasts/ReconcileUnrealizedCasts.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinDialect.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/OperationSupport.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Rewrite/FrozenRewritePatternSet.h"
#include "mlir/Transforms/DialectConversion.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include "mlir/Dialect/Func/Transforms/FuncConversions.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/Support/ErrorHandling.h>
#include <mlir/Dialect/Func/IR/FuncOps.h>
#include <mlir/IR/BuiltinTypeInterfaces.h>
#include <mlir/Pass/PassManager.h>
#include <mlir/Transforms/Passes.h>
#include <optional>

namespace isq{
namespace ir{
namespace passes{

namespace lower_to_qir{

const char* ISQ_DEINITIALIZED = "isq_deinitialized";
const char* ISQ_INITIALIZED = "isq_initialized";
const char* ISQ_FIRST_STORE = "isq_first_store";
mlir::Value qubit_ref(mlir::Location loc, mlir::PatternRewriter& rewriter, mlir::Value v){
    auto r = rewriter.create<mlir::UnrealizedConversionCastOp>(loc, mlir::TypeRange{QIRQubitType::get(rewriter.getContext())}, mlir::ValueRange{v});
    return r.getResult(0);
}
mlir::Value qubit_unref(mlir::Location loc, mlir::PatternRewriter& rewriter, mlir::Value v){
    auto r = rewriter.create<mlir::UnrealizedConversionCastOp>(loc, mlir::TypeRange{QStateType::get(rewriter.getContext())}, mlir::ValueRange{v});
    return r.getResult(0);
}

mlir::MemRefType qubit_ref_type(mlir::MLIRContext* ctx, mlir::Location loc, mlir::MemRefType memrefty){
    lower::QIRExternQuantumFunc utils;
    auto new_memrefty = mlir::MemRefType::get(memrefty.getShape(), utils.getQIRQubitType(ctx), memrefty.getLayout(), memrefty.getMemorySpace());
    return new_memrefty;
}

class RuleReplaceAssertQ : public mlir::OpRewritePattern<AssertQOp>{
    mlir::ModuleOp rootModule;
public:
    RuleReplaceAssertQ(mlir::MLIRContext* ctx, mlir::ModuleOp module): mlir::OpRewritePattern<AssertQOp>(ctx, 1), rootModule(module){}

    mlir::LogicalResult matchAndRewrite(AssertQOp op, mlir::PatternRewriter &rewriter) const override{
        auto ctx = op->getContext();
        mlir::Location loc = op->getLoc();

        mlir::Value q = op.getCond();
        ::isq::ir::DenseComplexF64MatrixAttr mat = op.getSpace();
        auto val = mat.toMatrixVal();
        int64_t matLen = val.size();
        mlir::Float64Type floatType = mlir::Float64Type::get(ctx);
        mlir::MemRefType memrefType = mlir::MemRefType::get(llvm::ArrayRef<int64_t>{mlir::ShapedType::kDynamic}, floatType);
        rewriter.setInsertionPoint(op);
        mlir::arith::ConstantIndexOp length = rewriter.create<mlir::arith::ConstantIndexOp>(loc, matLen * matLen *2);
        mlir::Value memref = rewriter.create<mlir::memref::AllocOp>(loc, memrefType, mlir::ValueRange{length});
        int idx = 0;
        for (auto row : val) {
            for (Eigen::dcomplex v : row) {
                mlir::Value real = rewriter.create<mlir::arith::ConstantFloatOp>(loc, llvm::APFloat(v.real()), floatType);
                mlir::arith::ConstantIndexOp index = rewriter.create<mlir::arith::ConstantIndexOp>(loc, idx++);
                rewriter.create<mlir::memref::StoreOp>(loc, real, memref, mlir::ValueRange{index});
                mlir::Value imag = rewriter.create<mlir::arith::ConstantFloatOp>(loc, llvm::APFloat(v.imag()), floatType);
                mlir::arith::ConstantIndexOp index2 = rewriter.create<mlir::arith::ConstantIndexOp>(loc, idx++);
                rewriter.create<mlir::memref::StoreOp>(loc, imag, memref, mlir::ValueRange{index2});
            }
        }
        lower::QIRExternQuantumFunc utils;
        utils.projectionAssert(loc, rewriter, rootModule, q, memref);
        rewriter.eraseOp(op);
        return mlir::success();
    }
};

// Remove gphase.
class RuleRemoveGPhaseAux : public mlir::OpRewritePattern<AccumulateGPhase>{
public:
    RuleRemoveGPhaseAux(mlir::MLIRContext* ctx): mlir::OpRewritePattern<AccumulateGPhase>(ctx, 1){}
    mlir::LogicalResult matchAndRewrite(AccumulateGPhase op,  mlir::PatternRewriter &rewriter) const override{
        rewriter.eraseOp(op);
        return mlir::success();
    }
};
// Insert QIR-qubit-alloc.
class RuleInitializeAllocQubit : public mlir::OpRewritePattern<mlir::memref::AllocOp>{
    mlir::ModuleOp rootModule;
public:
    RuleInitializeAllocQubit(mlir::MLIRContext* ctx, mlir::ModuleOp module): mlir::OpRewritePattern<mlir::memref::AllocOp>(ctx, 1), rootModule(module){}
    mlir::LogicalResult matchAndRewrite(mlir::memref::AllocOp op,  mlir::PatternRewriter &rewriter) const override{
        lower::QIRExternQuantumFunc utils;
        auto ctx = op->getContext();
        auto memrefty = op.getType();
        if(!memrefty.getElementType().isa<QStateType>()) return mlir::failure();
        auto shape = memrefty.getShape();
        // One-dim known arrays supported only.
        if(shape.size()!=1) return mlir::failure();
        if(op->hasAttr(ISQ_INITIALIZED)) return mlir::failure();
        
        rewriter.updateRootInPlace(op, [&]{
            op->setAttr(ISQ_INITIALIZED, ::mlir::UnitAttr::get(ctx));
        });
        mlir::PatternRewriter::InsertionGuard guard(rewriter);
        rewriter.setInsertionPointAfter(op);
        auto loc = op.getLoc();
        // Create an `scf.for` op. 
        auto lo = rewriter.create<mlir::arith::ConstantIndexOp>(loc, 0);
        mlir::Value hi;
        if (memrefty.isDynamicDim(0)) {
            hi = *op.getDynamicSizes().begin();
        }
        else {
            hi = rewriter.create<mlir::arith::ConstantIndexOp>(loc, memrefty.getDimSize(0));
        }
        auto step = rewriter.create<mlir::arith::ConstantIndexOp>(loc, 1);
        auto loop =
          rewriter.create<mlir::scf::ForOp>(loc, lo, hi, step, mlir::ValueRange{}, [&](mlir::OpBuilder& b, mlir::Location loc, mlir::Value iv, mlir::ValueRange iterArgs){
              
          });
        rewriter.updateRootInPlace(loop, [&]{
            rewriter.setInsertionPointToEnd(loop.getBody());
            auto alloc_qubit = utils.allocQubit(loc, rewriter, rootModule);
            auto store = rewriter.create<mlir::memref::StoreOp>(loc, qubit_unref(loc, rewriter, alloc_qubit), op.getMemref(), mlir::ValueRange{loop.getInductionVar()});
            store->setAttr(ISQ_FIRST_STORE, ::mlir::UnitAttr::get(ctx));
            rewriter.create<mlir::scf::YieldOp>(loc);
        });
        return mlir::success();
    }
};
class RuleDeinitializeFreeQubit : public mlir::OpRewritePattern<mlir::memref::DeallocOp>{
    mlir::ModuleOp rootModule;
public:
    RuleDeinitializeFreeQubit(mlir::MLIRContext* ctx, mlir::ModuleOp module): mlir::OpRewritePattern<mlir::memref::DeallocOp>(ctx, 1), rootModule(module){}
    mlir::LogicalResult matchAndRewrite(mlir::memref::DeallocOp op,  mlir::PatternRewriter &rewriter) const override{
        lower::QIRExternQuantumFunc utils;
        auto ctx = op->getContext();
        auto memrefty = op.getMemref().getType().dyn_cast<mlir::MemRefType>();
        assert(memrefty);
        if(!memrefty) return mlir::failure();
        if(!memrefty.getElementType().isa<QStateType>() && !memrefty.getElementType().isa<QIRQubitType>()) return mlir::failure();

        auto shape = memrefty.getShape();
        // One-dim arrays supported only.
        if(shape.size()!=1) return mlir::failure();
        if(op->hasAttr(ISQ_DEINITIALIZED)) return mlir::failure();
        
        rewriter.updateRootInPlace(op, [&]{
            op->setAttr(ISQ_DEINITIALIZED, ::mlir::UnitAttr::get(ctx));
        });
        mlir::PatternRewriter::InsertionGuard guard(rewriter);
        rewriter.setInsertionPoint(op);
        auto loc = op.getLoc();
        // Create an `scf.for` op. 
        auto lo = rewriter.create<mlir::arith::ConstantIndexOp>(loc, 0);
        auto hi = rewriter.create<mlir::memref::DimOp>(loc, op.getMemref(), 0);
        auto step = rewriter.create<mlir::arith::ConstantIndexOp>(loc, 1);
        auto loop =
          rewriter.create<mlir::scf::ForOp>(loc, lo, hi, step, mlir::ValueRange{}, [&](mlir::OpBuilder& b, mlir::Location loc, mlir::Value iv, mlir::ValueRange iterArgs){
        });
        rewriter.updateRootInPlace(loop, [&]{
            rewriter.setInsertionPointToEnd(loop.getBody());
            auto load = rewriter.create<mlir::memref::LoadOp>(loc, utils.getQStateType(ctx), op.getMemref(), mlir::ValueRange{loop.getInductionVar()});
            utils.releaseQubit(loc, rewriter, rootModule, qubit_ref(loc, rewriter, load));
            rewriter.create<mlir::scf::YieldOp>(loc);
        });
        return mlir::success();
    }
};

class RuleInitDeinitGlobalQubit : public mlir::OpRewritePattern<mlir::memref::GlobalOp>{
    mlir::ModuleOp rootModule;
public:
    RuleInitDeinitGlobalQubit(mlir::MLIRContext* ctx, mlir::ModuleOp module): mlir::OpRewritePattern<mlir::memref::GlobalOp>(ctx, 1), rootModule(module){}
    mlir::LogicalResult matchAndRewrite(mlir::memref::GlobalOp op,  mlir::PatternRewriter &rewriter) const override{
        lower::QIRExternQuantumFunc utils;
        auto ctx = op->getContext();
        auto memrefty = op.getType().dyn_cast<mlir::MemRefType>();
        assert(memrefty);
        if(!memrefty) return mlir::failure();
        if(!memrefty.getElementType().isa<QStateType>()) return mlir::failure();

        auto shape = memrefty.getShape();
        // One-dim known arrays supported only.
        if(shape.size()!=1) return mlir::failure();
        if(memrefty.isDynamicDim(0)){
            return mlir::failure();
        }
        if(op->hasAttr(ISQ_INITIALIZED)) return mlir::failure();
        
        rewriter.updateRootInPlace(op, [&]{
            op->setAttr(ISQ_INITIALIZED, ::mlir::UnitAttr::get(ctx));
        });
        mlir::PatternRewriter::InsertionGuard guard(rewriter);
        rewriter.setInsertionPoint(op);
        auto loc = op.getLoc();
        auto rootModule = this->rootModule;
        // Ctor
        do{
        auto ctor = rootModule.lookupSymbol<mlir::func::FuncOp>("__isq__global_initialize");
        //assert(ctor);
        rewriter.setInsertionPointToStart(&*ctor.getBody().begin());
        auto ctor_used_memref = rewriter.create<mlir::memref::GetGlobalOp>(loc, op.getType(), op.getSymName());
        auto ctor_lo = rewriter.create<mlir::arith::ConstantIndexOp>(loc, 0);
        auto ctor_hi = rewriter.create<mlir::arith::ConstantIndexOp>(loc, memrefty.getDimSize(0));
        auto ctor_step = rewriter.create<mlir::arith::ConstantIndexOp>(loc, 1);
        auto ctor_loop =
          rewriter.create<mlir::scf::ForOp>(loc, ctor_lo, ctor_hi, ctor_step, mlir::ValueRange{}, [&](mlir::OpBuilder& b, mlir::Location loc, mlir::Value iv, mlir::ValueRange iterArgs){
              
        });
        rewriter.updateRootInPlace(ctor_loop, [&]{
            rewriter.setInsertionPointToEnd(ctor_loop.getBody());
            auto alloc_qubit = utils.allocQubit(loc, rewriter, rootModule);
            auto store = rewriter.create<mlir::memref::StoreOp>(loc, qubit_unref(loc, rewriter, alloc_qubit), ctor_used_memref.getResult(), mlir::ValueRange{ctor_loop.getInductionVar()});
            store->setAttr(ISQ_FIRST_STORE, ::mlir::UnitAttr::get(ctx));
            rewriter.create<mlir::scf::YieldOp>(loc);
        });

        }while(0);
        // Dtor
        do{
        auto dtor = rootModule.lookupSymbol<mlir::func::FuncOp>("__isq__global_finalize");
        //assert(dtor);
        rewriter.setInsertionPointToStart(&*dtor.getBody().begin());
        auto dtor_used_memref = rewriter.create<mlir::memref::GetGlobalOp>(loc, op.getType(), op.getSymName());
        auto dtor_lo = rewriter.create<mlir::arith::ConstantIndexOp>(loc, 0);
        auto dtor_hi = rewriter.create<mlir::arith::ConstantIndexOp>(loc, memrefty.getDimSize(0));
        auto dtor_step = rewriter.create<mlir::arith::ConstantIndexOp>(loc, 1);
        auto dtor_loop =
          rewriter.create<mlir::scf::ForOp>(loc, dtor_lo, dtor_hi, dtor_step, mlir::ValueRange{}, [&](mlir::OpBuilder& b, mlir::Location loc, mlir::Value iv, mlir::ValueRange iterArgs){
              
        });
        rewriter.updateRootInPlace(dtor_loop, [&]{
            rewriter.setInsertionPointToEnd(dtor_loop.getBody());
            auto load = rewriter.create<mlir::memref::LoadOp>(loc, utils.getQStateType(ctx), dtor_used_memref.getResult(), mlir::ValueRange{dtor_loop.getInductionVar()});
            utils.releaseQubit(loc, rewriter, rootModule, qubit_ref(loc, rewriter, load));
            rewriter.create<mlir::scf::YieldOp>(loc);
        });
        }while(0);
        return mlir::success();
    }
};

class RuleExpandApplyQIR : public mlir::OpRewritePattern<ApplyGateOp>{
    mlir::ModuleOp rootModule;
public:
    RuleExpandApplyQIR(mlir::MLIRContext* ctx, mlir::ModuleOp module): mlir::OpRewritePattern<isq::ir::ApplyGateOp>(ctx, 2), rootModule(module){

    }
    mlir::LogicalResult matchAndRewrite(isq::ir::ApplyGateOp op,  mlir::PatternRewriter &rewriter) const override{
        auto use_op = mlir::dyn_cast_or_null<UseGateOp>(op.getGate().getDefiningOp());
        auto ctx = op->getContext();
        if(!use_op) return mlir::failure();
        auto defgate = mlir::SymbolTable::lookupNearestSymbolFrom<DefgateOp>(use_op.getOperation(), use_op.getName());
        assert(defgate);
        if(!defgate.getDefinition()) return mlir::failure();
        int id = 0;
        for(auto def: defgate.getDefinition()->getAsRange<GateDefinition>()){
            auto d = AllGateDefs::parseGateDefinition(defgate, id, defgate.getType(), def);
            if(d==std::nullopt) llvm_unreachable("bad");
            auto qirf= llvm::dyn_cast_or_null<QIRDefinition>(&**d);
            if(!qirf){
                id++;
                continue;
            }
            
            auto qir_name = qirf->getQIRName();
            mlir::SmallVector<mlir::Value> new_args;
            for(auto used_args : use_op.getParameters()){
                new_args.push_back(used_args);
            }
            for(auto i=0; i<op.getArgs().size(); i++){
                auto qarg = op.getArgs()[i];
                auto qout = op.getResult(i);
                qout.replaceAllUsesWith(qarg);
                auto qref = qubit_ref(op->getLoc(), rewriter, qarg);
                new_args.push_back(qref);
            }
            auto call = rewriter.create<mlir::func::CallOp>(op.getLoc(), qir_name, ::mlir::TypeRange{}, new_args);
            rewriter.eraseOp(op);
            return mlir::success();

        }
        return mlir::failure();
    }
};
class RuleReplaceQIRQOps : public mlir::OpRewritePattern<CallQOpOp>{
    mlir::ModuleOp rootModule;
public:
    RuleReplaceQIRQOps(mlir::MLIRContext* ctx, mlir::ModuleOp module): mlir::OpRewritePattern<CallQOpOp>(ctx, 1), rootModule(module){}

    mlir::LogicalResult matchAndRewrite(CallQOpOp op,  
    mlir::PatternRewriter &rewriter) const override{
        lower::QIRExternQuantumFunc utils;
        auto rootModule = this->rootModule;
        auto qop = rootModule.lookupSymbol<DeclareQOpOp>(op.getCallee());
        assert(qop);
        // First, we unwire next ops.
        #define UNWIRE \
        for(auto i=0; i<op.getSize(); i++){ \
            auto output = op->getResult(i); \
            auto input = op->getOperand(i); \
            output.replaceAllUsesWith(input); \
        }
        auto loc = op->getLoc();
        if(qop.getSymName() == "__isq__builtin__measure"){
            UNWIRE;
            auto meas_result = utils.measureQubit(loc, rewriter, rootModule, qubit_ref(loc, rewriter, op->getOperand(0)));
            op->getResult(1).replaceAllUsesWith(meas_result);
            rewriter.eraseOp(op);
            return mlir::success();
        }
        if(qop.getSymName() == "__isq__builtin__reset"){
            UNWIRE;
            utils.reset(loc, rewriter, rootModule, qubit_ref(loc, rewriter, op->getOperand(0)));
            rewriter.eraseOp(op);
            return mlir::success();
        }
        if(qop.getSymName() == "__isq__builtin__bp"){
            // Don't unwire.
            utils.breakPoint(loc, rewriter, rootModule, op.getOperand(0));
            rewriter.eraseOp(op);
            return mlir::success();
        }
        if(qop.getSymName() == "__isq__builtin__print_int"){
            // Don't unwire.
            utils.printInt(loc, rewriter, rootModule, op.getOperand(0));
            rewriter.eraseOp(op);
            return mlir::success();
        }
        if(qop.getSymName() == "__isq__builtin__print_double"){
            // Don't unwire.
            utils.printFloat(loc, rewriter, rootModule, op.getOperand(0));
            rewriter.eraseOp(op);
            return mlir::success();
        }
        if (qop.getSymName() == "__isq__qmpiprim__csend") {
            utils.qmpiCsend(loc, rewriter, rootModule, op.getOperand(0), op.getOperand(1), op.getOperand(2));
            rewriter.eraseOp(op);
            return mlir::success();
        }
        if (qop.getSymName() == "__isq__qmpiprim__crecv") {
            auto val = utils.qmpiCrecv(loc, rewriter, rootModule, op.getOperand(0), op.getOperand(1));
            op->getResult(0).replaceAllUsesWith(val);
            rewriter.eraseOp(op);
            return mlir::success();
        }
        if (qop.getSymName() == "__isq__qmpiprim__size") {
            auto val = utils.qmpiSize(loc, rewriter, rootModule);
            op->getResult(0).replaceAllUsesWith(val);
            rewriter.eraseOp(op);
            return mlir::success();
        }

        return mlir::failure();
    }
};

class RuleEliminateRefStore : public mlir::OpRewritePattern<mlir::memref::StoreOp>{
public:
    RuleEliminateRefStore(mlir::MLIRContext* ctx): mlir::OpRewritePattern<mlir::memref::StoreOp>(ctx, 1){

    }
    mlir::LogicalResult matchAndRewrite(mlir::memref::StoreOp op,  mlir::PatternRewriter &rewriter) const override{
        if(op.getValue().getType().isa<QStateType>() && !op->hasAttr(ISQ_FIRST_STORE)){
            rewriter.eraseOp(op);
            return mlir::success();
        }
        return mlir::failure();
    }
};

template<class T>
class RuleErase : public mlir::OpRewritePattern<T>{
public:
    RuleErase(mlir::MLIRContext* ctx): mlir::OpRewritePattern<T>(ctx, 1){
    }
    mlir::LogicalResult matchAndRewrite(T op,  mlir::PatternRewriter &rewriter) const override{
        rewriter.eraseOp(op);
        return ::mlir::success();
    }
};


template<class T>
class RuleRemoveDeinitAttr : public mlir::OpRewritePattern<T>{
public:
    RuleRemoveDeinitAttr(mlir::MLIRContext* ctx): mlir::OpRewritePattern<T>(ctx, 1){

    }
    mlir::LogicalResult matchAndRewrite(T op,  mlir::PatternRewriter &rewriter) const override{
        if(!op->hasAttr(ISQ_DEINITIALIZED)){
            return ::mlir::failure();
        }
        op->removeAttr(ISQ_DEINITIALIZED);
        return ::mlir::success();
    }
};

template<class T>
class RuleRemoveInitAttr : public mlir::OpRewritePattern<T>{
public:
    RuleRemoveInitAttr(mlir::MLIRContext* ctx): mlir::OpRewritePattern<T>(ctx, 1){

    }
    mlir::LogicalResult matchAndRewrite(T op,  mlir::PatternRewriter &rewriter) const override{
        if(!op->hasAttr(ISQ_INITIALIZED)){
            return ::mlir::failure();
        }
        op->removeAttr(ISQ_INITIALIZED);
        return ::mlir::success();
    }
};

class RuleRemoveFirstStoreAttr : public mlir::OpRewritePattern<mlir::memref::StoreOp>{
public:
    RuleRemoveFirstStoreAttr(mlir::MLIRContext* ctx): mlir::OpRewritePattern<mlir::memref::StoreOp>(ctx, 1){

    }
    mlir::LogicalResult matchAndRewrite(mlir::memref::StoreOp op,  mlir::PatternRewriter &rewriter) const override{
        if(!op->hasAttr(ISQ_FIRST_STORE)){
            return ::mlir::failure();
        }
        op->removeAttr(ISQ_FIRST_STORE);
        return ::mlir::success();
    }
};

template<class T>
class TypeReplacer : public mlir::OpConversionPattern<T>{
protected:
    mlir::TypeConverter& converter;
    mlir::UnrealizedConversionCastOp legalize(mlir::Location loc, mlir::PatternRewriter& rewriter, mlir::Value curr) const{
        return rewriter.create<mlir::UnrealizedConversionCastOp>(loc, mlir::TypeRange{converter.convertType(curr.getType())}, mlir::ValueRange{curr});
    }
    mlir::UnrealizedConversionCastOp unlegalize(mlir::Location loc, mlir::PatternRewriter& rewriter, mlir::Value curr) const{
        auto old_type = curr.getType();
        curr.setType(converter.convertType(curr.getType()));
        return rewriter.create<mlir::UnrealizedConversionCastOp>(loc, mlir::TypeRange{old_type}, mlir::ValueRange{curr});
    }
public:
    TypeReplacer(mlir::MLIRContext* ctx, mlir::TypeConverter& converter): mlir::OpConversionPattern<T>(ctx, 1), converter(converter){
    }
};

class LowerLoad : public TypeReplacer<mlir::memref::LoadOp>{
public:
    LowerLoad(mlir::MLIRContext* ctx, mlir::TypeConverter& converter): TypeReplacer<mlir::memref::LoadOp>(ctx, converter){}
    mlir::LogicalResult matchAndRewrite(mlir::memref::LoadOp op,  OpAdaptor adaptor, mlir::ConversionPatternRewriter &rewriter) const override{
        rewriter.startRootUpdate(op);
        op.setMemRef(adaptor.getMemref());
        op.getResult().setType(converter.convertType(op.getResult().getType()));
        rewriter.finalizeRootUpdate(op);
        return mlir::success();
    }
};
class LowerStore : public TypeReplacer<mlir::memref::StoreOp>{
public:
    LowerStore(mlir::MLIRContext* ctx, mlir::TypeConverter& converter): TypeReplacer<mlir::memref::StoreOp>(ctx, converter){}
    mlir::LogicalResult matchAndRewrite(mlir::memref::StoreOp op,  OpAdaptor adaptor, mlir::ConversionPatternRewriter &rewriter) const override{
        rewriter.startRootUpdate(op);
        auto new_value = this->legalize(op->getLoc(), rewriter, adaptor.getValue());
        op.getValueMutable().assign(new_value.getResult(0));
        op.setMemRef(adaptor.getMemref());
        rewriter.finalizeRootUpdate(op);
        return mlir::success();
    }
};


template<class T>
class LowerAllocLike : public TypeReplacer<T>{
public:
    LowerAllocLike(mlir::MLIRContext* ctx, mlir::TypeConverter& converter): TypeReplacer<T>(ctx, converter){}
    mlir::LogicalResult matchAndRewrite(T op, typename TypeReplacer<T>::OpAdaptor adaptor, mlir::ConversionPatternRewriter &rewriter) const override{
        rewriter.startRootUpdate(op);
        op.getMemref().setType(this->converter.convertType(op.getMemref().getType()));
        rewriter.finalizeRootUpdate(op);
        return mlir::success();
    }
};
using LowerAlloc = LowerAllocLike<mlir::memref::AllocOp>;
using LowerAlloca = LowerAllocLike<mlir::memref::AllocaOp>;

class LowerSubView : public TypeReplacer<mlir::memref::SubViewOp>{
public:
    LowerSubView(mlir::MLIRContext* ctx, mlir::TypeConverter& converter): TypeReplacer<mlir::memref::SubViewOp>(ctx, converter){}
    mlir::LogicalResult matchAndRewrite(mlir::memref::SubViewOp op,  OpAdaptor adaptor, mlir::ConversionPatternRewriter &rewriter) const override{
        
        rewriter.startRootUpdate(op);
        op->setOperands(adaptor.getOperands());
        op.getResult().setType(this->converter.convertType(op.getResult().getType()));
        rewriter.finalizeRootUpdate(op);
        return mlir::success();
    }
};
class LowerCast : public TypeReplacer<mlir::memref::CastOp>{
public:
    LowerCast(mlir::MLIRContext* ctx, mlir::TypeConverter& converter): TypeReplacer<mlir::memref::CastOp>(ctx, converter){}
    mlir::LogicalResult matchAndRewrite(mlir::memref::CastOp op,  OpAdaptor adaptor, mlir::ConversionPatternRewriter &rewriter) const override{
        rewriter.startRootUpdate(op);
        op->setOperands(adaptor.getOperands());
        op.getResult().setType(this->converter.convertType(op.getResult().getType()));
        rewriter.finalizeRootUpdate(op);
        return mlir::success();
    }
};
class LowerGlobal : public TypeReplacer<mlir::memref::GlobalOp>{
public:
    LowerGlobal(mlir::MLIRContext* ctx, mlir::TypeConverter& converter): TypeReplacer<mlir::memref::GlobalOp>(ctx, converter){}
    mlir::LogicalResult matchAndRewrite(mlir::memref::GlobalOp op,  OpAdaptor adaptor, mlir::ConversionPatternRewriter &rewriter) const override{
        auto old_type = op.getType();
        rewriter.startRootUpdate(op);
        op.setTypeAttr(::mlir::TypeAttr::get(converter.convertType(old_type)));
        rewriter.finalizeRootUpdate(op);
        return mlir::success();
    }
};
class LowerGetGlobal : public TypeReplacer<mlir::memref::GetGlobalOp>{
public:
    LowerGetGlobal(mlir::MLIRContext* ctx, mlir::TypeConverter& converter): TypeReplacer<mlir::memref::GetGlobalOp>(ctx, converter){}
    mlir::LogicalResult matchAndRewrite(mlir::memref::GetGlobalOp op,  OpAdaptor adaptor, mlir::ConversionPatternRewriter &rewriter) const override{
        rewriter.startRootUpdate(op);
        op.getResult().setType(this->converter.convertType(op.getResult().getType()));
        rewriter.finalizeRootUpdate(op);
        return mlir::success();
    }
};
class LowerDim : public TypeReplacer<mlir::memref::DimOp>{
public:
    LowerDim(mlir::MLIRContext* ctx, mlir::TypeConverter& converter): TypeReplacer<mlir::memref::DimOp>(ctx, converter){}
    mlir::LogicalResult matchAndRewrite(mlir::memref::DimOp op,  OpAdaptor adaptor, mlir::ConversionPatternRewriter &rewriter) const override{
        rewriter.startRootUpdate(op);
        auto new_value = this->legalize(op->getLoc(), rewriter, adaptor.getSource());
        op.getSourceMutable().assign(new_value.getResult(0));
        rewriter.finalizeRootUpdate(op);
        return mlir::success();
    }
};
class LowerConstant : public TypeReplacer<mlir::func::ConstantOp>{
public:
    LowerConstant(mlir::MLIRContext* ctx, mlir::TypeConverter& converter): TypeReplacer<mlir::func::ConstantOp>(ctx, converter){}
    mlir::LogicalResult matchAndRewrite(mlir::func::ConstantOp op,  OpAdaptor adaptor, mlir::ConversionPatternRewriter &rewriter) const override{
        rewriter.startRootUpdate(op);
        op.getResult().setType(this->converter.convertType(op.getResult().getType()));
        
        auto result = op.getResult();
        if (!result.use_empty()){
            mlir::SmallVector<mlir::Operation*> users(result.getUsers().begin(), result.getUsers().end());
            for(auto user: users){
                if (auto uop = mlir::dyn_cast_or_null<mlir::UnrealizedConversionCastOp>(user)){
                    uop.getResult(0).setType(this->converter.convertType(uop.getResult(0).getType()));
                }
            }
        }
        
        rewriter.finalizeRootUpdate(op);
        return mlir::success();
    }
};
class LowerCallIndirectOp : public TypeReplacer<mlir::func::CallIndirectOp>{
public:
    LowerCallIndirectOp(mlir::MLIRContext* ctx, mlir::TypeConverter& converter): TypeReplacer<mlir::func::CallIndirectOp>(ctx, converter){}
    mlir::LogicalResult matchAndRewrite(mlir::func::CallIndirectOp op,  OpAdaptor adaptor, mlir::ConversionPatternRewriter &rewriter) const override{
        auto old_type = op.getCallee().getType();
        rewriter.startRootUpdate(op);
        for (auto ope : op.getCalleeOperands()){
            ope.setType(converter.convertType(ope.getType()));
        }
        op.getCallee().setType(converter.convertType(old_type));
        rewriter.finalizeRootUpdate(op);
        return mlir::success();
    }
};
struct LowerToQIRRepPass : public mlir::PassWrapper<LowerToQIRRepPass, mlir::OperationPass<mlir::ModuleOp>>{
    void populateUsefulPatternSets(mlir::RewritePatternSet& patterns, mlir::TypeConverter& converter ){
        mlir::populateFunctionOpInterfaceTypeConversionPattern<mlir::func::FuncOp>(patterns, converter);
        mlir::populateCallOpTypeConversionPattern(patterns, converter);
        mlir::populateBranchOpInterfaceTypeConversionPattern(patterns, converter);
        mlir::populateReturnOpTypeConversionPattern(patterns, converter);
    }
    void runOnOperation() override {
        mlir::ModuleOp m = this->getOperation();
        auto ctx = m->getContext();
        
        do{
        mlir::RewritePatternSet rps(ctx);
        rps.add<RuleReplaceAssertQ>(ctx, m);
        rps.add<RuleRemoveGPhaseAux>(ctx);
        rps.add<RuleInitializeAllocQubit>(ctx, m);
        rps.add<RuleDeinitializeFreeQubit>(ctx, m);
        rps.add<RuleInitDeinitGlobalQubit>(ctx, m);
        rps.add<RuleExpandApplyQIR>(ctx, m);
        rps.add<RuleReplaceQIRQOps>(ctx, m);
        mlir::FrozenRewritePatternSet frps(std::move(rps));
        (void)mlir::applyPatternsAndFoldGreedily(m.getOperation(), frps);
        }while(0);
        do{
        mlir::RewritePatternSet rps(ctx);
        rps.add<RuleEliminateRefStore>(ctx);
        mlir::FrozenRewritePatternSet frps(std::move(rps));
        (void)mlir::applyPatternsAndFoldGreedily(m.getOperation(), frps);
        }while(0);
        // erase temporary attr.
        mlir::RewritePatternSet rps2(ctx);
        rps2.add<RuleRemoveDeinitAttr<mlir::memref::DeallocOp>>(ctx);
        rps2.add<RuleRemoveInitAttr<mlir::memref::AllocOp>>(ctx);
        rps2.add<RuleRemoveInitAttr<mlir::memref::GlobalOp>>(ctx);
        rps2.add<RuleErase<DeclareQOpOp>>(ctx);
        rps2.add<RuleErase<UseGateOp>>(ctx);
        rps2.add<RuleErase<DefgateOp>>(ctx);
        rps2.add<RuleRemoveFirstStoreAttr>(ctx);
        mlir::FrozenRewritePatternSet frps2(std::move(rps2));
        (void)mlir::applyPatternsAndFoldGreedily(m.getOperation(), frps2);
        // finally, convert all types.
        do{
            // Type conversion.
            mlir::RewritePatternSet rps(ctx);
            mlir::TypeConverter converter;
            converter.addConversion([](mlir::Type ty){
                return ty;
            });
            converter.addConversion([&](QStateType ty){
                return QIRQubitType::get(ctx);
            });
            converter.addConversion([&](mlir::FunctionType ty){
                llvm::SmallVector<mlir::Type> input = {};
                for (auto ity: ty.getInputs()){
                    input.push_back(converter.convertType(ity));
                }
                llvm::SmallVector<mlir::Type> output = {};
                for (auto oty: ty.getResults()){
                    output.push_back(converter.convertType(oty));
                }
                auto newty =  ty.clone(input, output);
                return newty;
            });
            converter.addConversion([&](mlir::MemRefType ty){
                return mlir::MemRefType::get(ty.getShape(), converter.convertType(ty.getElementType()), ty.getLayout(), ty.getMemorySpace());
            });
            auto addUnrealizedCast = [](mlir::OpBuilder &builder, mlir::Type type, mlir::ValueRange inputs,
                                        mlir::Location loc) {
                auto cast = builder.create<mlir::UnrealizedConversionCastOp>(loc, type, inputs);
                return mlir::Optional<mlir::Value>(cast.getResult(0));
            };

            converter.addSourceMaterialization(addUnrealizedCast);
            converter.addTargetMaterialization(addUnrealizedCast);
            rps.add<LowerLoad>(ctx, converter);
            rps.add<LowerStore>(ctx, converter);
            rps.add<LowerAlloc>(ctx, converter);
            rps.add<LowerAlloca>(ctx, converter);
            rps.add<LowerCast>(ctx, converter);
            rps.add<LowerSubView>(ctx, converter);
            rps.add<LowerGlobal>(ctx, converter);
            rps.add<LowerGetGlobal>(ctx, converter);
            rps.add<LowerDim>(ctx, converter);
            rps.add<LowerConstant>(ctx, converter);
            rps.add<LowerCallIndirectOp>(ctx, converter);
            populateUsefulPatternSets(rps, converter);
            mlir::ConversionTarget target(*ctx);
            target.addIllegalDialect<ISQDialect>();
            target.addLegalOp<AssertOp>();
            target.addLegalDialect<mlir::arith::ArithDialect>();
            target.addLegalOp<mlir::UnrealizedConversionCastOp>();
            target.addDynamicallyLegalOp<mlir::func::FuncOp>(
                [&](mlir::func::FuncOp op) { return converter.isSignatureLegal(op.getFunctionType()); });
            target.addDynamicallyLegalOp<mlir::func::ReturnOp>(
                [&](mlir::func::ReturnOp op) { return converter.isLegal(op.getOperandTypes()); });
            target.addDynamicallyLegalOp<mlir::func::CallOp>([&](mlir::func::CallOp op) {
                return converter.isSignatureLegal(op.getCalleeType());
            });
            target.addDynamicallyLegalDialect<mlir::memref::MemRefDialect>([&](mlir::Operation* op){
                if(auto global = mlir::dyn_cast<mlir::memref::GlobalOp>(op)){
                    return converter.isLegal(global.getType());
                }
                return converter.isLegal(op);
            });
            if (failed(applyPartialConversion(m, target, std::move(rps)))){
                m.dump();
                signalPassFailure();
                return;
            }
        }while(0);
        // Lastly, throw off intermediate unrealized casts.
        do{
            mlir::RewritePatternSet rps(ctx);
            mlir::populateReconcileUnrealizedCastsPatterns(rps);
            mlir::FrozenRewritePatternSet frps(std::move(rps));
            (void)mlir::applyPatternsAndFoldGreedily(m.getOperation(), frps);
        }while(0);
        /*
        mlir::PassManager pm(ctx);
        applyPassManagerCLOptions(pm);
        pm.addNestedPass<mlir::FuncOp>(mlir::createCanonicalizerPass());
        if (mlir::failed(pm.run(m))){
            return signalPassFailure();
        }
        */
    }
  mlir::StringRef getArgument() const final {
    return "isq-lower-to-qir-rep";
  }
  mlir::StringRef getDescription() const final {
    return  "Lower QState-based representation to QIR-alike reference representations.";
  }
};


}

void registerLowerToQIRRep(){
    using namespace lower_to_qir;
    mlir::PassRegistration<LowerToQIRRepPass>();
}

}
}
}