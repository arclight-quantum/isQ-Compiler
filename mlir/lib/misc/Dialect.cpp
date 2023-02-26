#include "isq/Dialect.h"
#include <isq/IR.h>
#include <llvm/ADT/StringExtras.h>
#include <isq/tblgen/ISQDialect.cpp.inc>
#include <llvm/Support/ErrorHandling.h>
#include <mlir/IR/Attributes.h>
#define GET_OP_CLASSES
#include <isq/tblgen/ISQOPs.cpp.inc>
#include <isq/tblgen/ISQEnums.cpp.inc>

#define GET_TYPEDEF_CLASSES
#include <isq/tblgen/ISQTypes.cpp.inc>
#define GET_ATTRDEF_CLASSES
#include <isq/tblgen/ISQAttrs.cpp.inc>

#include <isq/passes/Passes.h>
#include <mlir/Transforms/InliningUtils.h>
namespace isq {
namespace ir {
namespace{
    using namespace mlir;
    struct ISQInlinerInterface : public mlir::DialectInlinerInterface {
        using DialectInlinerInterface::DialectInlinerInterface;

        /// This hook checks to see if the given callable operation is legal to
        /// inline into the given call. For Toy this hook can simply return
        /// true, as the Toy Call operation is always inlinable.
        bool isLegalToInline(Operation *call, Operation *callable,
                             bool wouldBeCloned) const final {
            return true;
        }

        /// This hook checks to see if the given operation is legal to inline
        /// into the given region. For Toy this hook can simply return true, as
        /// all Toy operations are inlinable.
        bool isLegalToInline(Operation *, Region *, bool,
                             BlockAndValueMapping &) const final {
            return true;
        }

        /// This hook cheks if the given 'src' region can be inlined into the
        /// 'dest' region. The regions here are the bodies of the callable
        /// functions. For Toy, any function can be inlined, so we simply return
        /// true.
        bool isLegalToInline(Region *dest, Region *src, bool wouldBeCloned,
                             BlockAndValueMapping &valueMapping) const final {
            return true;
        }

        /// This hook is called when a terminator operation has been inlined.
        /// The only terminator that we have in the Toy dialect is the return
        /// operation(toy.return). We handle the return by replacing the values
        /// previously returned by the call operation with the operands of the
        /// return.
        void handleTerminator(Operation *op,
                              ArrayRef<Value> valuesToRepl) const final {
            llvm_unreachable("We don't have terminator ops.");
        }
    };
}


void ISQDialect::getCanonicalizationPatterns(mlir::RewritePatternSet &results) const {

}

mlir::Type ISQDialect::parseType(mlir::DialectAsmParser &parser) const {
    /*
    llvm::outs()<<"isq type\n";
    mlir::StringRef kw;
    auto kwLoc = parser.getCurrentLocation();
    parser.emitError(kwLoc, "Here!");
    if (parser.parseKeyword(&kw)) {
        parser.emitError(kwLoc, "unrecognized type");
        return nullptr;
    }
    
    kwLoc = parser.getCurrentLocation();
    parser.emitError(kwLoc, "Here2!");
    */
    auto kwLoc = parser.getCurrentLocation();
    mlir::StringRef kw;
    mlir::Type ty;
    auto ret =
        generatedTypeParser(parser, &kw, ty);
    if (!ret.hasValue()) {
        parser.emitError(kwLoc, "unrecognized type");
        return nullptr;
    }

    return ty;
}
void ISQDialect::printType(::mlir::Type type,
                           ::mlir::DialectAsmPrinter &printer) const {
    if (mlir::failed(generatedTypePrinter(type, printer))) {
        llvm_unreachable("bad ISQ type for printer.");
    }
}

mlir::Attribute ISQDialect::parseAttribute(::mlir::DialectAsmParser &parser,
                                           ::mlir::Type type) const {
    mlir::StringRef kw;
    auto kwLoc = parser.getCurrentLocation();
    if (parser.parseKeyword(&kw)) {
        parser.emitError(kwLoc, "unrecognized attribute");
        return nullptr;
    }
    mlir::Attribute attr;
    auto ret = generatedAttributeParser(parser, &kw, type, attr);
    if (!ret.hasValue()) {
        parser.emitError(kwLoc, "unrecognized attribute");
        return nullptr;
    }
    return attr;
}
void ISQDialect::printAttribute(::mlir::Attribute attr,
                                ::mlir::DialectAsmPrinter &os) const {
    if (mlir::failed(generatedAttributePrinter(attr, os))) {
        llvm_unreachable("bas ISQ attribute for printer");
    }
}

void ISQDialect::initialize() {
    addTypes<
#define GET_TYPEDEF_LIST
#include <isq/tblgen/ISQTypes.cpp.inc>
        >();
    addAttributes<
#define GET_ATTRDEF_LIST
#include <isq/tblgen/ISQAttrs.cpp.inc>
        >();
    addOperations<
#define GET_OP_LIST
#include <isq/tblgen/ISQOPs.cpp.inc>
        >();
    addInterfaces<ISQInlinerInterface>();

    
}

} // namespace ir
} // namespace isq
