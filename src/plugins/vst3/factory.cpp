#include "bridge_controller.hpp"
#include "bridge_ids.hpp"
#include "bridge_processor.hpp"

#include "public.sdk/source/main/pluginfactory_constexpr.h"

#define UBRIDGE_VST3_NAME "Universal Bridge Client (Experimental)"
#define UBRIDGE_VST3_VERSION "0.5.0"

BEGIN_FACTORY_DEF("Universal Bridge", "https://github.com/jayprophit/Universal-Bridge", "", 2)

DEF_CLASS(UniversalBridge::Vst3Client::kProcessorUid, Steinberg::PClassInfo::kManyInstances, kVstAudioEffectClass,
          UBRIDGE_VST3_NAME, Steinberg::Vst::kDistributable, "Fx|Tools", UBRIDGE_VST3_VERSION, kVstVersionString,
          UniversalBridge::Vst3Client::BridgeProcessor::create_instance, nullptr)

DEF_CLASS(UniversalBridge::Vst3Client::kControllerUid, Steinberg::PClassInfo::kManyInstances,
          kVstComponentControllerClass, UBRIDGE_VST3_NAME " Controller", 0, "", UBRIDGE_VST3_VERSION, kVstVersionString,
          UniversalBridge::Vst3Client::BridgeController::create_instance, nullptr)

END_FACTORY
