#include "bytecode/helpers/object_helpers.h"
#include "bytecode/helpers/helper_context.h"
#include "bytecode/helpers/runtime_helpers.h"

#include "as_scriptengine.h"
#include "as_scriptobject.h"
#include "as_property.h"
#include "as_texts.h"

#include <atomic>
#include <cassert>
#include <cmath>
#include <cstring>
#include <mutex>
#include <vector>

namespace asjitx86::detail {

struct ScalarObjectPoolBucket {
    asCScriptEngine* engine = nullptr;
    asCObjectType* objectType = nullptr;
    std::atomic<asPWORD> owner{0};
    void* head = nullptr;
    unsigned count = 0;
};

struct ScalarObjectPool::Impl {
    explicit Impl(asCScriptEngine* value) : engine(value) {}

    asCScriptEngine* engine;
    std::mutex mutex;
    std::vector<std::unique_ptr<ScalarObjectPoolBucket>> buckets;
};

namespace {

bool IsPoolOwner(ScalarObjectPoolBucket* bucket) {
    static thread_local const char threadToken = 0;
    const asPWORD token = reinterpret_cast<asPWORD>(&threadToken);
    asPWORD owner = bucket->owner.load(std::memory_order_acquire);
    if (owner == token) return true;
    if (owner != 0) return false;
    return bucket->owner.compare_exchange_strong(
        owner, token, std::memory_order_acq_rel,
        std::memory_order_acquire);
}

}

ScalarObjectPool::ScalarObjectPool(asIScriptEngine* engine)
    : impl_(std::make_unique<Impl>(static_cast<asCScriptEngine*>(engine))) {}

ScalarObjectPool::~ScalarObjectPool() {
    Clear();
}

ScalarObjectPoolBucket* ScalarObjectPool::GetBucket(
    asCObjectType* objectType) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    for (const auto& bucket : impl_->buckets) {
        if (bucket->objectType == objectType) return bucket.get();
    }
    auto bucket = std::make_unique<ScalarObjectPoolBucket>();
    bucket->engine = impl_->engine;
    bucket->objectType = objectType;
    objectType->AddRef();
    ScalarObjectPoolBucket* result = bucket.get();
    impl_->buckets.push_back(std::move(bucket));
    return result;
}

void ScalarObjectPool::Clear() {
    std::lock_guard<std::mutex> poolLock(impl_->mutex);
    for (const auto& bucket : impl_->buckets) {
        void* object = bucket->head;
        bucket->head = nullptr;
        bucket->count = 0;
        while (object) {
            void* next = *static_cast<void**>(object);
            impl_->engine->CallFree(object);
            object = next;
        }
        if (bucket->objectType) {
            bucket->objectType->Release();
            bucket->objectType = nullptr;
        }
    }
}

bool IsScalarOnlyScriptObject(asCObjectType* objectType) {
    if (!objectType || (objectType->flags & asOBJ_GC)) return false;
    for (asCObjectType* type = objectType; type; type = type->derivedFrom) {
        if (type->beh.destruct) return false;
    }
    for (asUINT index = 0; index < objectType->properties.GetLength();
         index++) {
        const asCDataType& type = objectType->properties[index]->type;
        if (type.IsObject() || type.IsFuncdef()) return false;
    }
    return true;
}

void* CreatePooledScriptObject(ScalarObjectPoolBucket* bucket) {
    void* memory = nullptr;
    if (IsPoolOwner(bucket)) {
        memory = bucket->head;
        if (memory) {
            bucket->head = *static_cast<void**>(memory);
            bucket->count--;
        }
    }
    if (!memory) memory = bucket->engine->CallAlloc(bucket->objectType);
    ScriptObject_Construct(bucket->objectType,
                           static_cast<asCScriptObject*>(memory));
    return memory;
}

void ReleasePooledScriptObject(void* object,
                               ScalarObjectPoolBucket* bucket) {
    auto* scriptObject = static_cast<asCScriptObject*>(object);
    if (scriptObject->asCScriptObject::GetObjectType() !=
            bucket->objectType) {
        scriptObject->asCScriptObject::Release();
        return;
    }
    if (!scriptObject->asCScriptObject::ReleaseScalarOnlyWithoutFree())
        return;

    const bool pooled = IsPoolOwner(bucket) && bucket->count < 64;
    if (pooled) {
        *static_cast<void**>(object) = bucket->head;
        bucket->head = object;
        bucket->count++;
    }
    if (!pooled) bucket->engine->CallFree(object);
}

