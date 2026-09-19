#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"

namespace UniversalBridge::Vst3Client {

class BridgeController final : public Steinberg::Vst::EditController {
  public:
    static Steinberg::FUnknown* create_instance(void*);

    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) override;
    Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream* state) override;
};

} // namespace UniversalBridge::Vst3Client
