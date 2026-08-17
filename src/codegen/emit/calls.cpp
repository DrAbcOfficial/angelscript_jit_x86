#include "codegen/emit/emitter.h"

#include "bytecode/bc_info.h"
#include "bytecode/helpers/object_helpers.h"
#include "bytecode/helpers/runtime_helpers.h"

#include "as_objecttype.h"
#include "as_callfunc.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"
#include "as_scriptobject.h"

#include <cstddef>
#include <vector>

namespace asjitx86::emit {

namespace {

struct SimpleFactoryTarget {
    asCObjectType* objectType = nullptr;
    asCScriptFunction* constructor = nullptr;
};

struct InlineScriptBody {
    std::vector<const asDWORD*> instructions;
    std::vector<SimpleFactoryTarget> factories;
    std::vector<int> indexOfOffset;
    int returnDwords = -1;
};

bool DecodeSimpleFactory(asCScriptEngine* engine, asCScriptFunction* factory,
                         SimpleFactoryTarget& target);

bool IsInlineScriptOp(asEBCInstr op) {
    switch (op) {
    case asBC_JitEntry:
    case asBC_SetV4:
    case asBC_SetV8:
    case asBC_CpyVtoV4:
    case asBC_CpyVtoR4:
    case asBC_CpyVtoR8:
    case asBC_CpyRtoV4:
    case asBC_CpyVtoG4:
    case asBC_CpyGtoV4:
    case asBC_PshV4:
    case asBC_PshVPtr:
    case asBC_PopPtr:
    case asBC_PopRPtr:
    case asBC_LoadThisR:
    case asBC_LoadRObjR:
    case asBC_RDR4:
    case asBC_WRTV4:
    case asBC_INCi:
    case asBC_DECi:
    case asBC_ADDi:
    case asBC_SUBi:
    case asBC_MULi:
    case asBC_ADDf:
    case asBC_SUBf:
    case asBC_MULf:
    case asBC_ADDd:
    case asBC_SUBd:
    case asBC_MULd:
    case asBC_ADDIi:
    case asBC_SUBIi:
    case asBC_MULIi:
    case asBC_ADDIf:
    case asBC_SUBIf:
    case asBC_MULIf:
    case asBC_NEGi:
    case asBC_BNOT:
    case asBC_CMPi:
    case asBC_CMPIi:
    case asBC_TZ:
    case asBC_TNZ:
    case asBC_TS:
    case asBC_TNS:
    case asBC_TP:
    case asBC_TNP:
    case asBC_JMP:
    case asBC_JZ:
    case asBC_JNZ:
    case asBC_JS:
    case asBC_JNS:
    case asBC_JP:
    case asBC_JNP:
    case asBC_JLowZ:
    case asBC_JLowNZ:
    case asBC_CALL:
    case asBC_STOREOBJ:
    case asBC_LOADOBJ:
    case asBC_ChkNullV:
    case asBC_RefCpyV:
    case asBC_CallPtr:
    case asBC_FREE:
    case asBC_RET:
        return true;
    default:
        return false;
    }
}

bool IsInlineBranch(asEBCInstr op) {
    switch (op) {
    case asBC_JMP:
    case asBC_JZ:
    case asBC_JNZ:
    case asBC_JS:
    case asBC_JNS:
    case asBC_JP:
    case asBC_JNP:
    case asBC_JLowZ:
    case asBC_JLowNZ:
        return true;
    default:
        return false;
    }
}

bool DecodeInlineScriptBody(asCScriptFunction* function,
                            InlineScriptBody& body) {
    if (!function || function->funcType != asFUNC_SCRIPT ||
        !function->scriptData || function->DoesReturnOnStack() ||
        function->scriptData->tryCatchInfo.GetLength() ||
        function->scriptData->variableSpace > 64)
        return false;
    asUINT length = 0;
    asDWORD* bytecode = function->GetByteCode(&length);
    if (!bytecode || !length) return false;
    body.indexOfOffset.assign(length, -1);
    bool pendingFactoryObject = false;
    int factoryObjectLocal = -1;
    for (asUINT offset = 0; offset < length;) {
        const asDWORD* instruction = bytecode + offset;
        const asEBCInstr op =
            static_cast<asEBCInstr>(*instruction & 0xFF);
        const int size = BcSize(op);
        if (size <= 0 || !IsInlineScriptOp(op) ||
            body.instructions.size() >= 32)
            return false;
        body.indexOfOffset[offset] =
            static_cast<int>(body.instructions.size());
        body.instructions.push_back(instruction);
        SimpleFactoryTarget factory;
        if (op == asBC_CALL) {
            auto* called = function->engine->scriptFunctions[
                asBC_INTARG(instruction)];
            if (!DecodeSimpleFactory(function->engine, called, factory))
                return false;
            if (pendingFactoryObject || factoryObjectLocal >= 0)
                return false;
            pendingFactoryObject = true;
        } else if (op == asBC_STOREOBJ) {
            if (!pendingFactoryObject) return false;
            pendingFactoryObject = false;
            factoryObjectLocal = asBC_SWORDARG0(instruction);
        } else if (op == asBC_ChkNullV) {
            if (pendingFactoryObject || factoryObjectLocal < 0 ||
                asBC_SWORDARG0(instruction) != factoryObjectLocal)
                return false;
        } else if (op == asBC_LOADOBJ) {
            if (pendingFactoryObject) return false;
            if (factoryObjectLocal >= 0) {
                if (asBC_SWORDARG0(instruction) != factoryObjectLocal)
                    return false;
                factoryObjectLocal = -1;
            }
        } else if (op == asBC_FREE) {
            auto* objectType = reinterpret_cast<asCObjectType*>(
                asBC_PTRARG(instruction));
            if (!objectType ||
                (!(objectType->flags & asOBJ_FUNCDEF) &&
                 objectType != &function->engine->functionBehaviours))
                return false;
        } else if (pendingFactoryObject && op != asBC_JitEntry) {
            return false;
        }
        body.factories.push_back(factory);
        if (op == asBC_RET) {
            const int returnDwords = asBC_WORDARG0(instruction);
            if (body.returnDwords >= 0 &&
                body.returnDwords != returnDwords)
                return false;
            body.returnDwords = returnDwords;
        }
        offset += static_cast<asUINT>(size);
        if (offset > length) return false;
    }
    const int expectedReturnDwords =
        function->GetSpaceNeededForArguments() +
        (function->objectType ? AS_PTR_SIZE : 0) +
        (function->DoesReturnOnStack() ? AS_PTR_SIZE : 0);
    if (body.returnDwords != expectedReturnDwords) return false;
    if (pendingFactoryObject || factoryObjectLocal >= 0) return false;

    for (size_t index = 0; index < body.instructions.size(); index++) {
        const asEBCInstr op = static_cast<asEBCInstr>(
            *body.instructions[index] & 0xFF);
        if (op == asBC_LOADOBJ) {
            const int local = asBC_SWORDARG0(body.instructions[index]);
            const bool factoryLoad =
                index > 0 &&
                static_cast<asEBCInstr>(
                    *body.instructions[index - 1] & 0xFF) == asBC_ChkNullV &&
                asBC_SWORDARG0(body.instructions[index - 1]) == local;
            const bool handleLoad =
                index >= 3 &&
                static_cast<asEBCInstr>(
                    *body.instructions[index - 3] & 0xFF) == asBC_PshVPtr &&
                static_cast<asEBCInstr>(
                    *body.instructions[index - 2] & 0xFF) == asBC_RefCpyV &&
                static_cast<asEBCInstr>(
                    *body.instructions[index - 1] & 0xFF) == asBC_PopPtr;
            if (!factoryLoad && !handleLoad) return false;
        }
        if (op != asBC_PshVPtr && op != asBC_RefCpyV &&
            op != asBC_PopPtr && op != asBC_PopRPtr)
            continue;
        if (op == asBC_PshVPtr && index + 1 < body.instructions.size() &&
            static_cast<asEBCInstr>(
                *body.instructions[index + 1] & 0xFF) == asBC_PopRPtr) {
            index += 1;
            continue;
        }
        if (index + 3 >= body.instructions.size() ||
            static_cast<asEBCInstr>(*body.instructions[index] & 0xFF) !=
                asBC_PshVPtr ||
            static_cast<asEBCInstr>(*body.instructions[index + 1] & 0xFF) !=
                asBC_RefCpyV ||
            static_cast<asEBCInstr>(*body.instructions[index + 2] & 0xFF) !=
                asBC_PopPtr ||
            static_cast<asEBCInstr>(*body.instructions[index + 3] & 0xFF) !=
                asBC_LOADOBJ)
            return false;
        const asDWORD* copy = body.instructions[index + 1];
        auto* objectType =
            reinterpret_cast<asCObjectType*>(asBC_PTRARG(copy));
        const int returnLocal = asBC_SWORDARG0(copy);
        if (!objectType || !(objectType->flags & asOBJ_SCRIPT_OBJECT) ||
            returnLocal != int(function->scriptData->variableSpace) ||
            asBC_SWORDARG0(body.instructions[index + 3]) != returnLocal)
            return false;
        index += 3;
    }

    for (const asDWORD* ip : body.instructions) {
        const asEBCInstr op = static_cast<asEBCInstr>(*ip & 0xFF);
        if (!IsInlineBranch(op)) continue;
        const ptrdiff_t offset = ip - bytecode;
        const int64_t target = int64_t(offset) + 2 + asBC_INTARG(ip);
        if (target < 0 || target >= int64_t(length) ||
            body.indexOfOffset[static_cast<size_t>(target)] < 0)
            return false;
    }
    return true;
}

bool MatchesVirtualMethod(void* object, int slot,
                          asCScriptFunction* expected) {
    if (!object || slot < 0) return false;
    auto* objectType = static_cast<asCObjectType*>(
        static_cast<asCScriptObject*>(object)
            ->asCScriptObject::GetObjectType());
    if (objectType == expected->objectType) return true;
    return asUINT(slot) < objectType->virtualFunctionTable.GetLength() &&
           objectType->virtualFunctionTable[slot] == expected;
}

void AddRefScriptObject(void* object) {
    static_cast<asCScriptObject*>(object)->asCScriptObject::AddRef();
}

bool IsSimpleConstructor(asCScriptFunction* function) {
    if (!function || function->funcType != asFUNC_SCRIPT ||
        !function->scriptData || !function->objectType ||
        function->DoesReturnOnStack() ||
        function->scriptData->tryCatchInfo.GetLength() ||
        function->scriptData->variableSpace > 64)
        return false;
    asUINT length = 0;
    asDWORD* bytecode = function->GetByteCode(&length);
    if (!bytecode || !length) return false;

    bool sawReturn = false;
    unsigned operationCount = 0;
    for (asUINT offset = 0; offset < length;) {
        const asDWORD* instruction = bytecode + offset;
        asEBCInstr op = static_cast<asEBCInstr>(*instruction & 0xFF);
        if (sawReturn || operationCount++ >= 16) return false;
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
            if (asBC_WORDARG0(instruction) !=
                function->GetSpaceNeededForArguments() + AS_PTR_SIZE)
                return false;
            sawReturn = true;
            break;
        default:
            return false;
        }
        offset += BcSize(op);
    }
    return sawReturn;
}

bool EmitSimpleConstructorBody(asmjit::x86::Compiler& cc,
                               asCScriptFunction* constructor,
                               const asmjit::x86::Gp& frame) {
    using namespace asmjit;

    asUINT length = 0;
    asDWORD* bytecode = constructor->GetByteCode(&length);
    if (!bytecode || !length) return false;
    std::vector<x86::Gp> locals(
        static_cast<size_t>(constructor->scriptData->variableSpace) + 1);
    for (size_t local = 1; local < locals.size(); local++) {
        locals[local] = cc.new_gp32("constructorLocal");
        cc.xor_(locals[local], locals[local]);
    }
    auto loadValue = [&](int offset, const x86::Gp& destination) {
        if (offset > 0 && size_t(offset) < locals.size())
            cc.mov(destination, locals[static_cast<size_t>(offset)]);
        else
            cc.mov(destination, x86::dword_ptr(frame, -offset * 4));
    };
    auto storeValue = [&](int offset, const x86::Gp& source) {
        if (offset > 0 && size_t(offset) < locals.size())
            cc.mov(locals[static_cast<size_t>(offset)], source);
        else
            cc.mov(x86::dword_ptr(frame, -offset * 4), source);
    };
    x86::Gp value = cc.new_gp32("constructorValue");
    for (asUINT offset = 0; offset < length;) {
        const asDWORD* ip = bytecode + offset;
        const asEBCInstr op = static_cast<asEBCInstr>(*ip & 0xFF);
        if (op == asBC_RET) return true;
        switch (op) {
        case asBC_JitEntry:
            break;
        case asBC_SetV4: {
            const int destination = asBC_SWORDARG0(ip);
            x86::Gp immediate = cc.new_gp32("constructorImmediate");
            cc.mov(immediate,
                   Imm(int64_t(static_cast<int32_t>(asBC_DWORDARG(ip)))));
            storeValue(destination, immediate);
            break;
        }
        case asBC_LoadThisR:
            cc.mov(value, x86::dword_ptr(frame));
            cc.add(value, asBC_SWORDARG0(ip));
            break;
        case asBC_WRTV4: {
            x86::Gp source = cc.new_gp32("constructorStore");
            loadValue(asBC_SWORDARG0(ip), source);
            cc.mov(x86::dword_ptr(value), source);
            break;
        }
        case asBC_LoadRObjR:
            loadValue(asBC_SWORDARG0(ip), value);
            cc.add(value, asBC_SWORDARG1(ip));
            break;
        case asBC_RDR4: {
            x86::Gp loaded = cc.new_gp32("constructorLoad");
            cc.mov(loaded, x86::dword_ptr(value));
            storeValue(asBC_SWORDARG0(ip), loaded);
            break;
        }
        case asBC_ADDIi: {
            x86::Gp added = cc.new_gp32("constructorAdd");
            loadValue(asBC_SWORDARG1(ip), added);
            cc.add(added, asBC_INTARG(ip + 1));
            storeValue(asBC_SWORDARG0(ip), added);
            break;
        }
        default:
            return false;
        }
        offset += BcSize(op);
    }
    return false;
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

    auto emitCreateScriptObject = [&](asCObjectType* objectType,
                                      const x86::Gp& object) -> bool {
        InvokeNode* invocation = nullptr;
        Error err;
        asDWORD* destructorGlobal = nullptr;
        int destructorDelta = 0;
        const bool pooledGlobalDestructor =
            detail::DecodePooledGlobalDestructor(
                objectType, destructorGlobal, destructorDelta);
        if (detail::IsScalarOnlyScriptObject(objectType) ||
            pooledGlobalDestructor) {
            auto* bucket = objectPool_.GetBucket(objectType);
            err = cc.invoke(
                Out<InvokeNode*>(invocation),
                Imm(int64_t((intptr_t)&detail::CreatePooledScriptObject)),
                FuncSignature::build<void*,
                                     detail::ScalarObjectPoolBucket*>());
            if (err == kErrorOk)
                invocation->set_arg(
                    0, Imm(int64_t((intptr_t)bucket)));
        } else {
            err = cc.invoke(
                Out<InvokeNode*>(invocation),
                Imm(int64_t((intptr_t)&detail::CreateScriptObject)),
                FuncSignature::build<void*, asSVMRegisters*,
                                     asCObjectType*>());
            if (err == kErrorOk) {
                invocation->set_arg(0, regs_);
                invocation->set_arg(
                    1, Imm(int64_t((intptr_t)objectType)));
            }
        }
        if (err != kErrorOk) return false;
        invocation->set_ret(0, object);
        return true;
    };

    auto emitSimpleFactory = [&](asCScriptFunction* factory,
                                 const SimpleFactoryTarget& factoryTarget)
        -> bool {
        x86::Gp factorySp = cc.new_gp32("factorySp");
        x86::Gp object = cc.new_gp32("factoryObject");
        x86::Gp constructorFrame = cc.new_gp32("constructorFrame");
        LoadSp(factorySp);
        cc.mov(x86::dword_ptr(regs_, ppOff),
               Imm(int64_t((intptr_t)ip)));

        if (!emitCreateScriptObject(factoryTarget.objectType, object))
            return false;

        cc.mov(constructorFrame, factorySp);
        cc.sub(constructorFrame, AS_PTR_SIZE * 4);
        cc.mov(x86::dword_ptr(constructorFrame), object);

        if (!EmitSimpleConstructorBody(cc, factoryTarget.constructor,
                                       constructorFrame))
            return false;

        const int argumentBytes = factory->GetSpaceNeededForArguments() * 4;
        if (argumentBytes) cc.add(factorySp, argumentBytes);
        StoreSp(factorySp);
        cc.mov(x86::dword_ptr(
                   regs_, offsetof(asSVMRegisters, objectRegister)),
               object);
        cc.mov(x86::dword_ptr(
                   regs_, offsetof(asSVMRegisters, objectType)),
               0);
        return true;
    };

    auto emitInlineBody = [&](asCScriptFunction* target,
                              const InlineScriptBody& body,
                              const Label& slow,
                              const Label& fastDone) -> bool {
        x86::Gp callFrame = cc.new_gp32("inlineFrame");
        LoadSp(callFrame);

        std::vector<x86::Gp> locals(
            static_cast<size_t>(target->scriptData->variableSpace) + 1);
        for (size_t local = 1; local < locals.size(); local++) {
            locals[local] = cc.new_gp32("inlineLocal");
            cc.xor_(locals[local], locals[local]);
        }

        auto loadValue = [&](int offset, const x86::Gp& destination) {
            if (offset > 0 && size_t(offset) < locals.size())
                cc.mov(destination, locals[static_cast<size_t>(offset)]);
            else
                cc.mov(destination,
                       x86::dword_ptr(callFrame, -offset * 4));
        };
        auto storeValue = [&](int offset, const x86::Gp& source) {
            if (offset > 0 && size_t(offset) < locals.size())
                cc.mov(locals[static_cast<size_t>(offset)], source);
            else
                cc.mov(x86::dword_ptr(callFrame, -offset * 4), source);
        };
        auto loadValue64 = [&](int offset, const x86::Vec& destination) {
            if (offset > 1 && size_t(offset) < locals.size()) {
                x86::Vec high = cc.new_xmm("inlineHigh64");
                if (useAvx_) {
                    cc.vmovd(destination, locals[static_cast<size_t>(offset)]);
                    cc.vmovd(high, locals[static_cast<size_t>(offset - 1)]);
                    cc.vpsllq(high, high, 32);
                    cc.vpor(destination, destination, high);
                } else {
                    cc.movd(destination, locals[static_cast<size_t>(offset)]);
                    cc.movd(high, locals[static_cast<size_t>(offset - 1)]);
                    cc.psllq(high, 32);
                    cc.por(destination, high);
                }
            } else {
                if (useAvx_)
                    cc.vmovq(destination,
                            x86::qword_ptr(callFrame, -offset * 4));
                else
                    cc.movq(destination,
                            x86::qword_ptr(callFrame, -offset * 4));
            }
        };
        auto storeValue64 = [&](int offset, const x86::Vec& source) {
            if (offset > 1 && size_t(offset) < locals.size()) {
                x86::Vec high = cc.new_xmm("inlineHigh64");
                if (useAvx_) {
                    cc.vmovd(locals[static_cast<size_t>(offset)], source);
                    cc.vmovq(high, source);
                    cc.vpsrlq(high, high, 32);
                    cc.vmovd(locals[static_cast<size_t>(offset - 1)], high);
                } else {
                    cc.movd(locals[static_cast<size_t>(offset)], source);
                    cc.movq(high, source);
                    cc.psrlq(high, 32);
                    cc.movd(locals[static_cast<size_t>(offset - 1)], high);
                }
            } else {
                if (useAvx_)
                    cc.vmovq(x86::qword_ptr(callFrame, -offset * 4), source);
                else
                    cc.movq(x86::qword_ptr(callFrame, -offset * 4), source);
            }
        };

        std::vector<Label> labels(body.instructions.size());
        for (Label& label : labels) label = cc.new_label();
        asDWORD* targetBytecode = target->scriptData->byteCode.AddressOf();
        auto branchTarget = [&](const asDWORD* branchIp) -> int {
            const int64_t targetOffset =
                int64_t(branchIp - targetBytecode) + 2 +
                asBC_INTARG(branchIp);
            if (targetOffset < 0 ||
                targetOffset >= int64_t(body.indexOfOffset.size()))
                return -1;
            return body.indexOfOffset[static_cast<size_t>(targetOffset)];
        };

        for (size_t bodyIndex = 0; bodyIndex < body.instructions.size();
             bodyIndex++) {
            cc.bind(labels[bodyIndex]);
            const asDWORD* bodyIp = body.instructions[bodyIndex];
            const asEBCInstr op =
                static_cast<asEBCInstr>(*bodyIp & 0xFF);
            switch (op) {
            case asBC_JitEntry:
                break;
            case asBC_SetV4: {
                x86::Gp value = cc.new_gp32("inlineValue");
                cc.mov(value, Imm(int64_t(
                                  static_cast<int32_t>(
                                      asBC_DWORDARG(bodyIp)))));
                storeValue(asBC_SWORDARG0(bodyIp), value);
                break;
            }
            case asBC_SetV8: {
                const int destination = asBC_SWORDARG0(bodyIp);
                const asQWORD immediate = asBC_QWORDARG(bodyIp);
                x86::Gp low = cc.new_gp32("inlineLow64");
                x86::Gp high = cc.new_gp32("inlineHigh64");
                cc.mov(low, Imm(int64_t(static_cast<int32_t>(
                                static_cast<asDWORD>(immediate)))));
                cc.mov(high, Imm(int64_t(static_cast<int32_t>(
                                 static_cast<asDWORD>(immediate >> 32)))));
                storeValue(destination, low);
                storeValue(destination - 1, high);
                break;
            }
            case asBC_CpyVtoV4: {
                x86::Gp value = cc.new_gp32("inlineValue");
                loadValue(asBC_SWORDARG1(bodyIp), value);
                storeValue(asBC_SWORDARG0(bodyIp), value);
                break;
            }
            case asBC_CpyVtoR4: {
                x86::Gp value = cc.new_gp32("inlineValue");
                loadValue(asBC_SWORDARG0(bodyIp), value);
                cc.mov(x86::dword_ptr(
                           regs_, offsetof(asSVMRegisters, valueRegister)),
                       value);
                break;
            }
            case asBC_CpyVtoR8: {
                x86::Vec value = cc.new_xmm("inlineValue64");
                loadValue64(asBC_SWORDARG0(bodyIp), value);
                if (useAvx_)
                    cc.vmovq(x86::qword_ptr(
                                regs_, offsetof(asSVMRegisters, valueRegister)),
                            value);
                else
                    cc.movq(x86::qword_ptr(
                                regs_, offsetof(asSVMRegisters, valueRegister)),
                            value);
                break;
            }
            case asBC_CpyRtoV4: {
                x86::Gp value = cc.new_gp32("inlineValue");
                cc.mov(value,
                       x86::dword_ptr(
                           regs_, offsetof(asSVMRegisters, valueRegister)));
                storeValue(asBC_SWORDARG0(bodyIp), value);
                break;
            }
            case asBC_CpyVtoG4: {
                x86::Gp value = cc.new_gp32("inlineGlobalValue");
                x86::Gp address = cc.new_gp32("inlineGlobalAddress");
                loadValue(asBC_SWORDARG0(bodyIp), value);
                cc.mov(address,
                       Imm(int64_t((intptr_t)asBC_PTRARG(bodyIp))));
                cc.mov(x86::dword_ptr(address), value);
                break;
            }
            case asBC_CpyGtoV4: {
                x86::Gp value = cc.new_gp32("inlineGlobalValue");
                x86::Gp address = cc.new_gp32("inlineGlobalAddress");
                cc.mov(address,
                       Imm(int64_t((intptr_t)asBC_PTRARG(bodyIp))));
                cc.mov(value, x86::dword_ptr(address));
                storeValue(asBC_SWORDARG0(bodyIp), value);
                break;
            }
            case asBC_PshV4: {
                x86::Gp sp = cc.new_gp32("inlinePushSp");
                x86::Gp value = cc.new_gp32("inlinePushValue");
                LoadSp(sp);
                cc.sub(sp, 4);
                loadValue(asBC_SWORDARG0(bodyIp), value);
                cc.mov(x86::dword_ptr(sp), value);
                StoreSp(sp);
                break;
            }
            case asBC_PshVPtr: {
                x86::Gp sp = cc.new_gp32("inlinePushPtrSp");
                x86::Gp value = cc.new_gp32("inlinePushPtrValue");
                LoadSp(sp);
                cc.sub(sp, AS_PTR_SIZE * 4);
                loadValue(asBC_SWORDARG0(bodyIp), value);
                cc.mov(x86::dword_ptr(sp), value);
                StoreSp(sp);
                break;
            }
            case asBC_PopPtr: {
                x86::Gp sp = cc.new_gp32("inlinePopPtrSp");
                LoadSp(sp);
                cc.add(sp, AS_PTR_SIZE * 4);
                StoreSp(sp);
                break;
            }
            case asBC_PopRPtr: {
                x86::Gp sp = cc.new_gp32("inlinePopRPtrSp");
                x86::Gp value = cc.new_gp32("inlinePopRPtrValue");
                LoadSp(sp);
                cc.mov(value, x86::dword_ptr(sp));
                cc.add(sp, AS_PTR_SIZE * 4);
                StoreSp(sp);
                cc.mov(x86::dword_ptr(
                           regs_, offsetof(asSVMRegisters, valueRegister)),
                       value);
                break;
            }
            case asBC_LoadThisR:
            case asBC_LoadRObjR: {
                const int source = op == asBC_LoadThisR
                                       ? 0
                                       : asBC_SWORDARG0(bodyIp);
                const int propertyOffset = op == asBC_LoadThisR
                                               ? asBC_SWORDARG0(bodyIp)
                                               : asBC_SWORDARG1(bodyIp);
                x86::Gp object = cc.new_gp32("inlineObject");
                loadValue(source, object);
                cc.test(object, object);
                cc.jz(slow);
                cc.add(object, propertyOffset);
                cc.mov(x86::dword_ptr(
                           regs_, offsetof(asSVMRegisters, valueRegister)),
                       object);
                break;
            }
            case asBC_RDR4: {
                x86::Gp address = cc.new_gp32("inlineAddress");
                x86::Gp value = cc.new_gp32("inlineValue");
                cc.mov(address,
                       x86::dword_ptr(
                           regs_, offsetof(asSVMRegisters, valueRegister)));
                cc.mov(value, x86::dword_ptr(address));
                storeValue(asBC_SWORDARG0(bodyIp), value);
                break;
            }
            case asBC_WRTV4: {
                x86::Gp address = cc.new_gp32("inlineWriteAddress");
                x86::Gp value = cc.new_gp32("inlineWriteValue");
                cc.mov(address,
                       x86::dword_ptr(
                           regs_, offsetof(asSVMRegisters, valueRegister)));
                loadValue(asBC_SWORDARG0(bodyIp), value);
                cc.mov(x86::dword_ptr(address), value);
                break;
            }
            case asBC_INCi:
            case asBC_DECi: {
                x86::Gp address = cc.new_gp32("inlineIncAddress");
                cc.mov(address,
                       x86::dword_ptr(
                           regs_, offsetof(asSVMRegisters, valueRegister)));
                if (op == asBC_INCi)
                    cc.inc(x86::dword_ptr(address));
                else
                    cc.dec(x86::dword_ptr(address));
                break;
            }
            case asBC_ADDi:
            case asBC_SUBi:
            case asBC_MULi: {
                x86::Gp left = cc.new_gp32("inlineLeft");
                x86::Gp right = cc.new_gp32("inlineRight");
                loadValue(asBC_SWORDARG1(bodyIp), left);
                loadValue(asBC_SWORDARG2(bodyIp), right);
                if (op == asBC_ADDi)
                    cc.add(left, right);
                else if (op == asBC_SUBi)
                    cc.sub(left, right);
                else
                    cc.imul(left, right);
                storeValue(asBC_SWORDARG0(bodyIp), left);
                break;
            }
            case asBC_ADDf:
            case asBC_SUBf:
            case asBC_MULf: {
                x86::Gp leftBits = cc.new_gp32("inlineFloatLeftBits");
                x86::Gp rightBits = cc.new_gp32("inlineFloatRightBits");
                x86::Gp resultBits = cc.new_gp32("inlineFloatResultBits");
                x86::Vec value = cc.new_xmm_ss("inlineFloatValue");
                x86::Vec operand = cc.new_xmm_ss("inlineFloatOperand");
                loadValue(asBC_SWORDARG1(bodyIp), leftBits);
                loadValue(asBC_SWORDARG2(bodyIp), rightBits);
                if (useAvx_) {
                    cc.vmovd(value, leftBits);
                    cc.vmovd(operand, rightBits);
                    if (op == asBC_ADDf)
                        cc.vaddss(value, value, operand);
                    else if (op == asBC_SUBf)
                        cc.vsubss(value, value, operand);
                    else
                        cc.vmulss(value, value, operand);
                    cc.vmovd(resultBits, value);
                } else {
                    cc.movd(value, leftBits);
                    cc.movd(operand, rightBits);
                    if (op == asBC_ADDf)
                        cc.addss(value, operand);
                    else if (op == asBC_SUBf)
                        cc.subss(value, operand);
                    else
                        cc.mulss(value, operand);
                    cc.movd(resultBits, value);
                }
                storeValue(asBC_SWORDARG0(bodyIp), resultBits);
                break;
            }
            case asBC_ADDd:
            case asBC_SUBd:
            case asBC_MULd: {
                x86::Vec value = cc.new_xmm_sd("inlineDoubleValue");
                x86::Vec operand = cc.new_xmm_sd("inlineDoubleOperand");
                loadValue64(asBC_SWORDARG1(bodyIp), value);
                loadValue64(asBC_SWORDARG2(bodyIp), operand);
                if (useAvx_) {
                    if (op == asBC_ADDd)
                        cc.vaddsd(value, value, operand);
                    else if (op == asBC_SUBd)
                        cc.vsubsd(value, value, operand);
                    else
                        cc.vmulsd(value, value, operand);
                } else if (op == asBC_ADDd) {
                    cc.addsd(value, operand);
                } else if (op == asBC_SUBd) {
                    cc.subsd(value, operand);
                } else {
                    cc.mulsd(value, operand);
                }
                storeValue64(asBC_SWORDARG0(bodyIp), value);
                break;
            }
            case asBC_ADDIi:
            case asBC_SUBIi:
            case asBC_MULIi: {
                x86::Gp value = cc.new_gp32("inlineValue");
                loadValue(asBC_SWORDARG1(bodyIp), value);
                const int32_t immediate = asBC_INTARG(bodyIp + 1);
                if (op == asBC_ADDIi)
                    cc.add(value, immediate);
                else if (op == asBC_SUBIi)
                    cc.sub(value, immediate);
                else
                    cc.imul(value, value, immediate);
                storeValue(asBC_SWORDARG0(bodyIp), value);
                break;
            }
            case asBC_ADDIf:
            case asBC_SUBIf:
            case asBC_MULIf: {
                x86::Gp valueBits = cc.new_gp32("inlineFloatValueBits");
                x86::Gp immediate = cc.new_gp32("inlineFloatImmediate");
                x86::Gp resultBits = cc.new_gp32("inlineFloatResultBits");
                x86::Vec value = cc.new_xmm_ss("inlineFloatValue");
                x86::Vec operand = cc.new_xmm_ss("inlineFloatOperand");
                loadValue(asBC_SWORDARG1(bodyIp), valueBits);
                if (useAvx_)
                    cc.vmovd(value, valueBits);
                else
                    cc.movd(value, valueBits);
                cc.mov(immediate,
                       Imm(int64_t(static_cast<int32_t>(
                           asBC_DWORDARG(bodyIp + 1)))));
                if (useAvx_)
                    cc.vmovd(operand, immediate);
                else
                    cc.movd(operand, immediate);
                if (useAvx_) {
                    if (op == asBC_ADDIf)
                        cc.vaddss(value, value, operand);
                    else if (op == asBC_SUBIf)
                        cc.vsubss(value, value, operand);
                    else
                        cc.vmulss(value, value, operand);
                    cc.vmovd(resultBits, value);
                } else {
                    if (op == asBC_ADDIf)
                        cc.addss(value, operand);
                    else if (op == asBC_SUBIf)
                        cc.subss(value, operand);
                    else
                        cc.mulss(value, operand);
                    cc.movd(resultBits, value);
                }
                storeValue(asBC_SWORDARG0(bodyIp), resultBits);
                break;
            }
            case asBC_NEGi:
            case asBC_BNOT: {
                const int destination = asBC_SWORDARG0(bodyIp);
                x86::Gp value = cc.new_gp32("inlineValue");
                loadValue(destination, value);
                if (op == asBC_NEGi)
                    cc.neg(value);
                else
                    cc.not_(value);
                storeValue(destination, value);
                break;
            }
            case asBC_CMPi:
            case asBC_CMPIi: {
                x86::Gp left = cc.new_gp32("inlineLeft");
                x86::Gp right = cc.new_gp32("inlineRight");
                loadValue(asBC_SWORDARG0(bodyIp), left);
                if (op == asBC_CMPi) {
                    loadValue(asBC_SWORDARG1(bodyIp), right);
                    cc.cmp(left, right);
                } else {
                    cc.cmp(left, Imm(int64_t(asBC_INTARG(bodyIp))));
                }
                cc.set(x86::CondCode::kSignedGT, left);
                cc.set(x86::CondCode::kSignedLT, right);
                cc.movzx(left, left.r8());
                cc.movzx(right, right.r8());
                cc.sub(left, right);
                cc.mov(x86::dword_ptr(
                           regs_, offsetof(asSVMRegisters, valueRegister)),
                       left);
                break;
            }
            case asBC_TZ:
            case asBC_TNZ:
            case asBC_TS:
            case asBC_TNS:
            case asBC_TP:
            case asBC_TNP: {
                x86::Gp value = cc.new_gp32("inlineTest");
                cc.mov(value,
                       x86::dword_ptr(
                           regs_, offsetof(asSVMRegisters, valueRegister)));
                cc.cmp(value, 0);
                x86::CondCode condition = x86::CondCode::kEqual;
                switch (op) {
                case asBC_TNZ:
                    condition = x86::CondCode::kNotEqual;
                    break;
                case asBC_TS:
                    condition = x86::CondCode::kSignedLT;
                    break;
                case asBC_TNS:
                    condition = x86::CondCode::kSignedGE;
                    break;
                case asBC_TP:
                    condition = x86::CondCode::kSignedGT;
                    break;
                case asBC_TNP:
                    condition = x86::CondCode::kSignedLE;
                    break;
                default:
                    break;
                }
                cc.set(condition, value);
                cc.movzx(value, value.r8());
                cc.mov(x86::dword_ptr(
                           regs_, offsetof(asSVMRegisters, valueRegister)),
                       value);
                cc.mov(x86::dword_ptr(
                           regs_, offsetof(asSVMRegisters, valueRegister) + 4),
                       0);
                break;
            }
            case asBC_JMP:
            case asBC_JZ:
            case asBC_JNZ:
            case asBC_JS:
            case asBC_JNS:
            case asBC_JP:
            case asBC_JNP:
            case asBC_JLowZ:
            case asBC_JLowNZ: {
                const int targetIndex = branchTarget(bodyIp);
                if (targetIndex < 0) return false;
                const Label& targetLabel =
                    labels[static_cast<size_t>(targetIndex)];
                if (op == asBC_JMP) {
                    cc.jmp(targetLabel);
                    break;
                }
                if (op == asBC_JLowZ || op == asBC_JLowNZ)
                    cc.cmp(x86::byte_ptr(
                               regs_, offsetof(asSVMRegisters, valueRegister)),
                           0);
                else
                    cc.cmp(x86::dword_ptr(
                               regs_, offsetof(asSVMRegisters, valueRegister)),
                           0);
                switch (op) {
                case asBC_JZ:
                case asBC_JLowZ:
                    cc.jz(targetLabel);
                    break;
                case asBC_JNZ:
                case asBC_JLowNZ:
                    cc.jnz(targetLabel);
                    break;
                case asBC_JS:
                    cc.js(targetLabel);
                    break;
                case asBC_JNS:
                    cc.jns(targetLabel);
                    break;
                case asBC_JP:
                    cc.jg(targetLabel);
                    break;
                case asBC_JNP:
                    cc.jle(targetLabel);
                    break;
                default:
                    break;
                }
                break;
            }
            case asBC_CALL: {
                auto* factory = target->engine->scriptFunctions[
                    asBC_INTARG(bodyIp)];
                if (!emitSimpleFactory(factory, body.factories[bodyIndex]))
                    return false;
                break;
            }
            case asBC_STOREOBJ: {
                x86::Gp object = cc.new_gp32("inlineStoredObject");
                cc.mov(object,
                       x86::dword_ptr(
                           regs_, offsetof(asSVMRegisters, objectRegister)));
                storeValue(asBC_SWORDARG0(bodyIp), object);
                cc.mov(x86::dword_ptr(
                           regs_, offsetof(asSVMRegisters, objectRegister)),
                       0);
                break;
            }
            case asBC_ChkNullV:
                break;
            case asBC_RefCpyV: {
                x86::Gp sp = cc.new_gp32("inlineRefCopySp");
                x86::Gp object = cc.new_gp32("inlineRefCopyObject");
                LoadSp(sp);
                cc.mov(object, x86::dword_ptr(sp));
                cc.test(object, object);
                Label copied = cc.new_label();
                cc.jz(copied);
                InvokeNode* invocation = nullptr;
                Error err = cc.invoke(
                    Out<InvokeNode*>(invocation),
                    Imm(int64_t((intptr_t)&AddRefScriptObject)),
                    FuncSignature::build<void, void*>());
                if (err != kErrorOk) return false;
                invocation->set_arg(0, object);
                cc.bind(copied);
                storeValue(asBC_SWORDARG0(bodyIp), object);
                break;
            }
            case asBC_LOADOBJ: {
                x86::Gp object = cc.new_gp32("inlineLoadedObject");
                loadValue(asBC_SWORDARG0(bodyIp), object);
                cc.mov(x86::dword_ptr(
                           regs_, offsetof(asSVMRegisters, objectType)),
                       0);
                cc.mov(x86::dword_ptr(
                           regs_, offsetof(asSVMRegisters, objectRegister)),
                       object);
                x86::Gp zero = cc.new_gp32("inlineZeroObject");
                cc.xor_(zero, zero);
                storeValue(asBC_SWORDARG0(bodyIp), zero);
                break;
            }
            case asBC_CallPtr: {
                x86::Gp function = cc.new_gp32("inlineFunctionPointer");
                loadValue(asBC_SWORDARG0(bodyIp), function);
                InvokeNode* invocation = nullptr;
                Error err = cc.invoke(
                    Out<InvokeNode*>(invocation),
                    Imm(int64_t((intptr_t)&detail::CallFunctionPointer)),
                    FuncSignature::build<int, asSVMRegisters*,
                                         asCScriptFunction*,
                                         const asDWORD*, const asDWORD*>());
                if (err != kErrorOk) return false;
                x86::Gp result = cc.new_gp32("inlineFunctionResult");
                invocation->set_arg(0, regs_);
                invocation->set_arg(1, function);
                invocation->set_arg(2, Imm(int64_t((intptr_t)ip)));
                invocation->set_arg(
                    3, Imm(int64_t((intptr_t)(ip + instruction.size))));
                invocation->set_ret(0, result);
                cc.test(result, result);
                cc.jnz(exitLabel_);
                break;
            }
            case asBC_FREE: {
                const int offset = asBC_SWORDARG0(bodyIp);
                x86::Gp function = cc.new_gp32("inlineReleasedFunction");
                loadValue(offset, function);
                Label released = cc.new_label();
                cc.test(function, function);
                cc.jz(released);
                InvokeNode* invocation = nullptr;
                Error err = cc.invoke(
                    Out<InvokeNode*>(invocation),
                    Imm(int64_t((intptr_t)&detail::ReleaseScriptFunction)),
                    FuncSignature::build<void, asCScriptFunction*>());
                if (err != kErrorOk) return false;
                invocation->set_arg(0, function);
                cc.bind(released);
                x86::Gp zero = cc.new_gp32("inlineZeroFunction");
                cc.xor_(zero, zero);
                storeValue(offset, zero);
                break;
            }
            case asBC_RET:
                cc.jmp(fastDone);
                break;
            default:
                return false;
            }
        }
        return true;
    };

    auto emitInlineCall = [&](asCScriptFunction* target,
                              const InlineScriptBody& body,
                              bool virtualCall,
                              int virtualSlot,
                              bool indirectCall) -> EmitResult {
        Label slow = cc.new_label();
        Label fastDone = cc.new_label();
        Label done = cc.new_label();
        cc.cmp(x86::byte_ptr(
                   regs_, offsetof(asSVMRegisters, doProcessSuspend)),
               0);
        cc.jne(slow);
        if (virtualCall) {
            x86::Gp sp = cc.new_gp32("inlineGuardSp");
            x86::Gp object = cc.new_gp32("inlineGuardObject");
            x86::Gp matched = cc.new_gp32("inlineGuardMatched");
            LoadSp(sp);
            cc.mov(object, x86::dword_ptr(sp));
            cc.test(object, object);
            cc.jz(slow);
            InvokeNode* guard = nullptr;
            Error err = cc.invoke(
                Out<InvokeNode*>(guard),
                Imm(int64_t((intptr_t)&MatchesVirtualMethod)),
                FuncSignature::build<bool, void*, int,
                                     asCScriptFunction*>());
            if (err != kErrorOk) return EmitResult::Error;
            guard->set_arg(0, object);
            guard->set_arg(1, virtualSlot);
            guard->set_arg(2, Imm(int64_t((intptr_t)target)));
            guard->set_ret(0, matched);
            cc.test(matched.r8(), matched.r8());
            cc.jz(slow);
        }
        if (indirectCall) {
            x86::Gp actualTarget = cc.new_gp32("inlineIndirectTarget");
            LoadVar(asBC_SWORDARG0(ip), actualTarget);
            cc.cmp(actualTarget,
                   Imm(int64_t((intptr_t)target)));
            cc.jne(slow);
        }
        if (!emitInlineBody(target, body, slow, fastDone))
            return EmitResult::Error;
        cc.bind(fastDone);
        {
            x86::Gp sp = cc.new_gp32("inlineReturnSp");
            LoadSp(sp);
            if (body.returnDwords) cc.add(sp, body.returnDwords * 4);
            StoreSp(sp);
        }
        cc.mov(x86::dword_ptr(regs_, ppOff),
               Imm(int64_t((intptr_t)(ip + instruction.size))));
        cc.jmp(done);

        cc.bind(slow);
        if (virtualCall || indirectCall) {
            if (!EmitHelperCall(instruction, ip)) return EmitResult::Error;
        } else {
            InvokeNode* invocation = nullptr;
            Error err = cc.invoke(
                Out<InvokeNode*>(invocation),
                Imm(int64_t((intptr_t)&detail::CallScriptFunction)),
                FuncSignature::build<int, asSVMRegisters*,
                                     asCScriptFunction*, const asDWORD*>());
            if (err != kErrorOk) return EmitResult::Error;
            x86::Gp result = cc.new_gp32("inlineFallbackResult");
            invocation->set_arg(0, regs_);
            invocation->set_arg(1, Imm(int64_t((intptr_t)target)));
            invocation->set_arg(
                2, Imm(int64_t((intptr_t)(ip + instruction.size))));
            invocation->set_ret(0, result);
            cc.test(result, result);
            cc.jnz(exitLabel_);
        }
        cc.bind(done);
        return EmitResult::Success;
    };

    switch (instruction.op) {
    case asBC_ALLOC: {
        auto* objectType =
            reinterpret_cast<asCObjectType*>(asBC_PTRARG(ip));
        auto* constructor =
            engine_->scriptFunctions[asBC_INTARG(ip + AS_PTR_SIZE)];
        if (!objectType || !(objectType->flags & asOBJ_SCRIPT_OBJECT) ||
            !IsSimpleConstructor(constructor))
            return EmitResult::Unhandled;

        Label slow = cc.new_label();
        Label destinationDone = cc.new_label();
        Label done = cc.new_label();
        cc.cmp(x86::byte_ptr(
                   regs_, offsetof(asSVMRegisters, doProcessSuspend)),
               0);
        cc.jne(slow);

        x86::Gp allocSp = cc.new_gp32("inlineAllocSp");
        x86::Gp object = cc.new_gp32("inlineAllocatedObject");
        x86::Gp destination = cc.new_gp32("inlineAllocDestination");
        x86::Gp constructorFrame = cc.new_gp32("inlineConstructorFrame");
        LoadSp(allocSp);
        cc.mov(x86::dword_ptr(regs_, ppOff),
               Imm(int64_t((intptr_t)ip)));

        if (!emitCreateScriptObject(objectType, object))
            return EmitResult::Error;

        const int argumentDwords =
            constructor->GetSpaceNeededForArguments();
        cc.mov(destination,
               x86::dword_ptr(allocSp, argumentDwords * 4));
        cc.test(destination, destination);
        cc.jz(destinationDone);
        cc.mov(x86::dword_ptr(destination), object);
        cc.bind(destinationDone);

        cc.mov(constructorFrame, allocSp);
        cc.sub(constructorFrame, AS_PTR_SIZE * 4);
        cc.mov(x86::dword_ptr(constructorFrame), object);
        if (!EmitSimpleConstructorBody(cc, constructor, constructorFrame))
            return EmitResult::Error;

        if (argumentDwords) cc.add(allocSp, argumentDwords * 4);
        StoreSp(allocSp);
        cc.mov(x86::dword_ptr(regs_, ppOff),
               Imm(int64_t((intptr_t)(ip + instruction.size))));
        cc.jmp(done);

        cc.bind(slow);
        if (!EmitHelperCall(instruction, ip)) return EmitResult::Error;
        cc.bind(done);
        return EmitResult::Success;
    }
    case asBC_CALL: {
        auto* target = engine_->scriptFunctions[asBC_INTARG(ip)];
        SimpleFactoryTarget factoryTarget;
        if (DecodeSimpleFactory(engine_, target, factoryTarget)) {
            Label slow = cc.new_label();
            Label done = cc.new_label();
            cc.cmp(x86::byte_ptr(
                       regs_, offsetof(asSVMRegisters, doProcessSuspend)),
                   0);
            cc.jne(slow);
            if (!emitSimpleFactory(target, factoryTarget))
                return EmitResult::Error;
            cc.mov(x86::dword_ptr(regs_, ppOff),
                   Imm(int64_t((intptr_t)(ip + instruction.size))));
            cc.jmp(done);

            cc.bind(slow);
            InvokeNode* invocation = nullptr;
            Error err = cc.invoke(
                Out<InvokeNode*>(invocation),
                Imm(int64_t((intptr_t)&detail::CallScriptFunction)),
                FuncSignature::build<int, asSVMRegisters*,
                                     asCScriptFunction*, const asDWORD*>());
            if (err != kErrorOk) return EmitResult::Error;
            x86::Gp result = cc.new_gp32("factoryFallbackResult");
            invocation->set_arg(0, regs_);
            invocation->set_arg(1, Imm(int64_t((intptr_t)target)));
            invocation->set_arg(
                2, Imm(int64_t((intptr_t)(ip + instruction.size))));
            invocation->set_ret(0, result);
            cc.test(result, result);
            cc.jnz(exitLabel_);
            cc.bind(done);
            return EmitResult::Success;
        }

        InlineScriptBody inlineBody;
        if (DecodeInlineScriptBody(target, inlineBody))
            return emitInlineCall(target, inlineBody, false, -1, false);

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
    case asBC_CallPtr: {
        asCScriptFunction* target = nullptr;
        bool ambiguous = false;
        for (const Instruction& candidateInstruction : instructions_) {
            if (candidateInstruction.op != asBC_FuncPtr) continue;
            auto* candidate = reinterpret_cast<asCScriptFunction*>(
                asBC_PTRARG(bytecode_ + candidateInstruction.off));
            if (!candidate || candidate->funcType != asFUNC_SCRIPT)
                continue;
            if (!target) {
                target = candidate;
            } else if (target != candidate) {
                ambiguous = true;
                break;
            }
        }
        InlineScriptBody inlineBody;
        if (!target || ambiguous ||
            !DecodeInlineScriptBody(target, inlineBody))
            return EmitResult::Unhandled;
        return emitInlineCall(target, inlineBody, false, -1, true);
    }
    case asBC_CALLINTF: {
        auto* declaration =
            engine_->GetScriptFunction(asBC_INTARG(ip));
        if (declaration->funcType != asFUNC_VIRTUAL ||
            !declaration->objectType || declaration->vfTableIdx < 0 ||
            asUINT(declaration->vfTableIdx) >=
                declaration->objectType->virtualFunctionTable.GetLength())
            return EmitResult::Unhandled;
        auto* target = declaration->objectType->virtualFunctionTable[
            declaration->vfTableIdx];
        InlineScriptBody inlineBody;
        if (!DecodeInlineScriptBody(target, inlineBody))
            return EmitResult::Unhandled;
        return emitInlineCall(target, inlineBody, true,
                              declaration->vfTableIdx, false);
    }
    case asBC_CALLSYS: {
        auto* target = engine_->scriptFunctions[asBC_INTARG(ip)];
        const bool fastSystemCall = detail::CanUseFastSystemCall(target);
        x86::Gp popDwords = cc.new_gp32("popDwords");
        cc.mov(x86::dword_ptr(regs_, ppOff),
               Imm(int64_t((intptr_t)ip)));

        x86::Gp context;
        if (!fastSystemCall) {
            context = cc.new_gp32("context");
            cc.mov(context,
                   x86::dword_ptr(regs_, offsetof(asSVMRegisters, ctx)));
        }
        InvokeNode* invocation = nullptr;
        Error err;
        if (fastSystemCall) {
            err = cc.invoke(
                Out<InvokeNode*>(invocation),
                Imm(int64_t((intptr_t)&detail::FastSystemCall)),
                FuncSignature::build<int, asSVMRegisters*,
                                     asCScriptFunction*>());
        } else {
            err = cc.invoke(
                Out<InvokeNode*>(invocation),
                Imm(int64_t((intptr_t)&CallSystemFunction)),
                FuncSignature::build<int, int, asCContext*>());
        }
        if (err != kErrorOk) return EmitResult::Error;
        if (fastSystemCall) {
            invocation->set_arg(0, regs_);
            invocation->set_arg(1, Imm(int64_t((intptr_t)target)));
        } else {
            invocation->set_arg(0, asBC_INTARG(ip));
            invocation->set_arg(1, context);
        }
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
                                     asCScriptFunction*,
                                     const asSTryCatchInfo*, int>());
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
            finish->set_arg(
                1, Imm(int64_t((intptr_t)scriptFunction_)));
            finish->set_arg(
                2, Imm(int64_t((intptr_t)localCatchInfo_[index])));
            finish->set_arg(3, localCatchNeedsCleanup_[index]);
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
