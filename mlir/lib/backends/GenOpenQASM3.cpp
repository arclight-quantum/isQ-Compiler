#include <iostream>
#include <map> 
#include <set>
#include <vector>
#include <algorithm>

#include "isq/Dialect.h"
#include <isq/utils/DispatchOperation.h>
#include <isq/IR.h>

#include "isq/Operations.h"
#include "isq/QAttrs.h"
//#include "isq/QStructs.h"
#include "isq/utils/Decomposition.h"
#include "mlir/Dialect/Arithmetic/IR/Arithmetic.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/Verifier.h"
#include "mlir/IR/IntegerSet.h"
#include "mlir/IR/Operation.h"


#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Dialect/ControlFlow/IR/ControlFlow.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LogicalResult.h"


#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/ScopedHashTable.h"

#include "llvm/Support/raw_ostream.h"
#include "isq/Backends.h"
#include "mlir/AsmParser/AsmParser.h"

#define TRY(x) if(mlir::failed(x)) return mlir::failure();

using namespace isq::ir;
using namespace isq;
using std::vector;
using std::string;
using std::map;
using std::set;
using std::to_string;
using std::tuple;
using std::get;
using std::make_tuple;
//using namespace std;

enum class OpType : uint32_t {
    EMPTY = 0,
    VAR = 1,
    ADD = 2,
    SUB = 3,
    MUL = 4,
    DIV = 5,
};


namespace isq{
namespace ir{

namespace details{
using namespace std;
using namespace mlir;
using namespace mlir::arith;
using namespace mlir::memref;
using namespace mlir::scf;
using CodegenOpVisitor = OpVisitor<
    mlir::func::FuncOp, AffineIfOp, AffineForOp,
    GetGlobalOp, GlobalOp,
    mlir::arith::ConstantOp, AllocaOp,
    AffineLoadOp, AffineStoreOp,
    IndexCastOp, CmpIOp,
    AddIOp, SubIOp, MulIOp, DivSIOp,
    UseGateOp, DecorateOp,
    ApplyGateOp, CallQOpOp,
    mlir::func::CallOp, mlir::func::ReturnOp,DefgateOp, WhileOp, ConditionOp,
    ModuleOp, SubViewOp, PassOp, AffineYieldOp, scf::YieldOp,
    memref::CastOp, mlir::scf::IfOp
    >;
}



/// Code generator from isQ MLIR Dialect to logical OpenQASM 3.0.
/// https://arxiv.org/pdf/2104.14722.pdf
class MLIRPassImpl: public details::CodegenOpVisitor{
public:
    MLIRPassImpl(mlir::MLIRContext &context, mlir::ModuleOp &module, llvm::raw_ostream &os) : context(&context), theModule(&module), os(os) {};
    

    mlir::LogicalResult mlirPass(){

        indent = 0;
        argCnt = 1;
        tmpVarCnt = 1;
        qVarCnt = 1;
        tmpVarHead = "tmp_i_";
        qVarHead = "tmp_q_";
        nowBlockOp = "";
        funcName = "";
        
        //printOperation(theModule->getOperation());
        //isqtool.initTools(context);
        initGate();
        openQasmHead();
        initializeIntegerSets();
        return visitOperation(theModule->getOperation());
    }

private:
    mlir::IntegerSet eq, slt, sle, sgt, sge;
    mlir::AffineMap singleSymbol;
    void initializeIntegerSets(){
        eq = mlir::parseIntegerSet("()[s0, s1]: (s0-s1 == 0)", context);
        // s0-s1<0 s0-s1+1<=0
        slt = mlir::parseIntegerSet("()[s0, s1]: (s1-s0-1 >= 0)", context);
        // s0-s1<=0
        sle = mlir::parseIntegerSet("()[s0, s1]: (s1-s0 >= 0)", context);
        // s0-s1>0 s0-s1-1>=0
        sgt = mlir::parseIntegerSet("()[s0, s1]: (s0-s1-1 >= 0)", context);
        // s0-s1>=0
        sge = mlir::parseIntegerSet("()[s0, s1]: (s0-s1 >= 0)", context);
        auto s0 = getAffineSymbolExpr(0, context);
        singleSymbol = mlir::AffineMap::get(0, 1, {s0}, context);
    }
    mlir::MLIRContext* context;
    mlir::ModuleOp* theModule;
    llvm::raw_ostream& os;

    int indent;
    int argCnt;
    int tmpVarCnt;
    int qVarCnt;
    bool hasRes;
    string nowBlockOp;
    string funcName;
    string tmpVarHead;
    string qVarHead;
    
