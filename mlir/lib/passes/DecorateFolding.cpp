#include "isq/GateDefTypes.h"
#include "isq/Operations.h"
#include "isq/QAttrs.h"
#include "isq/QTypes.h"
#include "isq/passes/Mem2Reg.h"
#include "isq/passes/Passes.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/IR/SymbolTable.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Rewrite/FrozenRewritePatternSet.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Support/LogicalResult.h"
#include "llvm/Support/Casting.h"
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/raw_ostream.h>
#include <mlir/IR/Attributes.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/OperationSupport.h>
#include <mlir/IR/TypeRange.h>
#include <mlir/Pass/PassManager.h>
#include <optional>
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
namespace isq{
namespace ir{
namespace passes{
namespace{

template<isq::ir::math::MatDouble M>
M appendMatrix(const M& mat, ::mlir::ArrayRef<bool> ctrl, bool adj){
    auto mat_qubit_num = (int)std::log2(mat.size());
    auto new_mat_size = ((1<<ctrl.size()) * mat.size());
    M new_matrix;
    new_matrix.resize(new_mat_size);
    for(auto i=0; i<new_mat_size; i++){
        new_matrix[i].resize(new_mat_size);
        new_matrix[i][i]=1.0;
    }
    uint64_t mat_mask = 0;
    for(auto i=0; i<ctrl.size(); i++){
        mat_mask = (mat_mask<<1) | (ctrl[i]?1:0);
    }
    mat_mask = mat_mask << mat_qubit_num;
    for(auto i=0; i<(1<<mat_qubit_num); i++){
        for(auto j=0; j<(1<<mat_qubit_num); j++){
            if(adj){
                new_matrix[i|mat_mask][j|mat_mask] = std::conj(mat[j][i]);
            }else{
                new_matrix[i|mat_mask][j|mat_mask] = mat[i][j];
            }
            
        }
    }
    return new_matrix;
}
namespace{
    const char* ISQ_ATTR_GATE_SIZE = "ISQ_ATTR_GATE_SIZE";
}
const char* ISQ_DECORATE_FOLDING_PROPAGATE_CTRL_START_INDEX = "ISQ_DECORATE_FOLDING_PROPAGATE_CTRL_START_INDEX";
const char* ISQ_DECORATE_FOLDING_PROPAGATE_CTRL_BITS = "ISQ_DECORATE_FOLDING_PROPAGATE_CTRL_BITS";
const char* ISQ_DECORATE_FOLDING_PROPAGATE_TO_PROCESS = "ISQ_DECORATE_FOLDING_PROPAGATE_TO_PROCESS";
const char* ISQ_FAKELOAD = "isq.intermediate.fakeload";
const char* ISQ_FAKESTORE = "isq.intermediate.fakestore";
const char* ISQ_FAKELOADSTORE_ID = "ISQ_FAKELOADSTORE_ID";
const char* ISQ_REPLACED_SIG = "ISQ_REPLACED_SIG";

struct DecorateFoldRewriteRule : public mlir::OpRewritePattern<isq::ir::ApplyGateOp>{
    mlir::ModuleOp rootModule;
    bool* dirty;
    bool ignore_sq_adj;
    DecorateFoldRewriteRule(mlir::MLIRContext* ctx, mlir::ModuleOp module, bool* dirty, bool ignore_sq_adj): mlir::OpRewritePattern<isq::ir::ApplyGateOp>(ctx, 1), rootModule(module), dirty(dirty), ignore_sq_adj(ignore_sq_adj){

    }

