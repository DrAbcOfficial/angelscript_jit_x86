#include "codegen/emit/emitter.h"

#include <cstddef>

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