    set<string> baseGate;
    map<string, string> gateMap;
    set<size_t> argSet;
    map<size_t, tuple<OpType, int, string>> symbolTable;
    map<size_t, tuple<bool, int, int>> ctrlGate;

    //isqTools isqtool;

    // Initialize predefined gates in OpenQASM 3.0.
    void initGate(){
        baseGate = {"H", "X", "Y", "Z", "S", "T", "CZ", "CX", "CNOT"};
        for (auto &gate: baseGate){
            string newgate = gate;
            transform(gate.begin(), gate.end(), newgate.begin(), [](unsigned char c) { return tolower(c); });
            gateMap[gate] = newgate;
        }
        gateMap["CNOT"] = "cx";
    }
    
    // Printing operation info to output stream. For debugging.
    void printOperation(mlir::Operation *op){
        
        printIndent() << "visit op: " << op->getName() << " with " << op->getNumOperands() << " operands and "
            << op->getNumResults() << " results\n";
        
        if (!op->getAttrs().empty()){
            printIndent() << op->getAttrs().size() << " attributes: \n";
            for (mlir::NamedAttribute attr : op->getAttrs()){
                printIndent() << " - '" << attr.getName() << "' : " << attr.getValue() << "'\n";
            }
        }

        for (auto indexOperand : llvm::enumerate(op->getOperands())){
            auto operand = indexOperand.value();
            
            printIndent() << " - Operand " << indexOperand.index() << " has hash code: " << mlir::hash_value(operand) << "\n";
            if (operand.getDefiningOp()){
                printIndent() << "    from " << operand.getDefiningOp()->getName() << "\n";    
            }
        }

        for (auto indexResult : llvm::enumerate(op->getResults())){
            auto result = indexResult.value();
            printIndent() << " - Result " << indexResult.index() << " type is: " << result.getType() << ", has hash code: " << mlir::hash_value(result) << "\n";
            if (result.use_empty()){
                printIndent() << "  has no use\n";
            }
            else{
                if (result.hasOneUse()){
                    printIndent() << "  has a single use:\n";
                }else{
                    printIndent() << "  has " << std::distance(result.getUses().begin(), result.getUses().end()) << " uses:\n";
                }
                for (mlir::Operation *userOp : result.getUsers()){
                    printIndent() << "    - " << userOp->getName() << "\n";
                }
            }
        }
        
        printIndent() << " " << op->getNumRegions() << " nested regions\n";
        
        auto indent = pushIndent();
        
        for (auto &r:op->getRegions()){
            printRegion(r);
        }
    }

    // Printing region info to output stream. For debugging.
    void printRegion(mlir::Region& region){
        printIndent() << "Region with " << region.getBlocks().size() << " blocks:\n";
        
        auto indent = pushIndent();

        for (auto &b : region.getBlocks()){
            printBlock(b);
        }
    }

    // Printing block info to output stream. For debugging.
    void printBlock(mlir::Block& block){
        
        printIndent() << "Block with " << block.getNumArguments() << " arguments, "
                    << block.getNumSuccessors() << " successors, and "
                    << block.getOperations().size() << " operations\n";

        for (auto &arg: block.getArguments()){
            printIndent() << " - Arg has code: " << mlir::hash_value(arg) << "\n";
            if (arg.use_empty()){
                printIndent() << "  has no use\n";
            }
            else{
                if (arg.hasOneUse()){
                    printIndent() << "  has a single use:\n";
                }else{
                    printIndent() << "  has " << std::distance(arg.getUses().begin(), arg.getUses().end()) << " uses:\n";
                }
                for (mlir::Operation *userOp : arg.getUsers()){
                    printIndent() << "    - " << userOp->getName() << "\n";
                }
            }
        }
        
        auto indent = pushIndent();

        for (auto &p : block.getOperations()){
            printOperation(&p);
        }

    }
    // First we handle blockful ops, i.e. operations with blocks.
    // Modules, If, While, For, Function.
    mlir::LogicalResult visitBlock(mlir::Block* block){
        if(!block) return mlir::success();
        for(auto& child: block->getOperations()){
            TRY(visitOperation(&child));
        }
        return mlir::success();
    }
    mlir::LogicalResult visitOp(mlir::ModuleOp curr_module) override{
        auto mod_name = curr_module.getSymName();
        /*
        if(mod_name){
            if(*mod_name == "isq_builtin"){
                // Just ignore it.
                return mlir::success();
            }
        }
        */
        TRY(visitBlock(curr_module.getBody()));
        return mlir::success();
    }
    mlir::LogicalResult visitOp(mlir::scf::IfOp if_stmt) override{
        llvm_unreachable("TODO");
        return mlir::failure();
    }
    mlir::LogicalResult visitOp(mlir::AffineIfOp if_stmt) override{
        // update_symbol_use_operation
        auto condition = if_stmt.getIntegerSet();

        string asso, lval, rval;

        if (condition == sgt){
            asso = ">";
        }else if (condition == sge){
            asso = ">=";
        }else if (condition == eq){
            asso = "==";
        }else if (condition == sle){
            asso = "<=";
        }else if(condition == slt){
            asso = "<";
        }else{
            if_stmt.emitError("Unsupported affine condition.");
            return mlir::failure();
        }

        for (auto indexOperand : llvm::enumerate(if_stmt->getOperands())){
            auto operand = indexOperand.value();
            size_t code = size_t(mlir::hash_value(operand));
            auto res = getSymbol(code);
            if (indexOperand.index() == 0){
                lval = get<2>(res);
            }else{
                rval = get<2>(res);
            }
        }
        if(!if_stmt.getResults().empty()){
            if_stmt.emitError("If-stmt with yielded value not supported.");
            return mlir::failure();
        }
        openQasmIf(lval, rval, asso);
        printIndent()<< "{\n";
        indent += 1;
        TRY(visitBlock(if_stmt.getThenBlock()));
        // update_symbol_use_operation
        indent-=1;
        openQasmNewLine();
        printIndent()<<"}\n";
        if(if_stmt.hasElse()){
            printIndent()<<"else\n";
            printIndent()<<"{\n";
            indent++;
            TRY(visitBlock(if_stmt.getElseBlock()));
            indent--;
            printIndent()<<"}\n";
        }
        openQasmNewLine();
        return mlir::success();
    }
    
