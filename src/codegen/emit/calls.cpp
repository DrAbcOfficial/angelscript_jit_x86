#include "codegen/emit/emitter.h"

#include "bytecode/bc_info.h"
#include "bytecode/helpers/object_helpers.h"
#include "bytecode/helpers/runtime_helpers.h"

#include "as_objecttype.h"
#include "as_callfunc.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"

#include <cstddef>
#include <vector>

namespace asjitx86::emit {

namespace {

struct SimpleFactoryTarget {
    asCObjectType* objectType = nullptr;
    asCScriptFunction* constructor = nullptr;
};

bool IsSimpleConstructor(asCScriptFunction* function) {
    if (!function || !function->scriptData) return false;
    asUINT length = 0;
    asDWORD* bytecode = function->GetByteCode(&length);
    if (!bytecode || !length) return false;

    bool sawReturn = false;
    for (asUINT offset = 0; offset < length;) {
        asEBCInstr op =
            static_cast<asEBCInstr>(bytecode[offset] & 0xFF);
        switch (op) {
        case asBC_JitEntry:
        case asBC_SetV4:
        case asBC_LoadThisR:
        case asBC_WRTV4:
        case asBC_LoadRObjR:
        case asBC_RDR4:
        case asBC_ADDIi:
            break;
        case asBC_RET:
            if (sawReturn) return false;
            sawReturn = true;
            break;
        default:
            return false;
        }
        offset += BcSize(op);
    }
    return sawReturn;
}

bool DecodeSimpleFactory(asCScriptEngine* engine, asCScriptFunction* factory,
                         SimpleFactoryTarget& target) {
    if (!factory || !factory->scriptData || factory->objectType ||
        factory->DoesReturnOnStack())
        return false;

    asUINT length = 0;
    asDWORD* bytecode = factory->GetByteCode(&length);
    if (!bytecode || !length) return false;

    std::vector<const asDWORD*> operations;
    for (asUINT offset = 0; offset < length;) {
        asEBCInstr op =
            static_cast<asEBCInstr>(bytecode[offset] & 0xFF);
        if (op != asBC_JitEntry) operations.push_back(bytecode + offset);
        offset += BcSize(op);
    }
    if (operations.size() < 4 ||
        (*operations.front() & 0xFF) != asBC_PSF ||
        (*operations[operations.size() - 3] & 0xFF) != asBC_ALLOC ||
        (*operations[operations.size() - 2] & 0xFF) != asBC_LOADOBJ ||
        (*operations.back() & 0xFF) != asBC_RET)
        return false;

    const int localOffset = asBC_SWORDARG0(operations.front());
    if (asBC_SWORDARG0(operations[operations.size() - 2]) != localOffset)
        return false;

    int pushedArgumentDwords = 0;
    for (size_t index = 1; index + 3 < operations.size(); index++) {
        asEBCInstr op =
            static_cast<asEBCInstr>(*operations[index] & 0xFF);
        if (op == asBC_PshV4 || op == asBC_PshVPtr)
            pushedArgumentDwords += 1;
        else if (op == asBC_PshV8)
            pushedArgumentDwords += 2;
        else
            return false;
    }

    const int factoryArgumentDwords = factory->GetSpaceNeededForArguments();
    if (pushedArgumentDwords != factoryArgumentDwords ||
        asBC_WORDARG0(operations.back()) != factoryArgumentDwords)
        return false;

    const asDWORD* allocate = operations[operations.size() - 3];
    auto* objectType =
        reinterpret_cast<asCObjectType*>(asBC_PTRARG(allocate));
    if (!objectType || !(objectType->flags & asOBJ_SCRIPT_OBJECT))
        return false;
    int constructorId = -1;
    for (asUINT index = 0; index < objectType->beh.factories.GetLength();
         index++) {
        if (objectType->beh.factories[index] == factory->id) {
            constructorId = objectType->beh.constructors[index];
            break;
        }
    }
    if (constructorId < 0) return false;
    auto* constructor = engine->scriptFunctions[constructorId];
    if (!constructor ||
        constructor->GetSpaceNeededForArguments() != factoryArgumentDwords ||
        !IsSimpleConstructor(constructor))
        return false;

    target.objectType = objectType;
    target.constructor = constructor;
    return true;
}

}

EmitResult FunctionEmitter::EmitCalls(size_t index,
                                      const Instruction& instruction,
                                      const asDWORD* ip) {
    using namespace asmjit;

    constexpr uint32_t ppOff = offsetof(asSVMRegisters, programPointer);
    auto& cc = Compiler();
    switch (instruction.op) {
    case asBC_CALL: {
        auto* target = engine_->scriptFunctions[asBC_INTARG(ip)];
        SimpleFactoryTarget factoryTarget;
        if (DecodeSimpleFactory(engine_, target, factoryTarget)) {
            x86::Gp factorySp = cc.new_gp32("factorySp");
            x86::Gp object = cc.new_gp32("factoryObject");
            x86::Gp constructorFrame =
                cc.new_gp32("constructorFrame");
            x86::Gp constructorValue =
                cc.new_gp32("constructorValue");
            LoadSp(factorySp);

            InvokeNode* invocation = nullptr;
            Error err = cc.invoke(
                Out<InvokeNode*>(invocation),
                Imm(int64_t((intptr_t)&detail::CreateScriptObject)),
                FuncSignature::build<void*, asSVMRegisters*,
                                     asCObjectType*>());
            if (err != kErrorOk) return EmitResult::Error;
            invocation->set_arg(0, regs_);
            invocation->set_arg(
                1, Imm(int64_t((intptr_t)factoryTarget.objectType)));
            invocation->set_ret(0, object);

            cc.mov(constructorFrame, factorySp);
            cc.sub(constructorFrame, AS_PTR_SIZE * 4);
            cc.mov(x86::dword_ptr(constructorFrame), object);

            asUINT constructorLength = 0;
            asDWORD* constructorBytecode =
                factoryTarget.constructor->GetByteCode(&constructorLength);
            for (asUINT constructorOffset = 0;
                 constructorOffset < constructorLength;) {
                const asDWORD* constructorIp =
                    constructorBytecode + constructorOffset;
                asEBCInstr constructorOp =
                    static_cast<asEBCInstr>(*constructorIp & 0xFF);
                if (constructorOp == asBC_RET) break;
                switch (constructorOp) {
                case asBC_JitEntry:
                    break;
                case asBC_SetV4: {
                    const int destination =
                        asBC_SWORDARG0(constructorIp);
                    cc.mov(
                        x86::dword_ptr(constructorFrame,
                                       -destination * 4),
                        Imm(int64_t(static_cast<int32_t>(
                            asBC_DWORDARG(constructorIp)))));
                    break;
                }
                case asBC_LoadThisR:
                    cc.mov(constructorValue,
                           x86::dword_ptr(constructorFrame));
                    cc.add(constructorValue,
                           asBC_SWORDARG0(constructorIp));
                    break;
                case asBC_WRTV4: {
                    const int source = asBC_SWORDARG0(constructorIp);
                    x86::Gp value =
                        cc.new_gp32("constructorStore");
                    cc.mov(value,
                           x86::dword_ptr(constructorFrame,
                                          -source * 4));
                    cc.mov(x86::dword_ptr(constructorValue), value);
                    break;
                }
                case asBC_LoadRObjR: {
                    const int source = asBC_SWORDARG0(constructorIp);
                    cc.mov(constructorValue,
                           x86::dword_ptr(constructorFrame,
                                          -source * 4));
                    cc.add(constructorValue,
                           asBC_SWORDARG1(constructorIp));
                    break;
                }
                case asBC_RDR4: {
                    const int destination =
                        asBC_SWORDARG0(constructorIp);
                    x86::Gp value =
                        cc.new_gp32("constructorLoad");
                    cc.mov(value, x86::dword_ptr(constructorValue));
                    cc.mov(x86::dword_ptr(constructorFrame,
                                          -destination * 4),
                           value);
                    break;
                }
                case asBC_ADDIi: {
                    const int destination =
                        asBC_SWORDARG0(constructorIp);
                    const int source = asBC_SWORDARG1(constructorIp);
                    x86::Gp value =
                        cc.new_gp32("constructorAdd");
                    cc.mov(value,
                           x86::dword_ptr(constructorFrame,
                                          -source * 4));
                    cc.add(value, asBC_INTARG(constructorIp + 1));
                    cc.mov(x86::dword_ptr(constructorFrame,
                                          -destination * 4),
                           value);
                    break;
                }
                default:
                    return EmitResult::Error;
                }
                constructorOffset += BcSize(constructorOp);
            }

            const int argumentBytes =
                target->GetSpaceNeededForArguments() * 4;
            if (argumentBytes) cc.add(factorySp, argumentBytes);
            StoreSp(factorySp);
            cc.mov(x86::dword_ptr(
                       regs_, offsetof(asSVMRegisters, objectRegister)),
                   object);
            cc.mov(x86::dword_ptr(
                       regs_, offsetof(asSVMRegisters, objectType)),
                   0);
            cc.mov(x86::dword_ptr(regs_, ppOff),
                   Imm(int64_t((intptr_t)(ip + instruction.size))));
            return EmitResult::Success;
        }

        InvokeNode* invocation = nullptr;
        Error err = cc.invoke(
            Out<InvokeNode*>(invocation),
            Imm(int64_t((intptr_t)&detail::CallScriptFunction)),
            FuncSignature::build<int, asSVMRegisters*, asCScriptFunction*,
                                 const asDWORD*>());
        if (err != kErrorOk) return EmitResult::Error;
        x86::Gp result = cc.new_gp32("result");
        invocation->set_arg(0, regs_);
        invocation->set_arg(1, Imm(int64_t((intptr_t)target)));
        invocation->set_arg(
            2, Imm(int64_t((intptr_t)(ip + instruction.size))));
        invocation->set_ret(0, result);
        cc.test(result, result);
        cc.jnz(exitLabel_);
        return EmitResult::Success;
    }
    case asBC_CALLSYS: {
        x86::Gp context = cc.new_gp32("context");
        x86::Gp popDwords = cc.new_gp32("popDwords");
        cc.mov(x86::dword_ptr(regs_, ppOff),
               Imm(int64_t((intptr_t)ip)));
        cc.mov(context,
               x86::dword_ptr(regs_, offsetof(asSVMRegisters, ctx)));

        InvokeNode* invocation = nullptr;
        Error err = cc.invoke(
            Out<InvokeNode*>(invocation),
            Imm(int64_t((intptr_t)&CallSystemFunction)),
            FuncSignature::build<int, int, asCContext*>());
        if (err != kErrorOk) return EmitResult::Error;
        invocation->set_arg(0, asBC_INTARG(ip));
        invocation->set_arg(1, context);
        invocation->set_ret(0, popDwords);

        x86::Gp sp = cc.new_gp32("sp");
        LoadSp(sp);
        cc.shl(popDwords, 2);
        cc.add(sp, popDwords);
        StoreSp(sp);
        cc.mov(x86::dword_ptr(regs_, ppOff),
               Imm(int64_t((intptr_t)(ip + instruction.size))));

        Label done = cc.new_label();
        cc.cmp(x86::byte_ptr(
                   regs_, offsetof(asSVMRegisters, doProcessSuspend)),
               0);
        cc.je(done);
        InvokeNode* finish = nullptr;
        const int catchTarget = localCatchTarget_[index];
        if (catchTarget >= 0) {
            err = cc.invoke(
                Out<InvokeNode*>(finish),
                Imm(int64_t((intptr_t)&detail::FinishSystemCallAt)),
                FuncSignature::build<int, asSVMRegisters*,
                                     asCScriptFunction*, const asDWORD*>());
        } else {
            err = cc.invoke(
                Out<InvokeNode*>(finish),
                Imm(int64_t((intptr_t)&detail::FinishSystemCall)),
                FuncSignature::build<int, asSVMRegisters*>());
        }
        if (err != kErrorOk) return EmitResult::Error;
        x86::Gp result = cc.new_gp32("systemCallResult");
        finish->set_arg(0, regs_);
        if (catchTarget >= 0) {
            const asDWORD* catchBc = bytecode_ +
                instructions_[static_cast<size_t>(catchTarget)].off;
            finish->set_arg(
                1, Imm(int64_t((intptr_t)scriptFunction_)));
            finish->set_arg(2, Imm(int64_t((intptr_t)catchBc)));
        }
        finish->set_ret(0, result);
        if (catchTarget >= 0) {
            cc.cmp(result, detail::kJitBcCaught);
            cc.je(labels_[static_cast<size_t>(catchTarget)]);
        }
        cc.test(result, result);
        cc.jnz(exitLabel_);
        cc.bind(done);
        return EmitResult::Success;
    }
    default:
        return EmitResult::Unhandled;
    }
}

}
