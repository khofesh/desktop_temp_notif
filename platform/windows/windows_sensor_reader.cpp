// Windows sensor reader via WMI, querying LibreHardwareMonitor or
// OpenHardwareMonitor. One of those applications must be running and have its
// WMI provider active.
//
// LibreHardwareMonitor: https://github.com/LibreHardwareMonitor/LibreHardwareMonitor
// OpenHardwareMonitor:  https://openhardwaremonitor.org/

#include "windows_sensor_reader.hpp"

#include <Windows.h>
#include <comdef.h>
#include <wbemidl.h>

#include <iostream>
#include <string>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace {

// Convert a BSTR (wide string) to a UTF-8 std::string.
std::string bstr_to_utf8(BSTR bstr) {
    if (!bstr) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, bstr, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string result(static_cast<size_t>(len - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, bstr, -1, result.data(), len, nullptr, nullptr);
    return result;
}

// Attempt to query temperature sensors from a given WMI hardware-monitor
// namespace. Returns true if at least one reading was obtained.
bool query_hwmon_namespace(const wchar_t* wmi_ns,
                           std::map<std::string, float>& result) {
    IWbemLocator* pLoc = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WbemLocator, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_IWbemLocator,
                                  reinterpret_cast<LPVOID*>(&pLoc));
    if (FAILED(hr)) return false;

    IWbemServices* pSvc = nullptr;
    hr = pLoc->ConnectServer(_bstr_t(wmi_ns), nullptr, nullptr, 0,
                              WBEM_FLAG_CONNECT_USE_MAX_WAIT, nullptr, 0, &pSvc);
    pLoc->Release();
    if (FAILED(hr)) return false;

    CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                      RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
                      nullptr, EOAC_NONE);

    IEnumWbemClassObject* pEnum = nullptr;
    hr = pSvc->ExecQuery(
        _bstr_t("WQL"),
        _bstr_t("SELECT Name, Value FROM Sensor WHERE SensorType='Temperature'"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr, &pEnum);
    if (FAILED(hr)) {
        pSvc->Release();
        return false;
    }

    IWbemClassObject* pObj = nullptr;
    ULONG fetched = 0;
    while (pEnum->Next(WBEM_INFINITE, 1, &pObj, &fetched) == S_OK) {
        VARIANT vName, vValue;
        VariantInit(&vName);
        VariantInit(&vValue);

        bool ok_name  = SUCCEEDED(pObj->Get(L"Name",  0, &vName,  nullptr, nullptr));
        bool ok_value = SUCCEEDED(pObj->Get(L"Value", 0, &vValue, nullptr, nullptr));

        if (ok_name && ok_value &&
            vName.vt == VT_BSTR &&
            (vValue.vt == VT_R4 || vValue.vt == VT_R8)) {
            std::string name = bstr_to_utf8(vName.bstrVal);
            float temp = (vValue.vt == VT_R4)
                             ? vValue.fltVal
                             : static_cast<float>(vValue.dblVal);
            result[name] = temp;
        }

        VariantClear(&vName);
        VariantClear(&vValue);
        pObj->Release();
    }

    pEnum->Release();
    pSvc->Release();
    return !result.empty();
}

} // namespace

WindowsSensorReader::WindowsSensorReader() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    com_initialized_ = SUCCEEDED(hr) || hr == S_FALSE; // S_FALSE = already initialised

    // Set default COM security. Ignored if already set by the process.
    CoInitializeSecurity(nullptr, -1, nullptr, nullptr,
                         RPC_C_AUTHN_LEVEL_DEFAULT,
                         RPC_C_IMP_LEVEL_IMPERSONATE,
                         nullptr, EOAC_NONE, nullptr);
}

WindowsSensorReader::~WindowsSensorReader() {
    if (com_initialized_) CoUninitialize();
}

std::map<std::string, float> WindowsSensorReader::read() {
    std::map<std::string, float> result;

    // Try LibreHardwareMonitor first, then OpenHardwareMonitor
    if (!query_hwmon_namespace(L"ROOT\\LibreHardwareMonitor", result)) {
        if (!query_hwmon_namespace(L"ROOT\\OpenHardwareMonitor", result)) {
            std::cerr << "Warning: no hardware monitor WMI namespace found.\n"
                         "  Start LibreHardwareMonitor or OpenHardwareMonitor "
                         "with 'Enable WMI' enabled.\n";
        }
    }

    return result;
}