    mlir::LogicalResult visitOp(mlir::scf::WhileOp while_stmt) override{
        // Generate while-statement using manual guard.
        return mlir::success();
    }
    mlir::LogicalResult visitOp(mlir::func::FuncOp func_op) override{
        // update_symbol_use_operation
        tmpVarCnt = 1;
        qVarCnt = 1;
        argCnt = 1;
        hasRes = false;

        auto attr = func_op.getSymNameAttr();
        
        funcName = attr.getValue().str();

        auto res_type = func_op.getFunctionType();
        for (auto &type: res_type.getResults()){
            if (type.isa<mlir::IntegerType>()){
                hasRes = true;
                break;
            }
        }
        // update_region
        auto not_main = func_op.getSymName() != "main";
        set<size_t> block_args;
        auto& func_body = func_op.getBody();
        if(!func_body.hasOneBlock()) {
            func_op.emitError("OpenQASM codegen only supports functions with exactly one basic block.");
            return mlir::failure();
        }
        auto func_block = func_body.getBlocks().begin();
        if(not_main){
            vector<tuple<string, string, int>> arglist;
            for (auto &arg: func_block->getArguments()){
                
                auto type = arg.getType();
                int shape = 1;
                string var_type = "int";
                if (type.isa<mlir::MemRefType>()){
                    shape = type.dyn_cast<mlir::MemRefType>().getShape()[0];
                    if (type.dyn_cast<mlir::MemRefType>().getElementType().isa<QStateType>()){
                        var_type = "qubit";
                    }
                }else if (type.isa<QStateType>()){
                    var_type = "qubit";
                }

                arglist.push_back(make_tuple("arg"+to_string(argCnt), var_type, shape));

                size_t code = size_t(mlir::hash_value(arg));
                argSet.insert(code);
                block_args.insert(code);
                TRY(symbolInsert(code, OpType::VAR, shape, "arg"+to_string(argCnt++)));
            }

            openQasmFunc(funcName, arglist);
            
            indent += 1;
        }
        TRY(visitBlock(&*func_block));
        if(not_main){
            indent -= 1;
            for (auto code: block_args){
                argSet.erase(code);
            }
            openQasmNewLine();
            os << "}\n\n";
        }
        return mlir::success();
    }
    mlir::LogicalResult visitOp(mlir::AffineForOp for_stmt) override{
        // update_symbol_use_operation
        string lval, rval;
        int idx = 0;
        
        auto lmap = for_stmt.getLowerBoundMap();
        if (lmap != singleSymbol){
            lval = to_string(lmap.getSingleConstantResult());
        }else{
            size_t opcode = size_t(mlir::hash_value(for_stmt.getLowerBound().getOperand(0)));
            auto res = getSymbol(opcode);
            lval = get<2>(res);
            idx += 1;
        }

        auto rmap = for_stmt.getUpperBoundMap();
        if (rmap != singleSymbol){
            rval = to_string(rmap.getSingleConstantResult());
        }else{
            size_t opcode = size_t(mlir::hash_value(for_stmt.getUpperBound().getOperand(0)));
            auto res = getSymbol(opcode);
            rval = get<2>(res);
            idx += 1;
        }
        
        openQasmFor(tmpVarHead+to_string(tmpVarCnt), lval, rval);
        
        // update_symbol_use_block
        auto block = for_stmt.getBody();
        if(block->getNumArguments()!=1){
            for_stmt->emitError("for-loop with more than 1 arguments (a.k.a. for-body yielding) is not supported");
            return mlir::failure();
        }
        TRY(symbolInsert(size_t(mlir::hash_value(block->getArgument(0))), OpType::VAR, 1, tmpVarHead+to_string(tmpVarCnt++)));
        indent += 1;
        TRY(visitBlock(block));
        indent -= 1;
        openQasmNewLine();
        os << "}\n";
        return mlir::success();
    }
    // Then we handle blockless operations.

