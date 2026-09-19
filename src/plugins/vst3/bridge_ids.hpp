#pragma once

#include "pluginterfaces/base/funknown.h"

namespace UniversalBridge::Vst3Client {

enum ParameterId : Steinberg::Vst::ParamID {
    kBridgeEnabledId = 100,
    kBypassId = 101,
};

static DECLARE_UID(kProcessorUid, 0x23ABE5AD, 0x23D84F15, 0xBDC6A80C,
                   0x4D20DFA2) static DECLARE_UID(kControllerUid, 0x17DCD884, 0x7CE64C17, 0xBA1EF0FA, 0x321FF840)

} // namespace UniversalBridge::Vst3Client
