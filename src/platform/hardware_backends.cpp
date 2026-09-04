#include "ubridge/platform/hardware_backends.hpp"
#ifdef _WIN32
#include <windows.h>
#include <cfgmgr32.h>
#include <initguid.h>
#include <devpkey.h>
#include <propkey.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <mmsystem.h>
#include <propvarutil.h>
#endif
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cwchar>
#include <cwctype>
#include <map>
#include <optional>
#include <unordered_map>
#include <limits>

namespace ubridge::platform {
namespace {
std::string normalized(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return std::isalnum(c) ? static_cast<char>(std::tolower(c)) : ' ';
    });
    value.erase(std::unique(value.begin(), value.end(), [](char a, char b) { return a == ' ' && b == ' '; }), value.end());
    return value;
}

int text_score(const std::string& candidate, const std::vector<std::string>& hints) {
    const auto haystack = normalized(candidate);
    int score = 0;
    for (const auto& hint : hints) {
        const auto needle = normalized(hint);
        if (!needle.empty() && haystack.find(needle) != std::string::npos) score += 20;
    }
    return score;
}
#ifdef _WIN32
std::string utf8(const std::wstring& v) {
    if (v.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, v.data(), static_cast<int>(v.size()), nullptr, 0, nullptr, nullptr);
    std::string r(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, v.data(), static_cast<int>(v.size()), r.data(), n, nullptr, nullptr);
    return r;
}
std::wstring property(DEVINST node, const DEVPROPKEY& key) {
    DEVPROPTYPE type{}; ULONG size = 0;
    if (CM_Get_DevNode_PropertyW(node, &key, &type, nullptr, &size, 0) != CR_BUFFER_SMALL || !size) return {};
    std::vector<BYTE> bytes(size);
    if (CM_Get_DevNode_PropertyW(node, &key, &type, bytes.data(), &size, 0) != CR_SUCCESS) return {};
    return reinterpret_cast<const wchar_t*>(bytes.data());
}
std::string guid_property(DEVINST node) {
    DEVPROPTYPE type{}; GUID value{}; ULONG size = sizeof(value);
    if (CM_Get_DevNode_PropertyW(node, &DEVPKEY_Device_ContainerId, &type, reinterpret_cast<PBYTE>(&value), &size, 0) != CR_SUCCESS || type != DEVPROP_TYPE_GUID) return {};
    wchar_t b[39]{};
    swprintf_s(b, L"{%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}", value.Data1, value.Data2, value.Data3, value.Data4[0], value.Data4[1], value.Data4[2], value.Data4[3], value.Data4[4], value.Data4[5], value.Data4[6], value.Data4[7]);
    return utf8(b);
}
class WindowsDeviceDiscovery final : public IDeviceDiscovery {
public:
    std::vector<DiscoveredDevice> enumerate() override {
        ULONG chars = 0;
        if (CM_Get_Device_ID_List_SizeW(&chars, nullptr, CM_GETIDLIST_FILTER_PRESENT) != CR_SUCCESS || chars < 2) return {};
        std::vector<wchar_t> ids(chars);
        if (CM_Get_Device_ID_ListW(nullptr, ids.data(), chars, CM_GETIDLIST_FILTER_PRESENT) != CR_SUCCESS) return {};
        std::map<std::string, DiscoveredDevice> grouped;
        for (const wchar_t* p = ids.data(); *p; p += std::wcslen(p) + 1) {
            std::wstring id(p), upper(id);
            std::transform(upper.begin(), upper.end(), upper.begin(), [](wchar_t c) { return std::towupper(c); });
            if (upper.find(L"USB\\VID_") == std::wstring::npos) continue;
            DEVINST node{}; if (CM_Locate_DevNodeW(&node, id.data(), 0) != CR_SUCCESS) continue;
            const auto container = guid_property(node); auto& device = grouped[container.empty() ? utf8(id) : container];
            const auto vid = upper.find(L"VID_"), pid = upper.find(L"PID_");
            if (vid != std::wstring::npos) device.vendor_id = static_cast<std::uint16_t>(std::wcstoul(upper.substr(vid + 4, 4).c_str(), nullptr, 16));
            if (pid != std::wstring::npos) device.product_id = static_cast<std::uint16_t>(std::wcstoul(upper.substr(pid + 4, 4).c_str(), nullptr, 16));
            device.container_id = container; const auto name = utf8(property(node, DEVPKEY_Device_FriendlyName));
            device.display_name = name.empty() ? utf8(id) : name;
            const bool mpc = device.vendor_id == mpc_sample_vendor_id && device.product_id == mpc_sample_product_id;
            if (mpc) device.display_name = "Akai MPC Sample (profile match; observed USB identity)";
            UsbInterface item{utf8(id), container, {}, utf8(property(node, DEVPKEY_Device_Service)), name};
            const auto mi = upper.find(L"&MI_"); if (mi != std::wstring::npos && mi + 6 <= upper.size()) item.interface_number = utf8(upper.substr(mi + 4, 2));
            device.interfaces.push_back(std::move(item));
        }
        std::vector<DiscoveredDevice> result;
        for (auto& [_, device] : grouped) {
            const bool mpc = device.vendor_id == mpc_sample_vendor_id && device.product_id == mpc_sample_product_id;
            device.diagnostics.push_back({core::DiagnosticSeverity::warning, mpc ? "mpc_sample_profile_match" : "generic_usb_observation", mpc ? "MPC Sample profile matched by USB identity; no proprietary control or write capability is enabled." : "Generic USB interfaces observed; protocol and write capability were not inferred.", "Qualify every endpoint independently before enabling control or synchronization."});
            result.push_back(std::move(device));
        }
        return result;
    }
};