    std::string next_tempInt(){
        auto id = tmpVarCnt++;
        auto name = tmpVarHead + to_string(id);
        openQasmVarDefine(name, "int", 1);
        return name;
    }
    std::string next_tempBit(){
        auto id = tmpVarCnt++;
        auto name = tmpVarHead + to_string(id);
        openQasmVarDefine(name, "bit", 1);
        return name;
    }
    std::string next_tempName(){
        auto id = tmpVarCnt++;
        auto name = tmpVarHead + to_string(id);
        return name;
    }

    mlir::LogicalResult visitOp(mlir::memref::GlobalOp op) override{
        auto id = op.sym_name();
        auto type = op.type();
        string var_type = "int";
        int size = type.getShape()[0];
        if (type.getElementType().isa<QStateType>()){
            var_type = "qubit";
        }
        openQasmVarDefine(id.str(), var_type, size);
        return mlir::success();
    }
    mlir::LogicalResult visitOp(mlir::memref::GetGlobalOp op) override{
        auto attr = op.nameAttr();
        //os << "global var: " << attr.getValue().str() << endl;
        auto type = op.result().getType().dyn_cast<mlir::MemRefType>();
        return symbolInsert(size_t(mlir::hash_value(op.result())), OpType::VAR, type.getShape()[0], attr.getValue().str());
    }
    mlir::LogicalResult visitOp(mlir::arith::ConstantOp op) override{
        auto attr = op.getValueAttr().dyn_cast<mlir::IntegerAttr>();
        return symbolInsert(size_t(mlir::hash_value(op->getOpResult(0))), OpType::VAR, 1, to_string(attr.getInt()));
    }
    mlir::LogicalResult visitOp(mlir::memref::AllocaOp op) override{
        // no use, jump
        auto result = op.getResult();
        if (result.use_empty()){
            return mlir::success();
        }
        
        auto type = op.getResult().getType().dyn_cast<mlir::MemRefType>();
        if (type.getShape()[0] == 1){
            // jump this one: single int/qubit variable assign func args.
            mlir::Operation* first_use;
            for (mlir::Operation *userOp : result.getUsers()){
                first_use = userOp;
            }
            //os << "first user: " << first_use->getName().getStringRef().str() << endl;
            if (auto first_store = llvm::dyn_cast<mlir::AffineStoreOp>(first_use)){
                auto operand = first_use->getOperand(0);
                size_t code = size_t(mlir::hash_value(operand));
                if (argSet.count(code) != 0){
                    auto res = getSymbol(code);
                    return symbolInsert(size_t(mlir::hash_value(op->getOpResult(0))), OpType::VAR, type.getShape()[0], get<2>(res));
                } 
            }
        }

        if (type.getElementType().isa<QStateType>()){
            openQasmVarDefine(qVarHead+to_string(qVarCnt), "qubit", type.getShape()[0]);
            return symbolInsert(size_t(mlir::hash_value(op->getOpResult(0))), OpType::VAR, type.getShape()[0], qVarHead+to_string(qVarCnt++));
        }else{
            openQasmVarDefine(tmpVarHead+to_string(tmpVarCnt), "int", type.getShape()[0]);
            return symbolInsert(size_t(mlir::hash_value(op->getOpResult(0))), OpType::VAR, type.getShape()[0], tmpVarHead+to_string(tmpVarCnt++));   
        }
    }
    mlir::LogicalResult visitOp(mlir::AffineLoadOp op) override{
        string loadstr = "";
        for (auto indexOperand : llvm::enumerate(op->getOperands())){
            auto operand = indexOperand.value();
            size_t code = size_t(mlir::hash_value(operand));
            auto res = getSymbol(code);
            //os << "operand " << indexOperand.index() << ": (" << get<0>(res) << ", " << get<1>(res) << ", " << get<2>(res) << ")";
            if (indexOperand.index() == 0){
                loadstr += get<2>(res);
                //if (get<1>(res) == 1)
                //    break;
            }else{
                loadstr += "[" + get<2>(res) + "]";
            }
        }
        if(op->getOperands().size()==2){
            if(op.getAffineMap().isConstant()){
                loadstr += "[" + to_string(op.getAffineMap().getConstantResults()[0]) + "]";
            }
        }
        //os << "; loadstr: " << loadstr << endl;
        return symbolInsert(size_t(mlir::hash_value(op->getOpResult(0))), OpType::VAR, 1, loadstr);
    }
    mlir::LogicalResult visitOp(mlir::AffineStoreOp op) override{
        string lval = "", rval = "";
        for (auto indexOperand : llvm::enumerate(op->getOperands())){
            auto operand = indexOperand.value();
            size_t code = size_t(mlir::hash_value(operand));
            auto res = getSymbol(code);
            //os << ", operand " << indexOperand.index() << ": (" << get<0>(res) << ", " << get<1>(res) << ", " << get<2>(res) << ")";
            
            if (get<0>(res) == OpType::EMPTY){
                op->emitError("Can't backtrace operand ")<<operand;
                return mlir::failure();
            }
            
            if (indexOperand.index() == 0){                    
                rval = get<2>(res);
            }else if (indexOperand.index() == 1){

                if (operand.getType().dyn_cast<mlir::MemRefType>().getElementType().isa<QStateType>()){                         
                    return mlir::success();
                }

                lval = get<2>(res);
                //if (get<1>(res) == 1)
                //    break;
            }else{
                lval += "[" + get<2>(res) + "]";
            }
        }
        if(op->getOperands().size()==2){
            if(op.getAffineMap().isConstant()){
                lval += "[" + to_string(op.getAffineMap().getConstantResults()[0]) + "]";
            }
        }
        if (lval != rval)
            openQasmAssign(lval, rval);
        return mlir::success();
    }
    mlir::LogicalResult visitOp(mlir::arith::IndexCastOp op) override{
        size_t opcode = size_t(mlir::hash_value(op->getOperand(0)));
            auto res = getSymbol(opcode);
        return symbolInsert(size_t(mlir::hash_value(op->getOpResult(0))), OpType::VAR, 1, get<2>(res));
    }
    mlir::LogicalResult visitBinaryOp(OpType op_t, char op, mlir::Value lhs, mlir::Value rhs, mlir::Value ret){
        auto lhs_s = get<2>(getSymbol(mlir::hash_value(lhs)));
        auto rhs_s = get<2>(getSymbol(mlir::hash_value(rhs)));
        auto temp = next_tempInt();
        openQasmAssign(temp+"[0]", lhs_s+op+rhs_s);
        return symbolInsert(ret, op_t, 1, temp+"[0]");
    }
    mlir::LogicalResult visitOp(mlir::arith::AddIOp op) override{
        return visitBinaryOp(OpType::ADD, '+', op.getLhs(), op.getRhs(), op.getResult());
    }
    mlir::LogicalResult visitOp(mlir::arith::SubIOp op) override{
        return visitBinaryOp(OpType::SUB, '-', op.getLhs(), op.getRhs(), op.getResult());
    }
    mlir::LogicalResult visitOp(mlir::arith::MulIOp op) override{
        return visitBinaryOp(OpType::MUL, '*', op.getLhs(), op.getRhs(), op.getResult());
    }
    mlir::LogicalResult visitOp(mlir::arith::DivSIOp op) override{
        return visitBinaryOp(OpType::DIV, '/', op.getLhs(), op.getRhs(), op.getResult());
    }
    mlir::LogicalResult visitOp(UseGateOp op) override{
        auto attr = op.nameAttr();
        //os << "use gate: " << attr.getLeafReference().str() << endl;
        auto leaf = attr.getLeafReference().str();

        return symbolInsert(op.getResult(), OpType::VAR, 0, leaf);
    }
    mlir::LogicalResult visitOp(DecorateOp op) override{
        auto modifiers = std::string();
        if(op.adjoint()) modifiers += "inv @ ";
        for(auto flag: op.ctrl()){
            if(flag){
                modifiers += "ctrl @ ";
            }else{
                modifiers += "nctrl @ ";
            }
        }
        auto prevSymbol = getSymbol(op.getOperand());
        return symbolInsert(op.getResult(), OpType::VAR, 1, modifiers + get<2>(prevSymbol));
    }
    mlir::LogicalResult visitOp(ApplyGateOp op) override{
        vector<string> qlist;
        bool inv = false;
        int ctrl = 0, nctrl = 0;
        string gate_name_with_modifier = "";
        gate_name_with_modifier = get<2>(getSymbol(op.gate()));
        for (auto indexed_operand : ::llvm::enumerate(op.args())){
            auto index = indexed_operand.index();
            auto operand = indexed_operand.value();
            auto res = getSymbol(operand);
            TRY(symbolInsert(op.getResult(index), OpType::VAR, 1, get<2>(getSymbol(operand))));
            qlist.push_back(get<2>(res));
        }
        openQasmUnitary(gate_name_with_modifier, qlist);
        return mlir::success();
    }
    mlir::LogicalResult visitOp(mlir::func::CallOp op) override{
        auto func_name = op.getCalleeAttr();
        string call_str = func_name.getValue().str() + "(";
        for (auto indexOperand : llvm::enumerate(op->getOperands())){
            auto operand = indexOperand.value();
            size_t code = size_t(mlir::hash_value(operand));
            auto res = getSymbol(code);
            if (indexOperand.index() > 0){
                call_str += ", ";
            }
            call_str += get<2>(res);
            call_str += "[0]";
        }
        call_str += ")";
        for (auto indexResult : llvm::enumerate(op->getResults())){
            auto result = indexResult.value();
            if (result.getType().isa<mlir::IndexType>()){
                auto tmp = next_tempInt();
                openQasmAssign(tmp+"[0]", call_str);
                return symbolInsert(size_t(mlir::hash_value(result)), OpType::VAR, 1, tmp+"[0]");
            }
        }
        if(op.getNumResults()==0){
            openQasmCall(call_str);
        }else{
            op->emitError("wtf?");
            return mlir::failure();
        }
        return mlir::success();
    }
    mlir::LogicalResult visitOp(CallQOpOp op) override {
        string qop_name = op->getAttr(llvm::StringRef("callee")).dyn_cast<mlir::SymbolRefAttr>().getLeafReference().str();
        string call_qop_str = qop_name + " ";

        for (auto indexOperand : llvm::enumerate(op->getOperands())){
            auto operand = indexOperand.value();
            size_t code = size_t(mlir::hash_value(operand));
            auto res = getSymbol(code);
            if (indexOperand.index() > 0){
                call_qop_str += ", ";
            }
            call_qop_str += get<2>(res);
            call_qop_str += "[0]";
        }
        auto result_range = op->getResults();

        // First N operands: build qubit mapping for results
        for (auto indexed_operand: llvm::enumerate(op.getOperands().take_front(op.size()))){
            auto index = indexed_operand.index();
            auto operand = indexed_operand.value();
            auto res = getSymbol(operand);
            TRY(symbolInsert(op.getResult(index), OpType::VAR, 1, get<2>(getSymbol(operand))));
        }
        // Rest results: assign integer.
        for (auto indexResult: result_range.drop_front(result_range.size()-op.size())){
            auto result = indexResult;
            // Only consider measure.
            if (result.getType().isa<mlir::IntegerType>()){
                auto temp_bool = next_tempBit();
                auto temp_int = next_tempInt();
                openQasmAssign(temp_bool+"[0]", call_qop_str);
                openQasmAssign(temp_int+"[0]", (string("(int)"))+temp_bool+"[0]");
                return symbolInsert(size_t(mlir::hash_value(result)), OpType::VAR, 1, temp_int+"[0]");
            }
            
        }
        // Only consider reset.
        if(op.getNumResults()==op.size()){
            openQasmCall(call_qop_str);
        }else{
            op->emitError("wtf?");
            return mlir::failure();
        }
        return mlir::success();
    }
    mlir::LogicalResult visitOp(mlir::func::ReturnOp op) override{
        if (hasRes){
            size_t opcode = size_t(mlir::hash_value(op->getOperand(0)));
            auto res = getSymbol(opcode);
            openQasmReturn(get<2>(res));
        }else{
            openQasmReturn("");
        }
        return mlir::success();
    }
    mlir::LogicalResult visitOp(DefgateOp op) override{
        string gate_name = op.sym_name().str();
        int shape = op.type().getSize();
        if(shape!=1){
            op->emitOpError("with shape > 1 not supported in this codegen. Decompose first.");
            return mlir::failure();
        }
        if (!op.definition()){
            op.emitOpError("without definition not supported.");
            return mlir::failure();
        }
        for(auto& def_ : *op.definition()){
            auto def = def_.cast<GateDefinition>();
            if(def.getType()=="unitary"){
                auto mat = def.getValue().cast<DenseComplexF64MatrixAttr>();
                auto matval = mat.toMatrixVal();
                /*
                auto mat = def.getValue().cast<mlir::ArrayAttr>();
                std::complex<double> matv[2][2];
                for(int i=0; i<2; i++){
                    for(int j=0; j<2; j++){
                        matv[i][j]=mat[i].cast<mlir::ArrayAttr>()[j].cast<ComplexF64Attr>().complexValue();
                    }
                }
                */
                std::complex<double> matv[2][2];
                for(int i=0; i<2; i++){
                    for(int j=0; j<2; j++){
                        matv[i][j]=matval[i][j];
                    }
                }
                openQasmGateDefine(gate_name, shape, matv);
                return mlir::success();
            }
        }
        op->emitOpError("without unitary definition not supported.");
        return mlir::failure();
    }
    mlir::LogicalResult visitOp(mlir::arith::CmpIOp op) override{
        int pred = static_cast<int>(op.getPredicate());
        string asso = "";
        switch (pred)
        {
        case 0:
            asso = " == ";
            break;
        case 1:
            asso = " != ";
            break;
        case 2:
            asso = " < ";
            break;
        case 3:
            asso = " <= ";
            break;
        case 4:
            asso = " > ";
            break;
        case 5:
            asso = " >= ";
            break;
        default:
            break;
        }
        
        string cmp_str = "";
        for (auto indexOperand : llvm::enumerate(op->getOperands())){
            auto operand = indexOperand.value();
            size_t code = size_t(mlir::hash_value(operand));
            auto res = getSymbol(code);
            cmp_str += get<2>(res);
            if (indexOperand.index() == 0){
                cmp_str += asso;
            }
        }
        auto temp_bool = next_tempBit();
        openQasmAssign(temp_bool+"[0]", cmp_str);
        return symbolInsert(size_t(mlir::hash_value(op->getOpResult(0))), OpType::VAR, 1, temp_bool+"[0]");
    }
    string memrefObtainArg(mlir::Operation::operand_range args, mlir::ArrayAttr static_args, size_t index){
        if(index>=args.size()){
            auto constant = static_args[index-args.size()].cast<mlir::IntegerAttr>();
            auto cval = constant.getInt();
            auto temp_constant = next_tempInt();
            openQasmAssign(temp_constant+"[0]", to_string(cval));
            return temp_constant+"[0]";
        }else{
            return get<2>(getSymbol(args[0]));
        }

    }
    mlir::LogicalResult visitOp(mlir::memref::SubViewOp op) override{
        auto arr_in = op.source();
        auto arr_out = op.result();
        auto arr_in_ty = op.getSourceType();
        auto arr_out_ty = arr_out.getType().cast<mlir::MemRefType>();
        if(arr_in_ty.getRank()!=1 || arr_out_ty.getRank()!=1){
            op.emitOpError("with rank!=1 is not supported");
            return mlir::failure();
        }
        
        auto offset = memrefObtainArg(op.offsets(), op.static_offsets(), 0);
        auto size = memrefObtainArg(op.sizes(), op.static_sizes(), 0);
        auto stride = memrefObtainArg(op.strides(), op.static_strides(), 0);
        auto temp_val = next_tempName();
        openQasmSlice(temp_val, get<2>(getSymbol(arr_in)), offset, string(offset)+"+"+size, stride);
        return symbolInsert(size_t(mlir::hash_value(op->getOpResult(0))), OpType::VAR, -1, temp_val);
    }
    mlir::LogicalResult visitOp(PassOp op) override{
        return mlir::success();
    }
    mlir::LogicalResult visitOp(mlir::AffineYieldOp) override{
        return mlir::success();
    }
    mlir::LogicalResult visitOp(mlir::scf::YieldOp) override{
        return mlir::success();
    }
    mlir::LogicalResult visitOp(mlir::memref::CastOp op) override{
        // todo: we hope it is identical cast.
        auto symbol = getSymbol(op.source());
        return symbolInsert(size_t(mlir::hash_value(op->getOpResult(0))), get<0>(symbol), get<1>(symbol), get<2>(symbol));
        return mlir::success();
    }
    mlir::LogicalResult visitOp(mlir::Operation* op) override{
        op->emitOpError("is unsupported in code generation");
        return mlir::failure();
    }
    
