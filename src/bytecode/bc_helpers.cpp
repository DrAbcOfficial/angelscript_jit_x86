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

int JitBcFallback(asSVMRegisters* regs, const asDWORD* bc) {
    using namespace detail;
    switch (static_cast<asEBCInstr>(bc[0] & 0xFF)) {
    case asBC_PopPtr:    return BcPopPtr(regs, bc);
    case asBC_PshGPtr:   return BcPshGPtr(regs, bc);
    case asBC_PshC4:     return BcPshC4(regs, bc);
    case asBC_PshV4:     return BcPshV4(regs, bc);
    case asBC_PSF:       return BcPSF(regs, bc);
    case asBC_SwapPtr:   return BcSwapPtr(regs, bc);
    case asBC_NOT:       return BcNot(regs, bc);
    case asBC_PshG4:     return BcPshG4(regs, bc);
    case asBC_LdGRdR4:   return BcLdGRdR4(regs, bc);
    case asBC_TZ:        return BcTestZero(regs, bc, true);
    case asBC_TNZ:       return BcTestZero(regs, bc, false);
    case asBC_TS:        return BcTestSign(regs, bc, true);
    case asBC_TNS:       return BcTestSign(regs, bc, false);
    case asBC_TP:        return BcTestPos(regs, bc, true);
    case asBC_TNP:       return BcTestPos(regs, bc, false);
    case asBC_NEGi:      return BcNegi(regs, bc);
    case asBC_NEGf:      return BcNegf(regs, bc);
    case asBC_NEGd:      return BcNegd(regs, bc);
    case asBC_INCi16:    return BcInci16(regs, bc);
    case asBC_INCi8:     return BcInci8(regs, bc);
    case asBC_DECi16:    return BcDeci16(regs, bc);
    case asBC_DECi8:     return BcDeci8(regs, bc);
    case asBC_INCi:      return BcInci(regs, bc);
    case asBC_DECi:      return BcDeci(regs, bc);
    case asBC_INCf:      return BcIncf(regs, bc);
    case asBC_DECf:      return BcDecf(regs, bc);
    case asBC_INCd:      return BcIncd(regs, bc);
    case asBC_DECd:      return BcDecd(regs, bc);
    case asBC_IncVi:     return BcIncVi(regs, bc);
    case asBC_DecVi:     return BcDecVi(regs, bc);
    case asBC_BNOT:      return BcBnot(regs, bc);
    case asBC_BAND:      return BcBand(regs, bc);
    case asBC_BOR:       return BcBor(regs, bc);
    case asBC_BXOR:      return BcBxor(regs, bc);
    case asBC_BSLL:      return BcBsll(regs, bc);
    case asBC_BSRL:      return BcBsrl(regs, bc);
    case asBC_BSRA:      return BcBsra(regs, bc);
    case asBC_COPY:      return BcCopy(regs, bc);
    case asBC_PshC8:     return BcPshC8(regs, bc);
    case asBC_PshVPtr:   return BcPshVPtr(regs, bc);
    case asBC_RDSPtr:    return BcRdsPtr(regs, bc);
    case asBC_CMPd:      return BcCmpd(regs, bc);
    case asBC_CMPu:      return BcCmpu(regs, bc);
    case asBC_CMPf:      return BcCmpf(regs, bc);
    case asBC_CMPi:      return BcCmpi(regs, bc);
    case asBC_CMPIi:     return BcCmpIi(regs, bc);
    case asBC_CMPIf:     return BcCmpIf(regs, bc);
    case asBC_CMPIu:     return BcCmpIu(regs, bc);
    case asBC_PopRPtr:   return BcPopRPtr(regs, bc);
    case asBC_PshRPtr:   return BcPshRPtr(regs, bc);
    case asBC_STR:       return BcStr(regs, bc);
    case asBC_SetV4:     return BcSetV4(regs, bc);
    case asBC_SetV8:     return BcSetV8(regs, bc);
    case asBC_SetV1:     return BcSetV4(regs, bc);
    case asBC_SetV2:     return BcSetV4(regs, bc);
    case asBC_ADDSi:     return BcAddSi(regs, bc);
    case asBC_CpyVtoV4:  return BcCpyVtoV4(regs, bc);
    case asBC_CpyVtoV8:  return BcCpyVtoV8(regs, bc);
    case asBC_CpyVtoR4:  return BcCpyVtoR4(regs, bc);
    case asBC_CpyVtoR8:  return BcCpyVtoR8(regs, bc);
    case asBC_CpyVtoG4:  return BcCpyVtoG4(regs, bc);
    case asBC_CpyRtoV4:  return BcCpyRtoV4(regs, bc);
    case asBC_CpyRtoV8:  return BcCpyRtoV8(regs, bc);
    case asBC_CpyGtoV4:  return BcCpyGtoV4(regs, bc);
    case asBC_WRTV1:     return BcWrtV1(regs, bc);
    case asBC_WRTV2:     return BcWrtV2(regs, bc);
    case asBC_WRTV4:     return BcWrtV4(regs, bc);
    case asBC_WRTV8:     return BcWrtV8(regs, bc);
    case asBC_RDR1:      return BcRdr1(regs, bc);
    case asBC_RDR2:      return BcRdr2(regs, bc);
    case asBC_RDR4:      return BcRdr4(regs, bc);
    case asBC_RDR8:      return BcRdr8(regs, bc);
    case asBC_LDG:       return BcLdg(regs, bc);
    case asBC_LDV:       return BcLdv(regs, bc);
    case asBC_PGA:       return BcPga(regs, bc);
    case asBC_CmpPtr:    return BcCmpPtr(regs, bc);
    case asBC_VAR:       return BcVar(regs, bc);
    case asBC_iTOf:      return BcItof(regs, bc);
    case asBC_fTOi:      return BcFtoi(regs, bc);
    case asBC_uTOf:      return BcUtof(regs, bc);
    case asBC_fTOu:      return BcFtoU(regs, bc);
    case asBC_sbTOi:     return BcSbtoi(regs, bc);
    case asBC_swTOi:     return BcSwtoi(regs, bc);
    case asBC_ubTOi:     return BcUbtoi(regs, bc);
    case asBC_uwTOi:     return BcUwtoi(regs, bc);
    case asBC_dTOi:      return BcDtoi(regs, bc);
    case asBC_dTOu:      return BcDtoU(regs, bc);
    case asBC_dTOf:      return BcDtof(regs, bc);
    case asBC_iTOd:      return BcItod(regs, bc);
    case asBC_uTOd:      return BcUtod(regs, bc);
    case asBC_fTOd:      return BcFtod(regs, bc);
    case asBC_ADDi:      return BcAddi(regs, bc);
    case asBC_SUBi:      return BcSubi(regs, bc);
    case asBC_MULi:      return BcMuli(regs, bc);
    case asBC_DIVi:      return BcDivi(regs, bc);
    case asBC_MODi:      return BcModi(regs, bc);
    case asBC_ADDf:      return BcAddf(regs, bc);
    case asBC_SUBf:      return BcSubf(regs, bc);
    case asBC_MULf:      return BcMulf(regs, bc);
    case asBC_DIVf:      return BcDivf(regs, bc);
    case asBC_MODf:      return BcModf(regs, bc);
    case asBC_ADDd:      return BcAddd(regs, bc);
    case asBC_SUBd:      return BcSubd(regs, bc);
    case asBC_MULd:      return BcMuld(regs, bc);
    case asBC_DIVd:      return BcDivd(regs, bc);
    case asBC_MODd:      return BcModd(regs, bc);
    case asBC_ADDIi:     return BcAddIi(regs, bc);
    case asBC_SUBIi:     return BcSubIi(regs, bc);
    case asBC_MULIi:     return BcMulIi(regs, bc);
    case asBC_ADDIf:     return BcAddIf(regs, bc);
    case asBC_SUBIf:     return BcSubIf(regs, bc);
    case asBC_MULIf:     return BcMulIf(regs, bc);
    case asBC_SetG4:     return BcSetG4(regs, bc);
    case asBC_iTOb:      return BcItob(regs, bc);
    case asBC_iTOw:      return BcItow(regs, bc);
    case asBC_i64TOi:    return BcI64toi(regs, bc);
    case asBC_uTOi64:    return BcUtoi64(regs, bc);
    case asBC_iTOi64:    return BcItoi64(regs, bc);
    case asBC_fTOi64:    return BcFtoi64(regs, bc);
    case asBC_dTOi64:    return BcDtoi64(regs, bc);
    case asBC_fTOu64:    return BcFtoU64(regs, bc);
    case asBC_dTOu64:    return BcDtoU64(regs, bc);
    case asBC_i64TOf:    return BcI64tof(regs, bc);
    case asBC_u64TOf:    return BcU64tof(regs, bc);
    case asBC_i64TOd:    return BcI64tod(regs, bc);
    case asBC_u64TOd:    return BcU64tod(regs, bc);
    case asBC_NEGi64:    return BcNegi64(regs, bc);
    case asBC_INCi64:    return BcInci64(regs, bc);
    case asBC_DECi64:    return BcDeci64(regs, bc);
    case asBC_BNOT64:    return BcBnot64(regs, bc);
    case asBC_ADDi64:    return BcAddi64(regs, bc);
    case asBC_SUBi64:    return BcSubi64(regs, bc);
    case asBC_MULi64:    return BcMuli64(regs, bc);
    case asBC_DIVi64:    return BcDivi64(regs, bc);
    case asBC_MODi64:    return BcModi64(regs, bc);
    case asBC_BAND64:    return BcBand64(regs, bc);
    case asBC_BOR64:     return BcBor64(regs, bc);
    case asBC_BXOR64:    return BcBxor64(regs, bc);
    case asBC_BSLL64:    return BcBsll64(regs, bc);
    case asBC_BSRL64:    return BcBsrl64(regs, bc);
    case asBC_BSRA64:    return BcBsra64(regs, bc);
    case asBC_CMPi64:    return BcCmpi64(regs, bc);
    case asBC_CMPu64:    return BcCmpu64(regs, bc);
    case asBC_ClrHi:     return BcClrHi(regs, bc);
    case asBC_PshV8:     return BcPshV8(regs, bc);
    case asBC_DIVu:      return BcDivu(regs, bc);
    case asBC_MODu:      return BcModu(regs, bc);
    case asBC_DIVu64:    return BcDivu64(regs, bc);
    case asBC_MODu64:    return BcModu64(regs, bc);
    case asBC_JitEntry:  return BcJitEntry(regs, bc);
    case asBC_CALL:      return BcCall(regs, bc);
    case asBC_RET:       return BcRet(regs, bc);
    case asBC_CALLSYS:   return BcCallSys(regs, bc);
    case asBC_CALLBND:   return BcCallBnd(regs, bc);
    case asBC_CALLINTF:  return BcCallIntf(regs, bc);
    case asBC_CallPtr:   return BcCallPtr(regs, bc);
    case asBC_ALLOC:     return BcAlloc(regs, bc);
    case asBC_Thiscall1: return BcThiscall1(regs, bc);
    case asBC_JMPP:      return BcJmpP(regs, bc);
    case asBC_SUSPEND:   return BcSuspend(regs, bc);
    case asBC_FREE:      return BcFree(regs, bc);
    case asBC_LOADOBJ:   return BcLoadObj(regs, bc);
    case asBC_STOREOBJ:  return BcStoreObj(regs, bc);
    case asBC_GETOBJ:    return BcGetObj(regs, bc);
    case asBC_GETOBJREF: return BcGetObjRef(regs, bc);
    case asBC_GETREF:    return BcGetRef(regs, bc);
    case asBC_REFCPY:    return BcRefCpy(regs, bc);
    case asBC_CHKREF:    return BcChkRef(regs, bc);
    case asBC_ChkRefS:   return BcChkRefS(regs, bc);
    case asBC_ChkNullV:  return BcChkNullV(regs, bc);
    case asBC_ChkNullS:  return BcChkNullS(regs, bc);
    case asBC_PshNull:   return BcPshNull(regs, bc);
    case asBC_ClrVPtr:   return BcClrVPtr(regs, bc);
    case asBC_OBJTYPE:   return BcObjType(regs, bc);
    case asBC_TYPEID:    return BcTypeId(regs, bc);
    case asBC_Cast:      return BcCast(regs, bc);
    case asBC_FuncPtr:   return BcFuncPtr(regs, bc);
    case asBC_LoadThisR: return BcLoadThisR(regs, bc);
    case asBC_LoadRObjR: return BcLoadRObjR(regs, bc);
    case asBC_LoadVObjR: return BcLoadVObjR(regs, bc);
    case asBC_RefCpyV:   return BcRefCpyV(regs, bc);
    case asBC_AllocMem:  return BcAllocMem(regs, bc);
    case asBC_SetListSize:  return BcSetListSize(regs, bc);
    case asBC_PshListElmnt: return BcPshListElmnt(regs, bc);
    case asBC_SetListType:  return BcSetListType(regs, bc);
    case asBC_POWi:      return BcPowi(regs, bc);
    case asBC_POWu:      return BcPowu(regs, bc);
    case asBC_POWf:      return BcPowf(regs, bc);
    case asBC_POWd:      return BcPowd(regs, bc);
    case asBC_POWdi:     return BcPowdi(regs, bc);
    case asBC_POWi64:    return BcPowi64(regs, bc);
    case asBC_POWu64:    return BcPowu64(regs, bc);
    default:
        assert(false);
        return JITBC_EXIT;
    }
}

}
