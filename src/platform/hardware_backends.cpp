#include "ubridge/platform/hardware_backends.hpp"

#ifdef _WIN32
#include <cfgmgr32.h>
#include <devpkey.h>
#include <windows.h>
#endif

#include <algorithm>
#include <cstdio>
#include <cwchar>
#include <cwctype>
#include <map>

namespace ubridge::platform {
namespace {

#ifdef _WIN32
std::string utf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring property(DEVINST node, const DEVPROPKEY& key) {
    DEVPROPTYPE type{};
    ULONG size = 0;
    if (CM_Get_DevNode_PropertyW(node, &key, &type, nullptr, &size, 0) != CR_BUFFER_SMALL || size == 0) return {};
    std::vector<BYTE> bytes(size);
    if (CM_Get_DevNode_PropertyW(node, &key, &type, bytes.data(), &size, 0) != CR_SUCCESS) return {};
    return reinterpret_cast<const wchar_t*>(bytes.data());
}

std::string guid_property(DEVINST node, const DEVPROPKEY& key) {
    DEVPROPTYPE type{};
    GUID value{};
    ULONG size = sizeof(value);
    if (CM_Get_DevNode_PropertyW(node, &key, &type, reinterpret_cast<PBYTE>(&value), &size, 0) != CR_SUCCESS || type != DEVPROP_TYPE_GUID) return {};
    wchar_t buffer[39]{};
    swprintf_s(buffer, L"{%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}",
        value.Data1, value.Data2, value.Data3, value.Data4[0], value.Data4[1], value.Data4[2], value.Data4[3],
        value.Data4[4], value.Data4[5], value.Data4[6], value.Data4[7]);
    return utf8(buffer);
}

class WindowsDeviceDiscovery final : public IDeviceDiscovery {
public:
    std::vector<DiscoveredDevice> enumerate() override {
        ULONG chars = 0;
        if (CM_Get_Device_ID_List_SizeW(&chars, nullptr, CM_GETIDLIST_FILTER_PRESENT) != CR_SUCCESS || chars < 2) return {};
        std::vector<wchar_t> ids(chars);
        if (CM_Get_Device_ID_ListW(nullptr, ids.data(), chars, CM_GETIDLIST_FILTER_PRESENT) != CR_SUCCESS) return {};
        std::map<std::string, DiscoveredDevice> grouped;
        for (const wchar_t* current = ids.data(); *current; current += std::wcslen(current) + 1) {
            std::wstring id(current);
            std::wstring upper = id;
            std::transform(upper.begin(), upper.end(), upper.begin(), [](wchar_t c) { return std::towupper(c); });
            if (upper.find(L"VID_09E8&PID_205C") == std::wstring::npos) continue;
            DEVINST node{};
            if (CM_Locate_DevNodeW(&node, id.data(), CM_LOCATE_DEVNODE_NORMAL) != CR_SUCCESS) continue;
            const auto container = guid_property(node, DEVPKEY_Device_ContainerId);
            auto& device = grouped[container.empty() ? utf8(id) : container];
            device.vendor_id = mpc_sample_vendor_id;
            device.product_id = mpc_sample_product_id;
            device.container_id = container;
            device.display_name = "Akai MPC Sample (observed USB identity)";
            UsbInterface interface;
            interface.instance_id = utf8(id);
            interface.container_id = container;
            interface.friendly_name = utf8(property(node, DEVPKEY_Device_FriendlyName));
            interface.service = utf8(property(node, DEVPKEY_Device_Service));
            const auto marker = upper.find(L"&MI_");
            if (marker != std::wstring::npos && marker + 6 <= upper.size()) interface.interface_number = utf8(upper.substr(marker + 4, 2));
            device.interfaces.push_back(std::move(interface));
        }
        std::vector<DiscoveredDevice> result;
        for (auto& [_, device] : grouped) {
            device.diagnostics.push_back({core::DiagnosticSeverity::warning, "mpc_sample_identity_only", "USB identity and shared-container interfaces were observed; no proprietary protocol or write capability is enabled.", "Qualify each interface, including any MI_03 CDC-NCM function, before opening or controlling it."});
            result.push_back(std::move(device));
        }
        return result;
    }
};
#else
class NullDeviceDiscovery final : public IDeviceDiscovery {
public:
    std::vector<DiscoveredDevice> enumerate() override { return {}; }
};
#endif
} // namespace

std::unique_ptr<IDeviceDiscovery> make_system_device_discovery() {
#ifdef _WIN32
    return std::make_unique<WindowsDeviceDiscovery>();
#else
    return std::make_unique<NullDeviceDiscovery>();
#endif
}

std::string to_string(BackendMaturity maturity) {
    switch (maturity) {
        case BackendMaturity::unavailable: return "unavailable";
        case BackendMaturity::scaffold: return "scaffold";
        case BackendMaturity::experimental: return "experimental";
        case BackendMaturity::qualified: return "qualified";
    }
    return "unavailable";
}
} // namespace ubridge::platform