    struct RaiiIndent{
        int& indent;
        RaiiIndent(int &indent): indent(indent){};
        ~RaiiIndent() {indent -= 1;};
    };

    RaiiIndent pushIndent(){
        return RaiiIndent(++indent);
    }

    llvm::raw_ostream &printIndent() {
        for (int i = 0; i < indent; ++i)
            os << "    ";
        return os;
    }

    void openQasmHead(){
        os << "OpenQasm 3.0;\ninclude \"stdgates.inc\";\n\n";
    }

    void openQasmNewLine(){
        for (int i = 0; i < indent; ++i)
            os << "    ";
    }

    void openQasmVarDefine(string name, string type, int size){
        openQasmNewLine();
        os << type;
        //if (size > 1){
        os << "[" << size << "]";
        //}
        os << " " << name << ";\n";
    }

    void openQasmGateDefine(string name, int shape, std::complex<double> matrix[2][2]){
        openQasmNewLine();
        auto zyz = zyzDecomposition(matrix);
        os << "gate "+ name;
        for (int i = 0; i < shape; i++){
            char tmp = 'a'+i;
            if (i > 0)
                os << ",";
            os << " " << tmp;
        }
        os << " {\n";
        os << "    U("<<zyz.theta<<","<<zyz.phi<<","<<zyz.lam<<") a;\n";
        os << "};\n";
    }

