#include <isq/IR.h>
#include <complex>
<<<<<<< HEAD
#include <mlir/Parser.h>
#include <mlir/Support/LogicalResult.h>
namespace isq {
namespace ir {
::std::complex<double> ComplexF64Attr::complexValue() {
    return ::std::complex<double>(this->getReal().convertToDouble(),
                                  this->getImag().convertToDouble());
}
::mlir::Attribute ComplexF64Attr::parseIR(::mlir::AsmParser &parser) {
    double real, imag;
    if (parser.parseLess() || parser.parseFloat(real) || parser.parseComma() ||
        parser.parseFloat(imag) || parser.parseGreater()) {
        return nullptr;
    }
    return get(parser.getBuilder().getContext(), ::llvm::APFloat(real),
               ::llvm::APFloat(imag));
}
void ComplexF64Attr::printIR(::mlir::AsmPrinter &p) const {
    p << "<";
    p << this->getReal();
    p << ", ";
    p << this->getImag();
    p << ">";
}
=======
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
        auto shape = mlir::RankedTensorType::get({(long)size, col}, mlir::ComplexType::get(mlir::Float64Type::get(ctx)));
        auto mat = DenseComplexF64MatrixAttr::get(ctx, mlir::DenseElementsAttr::get(shape, body));
        return mat;
    }

    MatrixVal DenseComplexF64MatrixAttr::toMatrixVal(){
        auto shape = this->getBody().getType();
        auto n_rows = shape.getShape()[0];
        auto n_cols = shape.getShape()[1];

        auto buf  = this->getBody().getValues<std::complex<llvm::APFloat>>();
        auto id = buf.begin();
        MatrixVal val;
        for(auto i=0; i<n_rows; i++){
            llvm::SmallVector<std::complex<double>> row;
            for(auto j=0; j<n_cols; j++){
                auto v = *id;
                row.push_back(std::complex(v.real().convertToDouble(), v.imag().convertToDouble()));
                id++;
            }
            val.push_back(std::move(row));
        }
        return val;
    }
>>>>>>> merge
} // namespace ir
} // namespace isq
