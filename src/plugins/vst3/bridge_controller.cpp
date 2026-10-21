#include "bridge_controller.hpp"

#include "bridge_ids.hpp"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"

namespace UniversalBridge::Vst3Client {
namespace {

constexpr Steinberg::int32 kStateMagic = 0x55425631;
constexpr Steinberg::int32 kStateSchema = 1;

} // namespace

Steinberg::FUnknown* BridgeController::create_instance(void*) {
    return static_cast<Steinberg::Vst::IEditController*>(new BridgeController());
}

Steinberg::tresult PLUGIN_API BridgeController::initialize(Steinberg::FUnknown* context) {
    const auto result = EditController::initialize(context);
    if (result != Steinberg::kResultOk) {
        return result;
    }
    parameters.addParameter(STR16("Bridge Enabled"), nullptr, 1, 0.0, Steinberg::Vst::ParameterInfo::kCanAutomate,
                            kBridgeEnabledId);
    parameters.addParameter(STR16("Bypass"), nullptr, 1, 0.0,
                            Steinberg::Vst::ParameterInfo::kCanAutomate | Steinberg::Vst::ParameterInfo::kIsBypass,
                            kBypassId);
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API BridgeController::setComponentState(Steinberg::IBStream* state) {
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
    setParamNormalized(kBridgeEnabledId, enabled != 0 ? 1.0 : 0.0);
    setParamNormalized(kBypassId, bypass != 0 ? 1.0 : 0.0);
    return Steinberg::kResultOk;
}

} // namespace UniversalBridge::Vst3Client