    void openQasmFunc(string name, vector<tuple<string, string, int>> &arglist){
        
        openQasmNewLine();
        os << "def " << name << "(";
        for (int i = 0; i < arglist.size(); i++){
            if (i > 0)
                os << ", ";
            os << get<1>(arglist[i]);
            if (get<2>(arglist[i]) > 1){
                os << "[" << get<2>(arglist[i]) << "]";
            }
            os << " " << get<0>(arglist[i]);
        }
        os << ")";
        if (hasRes){
            os << " -> int";
        }
        os << " {\n";
    }

    void openQasmAssign(string lval, string rval){
        openQasmNewLine();
        os << lval << " = " << rval << ";\n";
    }

    string openQasmGate(string name){
        if (baseGate.count(name) == 0)
            return name;

        return gateMap[name];
    }

    void openQasmUnitary(string name, vector<string>& qlist){
        openQasmNewLine();
        os << name << " ";
        for (int i = 0; i < qlist.size(); i++){
            if (i > 0)
                os << ", ";
            os << qlist[i] << "[0]";
        }
        os << ";\n";
    }

    void openQasmCall(string callstr){
        openQasmNewLine();
        os << callstr << ";\n";
    }

    void openQasmReturn(string res){
        openQasmNewLine();
        os << "return " << res << ";\n";
    }

