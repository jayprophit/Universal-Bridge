#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"

namespace UniversalBridge::Vst3Client {

// The experimental client is deliberately audio-transparent. It only proves the
// host-facing component/state boundary; service IPC is not performed from the
// processor and no operating-system call is permitted in process().
class BridgeProcessor final : public Steinberg::Vst::AudioEffect {
  public:
    BridgeProcessor();

    static Steinberg::FUnknown* create_instance(void*);

    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) override;
    Steinberg::tresult PLUGIN_API setBusArrangements(Steinberg::Vst::SpeakerArrangement* inputs,
                                                     Steinberg::int32 input_count,
                                                     Steinberg::Vst::SpeakerArrangement* outputs,
                                                     Steinberg::int32 output_count) override;
    Steinberg::tresult PLUGIN_API canProcessSampleSize(Steinberg::int32 sample_size) override;
    Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData& data) override;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state) override;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state) override;

  private:
    bool bridge_enabled_{false};
    bool bypass_{false};
};

} // namespace UniversalBridge::Vst3Client