std::string in_id(UINT i) { return "winmm-in:" + std::to_string(i); }
std::string out_id(UINT i) { return "winmm-out:" + std::to_string(i); }
class WindowsMidiBackend final : public IMidiBackend {
    struct Input { HMIDIIN handle{}; ReceiveCallback callback; };
    std::unordered_map<std::string, std::unique_ptr<Input>> inputs_;
    std::unordered_map<std::string, HMIDIOUT> outputs_;
    static std::optional<UINT> index(std::string_view id, std::string_view prefix) {
        if (!id.starts_with(prefix)) return std::nullopt;
        try { return static_cast<UINT>(std::stoul(std::string(id.substr(prefix.size())))); } catch (...) { return std::nullopt; }
    }
    static void CALLBACK receive(HMIDIIN, UINT type, DWORD_PTR instance, DWORD_PTR data, DWORD_PTR timestamp) {
        if (type != MIM_DATA || !instance) return;
        auto* input = reinterpret_cast<Input*>(instance);
        MidiMessage event;
        const auto status = static_cast<std::uint8_t>(data & 0xffU);
        const std::size_t count = ((status & 0xf0U) == 0xc0U || (status & 0xf0U) == 0xd0U) ? 2U : 3U;
        for (std::size_t i = 0; i < count; ++i) event.bytes.push_back(static_cast<std::uint8_t>((data >> (i * 8U)) & 0xffU));
        event.timestamp_microseconds = static_cast<std::uint64_t>(timestamp) * 1000ULL; if (input->callback) input->callback(event);
    }
public:
    ~WindowsMidiBackend() override { while (!inputs_.empty()) close(inputs_.begin()->first); while (!outputs_.empty()) close(outputs_.begin()->first); }
    BackendMaturity maturity() const noexcept override { return BackendMaturity::experimental; }
    std::vector<MidiEndpoint> enumerate_endpoints() const override {
        std::vector<MidiEndpoint> r;
        for (UINT i = 0; i < midiInGetNumDevs(); ++i) { MIDIINCAPSW c{}; if (midiInGetDevCapsW(i, &c, sizeof(c)) == MMSYSERR_NOERROR) r.push_back({in_id(i), utf8(c.szPname), std::to_string(c.wMid), EndpointDirection::input, "Windows WinMM fallback", "MIDI 1.0", true}); }
        for (UINT i = 0; i < midiOutGetNumDevs(); ++i) { MIDIOUTCAPSW c{}; if (midiOutGetDevCapsW(i, &c, sizeof(c)) == MMSYSERR_NOERROR) r.push_back({out_id(i), utf8(c.szPname), std::to_string(c.wMid), EndpointDirection::output, "Windows WinMM fallback", "MIDI 1.0", true}); }
        return r;
    }
    bool open_input(std::string_view id, ReceiveCallback callback) override {
        const auto i = index(id, "winmm-in:"); if (!i || inputs_.contains(std::string(id))) return false;
        auto input = std::make_unique<Input>(); input->callback = std::move(callback); HMIDIIN h{};
        if (midiInOpen(&h, *i, reinterpret_cast<DWORD_PTR>(&receive), reinterpret_cast<DWORD_PTR>(input.get()), CALLBACK_FUNCTION) != MMSYSERR_NOERROR) return false;
        input->handle = h; if (midiInStart(h) != MMSYSERR_NOERROR) { midiInClose(h); return false; }
        inputs_.emplace(std::string(id), std::move(input)); return true;
    }
    bool open_output(std::string_view id) override {
        const auto i = index(id, "winmm-out:"); if (!i || outputs_.contains(std::string(id))) return false;
        HMIDIOUT h{}; if (midiOutOpen(&h, *i, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) return false;
        outputs_.emplace(std::string(id), h); return true;
    }
    void close(std::string_view endpoint) noexcept override {
        const std::string id(endpoint);
        if (const auto it = inputs_.find(id); it != inputs_.end()) { midiInStop(it->second->handle); midiInReset(it->second->handle); midiInClose(it->second->handle); inputs_.erase(it); return; }
        if (const auto it = outputs_.find(id); it != outputs_.end()) { midiOutReset(it->second); midiOutClose(it->second); outputs_.erase(it); }
    }
    bool connected(std::string_view id) const noexcept override { return inputs_.contains(std::string(id)) || outputs_.contains(std::string(id)); }
    bool send(std::string_view id, std::span<const std::uint8_t> bytes) override {
        const auto it = outputs_.find(std::string(id)); if (it == outputs_.end() || bytes.empty() || bytes.size() > 3) return false;
        DWORD packed = 0; for (std::size_t i = 0; i < bytes.size(); ++i) packed |= static_cast<DWORD>(bytes[i]) << (i * 8U);
        return midiOutShortMsg(it->second, packed) == MMSYSERR_NOERROR;
    }
};

class WindowsAudioBackend final : public IAudioBackend {
public:
    BackendMaturity maturity() const noexcept override { return BackendMaturity::experimental; }
    std::vector<AudioEndpoint> enumerate_endpoints() const override {
        std::vector<AudioEndpoint> r; const HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED); const bool cleanup = SUCCEEDED(init);
        IMMDeviceEnumerator* e = nullptr; if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&e)))) { if (cleanup) CoUninitialize(); return r; }
        for (const auto direction : {EndpointDirection::input, EndpointDirection::output}) {
            IMMDeviceCollection* c = nullptr; const auto flow = direction == EndpointDirection::input ? eCapture : eRender;
            if (SUCCEEDED(e->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, &c))) { UINT count = 0; c->GetCount(&count);
                for (UINT i = 0; i < count; ++i) {
                    IMMDevice* d = nullptr; if (FAILED(c->Item(i, &d))) continue;
                    LPWSTR id = nullptr; IPropertyStore* s = nullptr; PROPVARIANT name; PropVariantInit(&name);
                    d->GetId(&id); d->OpenPropertyStore(STGM_READ, &s); if (s) s->GetValue(PKEY_Device_FriendlyName, &name);
                    AudioEndpoint endpoint{id ? utf8(id) : std::string{}, name.vt == VT_LPWSTR ? utf8(name.pwszVal) : std::string{}, direction, true};
                    IAudioClient* client = nullptr;
                    if (SUCCEEDED(d->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&client)))) {
                        WAVEFORMATEX* format = nullptr;
                        if (SUCCEEDED(client->GetMixFormat(&format)) && format) {
                            endpoint.sample_rate = format->nSamplesPerSec;
                            endpoint.channels = format->nChannels;
                            endpoint.bits_per_sample = format->wBitsPerSample;
                            CoTaskMemFree(format);
                        }
                        client->Release();
                    }
                    r.push_back(std::move(endpoint));
                    PropVariantClear(&name); if (s) s->Release(); if (id) CoTaskMemFree(id); d->Release();
                }
                c->Release(); }
        }
        e->Release(); if (cleanup) CoUninitialize(); return r;
    }
    AudioSignalProbe probe_input(std::string_view endpoint_id, std::uint32_t duration_ms) override {
        AudioSignalProbe result; const HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED); const bool cleanup = SUCCEEDED(init);
        IMMDeviceEnumerator* enumerator = nullptr; IMMDevice* device = nullptr; IAudioClient* client = nullptr; IAudioCaptureClient* capture = nullptr; WAVEFORMATEX* format = nullptr;
        const std::wstring id(endpoint_id.begin(), endpoint_id.end());
        if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator))) ||
            FAILED(enumerator->GetDevice(id.c_str(), &device)) ||
            FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&client))) ||
            FAILED(client->GetMixFormat(&format))) {
            result.status = "open_failed"; goto cleanup_probe;
        }
        {
            const REFERENCE_TIME buffer_duration = 1'000'000;
            if (FAILED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, buffer_duration, 0, format, nullptr)) ||
                FAILED(client->GetService(IID_PPV_ARGS(&capture))) || FAILED(client->Start())) {
                result.status = "initialize_failed"; goto cleanup_probe;
            }
            result.opened = true; result.status = "silence";
            const bool floating = format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
                (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                 reinterpret_cast<WAVEFORMATEXTENSIBLE*>(format)->SubFormat.Data1 == WAVE_FORMAT_IEEE_FLOAT);
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(duration_ms);
            while (std::chrono::steady_clock::now() < deadline) {
                UINT32 packets = 0; if (FAILED(capture->GetNextPacketSize(&packets))) { result.status = "read_failed"; break; }
                while (packets > 0) {
                    BYTE* data = nullptr; UINT32 frames = 0; DWORD flags = 0;
                    if (FAILED(capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr))) { result.status = "read_failed"; break; }
                    result.frames_observed += frames;
                    if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT) && data) {
                        const std::size_t samples = static_cast<std::size_t>(frames) * format->nChannels;
                        if (floating && format->wBitsPerSample == 32) {
                            const auto* values = reinterpret_cast<const float*>(data);
                            for (std::size_t i = 0; i < samples; ++i) result.peak = (std::max)(result.peak, std::abs(values[i]));
                        } else if (format->wBitsPerSample == 16) {
                            const auto* values = reinterpret_cast<const std::int16_t*>(data);
                            for (std::size_t i = 0; i < samples; ++i) result.peak = (std::max)(result.peak, std::abs(static_cast<float>(values[i]) / 32768.0F));
                        }
                    }
                    capture->ReleaseBuffer(frames); if (FAILED(capture->GetNextPacketSize(&packets))) { packets = 0; result.status = "read_failed"; }
                }
                Sleep(10);
            }
            client->Stop(); if (result.peak > 0.00001F) result.status = "signal_observed";
        }
