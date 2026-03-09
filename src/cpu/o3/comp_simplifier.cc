/*
 * Copyright (c) 2025 All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "cpu/o3/comp_simplifier.hh"

#include "arch/arm/regs/cc.hh"
#include "base/trace.hh"
#include "cpu/o3/cpu.hh"
#include "cpu/o3/dyn_inst.hh"
#include "cpu/op_class.hh"
#include "debug/CompSimp.hh"

namespace gem5
{

namespace o3
{

CompSimplifier::CompSimplifier(const Params &p)
    : SimObject(p),
      enabled(p.enabled),
      stats(this)
{
}

bool
CompSimplifier::trySimplify(const DynInstPtr &inst, CPU *cpu, RegVal &result)
{
    if (!enabled)
        return false;

    OpClass op_class = inst->opClass();
    if (op_class != IntMultOp && op_class != IntDivOp)
        return false;

    // Check DIT: if set, skip simplification (constant-time mode)
    ThreadID tid = inst->threadNumber;
    bool foundDit = false;
    for (int i = 0; i < inst->numSrcRegs(); i++) {
        if (inst->srcRegIdx(i) == ArmISA::cc_reg::Dit) {
            foundDit = true;
            RegVal ditVal = cpu->getReg(inst->renamedSrcIdx(i), tid);
            if (ditVal != 0) {
                DPRINTF(CompSimp, "DIT=1: skipping simplification for "
                        "[sn:%llu] PC %s\n",
                        inst->seqNum, inst->pcState());
                ++stats.ditSuppressed;
                return false;
            }
            break;
        }
    }
    // If DitCC not found (e.g., AArch32 path), skip simplification conservatively
    if (!foundDit) {
        DPRINTF(CompSimp, "DitCC src not found for [sn:%llu] PC %s, "
                "skipping simplification conservatively\n",
                inst->seqNum, inst->pcState());
        return false;
    }

    // Check destination register is integer and not always-ready.
    if (inst->numDestRegs() == 0)
        return false;

    PhysRegIdPtr destReg = inst->renamedDestIdx(0);
    if (destReg->classValue() != IntRegClass || destReg->isAlwaysReady())
        return false;

    // Count integer source registers only (skip CC, InvalidRegClass, etc.).
    // Only handle simple 2-operand forms (MUL, SDIV, UDIV).
    // This filters out MADD/MSUB which have 3 integer sources.
    int intSrcCount = 0;
    int intSrcIndices[2] = {-1, -1};
    for (int i = 0; i < inst->numSrcRegs(); i++) {
        PhysRegIdPtr srcReg = inst->renamedSrcIdx(i);
        if (srcReg->classValue() == IntRegClass &&
            !srcReg->is(InvalidRegClass)) {
            if (intSrcCount < 2)
                intSrcIndices[intSrcCount] = i;
            intSrcCount++;
        }
    }

    if (intSrcCount != 2)
        return false;

    ++stats.candidates;

    RegVal src0 = cpu->getReg(inst->renamedSrcIdx(intSrcIndices[0]), tid);
    RegVal src1 = cpu->getReg(inst->renamedSrcIdx(intSrcIndices[1]), tid);

    if (op_class == IntMultOp) {
        if (src0 == 0 || src1 == 0) {
            result = 0;
            ++stats.simplified;
            ++stats.multByZero;
            DPRINTF(CompSimp, "Simplified [sn:%llu] PC %s: "
                    "mult by zero (%#x * %#x = 0)\n",
                    inst->seqNum, inst->pcState(), src0, src1);
            return true;
        }
        if (src0 == 1) {
            result = src1;
            ++stats.simplified;
            ++stats.multByOne;
            DPRINTF(CompSimp, "Simplified [sn:%llu] PC %s: "
                    "mult by one (%#x * %#x = %#x)\n",
                    inst->seqNum, inst->pcState(), src0, src1, result);
            return true;
        }
        if (src1 == 1) {
            result = src0;
            ++stats.simplified;
            ++stats.multByOne;
            DPRINTF(CompSimp, "Simplified [sn:%llu] PC %s: "
                    "mult by one (%#x * %#x = %#x)\n",
                    inst->seqNum, inst->pcState(), src0, src1, result);
            return true;
        }
    } else {
        // IntDivOp
        if (src0 == 0 && src1 != 0) {
            result = 0;
            ++stats.simplified;
            ++stats.divOfZero;
            DPRINTF(CompSimp, "Simplified [sn:%llu] PC %s: "
                    "div of zero (%#x / %#x = 0)\n",
                    inst->seqNum, inst->pcState(), src0, src1);
            return true;
        }
        if (src1 == 1) {
            result = src0;
            ++stats.simplified;
            ++stats.divByOne;
            DPRINTF(CompSimp, "Simplified [sn:%llu] PC %s: "
                    "div by one (%#x / %#x = %#x)\n",
                    inst->seqNum, inst->pcState(), src0, src1, result);
            return true;
        }
    }

    return false;
}

CompSimplifier::CompSimplifierStats::CompSimplifierStats(CompSimplifier *cs)
    : statistics::Group(cs),
      ADD_STAT(simplified, statistics::units::Count::get(),
               "Number of instructions simplified (bypassed FU)"),
      ADD_STAT(candidates, statistics::units::Count::get(),
               "Number of qualifying 2-operand IntMult/IntDiv instructions"),
      ADD_STAT(coverage, statistics::units::Ratio::get(),
               "Fraction of candidates that were simplified",
               simplified / candidates),
      ADD_STAT(multByZero, statistics::units::Count::get(),
               "Number of multiply-by-zero simplifications"),
      ADD_STAT(multByOne, statistics::units::Count::get(),
               "Number of multiply-by-one simplifications"),
      ADD_STAT(divOfZero, statistics::units::Count::get(),
               "Number of zero-divided-by-x simplifications"),
      ADD_STAT(divByOne, statistics::units::Count::get(),
               "Number of divide-by-one simplifications"),
      ADD_STAT(ditSuppressed, statistics::units::Count::get(),
               "Number of simplifications suppressed by DIT")
{
    coverage.precision(6);
}

} // namespace o3
} // namespace gem5
