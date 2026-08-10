#pragma once

#include "angelscript.h"

namespace asjitx86 {

const asSBCInfo* GetBcInfo();

int BcSize(asEBCInstr op);

}