cleanup_probe:
        if (capture) capture->Release();
        if (format) CoTaskMemFree(format);
        if (client) client->Release();
        if (device) device->Release();
        if (enumerator) enumerator->Release();
        if (cleanup) CoUninitialize();
        return result;
    }
    bool begin_capture(std::string_view, int, int) override { return false; }
    void stop_capture() noexcept override {}
};
#else
class NullDeviceDiscovery final : public IDeviceDiscovery { public: std::vector<DiscoveredDevice> enumerate() override { return {}; } };
class NullMidiBackend final : public IMidiBackend { public: BackendMaturity maturity() const noexcept override { return BackendMaturity::unavailable; } std::vector<MidiEndpoint> enumerate_endpoints() const override { return {}; } bool open_input(std::string_view, ReceiveCallback) override { return false; } bool open_output(std::string_view) override { return false; } void close(std::string_view) noexcept override {} bool connected(std::string_view) const noexcept override { return false; } bool send(std::string_view, std::span<const std::uint8_t>) override { return false; } };
class NullAudioBackend final : public IAudioBackend { public: BackendMaturity maturity() const noexcept override { return BackendMaturity::unavailable; } std::vector<AudioEndpoint> enumerate_endpoints() const override { return {}; } AudioSignalProbe probe_input(std::string_view, std::uint32_t) override { return {}; } bool begin_capture(std::string_view, int, int) override { return false; } void stop_capture() noexcept override {} };
#endif
} // namespace

