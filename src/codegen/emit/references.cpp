#include "codegen/emit/emitter.h"

#include "bytecode/helpers/object_helpers.h"

#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"
#include "as_scriptobject.h"

#include <cstddef>

namespace asjitx86::emit {

namespace {

void FastReleaseScriptObject(void* object) {
    static_cast<asCScriptObject*>(object)->asCScriptObject::Release();
}

void FastRefCopyScriptObject(void** destination, void* source) {
    auto* current = static_cast<asCScriptObject*>(*destination);
    if (current) current->asCScriptObject::Release();
    if (source)
        static_cast<asCScriptObject*>(source)->asCScriptObject::AddRef();
    *destination = source;
}

void FastMoveScriptObject(void** destination, void** source) {
    auto* current = static_cast<asCScriptObject*>(*destination);
    if (current) current->asCScriptObject::Release();
    *destination = *source;
    *source = nullptr;
}

void FastReleaseScriptFunction(void* function) {
    static_cast<asCScriptFunction*>(function)->Release();
}

void FastRefCopyScriptFunction(void** destination, void* source) {
    auto* current = static_cast<asCScriptFunction*>(*destination);
    if (current) current->Release();
    if (source) static_cast<asCScriptFunction*>(source)->AddRef();
    *destination = source;
}

void FastMoveScriptFunction(void** destination, void** source) {
    auto* current = static_cast<asCScriptFunction*>(*destination);
    if (current) current->Release();
    *destination = *source;
    *source = nullptr;
}

bool TryScriptObjectCast(void* object, asCObjectType* targetType) {
    auto* scriptObject = static_cast<asCScriptObject*>(object);
    auto* objectType = static_cast<asCObjectType*>(
        scriptObject->asCScriptObject::GetObjectType());
    if (objectType != targetType && !objectType->Implements(targetType) &&
        !objectType->DerivesFrom(targetType))
        return false;
    scriptObject->asCScriptObject::AddRef();
    return true;
}

}

EmitResult FunctionEmitter::EmitReferences(
    size_t index, const Instruction& instruction, const asDWORD* ip) {
    using namespace asmjit;

    constexpr uint32_t ppOff = offsetof(asSVMRegisters, programPointer);
    auto& cc = Compiler();
    switch (instruction.op) {
    case asBC_PshVPtr: {
        if (refCopyFusionSpan_[index]) {
            const unsigned span = refCopyFusionSpan_[index];
            const asDWORD* copyIp =
                bytecode_ + instructions_[index + 1].off;
            auto* objectType =
                reinterpret_cast<asCObjectType*>(asBC_PTRARG(copyIp));
            const bool functionObject =
                (objectType->flags & asOBJ_FUNCDEF) != 0;
            const int sourceOffset = asBC_SWORDARG0(ip);
            const int destinationOffset = asBC_SWORDARG0(copyIp);
            x86::Gp sourceSlot = cc.new_gp32("sourceSlot");
            x86::Gp destination = cc.new_gp32("destination");
            cc.lea(sourceSlot,
                   x86::dword_ptr(fp_, -sourceOffset * 4));
            cc.lea(destination,
                   x86::dword_ptr(fp_, -destinationOffset * 4));
            cc.mov(x86::dword_ptr(regs_, ppOff),
                   Imm(int64_t((intptr_t)copyIp)));

            InvokeNode* invocation = nullptr;
            if (span == 4) {
                Error err = cc.invoke(
                    Out<InvokeNode*>(invocation),
                    Imm(int64_t((intptr_t)(
                        functionObject ? &FastMoveScriptFunction
                                       : &FastMoveScriptObject))),
                    FuncSignature::build<void, void**, void**>());
                if (err != kErrorOk) return EmitResult::Error;
                invocation->set_arg(0, destination);
                invocation->set_arg(1, sourceSlot);
            } else {
                x86::Gp source = cc.new_gp32("source");
                cc.mov(source, x86::dword_ptr(sourceSlot));
                Error err = cc.invoke(
                    Out<InvokeNode*>(invocation),
                    Imm(int64_t((intptr_t)(
                        functionObject ? &FastRefCopyScriptFunction
                                       : &FastRefCopyScriptObject))),
                    FuncSignature::build<void, void**, void*>());
                if (err != kErrorOk) return EmitResult::Error;
                invocation->set_arg(0, destination);
                invocation->set_arg(1, source);
            }
            const Instruction& last = instructions_[index + span - 1];
            cc.mov(x86::dword_ptr(regs_, ppOff),
                   Imm(int64_t((intptr_t)(
                       bytecode_ + last.off + last.size))));
            return EmitResult::Success;
        }

        const int offset = asBC_SWORDARG0(ip);
        x86::Gp sp = cc.new_gp32("sp");
        x86::Gp value = cc.new_gp32("value");
        LoadSp(sp);
        cc.sub(sp, AS_PTR_SIZE * 4);
        LoadVar(offset, value);
        cc.mov(x86::dword_ptr(sp), value);
        StoreSp(sp);
        cc.mov(x86::dword_ptr(regs_, ppOff),
               Imm(int64_t((intptr_t)(ip + 1))));
        return EmitResult::Success;
    }
    case asBC_ALLOC: {
        auto* objectType =
            reinterpret_cast<asCObjectType*>(asBC_PTRARG(ip));
        if (!(objectType->flags & asOBJ_SCRIPT_OBJECT)) {
            if (!EmitHelperCall(instruction, ip)) return EmitResult::Error;
            return EmitResult::Success;
        }

        auto* constructor =
            engine_->scriptFunctions[asBC_INTARG(ip + AS_PTR_SIZE)];
        InvokeNode* invocation = nullptr;
        Error err = cc.invoke(
            Out<InvokeNode*>(invocation),
            Imm(int64_t((intptr_t)&detail::AllocScriptObject)),
            FuncSignature::build<int, asSVMRegisters*, asCObjectType*,
                                 asCScriptFunction*, const asDWORD*>());
        if (err != kErrorOk) return EmitResult::Error;
        x86::Gp result = cc.new_gp32("result");
        invocation->set_arg(0, regs_);
        invocation->set_arg(1, Imm(int64_t((intptr_t)objectType)));
        invocation->set_arg(2, Imm(int64_t((intptr_t)constructor)));
        invocation->set_arg(
            3, Imm(int64_t((intptr_t)(ip + instruction.size))));
        invocation->set_ret(0, result);
        cc.test(result, result);
        cc.jnz(exitLabel_);
        return EmitResult::Success;
    }
    case asBC_FREE: {
        auto* objectType =
            reinterpret_cast<asCObjectType*>(asBC_PTRARG(ip));
        const bool scriptObject =
            (objectType->flags & asOBJ_SCRIPT_OBJECT) != 0;
        const bool functionObject =
            (objectType->flags & asOBJ_FUNCDEF) != 0;
        const bool scalarOnlyObject =
            scriptObject && detail::IsScalarOnlyScriptObject(objectType);
        if (!scriptObject && !functionObject) {
            if (!EmitHelperCall(instruction, ip)) return EmitResult::Error;
            return EmitResult::Success;
        }

        const int offset = asBC_SWORDARG0(ip);
        x86::Gp object = cc.new_gp32("object");
        Label done = cc.new_label();
        LoadVar(offset, object);
        cc.test(object, object);
        cc.jz(done);

        InvokeNode* invocation = nullptr;
        cc.mov(x86::dword_ptr(regs_, ppOff),
               Imm(int64_t((intptr_t)ip)));
        Error err;
        if (scalarOnlyObject) {
            auto* bucket = objectPool_.GetBucket(objectType);
            err = cc.invoke(
                Out<InvokeNode*>(invocation),
                Imm(int64_t((intptr_t)&detail::ReleasePooledScriptObject)),
                FuncSignature::build<void, void*,
                                     detail::ScalarObjectPoolBucket*>());
            if (err == kErrorOk)
                invocation->set_arg(
                    1, Imm(int64_t((intptr_t)bucket)));
        } else {
            err = cc.invoke(
                Out<InvokeNode*>(invocation),
                Imm(int64_t((intptr_t)(
                    functionObject ? &FastReleaseScriptFunction
                                   : &FastReleaseScriptObject))),
                FuncSignature::build<void, void*>());
        }
        if (err != kErrorOk) return EmitResult::Error;
        invocation->set_arg(0, object);
        cc.mov(x86::dword_ptr(fp_, -offset * 4), 0);
        cc.bind(done);
        cc.mov(x86::dword_ptr(regs_, ppOff),
               Imm(int64_t((intptr_t)(ip + instruction.size))));
        return EmitResult::Success;
    }
    case asBC_STOREOBJ: {
        const int destination = asBC_SWORDARG0(ip);
        x86::Gp object = cc.new_gp32("object");
        cc.mov(object,
               x86::dword_ptr(
                   regs_, offsetof(asSVMRegisters, objectRegister)));
        StoreVar(destination, object);
        cc.mov(x86::dword_ptr(
                   regs_, offsetof(asSVMRegisters, objectRegister)),
               0);
        return EmitResult::Success;
    }
    case asBC_LOADOBJ: {
        const int source = asBC_SWORDARG0(ip);
        x86::Gp object = cc.new_gp32("object");
        LoadVar(source, object);
        cc.mov(x86::dword_ptr(
                   regs_, offsetof(asSVMRegisters, objectType)),
               0);
        cc.mov(x86::dword_ptr(
                   regs_, offsetof(asSVMRegisters, objectRegister)),
               object);
        cc.mov(x86::dword_ptr(fp_, -source * 4), 0);
        return EmitResult::Success;
    }
    case asBC_GETOBJ:
    case asBC_GETOBJREF: {
        x86::Gp sp = cc.new_gp32("sp");
        x86::Gp slot = cc.new_gp32("slot");
        x86::Gp offset = cc.new_gp32("offset");
        x86::Gp object = cc.new_gp32("object");
        LoadSp(sp);
        cc.lea(slot, x86::dword_ptr(sp, asBC_WORDARG0(ip) * 4));
        cc.mov(offset, x86::dword_ptr(slot));
        cc.shl(offset, 2);
        cc.neg(offset);
        cc.mov(object, x86::dword_ptr(fp_, offset));
        cc.mov(x86::dword_ptr(slot), object);
        if (instruction.op == asBC_GETOBJ)
            cc.mov(x86::dword_ptr(fp_, offset), 0);
        return EmitResult::Success;
    }
    case asBC_GETREF: {
        x86::Gp sp = cc.new_gp32("sp");
        x86::Gp slot = cc.new_gp32("slot");
        x86::Gp offset = cc.new_gp32("offset");
        x86::Gp address = cc.new_gp32("address");
        LoadSp(sp);
        cc.lea(slot, x86::dword_ptr(sp, asBC_WORDARG0(ip) * 4));
        cc.mov(offset, x86::dword_ptr(slot));
        cc.shl(offset, 2);
        cc.neg(offset);
        cc.lea(address, x86::dword_ptr(fp_, offset));
        cc.mov(x86::dword_ptr(slot), address);
        return EmitResult::Success;
    }
    case asBC_REFCPY:
    case asBC_RefCpyV: {
        auto* objectType =
            reinterpret_cast<asCObjectType*>(asBC_PTRARG(ip));
        const bool scriptObject =
            (objectType->flags & asOBJ_SCRIPT_OBJECT) != 0;
        const bool functionObject =
            (objectType->flags & asOBJ_FUNCDEF) != 0;
        if (!scriptObject && !functionObject) {
            if (!EmitHelperCall(instruction, ip)) return EmitResult::Error;
            return EmitResult::Success;
        }

        x86::Gp sp = cc.new_gp32("sp");
        x86::Gp destination = cc.new_gp32("destination");
        x86::Gp source = cc.new_gp32("source");
        LoadSp(sp);
        if (instruction.op == asBC_REFCPY) {
            cc.mov(destination, x86::dword_ptr(sp));
            cc.add(sp, AS_PTR_SIZE * 4);
            StoreSp(sp);
        } else {
            cc.lea(destination,
                   x86::dword_ptr(fp_, -asBC_SWORDARG0(ip) * 4));
        }
        cc.mov(source, x86::dword_ptr(sp));
        cc.mov(x86::dword_ptr(regs_, ppOff),
               Imm(int64_t((intptr_t)ip)));

        InvokeNode* invocation = nullptr;
        Error err = cc.invoke(
            Out<InvokeNode*>(invocation),
            Imm(int64_t((intptr_t)(
                functionObject ? &FastRefCopyScriptFunction
                               : &FastRefCopyScriptObject))),
            FuncSignature::build<void, void**, void*>());
        if (err != kErrorOk) return EmitResult::Error;
        invocation->set_arg(0, destination);
        invocation->set_arg(1, source);
        cc.mov(x86::dword_ptr(regs_, ppOff),
               Imm(int64_t((intptr_t)(ip + instruction.size))));
        return EmitResult::Success;
    }
    case asBC_Cast: {
        auto* targetType = engine_->GetObjectTypeFromTypeId(
            static_cast<int>(asBC_DWORDARG(ip)));
        if (!targetType || !(targetType->flags & asOBJ_SCRIPT_OBJECT))
            return EmitResult::Unhandled;

        x86::Gp sp = cc.new_gp32("castSp");
        x86::Gp sourceSlot = cc.new_gp32("castSourceSlot");
        x86::Gp source = cc.new_gp32("castSource");
        Label handled = cc.new_label();
        LoadSp(sp);
        cc.mov(sourceSlot, x86::dword_ptr(sp));
        cc.test(sourceSlot, sourceSlot);
        cc.jz(handled);
        cc.mov(source, x86::dword_ptr(sourceSlot));
        cc.test(source, source);
        cc.jz(handled);

        InvokeNode* invocation = nullptr;
        Error err = cc.invoke(
            Out<InvokeNode*>(invocation),
            Imm(int64_t((intptr_t)&TryScriptObjectCast)),
            FuncSignature::build<bool, void*, asCObjectType*>());
        if (err != kErrorOk) return EmitResult::Error;
        x86::Gp matched = cc.new_gp32("castMatched");
        invocation->set_arg(0, source);
        invocation->set_arg(1, Imm(int64_t((intptr_t)targetType)));
        invocation->set_ret(0, matched);
        cc.test(matched.r8(), matched.r8());
        cc.jz(handled);
        cc.mov(x86::dword_ptr(
                   regs_, offsetof(asSVMRegisters, objectType)),
               0);
        cc.mov(x86::dword_ptr(
                   regs_, offsetof(asSVMRegisters, objectRegister)),
               source);

        cc.bind(handled);
        cc.add(sp, AS_PTR_SIZE * 4);
        StoreSp(sp);
        cc.mov(x86::dword_ptr(regs_, ppOff),
               Imm(int64_t((intptr_t)(ip + instruction.size))));
        return EmitResult::Success;
    }
    default:
        return EmitResult::Unhandled;
    }
}

}
