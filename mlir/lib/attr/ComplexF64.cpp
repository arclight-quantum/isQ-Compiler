#include <isq/IR.h>
#include <complex>
#include <llvm/ADT/APFloat.h>
#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/Parser/Parser.h>
#include <mlir/Support/LLVM.h>
#include <mlir/Support/LogicalResult.h>
namespace isq {
namespace ir {
    using MatrixVal = DenseComplexF64MatrixAttr::MatrixVal;
    DenseComplexF64MatrixAttr DenseComplexF64MatrixAttr::get(::mlir::MLIRContext *ctx, const MatrixVal& matrix){
        auto size = matrix.size();
        mlir::SmallVector<std::complex<llvm::APFloat>> body;
        auto col = -1;
        for(auto& row: matrix){
            if(col==-1) col = row.size();
            else assert(row.size() == col && "A matrix must be rectangular!");
            for(auto& elem: row){
                body.push_back(std::complex((llvm::APFloat)elem.real(), (llvm::APFloat)elem.imag()));
            }
        }
        assert(col!=-1 && "A matrix must not be empty!");
        llvm::outs()<<"Parsed matrix." << size<<" "<<col<<"\n";
        auto shape = mlir::RankedTensorType::get({(long)size, col}, mlir::ComplexType::get(mlir::Float64Type::get(ctx)));
        auto mat = DenseComplexF64MatrixAttr::get(ctx, mlir::DenseElementsAttr::get(shape, body));
        mat.dump();
        return mat;
    }

    MatrixVal DenseComplexF64MatrixAttr::toMatrixVal(){
        auto shape = this->getBody().getType();
        auto n_rows = shape.getShape()[0];
        auto n_cols = shape.getShape()[1];

        auto buf  = this->getBody().getValues<llvm::APFloat>();
        auto id = buf.begin();
        MatrixVal val;
        for(auto i=0; i<n_rows; i++){
            llvm::SmallVector<std::complex<double>> row;
            for(auto j=0; j<n_cols; j++){
                auto v = *id;
                row.push_back(v.convertToDouble());
                id++;
            }
            val.push_back(std::move(row));
        }
        return val;
    }
} // namespace ir
} // namespace isq
