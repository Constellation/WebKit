/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "B3PeelLoops.h"

#if ENABLE(B3_JIT)

#include "B3BasicBlock.h"
#include "B3EnsureLoopPreHeaders.h"
#include "B3FixSSA.h"
#include "B3NaturalLoops.h"
#include "B3PhaseScope.h"
#include "B3Procedure.h"
#include "B3ValueInlines.h"
#include <wtf/IndexSet.h>

namespace JSC { namespace B3 {

namespace {

namespace B3PeelLoopsInternal {
static constexpr bool verbose = false;
}

} // anonymous namespace

bool peelLoops(Procedure& proc)
{
    PhaseScope phaseScope(proc, "peelLoops"_s);

    ensureLoopPreHeaders(proc);

    NaturalLoops& loops = proc.naturalLoops();
    if (!loops.numLoops())
        return false;

    proc.resetValueOwners();

    // Identify qualifying loops: innermost only, small enough, no unclonable values.
    struct LoopInfo {
        const NaturalLoop* loop;
        BasicBlock* preHeader;
    };
    Vector<LoopInfo> qualifyingLoops;

    for (unsigned loopIndex = 0; loopIndex < loops.numLoops(); ++loopIndex) {
        const NaturalLoop& loop = loops.loop(loopIndex);

        if (!loop.isInnerMostLoop())
            continue;

        // Find the pre-header. The single predecessor of the header that is NOT in the loop.
        BasicBlock* header = loop.header();
        BasicBlock* preHeader = nullptr;
        for (BasicBlock* predecessor : header->predecessors()) {
            if (!loops.belongsTo(predecessor, loop)) {
                ASSERT(!preHeader); // ensureLoopPreHeaders guarantees a single pre-header.
                preHeader = predecessor;
            }
        }
        if (!preHeader) {
            dataLogLnIf(B3PeelLoopsInternal::verbose, "Fail to find pre-header ", header);
            continue;
        }

        auto canPeel = [&] -> bool {
            // Count total values in the loop (header + body blocks).
            // NaturalLoop body may include the header, so track visited blocks.
            unsigned valueCount = loop.header()->size();
            for (Value* value : *loop.header()) {
                if (value->kind().isCloningForbidden()) {
                    dataLogLnIf(B3PeelLoopsInternal::verbose, "Fail due to unclonable value ", value);
                    return false;
                }
            }

            for (unsigned i = 0; i < loop.size(); ++i) {
                BasicBlock* block = loop[i];
                if (block == loop.header())
                    continue;
                valueCount += block->size();
                for (Value* value : *block) {
                    if (value->kind().isCloningForbidden()) {
                        dataLogLnIf(B3PeelLoopsInternal::verbose, "Fail due to unclonable value ", value);
                        return false;
                    }
                }
            }

            if (valueCount > Options::maxB3LoopPeelingBodySize()) {
                dataLogLnIf(B3PeelLoopsInternal::verbose, "Fail due too many values ", valueCount);
                return false;
            }

            return true;
        };

        if (canPeel())
            qualifyingLoops.append({ &loop, preHeader });
    }

    if (qualifyingLoops.isEmpty())
        return false;

    // Collect all blocks belonging to qualifying loops.
    IndexSet<BasicBlock*> loopBlocks;
    for (auto& info : qualifyingLoops) {
        loopBlocks.add(info.loop->header());
        for (unsigned i = 0; i < info.loop->size(); ++i)
            loopBlocks.add(info.loop->at(i));
    }

    // Collect values to demote: Phi values in loop headers and any value defined
    // inside the loop that is used in a different block. This mirrors the approach
    // used by DuplicateTails and ensures all cross-block references go through
    // variables, making cloning safe regardless of block processing order.
    IndexSet<Value*> valuesToDemote;
    for (BasicBlock* block : proc) {
        for (Value* value : *block) {
            if (value->opcode() == Phi && loopBlocks.contains(block))
                valuesToDemote.add(value);
            for (Value* child : value->children()) {
                if (child->owner != block && loopBlocks.contains(child->owner))
                    valuesToDemote.add(child);
            }
        }
    }

    demoteValues(proc, valuesToDemote);

    // if (B3PeelLoopsInternal::verbose) {
    //     dataLogLn("Procedure after value demotion:");
    //     dataLog(proc);
    // }

    // Clone each qualifying loop's body to create the peeled first iteration.
    for (auto& info : qualifyingLoops) {
        const NaturalLoop& loop = *info.loop;
        BasicBlock* preHeader = info.preHeader;
        BasicBlock* header = loop.header();

        // Collect all blocks in the loop. The header is always first.
        // NaturalLoop body may or may not include the header, so use an IndexSet
        // to ensure each block appears exactly once.
        Vector<BasicBlock*> bodyBlocks;
        IndexSet<BasicBlock*> bodyBlockSet;
        bodyBlocks.append(header);
        bodyBlockSet.add(header);
        for (unsigned i = 0; i < loop.size(); ++i) {
            BasicBlock* block = loop[i];
            if (bodyBlockSet.add(block))
                bodyBlocks.append(block);
        }

        // Build block and value maps from original to cloned.
        UncheckedKeyHashMap<BasicBlock*, BasicBlock*> blockMap;
        UncheckedKeyHashMap<Value*, Value*> valueMap;

        // Create cloned blocks.
        for (BasicBlock* block : bodyBlocks) {
            BasicBlock* clonedBlock = proc.addBlock(block->frequency());
            blockMap.add(block, clonedBlock);
        }

        // Pass 1: Clone all values into cloned blocks and build the value map.
        // This must be done before remapping children since body blocks may not
        // be in domination order — a value in block B may reference a value in
        // block A, but B could appear before A in bodyBlocks.
        for (BasicBlock* block : bodyBlocks) {
            BasicBlock* clonedBlock = blockMap.get(block);
            for (Value* value : *block) {
                Value* clonedValue = proc.clone(value);
                if (value->type() != Void)
                    valueMap.add(value, clonedValue);
                clonedBlock->append(clonedValue);
            }
        }

        // Pass 2: Remap children of cloned values to point to cloned definitions.
        for (BasicBlock* block : bodyBlocks) {
            BasicBlock* clonedBlock = blockMap.get(block);
            for (Value* clonedValue : *clonedBlock) {
                for (Value*& child : clonedValue->children()) {
                    if (Value* replacement = valueMap.get(child))
                        child = replacement;
                }
            }
        }

        // Set successors for cloned blocks.
        for (BasicBlock* block : bodyBlocks) {
            BasicBlock* clonedBlock = blockMap.get(block);
            for (const FrequentedBlock& successor : block->successors()) {
                BasicBlock* target = successor.block();
                if (target == header) {
                    // Back-edge in the cloned copy should go to the original header,
                    // connecting the peeled iteration to the real loop.
                    clonedBlock->appendSuccessor(FrequentedBlock(header, successor.frequency()));
                } else if (BasicBlock* clonedTarget = blockMap.get(target)) {
                    // Intra-loop edge: point to cloned block.
                    clonedBlock->appendSuccessor(FrequentedBlock(clonedTarget, successor.frequency()));
                } else {
                    // Exit edge: keep original target.
                    clonedBlock->appendSuccessor(FrequentedBlock(target, successor.frequency()));
                }
            }
        }

        // Rewire: preHeader -> cloned header (instead of original header).
        BasicBlock* clonedHeader = blockMap.get(header);
        bool replaced = preHeader->replaceSuccessor(header, clonedHeader);
        RELEASE_ASSERT(replaced);

        if (B3PeelLoopsInternal::verbose) {
            dataLogLn("Peeled loop with header ", *header);
            dataLogLn("  Pre-header ", *preHeader, " now jumps to cloned header ", *clonedHeader);
        }
    }

    proc.resetReachability();
    proc.invalidateCFG();

    return true;
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
