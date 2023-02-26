#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/Passes.h"
//#include "PassDetail.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include "mlir/Transforms/Passes.h"

namespace isq_canonicalizer{
using namespace mlir;
template <typename DerivedT>
class CanonicalizerBase : public ::mlir::OperationPass<> {
public:
    using Base = CanonicalizerBase;

    CanonicalizerBase()
        : ::mlir::OperationPass<>(::mlir::TypeID::get<DerivedT>()) {}
    CanonicalizerBase(const CanonicalizerBase &other)
        : ::mlir::OperationPass<>(other) {}

    /// Returns the command-line argument attached to this pass.
    static constexpr ::llvm::StringLiteral getArgumentName() {
        return ::llvm::StringLiteral("isq-canonicalize");
    }
    ::llvm::StringRef getArgument() const override {
        return "isq-canonicalize";
    }

    ::llvm::StringRef getDescription() const override {
        return "isq-Canonicalize operations";
    }

    /// Returns the derived pass name.
    static constexpr ::llvm::StringLiteral getPassName() {
        return ::llvm::StringLiteral("ISQCanonicalizer");
    }
    ::llvm::StringRef getName() const override { return "ISQCanonicalizer"; }

    /// Support isa/dyn_cast functionality for the derived pass class.
    static bool classof(const ::mlir::Pass *pass) {
        return pass->getTypeID() == ::mlir::TypeID::get<DerivedT>();
    }

    /// A clone method to create a copy of this pass.
    std::unique_ptr<::mlir::Pass> clonePass() const override {
        return std::make_unique<DerivedT>(*static_cast<const DerivedT *>(this));
    }

    /// Return the dialect that must be loaded in the context before this pass.
    void
    getDependentDialects(::mlir::DialectRegistry &registry) const override {}

protected:
    ::mlir::Pass::Option<bool> topDownProcessingEnabled{
        *this, "top-down",
        ::llvm::cl::desc("Seed the worklist in general top-down order"),
        ::llvm::cl::init(true)};
    ::mlir::Pass::Option<bool> enableRegionSimplification{
        *this, "region-simplify",
        ::llvm::cl::desc("Seed the worklist in general top-down order"),
        ::llvm::cl::init(true)};
    ::mlir::Pass::Option<int64_t> maxIterations{
        *this, "max-iterations",
        ::llvm::cl::desc("Seed the worklist in general top-down order"),
        ::llvm::cl::init(10)};
    ::mlir::Pass::ListOption<std::string> disabledPatterns{
        *this, "disable-patterns",
        ::llvm::cl::desc("Labels of patterns that should be filtered out "
                         "during application")};
    ::mlir::Pass::ListOption<std::string> enabledPatterns{
        *this, "enable-patterns",
        ::llvm::cl::desc("Labels of patterns that should be used during "
                         "application, all other patterns are filtered out")};
};
struct Canonicalizer : public CanonicalizerBase<Canonicalizer> {
    Canonicalizer(const GreedyRewriteConfig &config,
                  ArrayRef<std::string> disabledPatterns,
                  ArrayRef<std::string> enabledPatterns)
        : config(config) {
        this->disabledPatterns = disabledPatterns;
        this->enabledPatterns = enabledPatterns;
    }

    Canonicalizer() {
        // Default constructed Canonicalizer takes its settings from command
        // line options.
        config.useTopDownTraversal = topDownProcessingEnabled;
        config.enableRegionSimplification = enableRegionSimplification;
        config.maxIterations = maxIterations;
    }

    /// Initialize the canonicalizer by building the set of patterns used during
    /// execution.
    LogicalResult initialize(MLIRContext *context) override {
        disabledPatterns.push_back(
            "(anonymous namespace)::SimplifyAffineOp<mlir::AffineStoreOp>");
        disabledPatterns.push_back(
            "(anonymous namespace)::SimplifyAffineOp<mlir::AffineLoadOp>");
        RewritePatternSet owningPatterns(context);
        for (auto *dialect : context->getLoadedDialects())
            dialect->getCanonicalizationPatterns(owningPatterns);
        for (RegisteredOperationName op : context->getRegisteredOperations())
            op.getCanonicalizationPatterns(owningPatterns, context);
        patterns = FrozenRewritePatternSet(std::move(owningPatterns),
                                           disabledPatterns, enabledPatterns);
        return success();
    }
    void runOnOperation() override {
        (void)applyPatternsAndFoldGreedily(getOperation()->getRegions(),
                                           patterns, config);
    }

    GreedyRewriteConfig config;
    FrozenRewritePatternSet patterns;
};
} // namespace

namespace isq::ir::passes{
    void registerISQCanonicalizer(){
        using namespace isq_canonicalizer;
        mlir::PassRegistration<Canonicalizer>();
    }
}