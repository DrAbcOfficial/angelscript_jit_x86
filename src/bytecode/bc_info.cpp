#include "bytecode/bc_info.h"

namespace asjitx86 {

int BcSize(asEBCInstr op) {
    return asBCTypeSize[asBCInfo[op].type];
}

}