int AllocScriptObject(asSVMRegisters* regs, asCObjectType* objectType,
                      asCScriptFunction* constructor, const asDWORD* nextBc) {
    auto* ctx = Ctx(regs);
    asDWORD* memory = static_cast<asDWORD*>(ctx->m_engine->CallAlloc(objectType));
    ScriptObject_Construct(objectType, reinterpret_cast<asCScriptObject*>(memory));
    auto** destination = reinterpret_cast<asDWORD**>(
        *reinterpret_cast<asPWORD*>(regs->stackPointer +
                                    constructor->GetSpaceNeededForArguments()));
    if (destination) *destination = memory;
    regs->stackPointer -= AS_PTR_SIZE;
    *reinterpret_cast<asPWORD*>(regs->stackPointer) =
        reinterpret_cast<asPWORD>(memory);
    return CallScriptFunction(regs, constructor, nextBc);
}

void* CreateScriptObject(asSVMRegisters* regs, asCObjectType* objectType) {
    auto* ctx = Ctx(regs);
    void* memory = ctx->m_engine->CallAlloc(objectType);
    ScriptObject_Construct(objectType, reinterpret_cast<asCScriptObject*>(memory));
    return memory;
}

int BcAlloc(asSVMRegisters* regs, const asDWORD* bc) {
    auto* ctx = Ctx(regs);
    asCObjectType* objType = (asCObjectType*)asBC_PTRARG(bc);
    int func = asBC_INTARG(bc + AS_PTR_SIZE);
    if (objType->flags & asOBJ_SCRIPT_OBJECT) {
        return AllocScriptObject(regs, objType,
                                 ctx->m_engine->scriptFunctions[func],
                                 NextBc(bc, 2 + AS_PTR_SIZE));
    }
    asDWORD* mem = (asDWORD*)ctx->m_engine->CallAlloc(objType);
    if (func) {
        regs->stackPointer -= AS_PTR_SIZE;
        *(asPWORD*)regs->stackPointer = (asPWORD)mem;
        regs->programPointer = const_cast<asDWORD*>(bc);
        regs->stackPointer += CallSystemFunction(func, ctx);
    }
    asDWORD** a = (asDWORD**)*(asPWORD*)regs->stackPointer;
    regs->stackPointer += AS_PTR_SIZE;
    if (a) *a = mem;
    regs->programPointer += 2 + AS_PTR_SIZE;
    if (regs->doProcessSuspend) {
        if (ctx->m_doSuspend) {
            ctx->m_status = asEXECUTION_SUSPENDED;
            return JITBC_EXIT;
        }
        if (ctx->m_status != asEXECUTION_ACTIVE) {
            ctx->m_engine->CallFree(mem);
            *a = 0;
            return JITBC_EXIT;
        }
    }
    return JITBC_CONTINUE;
}

int BcFree(asSVMRegisters* regs, const asDWORD* bc) {
    auto* ctx = Ctx(regs);
    asPWORD* a = (asPWORD*)(asPWORD)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    if (*a) {
        asCObjectType* objType = (asCObjectType*)asBC_PTRARG(bc);
        asSTypeBehaviour* beh = &objType->beh;
        regs->programPointer = const_cast<asDWORD*>(bc);
        if (objType->flags & asOBJ_REF) {
            assert((objType->flags & asOBJ_NOCOUNT) || beh->release);
            if (beh->release)
                ctx->m_engine->CallObjectMethod((void*)(asPWORD)*a, beh->release);
        }
        else {
            if (beh->destruct)
                ctx->m_engine->CallObjectMethod((void*)(asPWORD)*a, beh->destruct);
            else if (objType->flags & asOBJ_LIST_PATTERN)
                ctx->m_engine->DestroyList((asBYTE*)(asPWORD)*a, objType);
            ctx->m_engine->CallFree((void*)(asPWORD)*a);
        }
        *a = 0;
    }
    regs->programPointer = NextBc(bc, 1 + AS_PTR_SIZE);
    return JITBC_CONTINUE;
}

