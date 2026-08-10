#include "bytecode/bc_helpers.h"

#include "bytecode/helpers/comparison_helpers.h"
#include "bytecode/helpers/numeric32_helpers.h"
#include "bytecode/helpers/numeric64_helpers.h"
#include "bytecode/helpers/object_helpers.h"
#include "bytecode/helpers/power_helpers.h"
#include "bytecode/helpers/runtime_helpers.h"
#include "bytecode/helpers/stack_memory_helpers.h"

#include <cassert>

namespace asjitx86 {

namespace {

int BcTz(asSVMRegisters* regs, const asDWORD* bc) {
    return detail::BcTestZero(regs, bc, true);
}
int BcTnz(asSVMRegisters* regs, const asDWORD* bc) {
    return detail::BcTestZero(regs, bc, false);
}

int BcTs(asSVMRegisters* regs, const asDWORD* bc) {
    return detail::BcTestSign(regs, bc, true);
}

int BcTns(asSVMRegisters* regs, const asDWORD* bc) {
    return detail::BcTestSign(regs, bc, false);
}

int BcTp(asSVMRegisters* regs, const asDWORD* bc) {
    return detail::BcTestPos(regs, bc, true);
}

int BcTnp(asSVMRegisters* regs, const asDWORD* bc) {
    return detail::BcTestPos(regs, bc, false);
}

}

JitBcHelper GetJitBcHelper(asEBCInstr op) {
    using namespace detail;
    switch (op) {
    case asBC_PopPtr:    return &BcPopPtr;
    case asBC_PshGPtr:   return &BcPshGPtr;
    case asBC_PshC4:     return &BcPshC4;
    case asBC_PshV4:     return &BcPshV4;
    case asBC_PSF:       return &BcPSF;
    case asBC_SwapPtr:   return &BcSwapPtr;
    case asBC_NOT:       return &BcNot;
    case asBC_PshG4:     return &BcPshG4;
    case asBC_LdGRdR4:   return &BcLdGRdR4;
    case asBC_TZ:        return &BcTz;
    case asBC_TNZ:       return &BcTnz;
    case asBC_TS:        return &BcTs;
    case asBC_TNS:       return &BcTns;
    case asBC_TP:        return &BcTp;
    case asBC_TNP:       return &BcTnp;
    case asBC_NEGi:      return &BcNegi;
    case asBC_NEGf:      return &BcNegf;
    case asBC_NEGd:      return &BcNegd;
    case asBC_INCi16:    return &BcInci16;
    case asBC_INCi8:     return &BcInci8;
    case asBC_DECi16:    return &BcDeci16;
    case asBC_DECi8:     return &BcDeci8;
    case asBC_INCi:      return &BcInci;
    case asBC_DECi:      return &BcDeci;
    case asBC_INCf:      return &BcIncf;
    case asBC_DECf:      return &BcDecf;
    case asBC_INCd:      return &BcIncd;
    case asBC_DECd:      return &BcDecd;
    case asBC_IncVi:     return &BcIncVi;
    case asBC_DecVi:     return &BcDecVi;
    case asBC_BNOT:      return &BcBnot;
    case asBC_BAND:      return &BcBand;
    case asBC_BOR:       return &BcBor;
    case asBC_BXOR:      return &BcBxor;
    case asBC_BSLL:      return &BcBsll;
    case asBC_BSRL:      return &BcBsrl;
    case asBC_BSRA:      return &BcBsra;
    case asBC_COPY:      return &BcCopy;
    case asBC_PshC8:     return &BcPshC8;
    case asBC_PshVPtr:   return &BcPshVPtr;
    case asBC_RDSPtr:    return &BcRdsPtr;
    case asBC_CMPd:      return &BcCmpd;
    case asBC_CMPu:      return &BcCmpu;
    case asBC_CMPf:      return &BcCmpf;
    case asBC_CMPi:      return &BcCmpi;
    case asBC_CMPIi:     return &BcCmpIi;
    case asBC_CMPIf:     return &BcCmpIf;
    case asBC_CMPIu:     return &BcCmpIu;
    case asBC_PopRPtr:   return &BcPopRPtr;
    case asBC_PshRPtr:   return &BcPshRPtr;
    case asBC_STR:       return &BcStr;
    case asBC_SetV4:     return &BcSetV4;
    case asBC_SetV8:     return &BcSetV8;
    case asBC_SetV1:     return &BcSetV4;
    case asBC_SetV2:     return &BcSetV4;
    case asBC_ADDSi:     return &BcAddSi;
    case asBC_CpyVtoV4:  return &BcCpyVtoV4;
    case asBC_CpyVtoV8:  return &BcCpyVtoV8;
    case asBC_CpyVtoR4:  return &BcCpyVtoR4;
    case asBC_CpyVtoR8:  return &BcCpyVtoR8;
    case asBC_CpyVtoG4:  return &BcCpyVtoG4;
    case asBC_CpyRtoV4:  return &BcCpyRtoV4;
    case asBC_CpyRtoV8:  return &BcCpyRtoV8;
    case asBC_CpyGtoV4:  return &BcCpyGtoV4;
    case asBC_WRTV1:     return &BcWrtV1;
    case asBC_WRTV2:     return &BcWrtV2;
    case asBC_WRTV4:     return &BcWrtV4;
    case asBC_WRTV8:     return &BcWrtV8;
    case asBC_RDR1:      return &BcRdr1;
    case asBC_RDR2:      return &BcRdr2;
    case asBC_RDR4:      return &BcRdr4;
    case asBC_RDR8:      return &BcRdr8;
    case asBC_LDG:       return &BcLdg;
    case asBC_LDV:       return &BcLdv;
    case asBC_PGA:       return &BcPga;
    case asBC_CmpPtr:    return &BcCmpPtr;
    case asBC_VAR:       return &BcVar;
    case asBC_iTOf:      return &BcItof;
    case asBC_fTOi:      return &BcFtoi;
    case asBC_uTOf:      return &BcUtof;
    case asBC_fTOu:      return &BcFtoU;
    case asBC_sbTOi:     return &BcSbtoi;
    case asBC_swTOi:     return &BcSwtoi;
    case asBC_ubTOi:     return &BcUbtoi;
    case asBC_uwTOi:     return &BcUwtoi;
    case asBC_dTOi:      return &BcDtoi;
    case asBC_dTOu:      return &BcDtoU;
    case asBC_dTOf:      return &BcDtof;
    case asBC_iTOd:      return &BcItod;
    case asBC_uTOd:      return &BcUtod;
    case asBC_fTOd:      return &BcFtod;
    case asBC_ADDi:      return &BcAddi;
    case asBC_SUBi:      return &BcSubi;
    case asBC_MULi:      return &BcMuli;
    case asBC_DIVi:      return &BcDivi;
    case asBC_MODi:      return &BcModi;
    case asBC_ADDf:      return &BcAddf;
    case asBC_SUBf:      return &BcSubf;
    case asBC_MULf:      return &BcMulf;
    case asBC_DIVf:      return &BcDivf;
    case asBC_MODf:      return &BcModf;
    case asBC_ADDd:      return &BcAddd;
    case asBC_SUBd:      return &BcSubd;
    case asBC_MULd:      return &BcMuld;
    case asBC_DIVd:      return &BcDivd;
    case asBC_MODd:      return &BcModd;
    case asBC_ADDIi:     return &BcAddIi;
    case asBC_SUBIi:     return &BcSubIi;
    case asBC_MULIi:     return &BcMulIi;
    case asBC_ADDIf:     return &BcAddIf;
    case asBC_SUBIf:     return &BcSubIf;
    case asBC_MULIf:     return &BcMulIf;
    case asBC_SetG4:     return &BcSetG4;
    case asBC_iTOb:      return &BcItob;
    case asBC_iTOw:      return &BcItow;
    case asBC_i64TOi:    return &BcI64toi;
    case asBC_uTOi64:    return &BcUtoi64;
    case asBC_iTOi64:    return &BcItoi64;
    case asBC_fTOi64:    return &BcFtoi64;
    case asBC_dTOi64:    return &BcDtoi64;
    case asBC_fTOu64:    return &BcFtoU64;
    case asBC_dTOu64:    return &BcDtoU64;
    case asBC_i64TOf:    return &BcI64tof;
    case asBC_u64TOf:    return &BcU64tof;
    case asBC_i64TOd:    return &BcI64tod;
    case asBC_u64TOd:    return &BcU64tod;
    case asBC_NEGi64:    return &BcNegi64;
    case asBC_INCi64:    return &BcInci64;
    case asBC_DECi64:    return &BcDeci64;
    case asBC_BNOT64:    return &BcBnot64;
    case asBC_ADDi64:    return &BcAddi64;
    case asBC_SUBi64:    return &BcSubi64;
    case asBC_MULi64:    return &BcMuli64;
    case asBC_DIVi64:    return &BcDivi64;
    case asBC_MODi64:    return &BcModi64;
    case asBC_BAND64:    return &BcBand64;
    case asBC_BOR64:     return &BcBor64;
    case asBC_BXOR64:    return &BcBxor64;
    case asBC_BSLL64:    return &BcBsll64;
    case asBC_BSRL64:    return &BcBsrl64;
    case asBC_BSRA64:    return &BcBsra64;
    case asBC_CMPi64:    return &BcCmpi64;
    case asBC_CMPu64:    return &BcCmpu64;
    case asBC_ClrHi:     return &BcClrHi;
    case asBC_PshV8:     return &BcPshV8;
    case asBC_DIVu:      return &BcDivu;
    case asBC_MODu:      return &BcModu;
    case asBC_DIVu64:    return &BcDivu64;
    case asBC_MODu64:    return &BcModu64;
    case asBC_JitEntry:  return &BcJitEntry;
    case asBC_CALL:      return &BcCall;
    case asBC_RET:       return &BcRet;
    case asBC_CALLSYS:   return &BcCallSys;
    case asBC_CALLBND:   return &BcCallBnd;
    case asBC_CALLINTF:  return &BcCallIntf;
    case asBC_CallPtr:   return &BcCallPtr;
    case asBC_ALLOC:     return &BcAlloc;
    case asBC_Thiscall1: return &BcThiscall1;
    case asBC_JMPP:      return &BcJmpP;
    case asBC_SUSPEND:   return &BcSuspend;
    case asBC_FREE:      return &BcFree;
    case asBC_LOADOBJ:   return &BcLoadObj;
    case asBC_STOREOBJ:  return &BcStoreObj;
    case asBC_GETOBJ:    return &BcGetObj;
    case asBC_GETOBJREF: return &BcGetObjRef;
    case asBC_GETREF:    return &BcGetRef;
    case asBC_REFCPY:    return &BcRefCpy;
    case asBC_CHKREF:    return &BcChkRef;
    case asBC_ChkRefS:   return &BcChkRefS;
    case asBC_ChkNullV:  return &BcChkNullV;
    case asBC_ChkNullS:  return &BcChkNullS;
    case asBC_PshNull:   return &BcPshNull;
    case asBC_ClrVPtr:   return &BcClrVPtr;
    case asBC_OBJTYPE:   return &BcObjType;
    case asBC_TYPEID:    return &BcTypeId;
    case asBC_Cast:      return &BcCast;
    case asBC_FuncPtr:   return &BcFuncPtr;
    case asBC_LoadThisR: return &BcLoadThisR;
    case asBC_LoadRObjR: return &BcLoadRObjR;
    case asBC_LoadVObjR: return &BcLoadVObjR;
    case asBC_RefCpyV:   return &BcRefCpyV;
    case asBC_AllocMem:  return &BcAllocMem;
    case asBC_SetListSize:  return &BcSetListSize;
    case asBC_PshListElmnt: return &BcPshListElmnt;
    case asBC_SetListType:  return &BcSetListType;
    case asBC_POWi:      return &BcPowi;
    case asBC_POWu:      return &BcPowu;
    case asBC_POWf:      return &BcPowf;
    case asBC_POWd:      return &BcPowd;
    case asBC_POWdi:     return &BcPowdi;
    case asBC_POWi64:    return &BcPowi64;
    case asBC_POWu64:    return &BcPowu64;
    default:
        assert(false);
        return nullptr;
    }
}

int JitBcFallback(asSVMRegisters* regs, const asDWORD* bc) {
    JitBcHelper helper = GetJitBcHelper(static_cast<asEBCInstr>(bc[0] & 0xFF));
    return helper ? helper(regs, bc) : JITBC_EXIT;
}

}
