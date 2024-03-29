//===- Utils.cpp ---- Utilities for affine dialect transformation ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements miscellaneous transformation utilities for the Affine
// dialect.
//
//===----------------------------------------------------------------------===//

#include "mlir/Dialect/Affine/Utils.h"

#include "isq/Operations.h"
#include "mlir/Dialect/Affine/Analysis/Utils.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Affine/IR/AffineValueMap.h"
#include "mlir/Dialect/Affine/LoopUtils.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/IntegerSet.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include "isq/contrib/Affine.h"
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/SmallVector.h>
#include <mlir/IR/Value.h>
#include <mlir/Interfaces/SideEffectInterfaces.h>

#define DEBUG_TYPE "affine-utils"

using namespace mlir;
using namespace isq::contrib::mlir;

// Checks if a memref value is ``restricted'', i.e. there are no aliasings of the same value.
static const char* RESTRICTED = "memref.restricted";
bool isMemrefRestricted(Value memref){
  if(memref.getDefiningOp<memref::AllocaOp>()){
    return true;
  }
  if(memref.getDefiningOp<memref::AllocOp>()){
    return true;
  }
  if(auto memref_arg = memref.dyn_cast<::mlir::BlockArgument>()){
    if(auto func = ::llvm::dyn_cast_or_null<::mlir::func::FuncOp>(memref_arg.getParentBlock()->getParentOp())){
      for(auto i=0; i<func.getNumArguments(); i++){
        if(func.getArguments()[i]==memref){
          if(func.getArgAttr(i, RESTRICTED)){
            return true;
          }
        }
      }
    }

  }
  return false;

}

/// Attempt to eliminate loadOp by replacing it with a value stored into memory
/// which the load is guaranteed to retrieve. This check involves three
/// components: 1) The store and load must be on the same location 2) The store
/// must dominate (and therefore must always occur prior to) the load 3) No
/// other operations will overwrite the memory loaded between the given load
/// and store.  If such a value exists, the replaced `loadOp` will be added to
/// `loadOpsToErase` and its memref will be added to `memrefsToErase`.
static LogicalResult forwardStoreToLoad(
    AffineReadOpInterface loadOp, SmallVectorImpl<Operation *> &loadOpsToErase,
    SmallPtrSetImpl<Value> &memrefsToErase, DominanceInfo &domInfo) {

  // The store op candidate for forwarding that satisfies all conditions
  // to replace the load, if any.
  Operation *lastWriteStoreOp = nullptr;
  // Undef value analysis: alloc/alloca is directly forwarded as undef.
  for (auto *user : loadOp.getMemRef().getUsers()) {
    auto storeOp = dyn_cast<AffineWriteOpInterface>(user);
    if (!storeOp)
      continue;
    MemRefAccess srcAccess(storeOp);
    MemRefAccess destAccess(loadOp);

    // 1. Check if the store and the load have mathematically equivalent
    // affine access functions; this implies that they statically refer to the
    // same single memref element. As an example this filters out cases like:
    //     store %A[%i0 + 1]
    //     load %A[%i0]
    //     store %A[%M]
    //     load %A[%N]
    // Use the AffineValueMap difference based memref access equality checking.
    if (srcAccess != destAccess)
      continue;

    // 2. The store has to dominate the load op to be candidate.
    if (!domInfo.dominates(storeOp, loadOp))
      continue;

    // 3. Ensure there is no intermediate operation which could replace the
    // value in memory.
    if (!hasNoInterveningEffect<MemoryEffects::Write>(storeOp, loadOp))
      continue;

    // We now have a candidate for forwarding.
    assert(lastWriteStoreOp == nullptr &&
           "multiple simulataneous replacement stores");
    lastWriteStoreOp = storeOp;
  }

  if (!lastWriteStoreOp)
    return failure();

  // Perform the actual store to load forwarding.
  Value storeVal =
      cast<AffineWriteOpInterface>(lastWriteStoreOp).getValueToStore();
  // Check if 2 values have the same shape. This is needed for affine vector
  // loads and stores.
  if (storeVal.getType() != loadOp.getValue().getType())
    return failure();
  loadOp.getValue().replaceAllUsesWith(storeVal);
  // Record the memref for a later sweep to optimize away.
  memrefsToErase.insert(loadOp.getMemRef());
  // Record this to erase later.
  loadOpsToErase.push_back(loadOp);
  return success();
}