int BcLoadObj(asSVMRegisters* regs, const asDWORD* bc) {
    void** a = (void**)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->objectType = 0;
    regs->objectRegister = *a;
    *a = 0;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcStoreObj(asSVMRegisters* regs, const asDWORD* bc) {
    *(asPWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = asPWORD(regs->objectRegister);
    regs->objectRegister = 0;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcGetObj(asSVMRegisters* regs, const asDWORD* bc) {
    asPWORD* a = (asPWORD*)(regs->stackPointer + asBC_WORDARG0(bc));
    asPWORD offset = *a;
    asPWORD* v = (asPWORD*)(regs->stackFramePointer - offset);
    *a = *v;
    *v = 0;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcGetObjRef(asSVMRegisters* regs, const asDWORD* bc) {
    asPWORD* a = (asPWORD*)(regs->stackPointer + asBC_WORDARG0(bc));
    *(asPWORD**)a = *(asPWORD**)(regs->stackFramePointer - *a);
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcGetRef(asSVMRegisters* regs, const asDWORD* bc) {
    asPWORD* a = (asPWORD*)(regs->stackPointer + asBC_WORDARG0(bc));
    *(asPWORD**)a = (asPWORD*)(regs->stackFramePointer - (int)*a);
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcRefCpy(asSVMRegisters* regs, const asDWORD* bc) {
    auto* ctx = Ctx(regs);
    asCObjectType* objType = (asCObjectType*)asBC_PTRARG(bc);
    asSTypeBehaviour* beh = &objType->beh;
    void** d = (void**)*(asPWORD*)regs->stackPointer;
    regs->stackPointer += AS_PTR_SIZE;
    void* s = (void*)*(asPWORD*)regs->stackPointer;
    regs->programPointer = const_cast<asDWORD*>(bc);
    if (!(objType->flags & (asOBJ_NOCOUNT | asOBJ_VALUE))) {
        if (*d != 0 && beh->release)
            ctx->m_engine->CallObjectMethod(*d, beh->release);
        if (s != 0 && beh->addref)
            ctx->m_engine->CallObjectMethod(s, beh->addref);
    }
    *d = s;
    regs->programPointer = NextBc(bc, 1 + AS_PTR_SIZE);
    return JITBC_CONTINUE;
}

int BcChkRef(asSVMRegisters* regs, const asDWORD* bc) {
    asPWORD a = *(asPWORD*)regs->stackPointer;
    if (a == 0) {
        regs->programPointer = const_cast<asDWORD*>(bc);
        Ctx(regs)->SetInternalException(TXT_NULL_POINTER_ACCESS);
        return JITBC_EXIT;
    }
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcChkRefS(asSVMRegisters* regs, const asDWORD* bc) {
    asPWORD* a = (asPWORD*)*(asPWORD*)regs->stackPointer;
    if (*a == 0) {
        regs->programPointer = const_cast<asDWORD*>(bc);
        Ctx(regs)->SetInternalException(TXT_NULL_POINTER_ACCESS);
        return JITBC_EXIT;
    }
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcChkNullV(asSVMRegisters* regs, const asDWORD* bc) {
    asDWORD* a = *(asDWORD**)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    if (a == 0) {
        regs->programPointer = const_cast<asDWORD*>(bc);
        Ctx(regs)->SetInternalException(TXT_NULL_POINTER_ACCESS);
        return JITBC_EXIT;
    }
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcChkNullS(asSVMRegisters* regs, const asDWORD* bc) {
    asPWORD a = *(asPWORD*)(regs->stackPointer + asBC_WORDARG0(bc));
    if (a == 0) {
        regs->programPointer = const_cast<asDWORD*>(bc);
        Ctx(regs)->SetInternalException(TXT_NULL_POINTER_ACCESS);
        return JITBC_EXIT;
    }
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcPshNull(asSVMRegisters* regs, const asDWORD* bc) {
    regs->stackPointer -= AS_PTR_SIZE;
    *(asPWORD*)regs->stackPointer = 0;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcClrVPtr(asSVMRegisters* regs, const asDWORD* bc) {
    *(asPWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = 0;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcObjType(asSVMRegisters* regs, const asDWORD* bc) {
    regs->stackPointer -= AS_PTR_SIZE;
    *(asPWORD*)regs->stackPointer = asBC_PTRARG(bc);
    regs->programPointer = NextBc(bc, 1 + AS_PTR_SIZE);
    return JITBC_CONTINUE;
}

int BcTypeId(asSVMRegisters* regs, const asDWORD* bc) {
    --regs->stackPointer;
    *regs->stackPointer = asBC_DWORDARG(bc);
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcCast(asSVMRegisters* regs, const asDWORD* bc) {
    auto* ctx = Ctx(regs);
    asDWORD** a = (asDWORD**)*(asPWORD*)regs->stackPointer;
    if (a && *a) {
        asDWORD typeId = asBC_DWORDARG(bc);
        asCScriptObject* obj = (asCScriptObject*)*a;
        asCObjectType* objType = (asCObjectType*)obj->GetObjectType();
        asCObjectType* to = ctx->m_engine->GetObjectTypeFromTypeId(typeId);
        assert(objType->flags & asOBJ_SCRIPT_OBJECT);
        assert(to->flags & asOBJ_SCRIPT_OBJECT);
        if (objType->Implements(to) || objType->DerivesFrom(to)) {
            regs->objectType = 0;
            regs->objectRegister = obj;
            obj->AddRef();
        }
        else {
            assert(regs->objectRegister == 0);
        }
    }
    regs->stackPointer += AS_PTR_SIZE;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcFuncPtr(asSVMRegisters* regs, const asDWORD* bc) {
    regs->stackPointer -= AS_PTR_SIZE;
    *(asPWORD*)regs->stackPointer = asBC_PTRARG(bc);
    regs->programPointer = NextBc(bc, 1 + AS_PTR_SIZE);
    return JITBC_CONTINUE;
}

int BcLoadThisR(asSVMRegisters* regs, const asDWORD* bc) {
    asPWORD tmp = *(asPWORD*)regs->stackFramePointer;
    if (tmp == 0) {
        regs->programPointer = const_cast<asDWORD*>(bc);
        Ctx(regs)->SetInternalException(TXT_NULL_POINTER_ACCESS);
        return JITBC_EXIT;
    }
    tmp = tmp + asBC_SWORDARG0(bc);
    *(asPWORD*)&regs->valueRegister = tmp;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcLoadRObjR(asSVMRegisters* regs, const asDWORD* bc) {
    asPWORD tmp = *(asPWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    if (tmp == 0) {
        regs->programPointer = const_cast<asDWORD*>(bc);
        Ctx(regs)->SetInternalException(TXT_NULL_POINTER_ACCESS);
        return JITBC_EXIT;
    }
    tmp = tmp + asBC_SWORDARG1(bc);
    *(asPWORD*)&regs->valueRegister = tmp;
    regs->programPointer = NextBc(bc, 3);
    return JITBC_CONTINUE;
}

int BcLoadVObjR(asSVMRegisters* regs, const asDWORD* bc) {
    asPWORD tmp = (asPWORD)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    tmp = tmp + asBC_SWORDARG1(bc);
    *(asPWORD*)&regs->valueRegister = tmp;
    regs->programPointer = NextBc(bc, 3);
    return JITBC_CONTINUE;
}

int BcRefCpyV(asSVMRegisters* regs, const asDWORD* bc) {
    auto* ctx = Ctx(regs);
    asCObjectType* objType = (asCObjectType*)asBC_PTRARG(bc);
    asSTypeBehaviour* beh = &objType->beh;
    void** d = (void**)(asPWORD)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    void* s = (void*)*(asPWORD*)regs->stackPointer;
    regs->programPointer = const_cast<asDWORD*>(bc);
    if (!(objType->flags & (asOBJ_NOCOUNT | asOBJ_VALUE))) {
        if (*d != 0 && beh->release)
            ctx->m_engine->CallObjectMethod(*d, beh->release);
        if (s != 0 && beh->addref)
            ctx->m_engine->CallObjectMethod(s, beh->addref);
    }
    *d = s;
    regs->programPointer = NextBc(bc, 1 + AS_PTR_SIZE);
    return JITBC_CONTINUE;
}

int BcAllocMem(asSVMRegisters* regs, const asDWORD* bc) {
    asUINT size = asBC_DWORDARG(bc);
    asBYTE** var = (asBYTE**)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    *var = asNEWARRAY(asBYTE, size);
    memset(*var, 0, size);
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcSetListSize(asSVMRegisters* regs, const asDWORD* bc) {
    asBYTE* var = *(asBYTE**)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    asUINT off = asBC_DWORDARG(bc);
    asUINT size = asBC_DWORDARG(bc + 1);
    assert(var);
    *(asUINT*)(var + off) = size;
    regs->programPointer = NextBc(bc, 3);
    return JITBC_CONTINUE;
}

int BcPshListElmnt(asSVMRegisters* regs, const asDWORD* bc) {
    asBYTE* var = *(asBYTE**)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    asUINT off = asBC_DWORDARG(bc);
    assert(var);
    regs->stackPointer -= AS_PTR_SIZE;
    *(asPWORD*)regs->stackPointer = asPWORD(var + off);
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcSetListType(asSVMRegisters* regs, const asDWORD* bc) {
    asBYTE* var = *(asBYTE**)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    asUINT off = asBC_DWORDARG(bc);
    asUINT type = asBC_DWORDARG(bc + 1);
    assert(var);
    *(asUINT*)(var + off) = type;
    regs->programPointer = NextBc(bc, 3);
    return JITBC_CONTINUE;
}

}
