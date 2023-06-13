#include "isq/Enums.h"
#include "isq/GateDefTypes.h"
#include "isq/Operations.h"
#include "isq/QStructs.h"
#include "isq/QTypes.h"
#include "isq/passes/Passes.h"
#include <cctype>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/Casting.h>
#include <mlir/Dialect/StandardOps/IR/Ops.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/Location.h>
#include <mlir/IR/PatternMatch.h>
#include <mlir/IR/SymbolTable.h>
#include <mlir/Pass/Pass.h>
#include <mlir/Pass/PassRegistry.h>
#include <mlir/Rewrite/FrozenRewritePatternSet.h>
#include <mlir/Transforms/GreedyPatternRewriteDriver.h>
namespace isq{
namespace ir{
namespace passes{
const char* ISQ_FAMOUS = "isq_famous";
const char* BUILTIN_TOFFOLI_DECOMPOSITION = "$__isq__builtin__toffoli__decomposition__famous";
struct FamousGateDef{
    const char* qir_name;
    const char* famous_name;
    int gate_size;
    int param_size;
    std::optional<std::vector<std::vector<std::complex<double>>>> mat_def;
    FamousGateDef(const char* qir_name, const char* famous_name, std::vector<std::vector<std::complex<double>>> mat_def): qir_name(qir_name), famous_name(famous_name), mat_def(mat_def){
        param_size=0;
        gate_size = (int)std::log2(mat_def.size());
    }
    FamousGateDef(const char* qir_name, const char* famous_name, int param_size, int gate_size): qir_name(qir_name), famous_name(famous_name), param_size(param_size), gate_size(gate_size){

    }
};

struct RewritePreferFamousGate : public mlir::OpRewritePattern<UseGateOp>{
    mlir::ModuleOp rootModule;
    const std::vector<FamousGateDef>& famousGates;
    RewritePreferFamousGate(mlir::ModuleOp rootModule, const std::vector<FamousGateDef>& famousGates): mlir::OpRewritePattern<UseGateOp>(rootModule->getContext(), 1), rootModule(rootModule), famousGates(famousGates){
    }
    mlir::LogicalResult matchAndRewrite(UseGateOp use, mlir::PatternRewriter& rewriter) const override{
        auto defgate = llvm::dyn_cast_or_null<DefgateOp>(mlir::SymbolTable::lookupNearestSymbolFrom(use, use.name()));
        if(!defgate) return mlir::failure();
        for(auto attr: *defgate.definition()){
            auto def = attr.cast<GateDefinition>();
            if(def.type().strref()=="qir"){
                auto flat_symbol = def.value().cast<mlir::FlatSymbolRefAttr>();
                for(auto& famousGate: famousGates){
                    if(flat_symbol.getValue() == famousGate.qir_name){
                        if(famousGate.famous_name != defgate.sym_name()){
                            rewriter.updateRootInPlace(use, [&](){
                                use.nameAttr(mlir::FlatSymbolRefAttr::get(rewriter.getStringAttr(getFamousName(famousGate.famous_name))));
                            });
                            return mlir::success();
                        }else{
                            return mlir::failure();
                        }
                        
                    }
                }
            }
        }

        return mlir::failure();
    }
};

struct RecognizeFamousGatePass : public mlir::PassWrapper<RecognizeFamousGatePass, mlir::OperationPass<mlir::ModuleOp>>{
    std::vector<FamousGateDef> famousGates;
    RecognizeFamousGatePass(){
        famousGates.push_back(FamousGateDef("__quantum__qis__cnot", "cnot", {
            {std::complex<double>(1,0.),std::complex<double>(0.,0.),std::complex<double>(0.,0.),std::complex<double>(0.,0.)},
            {std::complex<double>(0.,0.),std::complex<double>(1.,0.),std::complex<double>(0.,0.),std::complex<double>(0.,0.)},
            {std::complex<double>(0.,0.),std::complex<double>(0.,0.),std::complex<double>(0.,0.),std::complex<double>(1.,0.)},
            {std::complex<double>(0.,0.),std::complex<double>(0.,0.),std::complex<double>(1.,0.),std::complex<double>(0.,0.)}
        }));
        famousGates.push_back(FamousGateDef("__quantum__qis__toffoli", "toffoli", {
            {std::complex<double>(1 ,0.),std::complex<double>(0.,0.),std::complex<double>(0.,0.),std::complex<double>(0.,0.),
            std::complex<double>(0.,0.),std::complex<double>(0.,0.),std::complex<double>(0.,0.),std::complex<double>(0.,0.)},
            {std::complex<double>(0.,0.),std::complex<double>(1.,0.),std::complex<double>(0.,0.),std::complex<double>(0.,0.),            std::complex<double>(0.,0.),std::complex<double>(0.,0.),
            std::complex<double>(0.,0.),std::complex<double>(0.,0.)},
            {std::complex<double>(0 ,0.),std::complex<double>(0.,0.),std::complex<double>(1.,0.),std::complex<double>(0.,0.),
            std::complex<double>(0.,0.),std::complex<double>(0.,0.),std::complex<double>(0.,0.),std::complex<double>(0.,0.)},
            {std::complex<double>(0.,0.),std::complex<double>(0.,0.),std::complex<double>(0.,0.),std::complex<double>(1.,0.),            std::complex<double>(0.,0.),std::complex<double>(0.,0.),
            std::complex<double>(0.,0.),std::complex<double>(0.,0.)},
            {std::complex<double>(0 ,0.),std::complex<double>(0.,0.),std::complex<double>(0.,0.),std::complex<double>(0.,0.),
            std::complex<double>(1.,0.),std::complex<double>(0.,0.),std::complex<double>(0.,0.),std::complex<double>(0.,0.)},
            {std::complex<double>(0.,0.),std::complex<double>(0.,0.),std::complex<double>(0.,0.),std::complex<double>(0.,0.),            std::complex<double>(0.,0.),std::complex<double>(1.,0.),
            std::complex<double>(0.,0.),std::complex<double>(0.,0.)},
            {std::complex<double>(0 ,0.),std::complex<double>(0.,0.),std::complex<double>(0.,0.),std::complex<double>(0.,0.),
            std::complex<double>(0.,0.),std::complex<double>(0.,0.),std::complex<double>(0.,0.),std::complex<double>(1.,0.)},
            {std::complex<double>(0.,0.),std::complex<double>(0.,0.),std::complex<double>(0.,0.),std::complex<double>(0.,0.),            std::complex<double>(0.,0.),std::complex<double>(0.,0.),
            std::complex<double>(1.,0.),std::complex<double>(0.,0.)}
        }));
        famousGates.push_back(FamousGateDef("__quantum__qis__h__body", "h", {
            {std::complex<double>(std::sqrt(0.5), 0.),std::complex<double>(std::sqrt(0.5), 0.)},
            {std::complex<double>(std::sqrt(0.5), 0.),std::complex<double>(-std::sqrt(0.5), 0.)}
        }));
        famousGates.push_back(FamousGateDef("__quantum__qis__s__body", "s", {
            {std::complex<double>(1., 0.),std::complex<double>(0., 0.)},
            {std::complex<double>(0., 0.),std::complex<double>(0., 1.)}
        }));
        famousGates.push_back(FamousGateDef("__quantum__qis__t__body", "t", {
            {std::complex<double>(1., 0.),std::complex<double>(0., 0.)},
            {std::complex<double>(0., 0.),std::complex<double>(std::sqrt(0.5), std::sqrt(0.5))}
        }));
        famousGates.push_back(FamousGateDef("__quantum__qis__x__body", "x", {
            {std::complex<double>(0., 0.),std::complex<double>(1., 0.)},
            {std::complex<double>(1., 0.),std::complex<double>(0., 0.)}
        }));
        famousGates.push_back(FamousGateDef("__quantum__qis__y__body", "y", {
            {std::complex<double>(0., 0.),std::complex<double>(0., -1.)},
            {std::complex<double>(0., 1.),std::complex<double>(0., 0.)}
        }));
        famousGates.push_back(FamousGateDef("__quantum__qis__z__body", "z", {
            {std::complex<double>(1., 0.),std::complex<double>(0., 0.)},
            {std::complex<double>(0., 0.),std::complex<double>(-1., 0.)}
        }));
        famousGates.push_back(FamousGateDef("__quantum__qis__rx__body", "rx", 1, 1));
        famousGates.push_back(FamousGateDef("__quantum__qis__ry__body", "ry", 1, 1));
        famousGates.push_back(FamousGateDef("__quantum__qis__rz__body", "rz", 1, 1));
        famousGates.push_back(FamousGateDef("__quantum__qis__u3", "u3", 3, 1));
    }

