#include <cstdio>
#include <cassert>
#include <mlir/IR/Types.h>
#include <mlir/IR/Dialect.h>
#include <mlir/IR/TypeSupport.h>
#include <mlir/Parser/Parser.h>
#include <mlir/AsmParser/AsmParserState.h>
#include <mlir/IR/DialectImplementation.h>
#include "llvm/ADT/TypeSwitch.h"
#include <mlir/InitAllDialects.h>
#include "isq/Dialect.h"
#include "mlir/Tools/mlir-opt/MlirOptMain.h"
#include "llvm/Support/CommandLine.h"
#include <mlir/InitAllPasses.h>
#include <mlir/IR/BuiltinTypes.h>
#include <algorithm>

#include <isq/IR.h>

int isq_mlir_opt_main(int argc, char **argv) {
    mlir::DialectRegistry registry;
    isq::ir::ISQToolsInitialize(registry);
    return mlir::asMainReturnCode(mlir::MlirOptMain(
        argc, argv, "MLIR modular optimizer driver for ISQ dialect\n", registry,
        /*preloadDialectsInContext=*/true));
}

int main(int argc, char **argv) { return isq_mlir_opt_main(argc, argv); }