std::unique_ptr<IDeviceDiscovery> make_system_device_discovery() {
#ifdef _WIN32
    return std::make_unique<WindowsDeviceDiscovery>();
#else
    return std::make_unique<NullDeviceDiscovery>();
#endif
}
std::unique_ptr<IMidiBackend> make_system_midi_backend() {
#ifdef _WIN32
    return std::make_unique<WindowsMidiBackend>();
#else
    return std::make_unique<NullMidiBackend>();
#endif
}
std::unique_ptr<IAudioBackend> make_system_audio_backend() {
#ifdef _WIN32
    return std::make_unique<WindowsAudioBackend>();
#else
    return std::make_unique<NullAudioBackend>();
#endif
}
std::string to_string(BackendMaturity v) { switch (v) { case BackendMaturity::unavailable: return "unavailable"; case BackendMaturity::scaffold: return "scaffold"; case BackendMaturity::experimental: return "experimental"; case BackendMaturity::qualified: return "qualified"; } return "unavailable"; }
std::string to_string(EndpointDirection v) { return v == EndpointDirection::input ? "input" : "output"; }
std::string midi_message_semantic(std::span<const std::uint8_t> message) {
    if (message.empty()) return "empty";
    const auto status = message[0];
    if (status >= 0xf0U) {
        switch (status) {
        case 0xf0U: return "system_exclusive";
        case 0xf1U: return "midi_time_code_quarter_frame";
        case 0xf2U: return "song_position_pointer";
        case 0xf3U: return "song_select";
        case 0xf6U: return "tune_request";
        case 0xf7U: return "system_exclusive_end";
        case 0xf8U: return "timing_clock";
        case 0xfaU: return "transport_start";
        case 0xfbU: return "transport_continue";
        case 0xfcU: return "transport_stop";
        case 0xfeU: return "active_sensing";
        case 0xffU: return "system_reset";
        default: return "system_undefined";
        }
    }
    switch (static_cast<std::uint8_t>(status & 0xf0U)) {
    case 0x80U: return "note_off";
    case 0x90U: return message.size() > 2 && message[2] == 0 ? "note_off" : "note_on";
    case 0xa0U: return "poly_key_pressure";
    case 0xb0U: return "control_change";
    case 0xc0U: return "program_change";
    case 0xd0U: return "channel_pressure";
    case 0xe0U: return "pitch_bend";
    default: return "unknown";
    }
}
EndpointMatch resolve_midi_endpoint(const std::vector<MidiEndpoint>& endpoints, const EndpointMatchRule& rule) {
    EndpointMatch result; int best = std::numeric_limits<int>::lowest();
    for (const auto& endpoint : endpoints) {
        if (!endpoint.available || endpoint.direction != rule.direction) continue;
        int score = 10 + text_score(endpoint.name + " " + endpoint.manufacturer, rule.name_hints);
        if (!rule.backend_hint.empty() && normalized(endpoint.backend).find(normalized(rule.backend_hint)) != std::string::npos) score += 5;
        if (!rule.protocol_hint.empty() && normalized(endpoint.protocol).find(normalized(rule.protocol_hint)) != std::string::npos) score += 5;
        if (score > best) { best = score; result = {endpoint.id, endpoint.name, score, false, "Resolved from portable MIDI role criteria at runtime."}; }
        else if (score == best) result.ambiguous = true;
    }
    if (best == std::numeric_limits<int>::lowest()) result.explanation = "No available MIDI endpoint satisfies the required direction.";
    else if (result.ambiguous) result.explanation = "More than one MIDI endpoint has the same best score; user selection is required before opening either endpoint.";
    return result;
}