    void emitToffoliConstruction(mlir::OpBuilder builder){
        auto ctx = builder.getContext();
        mlir::OpBuilder::InsertionGuard guard(builder);
        mlir::SmallVector<mlir::Type> toffoliArgType;
        toffoliArgType.append(3, QStateType::get(ctx));
        auto funcType = mlir::FunctionType::get(ctx, toffoliArgType, toffoliArgType);
        auto funcop = builder.create<mlir::FuncOp>(mlir::NameLoc::get(builder.getStringAttr("<builtin>")), BUILTIN_TOFFOLI_DECOMPOSITION, funcType, builder.getStringAttr("private"));
        auto body = funcop.addEntryBlock();
        builder.setInsertionPointToStart(body);
        mlir::SmallVector<mlir::Value> qubits;
        for(auto arg: body->getArguments()){ qubits.push_back(arg);}
        emitBuiltinGate(builder, "H", {&qubits[2]});
        emitBuiltinGate(builder, "CNOT", {&qubits[1], &qubits[2]});
        emitBuiltinGate(builder, "T", {&qubits[2]}, {}, nullptr, true);
        emitBuiltinGate(builder, "CNOT", {&qubits[0], &qubits[2]});
        emitBuiltinGate(builder, "T", {&qubits[2]});
        emitBuiltinGate(builder, "CNOT", {&qubits[1], &qubits[2]});
        emitBuiltinGate(builder, "T", {&qubits[2]}, {}, nullptr, true);
        emitBuiltinGate(builder, "CNOT", {&qubits[0], &qubits[2]});
        emitBuiltinGate(builder, "T", {&qubits[2]});
        emitBuiltinGate(builder, "T", {&qubits[1]});
        emitBuiltinGate(builder, "H", {&qubits[2]});
        emitBuiltinGate(builder, "CNOT", {&qubits[0], &qubits[1]});
        emitBuiltinGate(builder, "T", {&qubits[1]}, {}, nullptr, true);
        emitBuiltinGate(builder, "CNOT", {&qubits[0], &qubits[1]});
        emitBuiltinGate(builder, "T", {&qubits[0]});
        emitBuiltinGate(builder, "S", {&qubits[1]});
        builder.create<mlir::ReturnOp>(mlir::NameLoc::get(builder.getStringAttr("<builtin>")), qubits);
    }
    void emitFamousGate(const FamousGateDef& famousGate){
        auto moduleOp = getOperation();
        auto block = moduleOp.getBody();
        auto ctx = moduleOp->getContext();
        mlir::OpBuilder builder(moduleOp->getContext());
        builder.setInsertionPointToEnd(block);
        auto gateType = GateType::get(ctx, famousGate.gate_size, GateTrait::General);
        auto name = getFamousName(famousGate.famous_name);
        mlir::SmallVector<mlir::Attribute> definitions;
        definitions.push_back(GateDefinition::get(builder.getStringAttr("qir"), mlir::FlatSymbolRefAttr::get(builder.getStringAttr(famousGate.qir_name)),ctx));
        if(famousGate.mat_def){
            auto gate = createMatrixDef(ctx, *famousGate.mat_def);
            definitions.push_back(gate);
        }
        // toffoli special handling
        if(famousGate.famous_name == mlir::StringRef("toffoli")){
            definitions.push_back(GateDefinition::get(builder.getStringAttr("decomposition"), mlir::FlatSymbolRefAttr::get(builder.getStringAttr(BUILTIN_TOFFOLI_DECOMPOSITION)),ctx));
        }
        auto attrDefs = mlir::ArrayAttr::get(ctx, definitions);

        mlir::SmallVector<mlir::Type> paramTypes;
        mlir::SmallVector<mlir::Attribute> paramTypeAttrs;
        paramTypes.append(famousGate.param_size, builder.getF64Type());
        paramTypeAttrs.append(famousGate.param_size, mlir::TypeAttr::get(builder.getF64Type()));
        //DefgateOp::build(builder, state, gate, name, nested,{}, defs, params);
        auto defgate = builder.create<DefgateOp>(mlir::NameLoc::get(builder.getStringAttr("<builtin>")), mlir::TypeAttr::get(gateType), builder.getStringAttr(name), builder.getStringAttr("nested"), mlir::ArrayAttr(), attrDefs, mlir::ArrayAttr::get(ctx, paramTypeAttrs));
        defgate->setAttr(ISQ_FAMOUS, builder.getStringAttr(famousGate.famous_name));
        if(!mlir::SymbolTable::lookupSymbolIn(moduleOp, famousGate.qir_name)){
            // add the qir operation as well.
            paramTypes.append(famousGate.gate_size, QIRQubitType::get(ctx));
            auto funcType = mlir::FunctionType::get(ctx, paramTypes, (mlir::TypeRange){});
            builder.create<mlir::FuncOp>(mlir::NameLoc::get(builder.getStringAttr("<builtin>")), famousGate.qir_name, funcType, builder.getStringAttr("private"));
        }
        // toffoli: special handling
        if(famousGate.famous_name == mlir::StringRef("toffoli")){
            if(!mlir::SymbolTable::lookupSymbolIn(moduleOp, BUILTIN_TOFFOLI_DECOMPOSITION)){
                emitToffoliConstruction(builder);
            }
        }
    }
    void runOnOperation() override{
        auto moduleOp = getOperation();
        // First, emit all famous gates.
        for(auto& famousOp : this->famousGates){
            emitFamousGate(famousOp);
        }
        // Secondly, replace all references to famous gates.
        mlir::RewritePatternSet rps(moduleOp.getContext());
        rps.add<RewritePreferFamousGate>(moduleOp, famousGates);
        mlir::FrozenRewritePatternSet frps(std::move(rps));
        (void)mlir::applyPatternsAndFoldGreedily(moduleOp, frps);
    }
    mlir::StringRef getArgument() const final{
        return "isq-recognize-famous-gates";
    }
    mlir::StringRef getDescription() const final{
        return "Recognize famous quantum gates from their QIR description.";
    }
};

void registerRecognizeFamousGates(){
    mlir::PassRegistration<RecognizeFamousGatePass>();
}

// Example: CNOT -> __isq__builtin__cnot
llvm::SmallString<32> getFamousName(const char* famous_gate){
    llvm::SmallString<32> name("$__isq__builtin__");
    while(char ch = *(famous_gate++)){
        name+=llvm::toLower(ch);
    }
    return name;
}

void emitBuiltinGate(mlir::OpBuilder& builder, const char* famous_gate, mlir::ArrayRef<mlir::Value*> qubits, mlir::ArrayRef<mlir::Value> params, mlir::ArrayAttr ctrl, bool adjoint){
    auto ctx = builder.getContext();
    // use the gate.
    auto gate_size = qubits.size();
    if(!ctrl){
        ctrl = mlir::ArrayAttr::get(ctx, mlir::ArrayRef<mlir::Attribute>{});
    }
    gate_size-= ctrl.size();
    auto gate_type = GateType::get(ctx, gate_size, GateTrait::General);
    auto use_gate = builder.create<UseGateOp>(mlir::UnknownLoc::get(ctx), gate_type, mlir::FlatSymbolRefAttr::get(ctx, getFamousName(famous_gate)), params);
    auto used_gate = use_gate.result();
    if((ctrl && ctrl.size()>0) || adjoint){
        auto decorate_op = builder.create<DecorateOp>(mlir::UnknownLoc::get(ctx), GateType::get(ctx, qubits.size(), GateTrait::General), used_gate, adjoint, ctrl);
        used_gate = decorate_op.getResult();
    }
    gate_size = qubits.size();
    mlir::SmallVector<mlir::Type> qubitTypes;
    for(auto i=0; i<gate_size; i++) qubitTypes.push_back(QStateType::get(ctx));
    mlir::SmallVector<mlir::Value> qubitValues;
    for(auto i=0; i<gate_size; i++){
        qubitValues.push_back(*qubits[i]);
    }
    auto apply_op = builder.create<ApplyGateOp>(mlir::UnknownLoc::get(ctx), qubitTypes, used_gate, qubitValues);
    for(auto i=0; i<gate_size; i++){
        *qubits[i] = apply_op->getResult(i);
    }
}

bool isFamousGate(DefgateOp op, const char* famous_gate){
    if(!op->hasAttr(ISQ_FAMOUS)) return false;
    llvm::SmallString<32> gate(famous_gate);
    for(auto& c: gate){
        c=llvm::toLower(c);
    }
    return op->getAttrOfType<mlir::StringAttr>(ISQ_FAMOUS).strref()==gate;
}

}

}
}
