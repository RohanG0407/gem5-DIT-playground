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

#ifndef __CPU_O3_COMP_SIMPLIFIER_HH__
#define __CPU_O3_COMP_SIMPLIFIER_HH__

#include "base/statistics.hh"
#include "base/types.hh"
#include "cpu/o3/dyn_inst_ptr.hh"
#include "params/CompSimplifier.hh"
#include "sim/sim_object.hh"

namespace gem5
{

namespace o3
{

class CPU;

/**
 * Computation Simplifier.
 *
 * Detects trivial IntMult/IntDiv operations at issue time (when operands
 * are ready) and bypasses the multi-cycle functional unit entirely.
 *
 * Trivial cases:
 *  - Multiply: x * 0 = 0, x * 1 = x
 *  - Divide:   0 / x = 0, x / 1 = x
 *
 * This is NOT speculative — source operands are definitively ready in the
 * register file, so results are guaranteed correct.
 *
 * Integration points:
 *  - Check & store result in scheduleReadyInsts() (inst_queue.cc)
 *  - Write result in executeInsts() (iew.cc)
 */
class CompSimplifier : public SimObject
{
  public:
    PARAMS(CompSimplifier);

    CompSimplifier(const Params &p);

    /** Check if the predictor is enabled. */
    bool isEnabled() const { return enabled; }

    /**
     * Try to simplify a trivial IntMult/IntDiv instruction.
     *
     * Checks if the instruction is a qualifying 2-operand integer
     * multiply or divide with a trivial operand (0 or 1). If so,
     * computes the result and returns true.
     *
     * @param inst The instruction to check.
     * @param cpu Pointer to the CPU (for register file access).
     * @param result Output: the computed result if simplifiable.
     * @return true if the instruction was simplified.
     */
    bool trySimplify(const DynInstPtr &inst, CPU *cpu, RegVal &result);

  private:
    bool enabled;

    struct CompSimplifierStats : public statistics::Group
    {
        CompSimplifierStats(CompSimplifier *cs);

        statistics::Scalar simplified;
        statistics::Scalar candidates;
        statistics::Formula coverage;
        statistics::Scalar multByZero;
        statistics::Scalar multByOne;
        statistics::Scalar divOfZero;
        statistics::Scalar divByOne;
        statistics::Scalar ditSuppressed;
    } stats;
};

} // namespace o3
} // namespace gem5

#endif // __CPU_O3_COMP_SIMPLIFIER_HH__
