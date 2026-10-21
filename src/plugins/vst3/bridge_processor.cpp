#include "bridge_processor.hpp"

#include "bridge_ids.hpp"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#include <algorithm>
#include <cstdint>

namespace UniversalBridge::Vst3Client {
namespace {

constexpr Steinberg::int32 kStateMagic = 0x55425631; // "UBV1"
constexpr Steinberg::int32 kStateSchema = 1;

template <typename Sample>
void copy_audio(const Steinberg::Vst::AudioBusBuffers& input, Steinberg::Vst::AudioBusBuffers& output,
                const Steinberg::int32 sample_count) noexcept {
    const auto channel_count = std::min(input.numChannels, output.numChannels);
    auto** input_channels = reinterpret_cast<Sample**>(input.channelBuffers32);
    auto** output_channels = reinterpret_cast<Sample**>(output.channelBuffers32);
    for (Steinberg::int32 channel = 0; channel < channel_count; ++channel) {
        if (input_channels[channel] != output_channels[channel]) {
            std::copy_n(input_channels[channel], sample_count, output_channels[channel]);
        }
    }
    output.silenceFlags = input.silenceFlags;
}

} // namespace

BridgeProcessor::BridgeProcessor() { setControllerClass(Steinberg::FUID::fromTUID(kControllerUid)); }

Steinberg::FUnknown* BridgeProcessor::create_instance(void*) {
    return static_cast<Steinberg::Vst::IAudioProcessor*>(new BridgeProcessor());
}

Steinberg::tresult PLUGIN_API BridgeProcessor::initialize(Steinberg::FUnknown* context) {
    const auto result = AudioEffect::initialize(context);
    if (result != Steinberg::kResultOk) {
        return result;
    }
    addAudioInput(STR16("Stereo In"), Steinberg::Vst::SpeakerArr::kStereo);
    addAudioOutput(STR16("Stereo Out"), Steinberg::Vst::SpeakerArr::kStereo);
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API BridgeProcessor::setBusArrangements(Steinberg::Vst::SpeakerArrangement* inputs,
                                                                  const Steinberg::int32 input_count,
                                                                  Steinberg::Vst::SpeakerArrangement* outputs,
                                                                  const Steinberg::int32 output_count) {
    if (input_count != 1 || output_count != 1 || inputs[0] != outputs[0]) {
        return Steinberg::kResultFalse;
    }
    const auto channels = Steinberg::Vst::SpeakerArr::getChannelCount(inputs[0]);
    if (channels != 1 && channels != 2) {
        return Steinberg::kResultFalse;
    }
    return AudioEffect::setBusArrangements(inputs, input_count, outputs, output_count);
}

Steinberg::tresult PLUGIN_API BridgeProcessor::canProcessSampleSize(const Steinberg::int32 sample_size) {
    return (sample_size == Steinberg::Vst::kSample32 || sample_size == Steinberg::Vst::kSample64)
               ? Steinberg::kResultTrue
               : Steinberg::kResultFalse;
}

Steinberg::tresult PLUGIN_API BridgeProcessor::process(Steinberg::Vst::ProcessData& data) {
    if (data.inputParameterChanges != nullptr) {
        const auto parameter_count = data.inputParameterChanges->getParameterCount();
        for (Steinberg::int32 index = 0; index < parameter_count; ++index) {
            auto* queue = data.inputParameterChanges->getParameterData(index);
            if (queue == nullptr || queue->getPointCount() == 0) {
                continue;
            }
            Steinberg::int32 sample_offset = 0;
            Steinberg::Vst::ParamValue value = 0.0;
            if (queue->getPoint(queue->getPointCount() - 1, sample_offset, value) != Steinberg::kResultTrue) {
                continue;
            }
            if (queue->getParameterId() == kBridgeEnabledId) {
                bridge_enabled_ = value > 0.5;
            } else if (queue->getParameterId() == kBypassId) {
                bypass_ = value > 0.5;
            }
        }
    }

    // Both enabled and bypass modes are transparent until authenticated service
    // mediation is separately qualified. This path performs bounded memory copies only.
    if (data.numSamples <= 0 || data.numInputs < 1 || data.numOutputs < 1) {
        return Steinberg::kResultOk;
    }
    if (data.inputs[0].numChannels != data.outputs[0].numChannels) {
        return Steinberg::kResultFalse;
    }
    if (data.symbolicSampleSize == Steinberg::Vst::kSample32) {
        copy_audio<Steinberg::Vst::Sample32>(data.inputs[0], data.outputs[0], data.numSamples);
    } else if (data.symbolicSampleSize == Steinberg::Vst::kSample64) {
        copy_audio<Steinberg::Vst::Sample64>(data.inputs[0], data.outputs[0], data.numSamples);
    } else {
        return Steinberg::kResultFalse;
    }
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API BridgeProcessor::setState(Steinberg::IBStream* state) {
    if (state == nullptr) {
        return Steinberg::kInvalidArgument;
    }
    Steinberg::IBStreamer stream(state, kLittleEndian);
    Steinberg::int32 magic = 0;
    Steinberg::int32 schema = 0;
    Steinberg::int32 enabled = 0;
    Steinberg::int32 bypass = 0;
    if (!stream.readInt32(magic) || !stream.readInt32(schema) || !stream.readInt32(enabled) ||
        !stream.readInt32(bypass) || magic != kStateMagic || schema != kStateSchema) {
        return Steinberg::kResultFalse;
    }
    bridge_enabled_ = enabled != 0;
    bypass_ = bypass != 0;
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API BridgeProcessor::getState(Steinberg::IBStream* state) {
    if (state == nullptr) {
        return Steinberg::kInvalidArgument;
    }
    Steinberg::IBStreamer stream(state, kLittleEndian);
    if (!stream.writeInt32(kStateMagic) || !stream.writeInt32(kStateSchema) ||
        !stream.writeInt32(bridge_enabled_ ? 1 : 0) || !stream.writeInt32(bypass_ ? 1 : 0)) {
        return Steinberg::kResultFalse;
    }
    return Steinberg::kResultOk;
}

} // namespace UniversalBridge::Vst3Client
