#include <isq/ParsePrint.h>
#include <isq/QTypes.h>
#include <mlir/IR/Builders.h>
#include <mlir/Support/LogicalResult.h>
namespace isq {
namespace ir {

const char *QStateParsePrint::keyword() const { return "qstate"; }
bool QStateParsePrint::isa(::mlir::Type me) const {
    return me.isa<QStateType>();
}
::mlir::Type
QStateParsePrint::parseType(::mlir::AsmParser &parser) const {
    /*
    if (parser.parseKeyword(this->keyword())){
        return nullptr;
    }*/
    return QStateType::get(parser.getBuilder().getContext());
}
void QStateParsePrint::printType(::mlir::Type type,
                                 ::mlir::AsmPrinter &printer) const {
    //printer << this->keyword();
    //printer << "qstate";
}

::mlir::Type QStateType::parse(::mlir::AsmParser &odsParser){
    QStateParsePrint p;
    return p.parseType(odsParser);
}
void QStateType::print(::mlir::AsmPrinter &odsPrinter) const{
    QStateParsePrint p;
    p.printType(*this, odsPrinter);
}


} // namespace ir
} // namespace isq