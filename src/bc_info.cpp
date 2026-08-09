#include "bc_info.h"

namespace asjitx86 {

const asSBCInfo* GetBcInfo() {
    return asBCInfo;
}

int BcSize(asEBCInstr op) {
    return asBCTypeSize[asBCInfo[op].type];
}

}
