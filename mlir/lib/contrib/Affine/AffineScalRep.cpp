#include "isq/contrib/Affine.h"
#include "mlir/Dialect/Affine/Passes.h"
#include "mlir/Pass/Pass.h"
#include <mlir/IR/Dominance.h>
using namespace mlir;

namespace isq_affine_scalrep{
using namespace mlir::func;
template <typename DerivedT>
class AffineScalarReplacementBase : public ::mlir::OperationPass<FuncOp> {
public:
  using Base = AffineScalarReplacementBase;

  AffineScalarReplacementBase() : ::mlir::OperationPass<FuncOp>(::mlir::TypeID::get<DerivedT>()) {}
  AffineScalarReplacementBase(const AffineScalarReplacementBase &other) : ::mlir::OperationPass<FuncOp>(other) {}

  /// Returns the command-line argument attached to this pass.
  static constexpr ::llvm::StringLiteral getArgumentName() {
    return ::llvm::StringLiteral("isq-contrib-affine-scalrep");
  }
  ::llvm::StringRef getArgument() const override { return "isq-contrib-affine-scalrep"; }

  ::llvm::StringRef getDescription() const override { return "Replace affine memref acceses by scalars by forwarding stores to loads and eliminating redundant loads"; }

  /// Returns the derived pass name.
  static constexpr ::llvm::StringLiteral getPassName() {
    return ::llvm::StringLiteral("ISQContribAffineScalarReplacement");
  }
  ::llvm::StringRef getName() const override { return "ISQContribAffineScalarReplacement"; }

  /// Support isa/dyn_cast functionality for the derived pass class.
  static bool classof(const ::mlir::Pass *pass) {
    return pass->getTypeID() == ::mlir::TypeID::get<DerivedT>();
  }

  /// A clone method to create a copy of this pass.
  std::unique_ptr<::mlir::Pass> clonePass() const override {
    return std::make_unique<DerivedT>(*static_cast<const DerivedT *>(this));
  }

  /// Return the dialect that must be loaded in the context before this pass.
  void getDependentDialects(::mlir::DialectRegistry &registry) const override {
    
  }

protected:
};

struct AffineScalarReplacement
    : public AffineScalarReplacementBase<AffineScalarReplacement> {
  void runOnOperation() override;
};

void AffineScalarReplacement::runOnOperation() {
  using namespace isq_affine_scalrep;
  isq::contrib::mlir::affineScalarReplace(getOperation(), getAnalysis<DominanceInfo>(),
                      getAnalysis<PostDominanceInfo>());
}



} // namespace



void isq::contrib::mlir::registerAffineScalarReplacementPass() {
  using namespace isq_affine_scalrep;
  PassRegistration<AffineScalarReplacement>();
}