// This attempts to find stores which have no impact on the final result.
// A writing op writeA will be eliminated if there exists an op writeB if
// 1) writeA and writeB have mathematically equivalent affine access functions.
// 2) writeB postdominates writeA.
// 3) There is no potential read between writeA and writeB.
static void findUnusedStore(AffineWriteOpInterface writeA,
                            SmallVectorImpl<Operation *> &opsToErase,
                            SmallPtrSetImpl<Value> &memrefsToErase,
                            PostDominanceInfo &postDominanceInfo) {

  for (Operation *user : writeA.getMemRef().getUsers()) {
    // Only consider writing operations.
    auto writeB = dyn_cast<AffineWriteOpInterface>(user);
    if (!writeB)
      continue;

    // The operations must be distinct.
    if (writeB == writeA)
      continue;

    // Both operations must lie in the same region.
    if (writeB->getParentRegion() != writeA->getParentRegion())
      continue;

    // Both operations must write to the same memory.
    MemRefAccess srcAccess(writeB);
    MemRefAccess destAccess(writeA);

    if (srcAccess != destAccess)
      continue;

    // writeB must postdominate writeA.
    if (!postDominanceInfo.postDominates(writeB, writeA))
      continue;

    // There cannot be an operation which reads from memory between
    // the two writes.
    if (!hasNoInterveningEffect<MemoryEffects::Read>(writeA, writeB))
      continue;

    opsToErase.push_back(writeA);
    break;
  }
}

static void findUnloadedAlloc(memref::AllocOp allocOp, SmallVectorImpl<Operation*>& opsToErase, SmallPtrSetImpl<Value> &memrefsToErase){
  for(Operation* user : allocOp.getResult().getUsers()){
    if(hasSingleEffect<MemoryEffects::Read>(user)) return;
  }
  for(Operation* user : allocOp.getResult().getUsers()){
    if(hasSingleEffect<MemoryEffects::Write>(user)) {
      opsToErase.push_back(user);
    }
  }
  memrefsToErase.insert(allocOp.getResult());
}

// The load to load forwarding / redundant load elimination is similar to the
// store to load forwarding.
// loadA will be be replaced with loadB if:
// 1) loadA and loadB have mathematically equivalent affine access functions.
// 2) loadB dominates loadA.
// 3) There is no write between loadA and loadB.
static void loadCSE(AffineReadOpInterface loadA,
                    SmallVectorImpl<Operation *> &loadOpsToErase,
                    DominanceInfo &domInfo) {
  SmallVector<AffineReadOpInterface, 4> loadCandidates;
  for (auto *user : loadA.getMemRef().getUsers()) {
    auto loadB = dyn_cast<AffineReadOpInterface>(user);
    if (!loadB || loadB == loadA)
      continue;

    MemRefAccess srcAccess(loadB);
    MemRefAccess destAccess(loadA);

    // 1. The accesses have to be to the same location.
    if (srcAccess != destAccess) {
      continue;
    }

    // 2. The store has to dominate the load op to be candidate.
    if (!domInfo.dominates(loadB, loadA))
      continue;

    // 3. There is no write between loadA and loadB.
    if (!hasNoInterveningEffect<MemoryEffects::Write>(loadB.getOperation(),
                                                      loadA))
      continue;

    // Check if two values have the same shape. This is needed for affine vector
    // loads.
    if (loadB.getValue().getType() != loadA.getValue().getType())
      continue;

    loadCandidates.push_back(loadB);
  }

  // Of the legal load candidates, use the one that dominates all others
  // to minimize the subsequent need to loadCSE
  Value loadB;
  for (AffineReadOpInterface option : loadCandidates) {
    if (llvm::all_of(loadCandidates, [&](AffineReadOpInterface depStore) {
          return depStore == option ||
                 domInfo.dominates(option.getOperation(),
                                   depStore.getOperation());
        })) {
      loadB = option.getValue();
      break;
    }
  }

  if (loadB) {
    loadA.getValue().replaceAllUsesWith(loadB);
    // Record this to erase later.
    loadOpsToErase.push_back(loadA);
  }
}

