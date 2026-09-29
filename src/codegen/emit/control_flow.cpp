#include "codegen/emit/emitter.h"

#include "bytecode/bc_helpers.h"
#include "codegen/emit/context_layout.h"

#include <cstddef>
#include <cstdlib>

namespace asjitx86::emit {

bool FunctionEmitter::EmitEntryDispatch() {
    using namespace asmjit;

    auto& cc = Compiler();
    std::vector<size_t> entryIndices;
    entryIndices.reserve(instructions_.size());
    for (size_t i = 0; i < instructions_.size(); i++) {
        if (instructions_[i].op == asBC_JitEntry) entryIndices.push_back(i);
    }
    if (entryIndices.empty()) {
        cc.jmp(exitLabel_);
        return true;
    }

    cc.cmp(jitArg_, 1);
    cc.je(labels_[entryIndices.front()]);

    auto emitEntryRange = [&](auto&& self, size_t first, size_t last) -> void {
        const size_t middle = first + (last - first) / 2;
        cc.cmp(jitArg_, Imm(asPWORD(middle + 1)));
        cc.je(labels_[entryIndices[middle]]);
        if (first < middle && middle < last) {
            Label lower = cc.new_label();
            cc.jb(lower);
            self(self, middle + 1, last);
            cc.bind(lower);
            self(self, first, middle - 1);
        } else if (first < middle) {
            cc.ja(exitLabel_);
            self(self, first, middle - 1);
        } else if (middle < last) {
            cc.jb(exitLabel_);
            self(self, middle + 1, last);
        } else {
            cc.jmp(exitLabel_);
        }
    };
    if (entryIndices.size() > 1)
        emitEntryRange(emitEntryRange, 1, entryIndices.size() - 1);
    else
        cc.jmp(exitLabel_);
    return true;
}

EmitResult FunctionEmitter::EmitControlFlow(
    size_t index, const Instruction& instruction, const asDWORD* ip) {
    using namespace asmjit;

    auto& cc = Compiler();
    switch (instruction.op) {
    case asBC_JMP: {
        const int targetIndex = BranchTargetIndex(instruction, ip);
        if (targetIndex < 0) return EmitResult::Error;
        cc.jmp(labels_[static_cast<size_t>(targetIndex)]);
        return EmitResult::Success;
    }
    case asBC_JZ:
    case asBC_JNZ:
    case asBC_JS:
    case asBC_JNS:
    case asBC_JP:
    case asBC_JNP:
    case asBC_JLowZ:
    case asBC_JLowNZ: {
        const int targetIndex = BranchTargetIndex(instruction, ip);
        if (targetIndex < 0) return EmitResult::Error;
        if (instruction.op == asBC_JLowZ ||
            instruction.op == asBC_JLowNZ)
            cc.cmp(x86::byte_ptr(
                       regs_, offsetof(asSVMRegisters, valueRegister)),
                   0);
        else
            cc.cmp(x86::dword_ptr(
                       regs_, offsetof(asSVMRegisters, valueRegister)),
                   0);
        switch (instruction.op) {
        case asBC_JZ:
            cc.jz(labels_[static_cast<size_t>(targetIndex)]);
            break;
        case asBC_JNZ:
            cc.jnz(labels_[static_cast<size_t>(targetIndex)]);
            break;
        case asBC_JS:
            cc.js(labels_[static_cast<size_t>(targetIndex)]);
            break;
        case asBC_JNS:
            cc.jns(labels_[static_cast<size_t>(targetIndex)]);
            break;
        case asBC_JP:
            cc.jg(labels_[static_cast<size_t>(targetIndex)]);
            break;
        case asBC_JNP:
            cc.jle(labels_[static_cast<size_t>(targetIndex)]);
            break;
        case asBC_JLowZ:
            cc.jz(labels_[static_cast<size_t>(targetIndex)]);
            break;
        case asBC_JLowNZ:
            cc.jnz(labels_[static_cast<size_t>(targetIndex)]);
            break;
        default:
            break;
        }
        return EmitResult::Success;
    }
    case asBC_JMPP: {
        x86::Gp selector = cc.new_gp32("jmpSelector");
        LoadVar(asBC_SWORDARG0(ip), selector);
        size_t table = 0;
        for (size_t j = index + 1; j < instructions_.size(); j++) {
            if (instructions_[j].op != asBC_JMP) break;
            const int targetIndex = BranchTargetIndex(
                instructions_[j], bytecode_ + instructions_[j].off);
            if (targetIndex < 0) return EmitResult::Error;
            cc.cmp(selector, Imm(int64_t(int32_t(table))));
            cc.je(labels_[static_cast<size_t>(targetIndex)]);
            table++;
        }
        if (!EmitHelperCall(instruction, ip)) return EmitResult::Error;
        return EmitResult::Success;
    }
    case asBC_RET: {
        // Pop the jit call state inline (mirrors PopJitCallState) and fall
        // into the common exit path, returning straight to the native or C++
        // caller. The helper path handles the empty/marker-frame case that
        // terminates the outermost script execution.
        Label slow = cc.new_label();
        x86::Gp ctx = cc.new_gp32("retCtx");
        x86::Gp src = cc.new_gp32("retFrame");
        x86::Gp len = cc.new_gp32("retLen");
        x86::Gp tmp = cc.new_gp32("retTmp");
        cc.mov(ctx, x86::dword_ptr(regs_, offsetof(asSVMRegisters, ctx)));
        cc.mov(len, x86::dword_ptr(ctx, kCtxCallStack + kArrayLength));
        cc.test(len, len);
        cc.jz(slow);
        cc.mov(src, x86::dword_ptr(ctx, kCtxCallStack));
        cc.lea(src, x86::dword_ptr(src, len, 2,
                                    -CALLSTACK_FRAME_SIZE * 4));
        cc.mov(tmp, x86::dword_ptr(src));
        cc.test(tmp, tmp);
        cc.jz(slow);
        cc.mov(x86::dword_ptr(
                   regs_, offsetof(asSVMRegisters, stackFramePointer)),
               tmp);
        cc.mov(tmp, x86::dword_ptr(src, 4));
        cc.mov(x86::dword_ptr(ctx, kCtxCurrentFunction), tmp);
        cc.mov(tmp, x86::dword_ptr(src, 8));
        cc.mov(x86::dword_ptr(
                   regs_, offsetof(asSVMRegisters, programPointer)),
               tmp);
        cc.mov(tmp, x86::dword_ptr(src, 12));
        cc.add(tmp, asBC_WORDARG0(ip) * 4);
        cc.mov(x86::dword_ptr(
                   regs_, offsetof(asSVMRegisters, stackPointer)),
               tmp);
        cc.mov(tmp, x86::dword_ptr(src, 16));
        cc.mov(x86::dword_ptr(ctx, kCtxStackIndex), tmp);
        cc.sub(len, CALLSTACK_FRAME_SIZE);
        cc.mov(x86::dword_ptr(ctx, kCtxCallStack + kArrayLength), len);
        cc.jmp(exitLabel_);
        cc.bind(slow);
        if (!EmitHelperCall(instruction, ip)) return EmitResult::Error;
        cc.jmp(exitLabel_);
        return EmitResult::Success;
    }
    case asBC_JitEntry:
        return EmitResult::Success;
    case asBC_SUSPEND: {
        Label process = cc.new_label();
        Label done = cc.new_label();
        cc.cmp(x86::byte_ptr(
                   regs_, offsetof(asSVMRegisters, doProcessSuspend)),
               0);
        cc.jne(process);
        cc.jmp(done);
        cc.bind(process);
        if (!EmitHelperCall(instruction, ip)) return EmitResult::Error;
        cc.bind(done);
        return EmitResult::Success;
    }
    default:
        return EmitResult::Unhandled;
    }
}

}
