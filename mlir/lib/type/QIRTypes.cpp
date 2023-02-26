#include <isq/ParsePrint.h>
#include <isq/QTypes.h>
#include <mlir/IR/Builders.h>
#include <mlir/Support/LogicalResult.h>
namespace isq {
namespace ir {

const char *QIRQubitParsePrint::keyword() const { return "qir.qubit"; }
bool QIRQubitParsePrint::isa(::mlir::Type me) const {
    return me.isa<QIRQubitType>();
}
::mlir::Type
QIRQubitParsePrint::parseType(::mlir::AsmParser &parser) const {
    return QIRQubitType::get(parser.getBuilder().getContext());
}
void QIRQubitParsePrint::printType(::mlir::Type type,
                                 ::mlir::AsmPrinter &printer) const {
}


const char *QIRResultParsePrint::keyword() const { return "qir.result"; }
bool QIRResultParsePrint::isa(::mlir::Type me) const {
    return me.isa<QIRQubitType>();
}
::mlir::Type
QIRResultParsePrint::parseType(::mlir::AsmParser &parser) const {
    return QIRResultType::get(parser.getBuilder().getContext());
}
void QIRResultParsePrint::printType(::mlir::Type type,
                                 ::mlir::AsmPrinter &printer) const {
}

::mlir::Type QIRResultType::parse(::mlir::AsmParser &odsParser){
    QIRResultParsePrint p;
    return p.parseType(odsParser);
}
void QIRResultType::print(::mlir::AsmPrinter &odsPrinter) const{
    QIRResultParsePrint p;
    p.printType(*this, odsPrinter);
}
::mlir::Type QIRQubitType::parse(::mlir::AsmParser &odsParser){
    QIRQubitParsePrint p;
    return p.parseType(odsParser);
}
void QIRQubitType::print(::mlir::AsmPrinter &odsPrinter) const{
    QIRQubitParsePrint p;
    p.printType(*this, odsPrinter);
}


} // namespace ir
} // namespace isq