// We need an "undef" operation that satisfies the following requirements:
// 1. it carries information about the corresponding load-op, so that it can be restored back to load-op.
// 2. the operation is pure, so that it can be eliminated easily and restored to corresponding read-op.
static LogicalResult forwardUndef(AffineReadOpInterface loadOp, SmallVectorImpl<Operation *> &loadOpsToConvertToUndef,
    SmallPtrSetImpl<Value> &memrefsToErase, DominanceInfo &domInfo){
      auto memref = loadOp.getMemRef();
      if(memref.getDefiningOp<memref::AllocOp>() || memref.getDefiningOp<memref::AllocaOp>()){
        auto alloc = memref.getDefiningOp();
        assert(domInfo.dominates(alloc, loadOp));
        if(!hasNoInterveningEffect<MemoryEffects::Write>(alloc, loadOp)){
          return mlir::failure();
        }
        memrefsToErase.insert(memref);
        loadOpsToConvertToUndef.push_back(loadOp);
      }
      return mlir::failure();
}


// The store to load forwarding and load CSE rely on three conditions:
//
// 1) store/load providing a replacement value and load being replaced need to
// have mathematically equivalent affine access functions (checked after full
// composition of load/store operands); this implies that they access the same
// single memref element for all iterations of the common surrounding loop,
//
// 2) the store/load op should dominate the load op,
//
// 3) no operation that may write to memory read by the load being replaced can
// occur after executing the instruction (load or store) providing the
// replacement value and before the load being replaced (thus potentially
// allowing overwriting the memory read by the load).
//
// The above conditions are simple to check, sufficient, and powerful for most
// cases in practice - they are sufficient, but not necessary --- since they
// don't reason about loops that are guaranteed to execute at least once or
// multiple sources to forward from.
//
// TODO: more forwarding can be done when support for
// loop/conditional live-out SSA values is available.
// TODO: do general dead store elimination for memref's. This pass
// currently only eliminates the stores only if no other loads/uses (other
// than dealloc) remain.
//
void isq::contrib::mlir::affineScalarReplace(func::FuncOp f, DominanceInfo &domInfo,
                               PostDominanceInfo &postDomInfo) {
  // Load op's whose results were replaced by those forwarded from stores.
  SmallVector<Operation *, 8> opsToErase;
  // Load op's whose results were replaced by undef.
  SmallVector<Operation *, 8> opsToUndef;
  // A list of memref's that are potentially dead / could be eliminated.
  SmallPtrSet<Value, 4> memrefsToErase;

  // Walk all load's and perform store to load forwarding.
  f.walk([&](AffineReadOpInterface loadOp) {
    if (failed(
            forwardStoreToLoad(loadOp, opsToErase, memrefsToErase, domInfo))) {
      loadCSE(loadOp, opsToErase, domInfo);
      /*if(failed(forwardUndef(loadOp, opsToUndef, memrefsToErase, domInfo))){
        loadCSE(loadOp, opsToErase, domInfo);
      }*/
      
    }
  });
  OpBuilder builder(f->getContext());
  // Undef all ops
  for (auto* op : opsToUndef){
    builder.setInsertionPoint(op);
    auto undef = builder.create<isq::ir::ContribUndef>(op->getLoc(), op->getResult(0).getType(), op->getOperands() );
    op->getResult(0).replaceAllUsesWith(undef);
    op->erase();
  }
  opsToUndef.clear();
  // Erase all load op's whose results were replaced with store fwd'ed ones.
  for (auto *op : opsToErase)
    op->erase();
  opsToErase.clear();

  // Walk all store's and perform unused store elimination
  f.walk([&](AffineWriteOpInterface storeOp) {
    findUnusedStore(storeOp, opsToErase, memrefsToErase, postDomInfo);
  });
  // Walk all allocs and purge store-only ones.
  f.walk([&](memref::AllocOp allocOp){
    findUnloadedAlloc(allocOp, opsToErase, memrefsToErase);
  });
  // Erase all store op's which don't impact the program
  for (auto *op : opsToErase)
    op->erase();

  // Check if the store fwd'ed memrefs are now left with only stores and can
  // thus be completely deleted. Note: the canonicalize pass should be able
  // to do this as well, but we'll do it here since we collected these anyway.
  for (auto memref : memrefsToErase) {
    // If the memref hasn't been alloc'ed in this function, skip.
    Operation *defOp = memref.getDefiningOp();
    if (!defOp || !isa<memref::AllocOp>(defOp))
      // TODO: if the memref was returned by a 'call' operation, we
      // could still erase it if the call had no side-effects.
      continue;
    if (llvm::any_of(memref.getUsers(), [&](Operation *ownerOp) {
          return !isa<AffineWriteOpInterface, memref::DeallocOp>(ownerOp);
        }))
      continue;

    // Erase all stores, the dealloc, and the alloc on the memref.
    for (auto *user : llvm::make_early_inc_range(memref.getUsers()))
      user->erase();
    defOp->erase();
  }
}