    void openQasmIf(string lval, string rval, string op){
        openQasmNewLine();
        os << "if (" << lval << " " << op << " " << rval << ")";
    }

    void openQasmFor(string arg, string lval, string rval){
        openQasmNewLine();
        os << "for " << arg << " in [" << lval << ":" << rval << "] {\n";
    }

    void openQasmWhile(string cond){
        openQasmNewLine();
        os << "while (" << cond << ")";
    }
    void openQasmSlice(string ret, string arr, string lo, string hi,string stride){
        openQasmNewLine();
        os << "let "<<ret<<" = "<<arr<<"["<<lo<<":"<<stride<<":"<<hi<<"]\n";
    }

    mlir::LogicalResult symbolInsert(::mlir::Value v, OpType type, int shape, string str){
        return symbolInsert(mlir::hash_value(v), type, shape, str);
    }
    mlir::LogicalResult symbolInsert(size_t code, OpType type, int shape, string str){
        
        auto iter = symbolTable.find(code);
        if (iter != symbolTable.end()){
            return mlir::failure();
        }
        
        symbolTable.insert(make_pair(code, make_tuple(type, shape, str)));
        return mlir::success();
    }
    tuple<OpType, int, string> getSymbol(mlir::Value val){
        return getSymbol(mlir::hash_value(val));
    }
    tuple<OpType, int, string> getSymbol(size_t code){
        auto iter = symbolTable.find(code);
        if (iter != symbolTable.end()){
            return iter->second;
        }

        return make_tuple(OpType::EMPTY, 0, "");
    }

    mlir::LogicalResult ctrlGateInsert(size_t code, bool inv, int ctrl, int nctrl){
        
        auto iter = ctrlGate.find(code);
        if (iter != ctrlGate.end()){
            return mlir::failure();
        }

        ctrlGate.insert(make_pair(code, make_tuple(inv, ctrl, nctrl)));
        return mlir::success();
    }

    tuple<bool, int, int> getCtrl(size_t code){
        auto iter = ctrlGate.find(code);
        if (iter != ctrlGate.end()){
            return iter->second;
        }

        return make_tuple(false, -1, -1);
    }
}; 
}
}


namespace isq {
namespace ir{
mlir::LogicalResult generateOpenQASM3Logic(mlir::MLIRContext &context, mlir::ModuleOp &module, llvm::raw_fd_ostream &os) {
    return MLIRPassImpl(context, module, os).mlirPass();
}
}
}