    mlir::LogicalResult createControlledDefgate(isq::ir::DefgateOp defgate, mlir::ArrayRef<bool> ctrl, bool adj, mlir::FlatSymbolRefAttr sym, mlir::PatternRewriter &rewriter, mlir::ArrayAttr parameters) const{
        auto ctx = rewriter.getContext();
        mlir::SmallVector<mlir::Attribute> usefulGatedefs;
        auto id=0;
        auto new_qubit_num = (int)defgate.type().getSize() + ctrl.size();
        for(auto def: defgate.definition()->getAsRange<GateDefinition>()){
            auto d = AllGateDefs::parseGateDefinition(defgate, id, defgate.type(), def);
            if(d==std::nullopt) return mlir::failure();
            if(auto mat = llvm::dyn_cast_or_null<MatrixDefinition>(&**d)){
            // Don't fold SQ matrices, since they can be decomposed more easily using subsequent passes.
            if(defgate.type().getSize()==1 && ctrl.size()>0) continue;
                auto& old_matrix = mat->getMatrix();
                // construct new matrix.
                auto new_matrix = appendMatrix(old_matrix, ctrl, adj);
                auto matrix_dev = createMatrixDef(ctx, new_matrix);
                usefulGatedefs.push_back(createMatrixDef(ctx, new_matrix));
            }else if(auto decomp = llvm::dyn_cast_or_null<DecompositionDefinition>(&**d)){
                auto ip = rewriter.saveInsertionPoint();
                auto fn = decomp->getDecomposedFunc();
                // TODO: adjoint op support.
                // Do we need to "revert" all steps?
                if(!fn->hasAttr(ISQ_GPHASE_REMOVED)){
                    auto new_fn = fn.clone();
                    mlir::ModuleOp rootModule = this->rootModule;
                    rewriter.setInsertionPointToStart(rootModule.getBody());
                    rewriter.insert(new_fn);
                    rewriter.startRootUpdate(new_fn);
                    if(adj){
                        new_fn->setAttr(ISQ_ATTR_GATE_SIZE, rewriter.getI64IntegerAttr(defgate.type().getSize()));
                    }
                    auto new_fn_name = "$__isq__decomposition__"+sym.getValue();
                    new_fn.setSymNameAttr(mlir::StringAttr::get(ctx, new_fn_name));
                    new_fn.setSymVisibilityAttr(mlir::StringAttr::get(ctx, "private"));
                    mlir::SmallVector<mlir::Attribute> ctrl_attr;
                    for(auto b: ctrl){
                        ctrl_attr.push_back(mlir::BoolAttr::get(ctx, b));
                    } 

                    // insert control qubits.
                    auto old_size = new_fn.getFunctionType().getNumInputs();
                    mlir::SmallVector<mlir::Type> args;
                    mlir::SmallVector<mlir::Type> results;
                    for(auto input: new_fn.getFunctionType().getInputs()){
                        args.push_back(input);
                    }
                    for(auto output: new_fn.getFunctionType().getResults()){
                        results.push_back(output);
                    }
                    for(auto i=0; i<ctrl.size(); i++){
                        args.push_back(QStateType::get(ctx));
                        results.push_back(QStateType::get(ctx));
                    }
                    auto replaced_fn_signature = mlir::FunctionType::get(ctx, args, new_fn.getFunctionType().getResults());
                    auto final_fn_signature = mlir::FunctionType::get(ctx, args, results);
                    new_fn.setType(replaced_fn_signature);
                    new_fn->setAttr(ISQ_REPLACED_SIG, mlir::TypeAttr::get(final_fn_signature));
                    mlir::SmallVector<mlir::Value> controlQubits;
                    auto insert_index = old_size - defgate.type().getSize();
                    for(auto i=0; i<ctrl.size(); i++){
                        controlQubits.push_back(new_fn.getBody().insertArgument(new_fn.getArguments().begin()+insert_index+i, QStateType::get(ctx), mlir::UnknownLoc::get(ctx)));
                    }
                    new_fn->setAttr(ISQ_DECORATE_FOLDING_PROPAGATE_CTRL_START_INDEX, mlir::IntegerAttr::get(rewriter.getI64Type(), insert_index));
                    new_fn->setAttr(ISQ_DECORATE_FOLDING_PROPAGATE_CTRL_BITS, mlir::ArrayAttr::get(ctx, ctrl_attr));
                    new_fn.walk([=](ApplyGateOp op){
                        op->setAttr(ISQ_DECORATE_FOLDING_PROPAGATE_TO_PROCESS, mlir::UnitAttr::get(ctx));
                    });
                    new_fn.walk([=](ApplyGPhase op){
                        op->setAttr(ISQ_DECORATE_FOLDING_PROPAGATE_TO_PROCESS, mlir::UnitAttr::get(ctx));
                    });
                    rewriter.finalizeRootUpdate(new_fn);
                    rewriter.restoreInsertionPoint(ip);
                    usefulGatedefs.push_back(GateDefinition::get(ctx, mlir::StringAttr::get(ctx, "decomposition"), mlir::FlatSymbolRefAttr::get(mlir::StringAttr::get(ctx, new_fn_name))));
                }
                
            }
            id++;
        }
        if(usefulGatedefs.size()==0){
            return mlir::failure();
        }
        auto ip = rewriter.saveInsertionPoint();
        mlir::ModuleOp rootModule = this->rootModule;
        rewriter.setInsertionPointToStart(rootModule.getBody());
        rewriter.create<DefgateOp>(::mlir::UnknownLoc::get(ctx), mlir::TypeAttr::get(GateType::get(ctx, new_qubit_num, GateTrait::General)), sym.getAttr(), mlir::StringAttr::get(ctx, "nested"), ::mlir::ArrayAttr{}, ::mlir::ArrayAttr::get(ctx, usefulGatedefs), parameters);
        rewriter.restoreInsertionPoint(ip);
        
        return mlir::success();
    }
    bool hasDecomposition(DefgateOp op) const{
        auto defs = *op.definition();
        auto id=0;
        for(auto def: defs.getAsRange<GateDefinition>()){
            auto d = AllGateDefs::parseGateDefinition(op, id, op.type(), def);
            if(d==std::nullopt) {
                llvm_unreachable("bad");
            }
            if(auto decomp = llvm::dyn_cast_or_null<DecompositionDefinition>(&**d)){
                return true;
            }
            id++;
        }
        return false;
    }
    mlir::LogicalResult matchAndRewrite(isq::ir::ApplyGateOp op,  mlir::PatternRewriter &rewriter) const override{
        // Check if it is a use-decorate-apply pattern.
        auto decorate_op = mlir::dyn_cast_or_null<DecorateOp>(op.gate().getDefiningOp());
        if(!decorate_op) return mlir::failure();
        auto use_op = mlir::dyn_cast_or_null<UseGateOp>(decorate_op.args().getDefiningOp());
        if(!use_op) return mlir::failure();
        auto defgate = mlir::SymbolTable::lookupNearestSymbolFrom<DefgateOp>(use_op.getOperation(), use_op.name());
        assert(defgate);
        if(!defgate.definition()) return mlir::failure();
        auto is_decomposed = hasDecomposition(defgate);
        if(use_op.parameters().size()>0 && !is_decomposed){
            return mlir::failure(); // Only matrix-gates are supported.
        }
        // Ignore sq adj.
        if(!is_decomposed && defgate.type().getSize()==1 && decorate_op.adjoint() && decorate_op.ctrl().size()==0 && this->ignore_sq_adj){
            return mlir::failure();
        }
        // controlled-cnot is controlled-cx
        if(isFamousGate(defgate, "CNOT") || isFamousGate(defgate, "Toffoli")){
            auto ctx = getContext();
            mlir::SmallVector<mlir::Value> operands;
            mlir::SmallVector<mlir::Attribute> newCtrl;
            for(auto operand: op.args()){
                operands.push_back(operand);
            }
            mlir::SmallVector<mlir::Value*> newOperands;
            for(auto& operand: operands){
                newOperands.push_back(&operand);
            }
            newCtrl.push_back(rewriter.getBoolAttr(true));
            if(isFamousGate(defgate, "Toffoli")){
                newCtrl.push_back(rewriter.getBoolAttr(true));
            }
            for(auto attr: decorate_op.ctrl().getValue()){
                newCtrl.push_back(attr);
            }
            emitBuiltinGate(rewriter, "X", newOperands, {}, mlir::ArrayAttr::get(ctx, newCtrl), false);
            rewriter.replaceOp(op, operands);
            *dirty=true;
            return mlir::success();
        }
        // construct new matrix name.
        auto new_defgate_name = std::string(defgate.sym_name());
        if(decorate_op.adjoint()){
            new_defgate_name += "_adj";
        }
        if(decorate_op.ctrl().size()>0){
            new_defgate_name+="_ctrl_";
            for(auto c: decorate_op.ctrl().getAsValueRange<mlir::BoolAttr>()){
                new_defgate_name+= c?"1":"0";
            }
        }

        auto new_defgate_sym = mlir::FlatSymbolRefAttr::get(mlir::StringAttr::get(rewriter.getContext(), new_defgate_name));
        auto new_defgate = mlir::SymbolTable::lookupNearestSymbolFrom<DefgateOp>(op, new_defgate_sym);
            
        auto ctrl_array = mlir::SmallVector<bool>();
        for(auto attr: decorate_op.ctrl().getAsValueRange<mlir::BoolAttr>()){
            ctrl_array.push_back(attr);
        }
        if(!new_defgate){
            if(mlir::failed(createControlledDefgate(defgate, ctrl_array, decorate_op.adjoint(), new_defgate_sym, rewriter, defgate.parameters()))){
                return mlir::failure();
            }
        }
        new_defgate = mlir::SymbolTable::lookupNearestSymbolFrom<DefgateOp>(op, new_defgate_sym);
        auto ctx = getContext();
        auto new_qubit_num = (int)defgate.type().getSize() + ctrl_array.size();
        auto ip = rewriter.saveInsertionPoint();
        rewriter.setInsertionPoint(op);
        auto new_use_gate = rewriter.create<UseGateOp>(op->getLoc(), GateType::get(ctx, new_qubit_num, GateTrait::General), new_defgate_sym, use_op.parameters());
        rewriter.restoreInsertionPoint(ip);
        rewriter.replaceOpWithNewOp<ApplyGateOp>(op.getOperation(), op->getResultTypes(), new_use_gate.result(), op.args());
        *dirty=true;
        return mlir::success();
    }
};

}

// Insert controller bits and insert fake load/store pairs.
struct InsertControllerBits : public mlir::RewritePattern{
    ::mlir::ArrayAttr controls;
    int controlStartIndex;
    mlir::func::FuncOp currentFunc;
    InsertControllerBits(mlir::MLIRContext* ctx, ::mlir::ArrayAttr controls, int control_start_index, mlir::func::FuncOp fn) : mlir::RewritePattern(MatchAnyOpTypeTag(), 1, ctx), currentFunc(fn),controls(controls),controlStartIndex(control_start_index){}
    // First, decorate every apply gate with an additional decorate.
    // Second, insert a fake load-store pair and inject the qubits into apply op. These fake ops will be eliminated by mem2reg pass.
    mlir::LogicalResult matchAndRewrite(mlir::Operation* op, mlir::PatternRewriter& rewriter) const override{
        auto ctx = rewriter.getContext();
        if(!op->hasAttr(ISQ_DECORATE_FOLDING_PROPAGATE_TO_PROCESS)){
            return mlir::failure();
        }
        op->removeAttr(ISQ_DECORATE_FOLDING_PROPAGATE_TO_PROCESS);
        if(!llvm::isa<ApplyGateOp>(op) && !llvm::isa<ApplyGPhase>(op)){
            llvm_unreachable("bad attribute on op.");
        }
        mlir::PatternRewriter::InsertionGuard guard(rewriter);
        
        
        rewriter.setInsertionPoint(op);
        mlir::SmallVector<mlir::Value> ctrl_loaded;
        auto fn = currentFunc;
        for(auto i=0; i<controls.size(); i++){
            auto ctrl_argument = fn.getBody().getArgument(i+controlStartIndex);
            mlir::OperationState state(mlir::UnknownLoc::get(ctx), ISQ_FAKELOAD, mlir::ValueRange{ctrl_argument}, mlir::TypeRange{QStateType::get(ctx)}, mlir::ArrayRef<mlir::NamedAttribute>{mlir::NamedAttribute(mlir::StringAttr::get(ctx, ISQ_FAKELOADSTORE_ID), mlir::IntegerAttr::get(rewriter.getI64Type(), i))});
            auto fake_load = rewriter.create(state);
            ctrl_loaded.push_back(fake_load->getResult(0));
        }
        // Now get ready to recreate the applygate op...
        mlir::SmallVector<mlir::Value> args;
        mlir::SmallVector<mlir::Type> results;
        rewriter.setInsertionPointAfter(op);
        for(auto i=0; i<controls.size(); i++){
            args.push_back(ctrl_loaded[i]);
            results.push_back(QStateType::get(ctx));
        }
        mlir::SmallVector<mlir::Value> out_ctrls;
        if(auto orig_op = llvm::dyn_cast<ApplyGateOp>(op)){
            auto old_gate_type = orig_op.gate().getType().cast<GateType>();
            rewriter.setInsertionPoint(orig_op);
            args.append(orig_op.args().begin(), orig_op.args().end());
            results.append(orig_op->result_type_begin(), orig_op.result_type_end());
            auto new_decorate = rewriter.create<DecorateOp>(mlir::UnknownLoc::get(ctx), GateType::get(ctx, controls.size()+old_gate_type.getSize(), old_gate_type.getHints()), orig_op.gate(), false, controls);
            auto new_op = rewriter.create<ApplyGateOp>(op->getLoc(), results, new_decorate.getResult(), args);
            new_op->setAttrs(op->getAttrs());
            for(auto i=0; i<controls.size(); i++){
                out_ctrls.push_back(new_op.getResult(i));
            }
            // For the rest results, replace original op.
            rewriter.replaceOp(op, new_op->getResults().drop_front(controls.size()));
            rewriter.setInsertionPointAfter(new_op);
        }else if(auto orig_op = llvm::dyn_cast<ApplyGPhase>(op)){
            // Ctrl-GPhase.
            auto old_gate_type = orig_op.gate().getType().cast<GateType>();
            assert(old_gate_type.getSize()==0);
            rewriter.setInsertionPoint(orig_op);
            auto new_decorate = rewriter.create<DecorateOp>(mlir::UnknownLoc::get(ctx), GateType::get(ctx, controls.size()+old_gate_type.getSize(), old_gate_type.getHints()), orig_op.gate(), false, controls);
            auto new_op = rewriter.create<ApplyGateOp>(op->getLoc(), results, new_decorate.getResult(), args);
            new_op->setAttrs(op->getAttrs());
            for(auto i=0; i<controls.size(); i++){
                out_ctrls.push_back(new_op.getResult(i));
            }
            rewriter.eraseOp(op);
            rewriter.setInsertionPointAfter(new_op);
        }else{
            llvm_unreachable("unreachable");
        }

        
        
        // For the first results, fake-store back.
        for(auto i=0; i<controls.size(); i++){
            auto ctrl_argument = fn.getBody().getArgument(i+controlStartIndex);
            mlir::OperationState state(mlir::UnknownLoc::get(ctx), ISQ_FAKESTORE, mlir::ValueRange{out_ctrls[i], ctrl_argument}, mlir::TypeRange{},mlir::ArrayRef<mlir::NamedAttribute>{mlir::NamedAttribute(mlir::StringAttr::get(ctx, ISQ_FAKELOADSTORE_ID), mlir::IntegerAttr::get(rewriter.getI64Type(), i))});
            
            auto fake_store = rewriter.create(state);
        }
        
        return mlir::success();
    }
};


struct FakeMem2RegRewrite : public Mem2RegRewrite{
    bool isLoad(mlir::Operation* op) const {
        return op->getName().getStringRef()==ISQ_FAKELOAD;
    }
    int loadId(mlir::Operation* op) const {
        return op->getAttrOfType<mlir::IntegerAttr>(ISQ_FAKELOADSTORE_ID).getInt();
    }
    bool isStore(mlir::Operation* op) const {
        return op->getName().getStringRef()==ISQ_FAKESTORE;
    }
    int storeId(mlir::Operation* op) const {
        return op->getAttrOfType<mlir::IntegerAttr>(ISQ_FAKELOADSTORE_ID).getInt();
    }
    int storeValue(mlir::Operation* op) const {
        return 0;
    }
};

// Insert controller bits and insert fake load/store pairs.
struct FakeMem2Reg : public mlir::OpRewritePattern<mlir::func::FuncOp>{
    FakeMem2Reg(mlir::MLIRContext* ctx): mlir::OpRewritePattern<mlir::func::FuncOp>(ctx, 1){}

