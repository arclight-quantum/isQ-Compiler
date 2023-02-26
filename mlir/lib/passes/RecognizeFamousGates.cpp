#include "isq/Enums.h"
#include "isq/GateDefTypes.h"
#include "isq/Operations.h"
#include "isq/QTypes.h"
#include "isq/passes/Passes.h"
#include <cctype>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/raw_ostream.h>
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
    GateTrait gate_trait;
    std::optional<std::vector<std::vector<std::complex<double>>>> mat_def;
    FamousGateDef(const char* qir_name, const char* famous_name, std::vector<std::vector<std::complex<double>>> mat_def, GateTrait gate_trait): qir_name(qir_name), famous_name(famous_name), mat_def(mat_def), gate_trait(gate_trait){
        param_size=0;
        gate_size = (int)std::log2(mat_def.size());
    }
    FamousGateDef(const char* qir_name, const char* famous_name, int param_size, int gate_size, GateTrait gate_trait): qir_name(qir_name), famous_name(famous_name), param_size(param_size), gate_size(gate_size), gate_trait(gate_trait){

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
        auto defs = *defgate.definition();
        if(!defs) return mlir::failure();
        for(auto attr: defs.getValue()){
            auto def = attr.cast<GateDefinition>();
            if(def.getType().strref()=="qir"){
                auto flat_symbol = def.getValue().cast<mlir::FlatSymbolRefAttr>();
                for(auto& famousGate: famousGates){
                    if(flat_symbol.getValue() == famousGate.qir_name){
                        if(getFamousName(famousGate.famous_name) != defgate.sym_name()){
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

std::vector<FamousGateDef> FAMOUS_GATES;

struct RecognizeFamousGatePass : public mlir::PassWrapper<RecognizeFamousGatePass, mlir::OperationPass<mlir::ModuleOp>>{
    std::vector<FamousGateDef>& famousGates;
    RecognizeFamousGatePass(): famousGates(FAMOUS_GATES){
        if(famousGates.size()>0) return;
        famousGates.push_back(FamousGateDef("__quantum__qis__cnot", "cnot", {
            {std::complex<double>(1,0.),std::complex<double>(0.,0.),std::complex<double>(0.,0.),std::complex<double>(0.,0.)},
            {std::complex<double>(0.,0.),std::complex<double>(1.,0.),std::complex<double>(0.,0.),std::complex<double>(0.,0.)},
            {std::complex<double>(0.,0.),std::complex<double>(0.,0.),std::complex<double>(0.,0.),std::complex<double>(1.,0.)},
            {std::complex<double>(0.,0.),std::complex<double>(0.,0.),std::complex<double>(1.,0.),std::complex<double>(0.,0.)}
        }, GateTrait::Hermitian));
        famousGates.push_back(FamousGateDef("__quantum__qis__cz", "cz", {
            {std::complex<double>(1,0.),std::complex<double>(0.,0.),std::complex<double>(0.,0.),std::complex<double>(0.,0.)},
            {std::complex<double>(0.,0.),std::complex<double>(1.,0.),std::complex<double>(0.,0.),std::complex<double>(0.,0.)},
            {std::complex<double>(0.,0.),std::complex<double>(0.,0.),std::complex<double>(1.,0.),std::complex<double>(0.,0.)},
            {std::complex<double>(0.,0.),std::complex<double>(0.,0.),std::complex<double>(0.,0.),std::complex<double>(-1.,0.)}
        }, GateTrait::Hermitian|GateTrait::Symmetric|GateTrait::Diagonal|GateTrait::Phase));
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
        },GateTrait::Hermitian));
        famousGates.push_back(FamousGateDef("__quantum__qis__h__body", "h", {
            {std::complex<double>(std::sqrt(0.5), 0.),std::complex<double>(std::sqrt(0.5), 0.)},
            {std::complex<double>(std::sqrt(0.5), 0.),std::complex<double>(-std::sqrt(0.5), 0.)}
        },GateTrait::Hermitian|GateTrait::Symmetric));
        famousGates.push_back(FamousGateDef("__quantum__qis__s__body", "s", {
            {std::complex<double>(1., 0.),std::complex<double>(0., 0.)},
            {std::complex<double>(0., 0.),std::complex<double>(0., 1.)}
        },GateTrait::Symmetric|GateTrait::Phase|GateTrait::Diagonal));
        famousGates.push_back(FamousGateDef("__quantum__qis__t__body", "t", {
            {std::complex<double>(1., 0.),std::complex<double>(0., 0.)},
            {std::complex<double>(0., 0.),std::complex<double>(std::sqrt(0.5), std::sqrt(0.5))}
        },GateTrait::Symmetric|GateTrait::Phase|GateTrait::Diagonal));
        famousGates.push_back(FamousGateDef("__quantum__qis__s__adj", "sinv", {
            {std::complex<double>(1., 0.),std::complex<double>(0., 0.)},
            {std::complex<double>(0., 0.),std::complex<double>(0., -1.)}
        },GateTrait::Symmetric|GateTrait::Phase|GateTrait::Diagonal));
        famousGates.push_back(FamousGateDef("__quantum__qis__t__adj", "tinv", {
            {std::complex<double>(1., 0.),std::complex<double>(0., 0.)},
            {std::complex<double>(0., 0.),std::complex<double>(std::sqrt(0.5), -std::sqrt(0.5))}
        },GateTrait::Symmetric|GateTrait::Phase|GateTrait::Diagonal));
        famousGates.push_back(FamousGateDef("__quantum__qis__x__body", "x", {
            {std::complex<double>(0., 0.),std::complex<double>(1., 0.)},
            {std::complex<double>(1., 0.),std::complex<double>(0., 0.)}
        },GateTrait::Hermitian|GateTrait::Antidiagonal|GateTrait::Symmetric));
        famousGates.push_back(FamousGateDef("__quantum__qis__y__body", "y", {
            {std::complex<double>(0., 0.),std::complex<double>(0., -1.)},
            {std::complex<double>(0., 1.),std::complex<double>(0., 0.)}
        },GateTrait::Hermitian|GateTrait::Antidiagonal|GateTrait::Symmetric));
        famousGates.push_back(FamousGateDef("__quantum__qis__z__body", "z", {
            {std::complex<double>(1., 0.),std::complex<double>(0., 0.)},
            {std::complex<double>(0., 0.),std::complex<double>(-1., 0.)}
        },GateTrait::Hermitian|GateTrait::Phase|GateTrait::Diagonal|GateTrait::Symmetric));
        famousGates.push_back(FamousGateDef("__quantum__qis__x2p", "x2p", {
            {std::complex<double>(std::sqrt(0.5), 0.),std::complex<double>(0., -std::sqrt(0.5))},
            {std::complex<double>(0., -std::sqrt(0.5)),std::complex<double>(std::sqrt(0.5), 0.)}
        },GateTrait::Symmetric));
        famousGates.push_back(FamousGateDef("__quantum__qis__x2m", "x2m", {
            {std::complex<double>(std::sqrt(0.5), 0.),std::complex<double>(0., std::sqrt(0.5))},
            {std::complex<double>(0., std::sqrt(0.5)),std::complex<double>(std::sqrt(0.5), 0.)}
        },GateTrait::Symmetric));
        famousGates.push_back(FamousGateDef("__quantum__qis__y2p", "y2p", {
            {std::complex<double>(std::sqrt(0.5), 0.),std::complex<double>(-std::sqrt(0.5), 0.)},
            {std::complex<double>(std::sqrt(0.5), 0.),std::complex<double>(std::sqrt(0.5), 0.)}
        },GateTrait::Symmetric));
        famousGates.push_back(FamousGateDef("__quantum__qis__y2m", "y2m", {
            {std::complex<double>(std::sqrt(0.5), 0.),std::complex<double>(std::sqrt(0.5), 0.)},
            {std::complex<double>(-std::sqrt(0.5), 0.),std::complex<double>(std::sqrt(0.5), 0.)}
        },GateTrait::Symmetric));
        famousGates.push_back(FamousGateDef("__quantum__qis__rx__body", "rx", 1, 1, GateTrait::Symmetric));
        famousGates.push_back(FamousGateDef("__quantum__qis__ry__body", "ry", 1, 1, GateTrait::Symmetric));
        famousGates.push_back(FamousGateDef("__quantum__qis__rz__body", "rz", 1, 1, GateTrait::Diagonal | GateTrait::Symmetric));
        famousGates.push_back(FamousGateDef("__quantum__qis__gphase", "gphase", 1, 0, GateTrait::Diagonal | GateTrait::Symmetric | GateTrait::Phase ));
        famousGates.push_back(FamousGateDef("__quantum__qis__u3", "u3", 3, 1, GateTrait::Symmetric));
    }

    void emitToffoliConstruction(mlir::OpBuilder builder){
        auto ctx = builder.getContext();
        mlir::OpBuilder::InsertionGuard guard(builder);
        mlir::SmallVector<mlir::Type> toffoliArgType;
        toffoliArgType.append(3, QStateType::get(ctx));
        auto funcType = mlir::FunctionType::get(ctx, toffoliArgType, toffoliArgType);
        auto funcop = builder.create<mlir::func::FuncOp>(mlir::NameLoc::get(builder.getStringAttr("<builtin>")), BUILTIN_TOFFOLI_DECOMPOSITION, funcType, builder.getStringAttr("private"));
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
        emitBuiltinGate(builder, "T", {&qubits[1]},{},nullptr, true);
        emitBuiltinGate(builder, "H", {&qubits[2]});
        emitBuiltinGate(builder, "CNOT", {&qubits[0], &qubits[1]});
        emitBuiltinGate(builder, "T", {&qubits[1]}, {}, nullptr, true);
        emitBuiltinGate(builder, "CNOT", {&qubits[0], &qubits[1]});
        emitBuiltinGate(builder, "T", {&qubits[0]});
        emitBuiltinGate(builder, "S", {&qubits[1]});
        builder.create<mlir::func::ReturnOp>(mlir::NameLoc::get(builder.getStringAttr("<builtin>")), qubits);
    }
    void emitFamousGate(const FamousGateDef& famousGate){
        auto moduleOp = getOperation();
        auto block = moduleOp.getBody();
        auto ctx = moduleOp->getContext();
        mlir::OpBuilder builder(moduleOp->getContext());
        builder.setInsertionPointToEnd(block);
        auto gateType = GateType::get(ctx, famousGate.gate_size, famousGate.gate_trait);
        auto name = getFamousName(famousGate.famous_name);
        mlir::SmallVector<mlir::Attribute> definitions;
        definitions.push_back(GateDefinition::get(ctx, builder.getStringAttr("qir"), mlir::FlatSymbolRefAttr::get(builder.getStringAttr(famousGate.qir_name))));
        if(famousGate.mat_def){
            auto gate = createMatrixDef(ctx, *famousGate.mat_def);
            definitions.push_back(gate);
        }
        // toffoli special handling
        if(famousGate.famous_name == mlir::StringRef("toffoli")){
            definitions.push_back(GateDefinition::get(ctx, builder.getStringAttr("decomposition"), mlir::FlatSymbolRefAttr::get(builder.getStringAttr(BUILTIN_TOFFOLI_DECOMPOSITION))));
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
            builder.create<mlir::func::FuncOp>(mlir::NameLoc::get(builder.getStringAttr("<builtin>")), famousGate.qir_name, funcType, builder.getStringAttr("private"));
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
        addLegalizeTraitsRules(rps);
        mlir::FrozenRewritePatternSet frps(std::move(rps));
        mlir::GreedyRewriteConfig  config;
        //config.maxIterations=mlir::GreedyRewriteConfig::kNoIterationLimit;
        (void)mlir::applyPatternsAndFoldGreedily(moduleOp, frps, config);

        
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

FamousGateDef* findFamousGate(const char* famous_gate){
    llvm::SmallString<32> name1;
    while(char ch = *(famous_gate++)){
        name1+=llvm::toLower(ch);
    }
    for(auto i=0; i<FAMOUS_GATES.size(); i++){
        auto def = &FAMOUS_GATES[i];
        llvm::SmallString<32> name2;

        const char* known_gate = def->famous_name;
        while(char ch = *(known_gate++)){
            name2+=llvm::toLower(ch);
        }
        if(name1==name2){
            return def;
        }
        
    }
    llvm_unreachable("famous gate not found");
    assert(0);
}


mlir::Value emitUseBuiltinGate(mlir::OpBuilder& builder, int original_size, const char* famous_gate, mlir::ArrayRef<mlir::Value> params, mlir::ArrayAttr ctrl, bool adjoint){
    auto ctx = builder.getContext();
    if(!ctrl){
        ctrl = mlir::ArrayAttr::get(ctx, mlir::ArrayRef<mlir::Attribute>{});
    }
    auto famous_gate_def = findFamousGate(famous_gate);
    auto gate_type = GateType::get(ctx, original_size, famous_gate_def->gate_trait);
    auto use_gate = builder.create<UseGateOp>(mlir::UnknownLoc::get(ctx), gate_type, mlir::FlatSymbolRefAttr::get(ctx, getFamousName(famous_gate)), params);
    auto used_gate = use_gate.result();
    if((ctrl && ctrl.size()>0) || adjoint){
        auto ctrls = ctrl.getAsValueRange<mlir::BoolAttr>();
        auto all_one = std::all_of(ctrls.begin(), ctrls.end(), [](auto x){return x;});
        auto decorate_op = builder.create<DecorateOp>(mlir::UnknownLoc::get(ctx), GateType::get(ctx, original_size + ctrl.size(), DecorateOp::computePostDecorateTrait(gate_type.getHints(), ctrl.size(), adjoint, all_one)), used_gate, adjoint, ctrl);
        used_gate = decorate_op.getResult();
        return decorate_op;
    }
    return used_gate;
}

void emitBuiltinGate(mlir::OpBuilder& builder, const char* famous_gate, mlir::ArrayRef<mlir::Value*> qubits, mlir::ArrayRef<mlir::Value> params, mlir::ArrayAttr ctrl, bool adjoint){
    auto ctx = builder.getContext();
    if(!ctrl){
        ctrl = mlir::ArrayAttr::get(ctx, mlir::ArrayRef<mlir::Attribute>{});
    }
    // use the gate.
    auto used_gate = emitUseBuiltinGate(builder, qubits.size() - ctrl.size(), famous_gate, params, ctrl, adjoint);
    auto gate_size = qubits.size();
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
