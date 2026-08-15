#pragma once

#include "angelscript.h"

#include <memory>

class asCObjectType;
class asCScriptFunction;

namespace asjitx86::detail {

struct ScalarObjectPoolBucket;

class ScalarObjectPool {
public:
    explicit ScalarObjectPool(asIScriptEngine* engine);
    ~ScalarObjectPool();

    ScalarObjectPoolBucket* GetBucket(asCObjectType* objectType);
    void Clear();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

bool IsScalarOnlyScriptObject(asCObjectType* objectType);
bool DecodePooledGlobalDestructor(asCObjectType* objectType,
                                  asDWORD*& globalAddress, int& delta);
void* CreatePooledScriptObject(ScalarObjectPoolBucket* bucket);
void ReleasePooledScriptObject(void* object,
                               ScalarObjectPoolBucket* bucket);
void ReleasePooledScriptObjectWithGlobalDestructor(
    void* object, ScalarObjectPoolBucket* bucket, asSVMRegisters* regs,
    asDWORD* globalAddress, int delta);

int BcAlloc(asSVMRegisters* regs, const asDWORD* bc);
int AllocScriptObject(asSVMRegisters* regs, asCObjectType* objectType,
                      asCScriptFunction* constructor, const asDWORD* nextBc);
void* CreateScriptObject(asSVMRegisters* regs, asCObjectType* objectType);
int BcFree(asSVMRegisters* regs, const asDWORD* bc);
int BcLoadObj(asSVMRegisters* regs, const asDWORD* bc);
int BcStoreObj(asSVMRegisters* regs, const asDWORD* bc);
int BcGetObj(asSVMRegisters* regs, const asDWORD* bc);
int BcGetObjRef(asSVMRegisters* regs, const asDWORD* bc);
int BcGetRef(asSVMRegisters* regs, const asDWORD* bc);
int BcRefCpy(asSVMRegisters* regs, const asDWORD* bc);
int BcChkRef(asSVMRegisters* regs, const asDWORD* bc);
int BcChkRefS(asSVMRegisters* regs, const asDWORD* bc);
int BcChkNullV(asSVMRegisters* regs, const asDWORD* bc);
int BcChkNullS(asSVMRegisters* regs, const asDWORD* bc);
int BcPshNull(asSVMRegisters* regs, const asDWORD* bc);
int BcClrVPtr(asSVMRegisters* regs, const asDWORD* bc);
int BcObjType(asSVMRegisters* regs, const asDWORD* bc);
int BcTypeId(asSVMRegisters* regs, const asDWORD* bc);
int BcCast(asSVMRegisters* regs, const asDWORD* bc);
int BcFuncPtr(asSVMRegisters* regs, const asDWORD* bc);
int BcLoadThisR(asSVMRegisters* regs, const asDWORD* bc);
int BcLoadRObjR(asSVMRegisters* regs, const asDWORD* bc);
int BcLoadVObjR(asSVMRegisters* regs, const asDWORD* bc);
int BcRefCpyV(asSVMRegisters* regs, const asDWORD* bc);
int BcAllocMem(asSVMRegisters* regs, const asDWORD* bc);
int BcSetListSize(asSVMRegisters* regs, const asDWORD* bc);
int BcPshListElmnt(asSVMRegisters* regs, const asDWORD* bc);
int BcSetListType(asSVMRegisters* regs, const asDWORD* bc);

}