    mlir::LogicalResult matchAndRewrite(mlir::func::FuncOp op, mlir::PatternRewriter& rewriter) const override{
        auto ctx = rewriter.getContext();
        if(!op->hasAttr(ISQ_DECORATE_FOLDING_PROPAGATE_CTRL_BITS)){
            return mlir::failure();
        }
        int controlSize=op->getAttrOfType<mlir::ArrayAttr>(ISQ_DECORATE_FOLDING_PROPAGATE_CTRL_BITS).size();
        int controlStartIndex=op->getAttrOfType<mlir::IntegerAttr>(ISQ_DECORATE_FOLDING_PROPAGATE_CTRL_START_INDEX).getInt();
        int originalGateSize = op.getNumArguments() - controlStartIndex - controlSize;
        mlir::SmallVector<mlir::Value> args;
        mlir::SmallVector<mlir::Type> argTypes;
        for(auto i=0; i<controlSize; i++){
            args.push_back(op.getBody().getArgument(i+controlStartIndex));
            argTypes.push_back(QStateType::get(ctx));
        }
        rewriter.startRootUpdate(op);
        auto fn_type = op->getAttrOfType<mlir::TypeAttr>(ISQ_REPLACED_SIG);
        assert(fn_type);
        auto val = fn_type.getValue();
        if(!val.isa<mlir::FunctionType>()){
            assert(0);
        }
        op.setType(fn_type.getValue());
        op->removeAttr(ISQ_REPLACED_SIG);
        op->removeAttr(ISQ_DECORATE_FOLDING_PROPAGATE_CTRL_BITS);
        op->removeAttr(ISQ_DECORATE_FOLDING_PROPAGATE_CTRL_START_INDEX);
        FakeMem2RegRewrite mem2reg;
        for(auto& block: op.getBody()){
            if(block.isEntryBlock()){
                mem2reg.mem2regKeepBlockParam(&block, rewriter, args);
            }else{
                mem2reg.mem2regAlterBlockParam(argTypes, &block, rewriter);
            }
            if(auto last = llvm::dyn_cast<mlir::func::ReturnOp>(block.getTerminator())){
                // twist back.
                mlir::SmallVector<mlir::Value> twistedReturnOrder;
                for(auto i=0; i<controlSize; i++){
                    twistedReturnOrder.push_back(last.getOperand(originalGateSize+i));
                }
                for(auto i=0; i<originalGateSize; i++){
                    twistedReturnOrder.push_back(last.getOperand(i));
                }
                last.operandsMutable().assign(twistedReturnOrder);
            }
        }
        rewriter.finalizeRootUpdate(op);
        return mlir::success();
    }
};


struct GenerateInvertedGate : public mlir::PassWrapper<GenerateInvertedGate, mlir::OperationPass<mlir::func::FuncOp>>{
    void runOnOperation() override{
        mlir::func::FuncOp op = this->getOperation();
        auto ctx = op->getContext();
        auto size_attr = op->getAttrOfType<mlir::IntegerAttr>(ISQ_ATTR_GATE_SIZE);
        if(!size_attr) return;
        op->removeAttr(ISQ_ATTR_GATE_SIZE);
        auto size = size_attr.getInt();
        auto offset = op.getNumArguments()-size;
        // last qubits
        // backtrace all qubits.
        llvm::SmallVector<mlir::Value> results;
        // find terminator.
        mlir::Block* last_block = nullptr;
        for(auto& block : op.getBody().getBlocks()){
            if(llvm::isa<mlir::func::ReturnOp>(block.getTerminator())){
                if(last_block){
                    op->emitOpError("has multiple exit blocks");
                    return signalPassFailure();
                }
                last_block=&block;
            }
        }
        if(!last_block){
            op.emitOpError("has no exit block");
            return signalPassFailure();
        }
        auto ret = llvm::cast<mlir::func::ReturnOp>(last_block->getTerminator());
        mlir::OpBuilder builder(ret);
        results.append(ret.operands().begin(), ret.operands().end());
        for(auto i=0; i<results.size(); i++){
            auto val = results[i];
            while(val.getDefiningOp()){
                auto def_op = val.getDefiningOp();
                auto apply_op = llvm::dyn_cast<ApplyGateOp>(def_op);
                if(!apply_op) {
                    def_op->emitOpError("wrongly used in adjointed gate");
                    return signalPassFailure();
                }
                for(auto arg_index =0; arg_index < apply_op.args().size(); arg_index++){
                    if(apply_op.getResult(arg_index) == val){
                        val = apply_op.args()[arg_index];
                        break;
                    }
                }
                
            }
            if(val!=op.getArgument(offset+i)){
                op->emitError(mlir::StringRef("use-def chain of argument ") + std::to_string(i) + " cannot be traced.");
                return signalPassFailure();
            }
        }
        // start with noop.
        for(auto i=0; i<results.size(); i++){
            ret->setOperand(i, op.getArgument(offset+i));
        }
        // start reverting.
        bool flag = true;
        while(flag){
            flag=false;
            for(auto i=0; i<results.size(); i++){
                auto val = results[i];
                if(val==op.getArgument(offset+i)) continue;
                flag=true;
                auto apply_op = llvm::cast<ApplyGateOp>(val.getDefiningOp());
                mlir::SmallVector<int> indices;
                auto found = false;
                // first check of all apply_op results are in the array.
                // i.e. check if the gate is a ``last gate''.
                for(auto k=0; k<apply_op.getNumResults(); k++){
                    found=false;
                    for(auto j=0; j<results.size(); j++){
                        if(apply_op.getResult(k) == results[j]){
                            indices.push_back(j);
                            found=true; break;
                        }
                    }
                    if(!found){
                        break;
                    }
                }
                if(!found) continue;
                // take apply_op down.
                auto new_gate = builder.create<DecorateOp>(apply_op->getLoc(), apply_op.gate().getType(), apply_op.gate(), true, mlir::ArrayAttr::get(ctx, llvm::ArrayRef<mlir::Attribute>{}));
                auto new_op = builder.clone(*apply_op);
                for(auto i=0; i<indices.size(); i++){
                    new_op->setOperand(i+1 /* arg 0 is gate */, ret.getOperand(indices[i]));
                    ret.setOperand(indices[i], new_op->getResult(i));
                    results[indices[i]] = apply_op.args()[i];
                }
                new_op->setOperand(0, new_gate);
                apply_op.erase();
            }
        }
        // revert gphase.
        mlir::SmallVector<ApplyGPhase> gphase_ops;
        op.walk([&](ApplyGPhase apply_op){
            gphase_ops.push_back(apply_op);
        });
        for(auto apply_op : gphase_ops){
            builder.setInsertionPoint(apply_op);
            auto new_gate = builder.create<DecorateOp>(apply_op->getLoc(), apply_op.gate().getType(), apply_op.gate(), true, mlir::ArrayAttr::get(ctx, llvm::ArrayRef<mlir::Attribute>{}));
            apply_op.gateMutable().assign(new_gate);
        }
    }
};


struct DecorateFoldingPass : public mlir::PassWrapper<DecorateFoldingPass, mlir::OperationPass<mlir::ModuleOp>>{
    DecorateFoldingPass() = default;
    DecorateFoldingPass(const DecorateFoldingPass& pass) {}
    void runOnOperation() override {
        mlir::ModuleOp m = this->getOperation();
        auto ctx = m->getContext();
        bool dirty = true;
        auto sq_adj = this->ignore_sq_adj.getValue();
        while(dirty){
            dirty = false;
            do{
                mlir::RewritePatternSet rps(ctx);
                rps.add<DecorateFoldRewriteRule>(ctx, m, &dirty, sq_adj);
                mlir::FrozenRewritePatternSet frps(std::move(rps));
                (void)mlir::applyPatternsAndFoldGreedily(m.getOperation(), frps);
            }while(0);
            do{
                mlir::PassManager pm(ctx);
                pm.addNestedPass<mlir::func::FuncOp>(std::make_unique<GenerateInvertedGate>());
                if(failed(pm.run(m))){
                    return signalPassFailure();
                }
            }while(0);

            do{
                m->walk([=](mlir::func::FuncOp fn){
                    if(!fn->hasAttr(ISQ_DECORATE_FOLDING_PROPAGATE_CTRL_BITS)){
                        return;
                    }
                    auto controls=fn->getAttrOfType<mlir::ArrayAttr>(ISQ_DECORATE_FOLDING_PROPAGATE_CTRL_BITS);
                    int controlStartIndex=fn->getAttrOfType<mlir::IntegerAttr>(ISQ_DECORATE_FOLDING_PROPAGATE_CTRL_START_INDEX).getInt();
                    mlir::RewritePatternSet rps(ctx);
                    rps.add<InsertControllerBits>(ctx, controls, controlStartIndex, fn);
                    passes::addLegalizeTraitsRules(rps);
                    mlir::FrozenRewritePatternSet frps(std::move(rps));
                    (void)mlir::applyPatternsAndFoldGreedily(fn, frps);
                });
            }while(0);
            do{
                mlir::RewritePatternSet rps(ctx);
                rps.add<FakeMem2Reg>(ctx);
                mlir::FrozenRewritePatternSet frps(std::move(rps));
                (void)mlir::applyPatternsAndFoldGreedily(m.getOperation(), frps);
            }while(0);
        }
        
    }
    Option<bool> ignore_sq_adj{*this, "preserve-sq-adj", llvm::cl::desc("Preserve single-qubit adjoint gates. Useful for preserving optimization chances."), llvm::cl::init(false)};
    mlir::StringRef getArgument() const final {
        return "isq-fold-decorated-gates";
    }
    mlir::StringRef getDescription() const final {
        return  "Folding for known/decomposed decorated gates.";
    }
};

void registerDecorateFolding(){
    mlir::PassRegistration<DecorateFoldingPass>();
}

}
}
}