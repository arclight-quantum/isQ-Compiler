#ifndef _ISQ_QATTRS_H
#define _ISQ_QATTRS_H
#include <mlir/IR/Dialect.h>
#include <mlir/IR/DialectImplementation.h>
#include <mlir/IR/AttributeSupport.h>
#include <mlir/IR/Attributes.h>
#include <mlir/IR/BuiltinAttributes.h>
namespace isq::ir{
    
}
#define GET_ATTRDEF_CLASSES
#include <isq/tblgen/ISQAttrs.h.inc>
namespace isq::ir{
    using GateDefinition = GateDefinitionAttr;
    
}
#endif