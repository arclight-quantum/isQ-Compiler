#include "isq/Enums.h"
#include "isq/Operations.h"
#include <isq/QTypes.h>
#include <isq/IR.h>
#include <mlir/Dialect/StandardOps/IR/Ops.h>
#include <mlir/Support/LogicalResult.h>
namespace isq {
namespace ir {

const char *GateParsePrint::keyword() const { return "gate"; }
bool GateParsePrint::isa(::mlir::Type me) const { return me.isa<GateType>(); }
::mlir::Type GateParsePrint::parseType(::mlir::DialectAsmParser &parser) const {
    auto ctx = parser.getBuilder().getContext();
    if (parser.parseLess())
        return nullptr;
    auto typeLoc = parser.getCurrentLocation();
    int64_t gate_size = 0;
    if (parser.parseInteger(gate_size))
        return nullptr;
    GateTrait tr = GateTrait::General;
    while (mlir::succeeded(parser.parseOptionalComma())) {
        mlir::StringRef kw;
        if (parser.parseKeyword(&kw)) {
            return nullptr;
        }
        auto e = symbolizeEnum<GateTrait>(kw);
        if (!e.hasValue())
            return nullptr;
        if (e.getValue() == GateTrait::General)
            return nullptr;
        tr = tr | e.getValue();
    }
    if (parser.parseGreater()) {
        return nullptr;
    }
    if (gate_size <= 0) {
        parser.emitError(typeLoc, "gate size should be positive.");
        return nullptr;
    }
    return GateType::get(ctx, gate_size, tr);
}
void GateParsePrint::printType(::mlir::Type type,
                               ::mlir::DialectAsmPrinter &printer) const {
    GateType t = type.cast<GateType>();
    printer << "gate<" << t.getSize();
    auto traits = t.getHints();
    if (traits != GateTrait::General) {
        auto t = stringifyGateTrait(traits);
        for (auto i = t.begin(); i != t.end(); i++) {
            if (*i == '|')
                *i = ',';
        }
        printer << ",";
        printer << t;
    }
    printer << ">";
}

mlir::SmallVector<mlir::Type> GateType::getApplyParameterType(GateType ty) {
    mlir::SmallVector<mlir::Type> args;
    for (auto i = 0; i < ty.getSize(); i++) {
        args.push_back(QStateType::get(ty.getContext()));
    }
    return args;
}
} // namespace ir
} // namespace isq