EndpointMatch resolve_audio_endpoint(const std::vector<AudioEndpoint>& endpoints, const EndpointMatchRule& rule) {
    EndpointMatch result; int best = std::numeric_limits<int>::lowest();
    for (const auto& endpoint : endpoints) {
        if (!endpoint.active || endpoint.direction != rule.direction || endpoint.channels < rule.minimum_channels) continue;
        const int score = 10 + text_score(endpoint.name, rule.name_hints) + (endpoint.channels == rule.minimum_channels && rule.minimum_channels > 0 ? 3 : 0);
        if (score > best) { best = score; result = {endpoint.id, endpoint.name, score, false, "Resolved from portable audio role criteria at runtime."}; }
        else if (score == best) result.ambiguous = true;
    }
    if (best == std::numeric_limits<int>::lowest()) result.explanation = "No active audio endpoint satisfies the required direction and channel capacity.";
    else if (result.ambiguous) result.explanation = "More than one audio endpoint has the same best score; user selection is required before capture.";
    return result;
}
std::vector<core::ProtocolEvidence> protocol_evidence_for(const DiscoveredDevice& device) {
    std::vector<core::ProtocolEvidence> r;
    for (const auto& item : device.interfaces) { std::string service = item.service; std::transform(service.begin(), service.end(), service.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); }); core::ProtocolEvidence e; e.level = core::EvidenceLevel::observed; e.endpoint_id = item.instance_id;
        if (service == "usbccgp") { e.protocol = core::ProtocolKind::usb_composite; e.source = "Windows reported a USB composite parent; no child protocol was inferred."; }
        else if (service == "usbaudio" || service == "usbaudio2") { e.protocol = core::ProtocolKind::usb_audio; e.source = "Windows reported USB Audio class service '" + service + "'; stream direction and channels were not probed."; }
        else { e.protocol = core::ProtocolKind::unknown; e.source = "Windows exposed the interface, but its protocol and function remain unknown."; }
        r.push_back(std::move(e)); }
    return r;
}
} // namespace ubridge::platform
