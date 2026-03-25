// Windows desktop notifications via WinRT Toast API (Windows 10/11).
// Falls back to a MessageBox if the WinRT path fails.
//
// For an unpackaged Win32 app the process must have an explicit AppUserModelID
// registered in the registry (done in the constructor) so the notification
// centre can refer back to it.

#include "windows_notifier.hpp"

#include <Windows.h>
#include <wrl.h>
#include <wrl/wrappers/corewrappers.h>

// WinRT toast headers (Windows SDK 10.0.17763+)
#include <windows.ui.notifications.h>
#include <windows.data.xml.dom.h>

#include <roapi.h>
#include <shellapi.h>
#include <shlobj.h>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#pragma comment(lib, "runtimeobject.lib")
#pragma comment(lib, "shlwapi.lib")

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::UI::Notifications;
using namespace ABI::Windows::Data::Xml::Dom;

// Application User Model ID — must be registered in the registry once.
static constexpr wchar_t kAppId[] = L"Khofesh.DesktopTempNotif";

namespace {

// Register the AppUserModelID in HKCU so the toast system can find the app.
void register_app_id() {
    // Path: HKCU\SOFTWARE\Classes\AppUserModelId\<kAppId>
    std::wstring key_path =
        std::wstring(L"SOFTWARE\\Classes\\AppUserModelId\\") + kAppId;
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, key_path.c_str(), 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr,
                        &hKey, nullptr) == ERROR_SUCCESS) {
        const wchar_t* display_name = L"Desktop Temperature Monitor";
        RegSetValueExW(hKey, L"DisplayName", 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(display_name),
                       static_cast<DWORD>((wcslen(display_name) + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }
}

// Build the toast XML string.
// <toast>
//   <visual>
//     <binding template="ToastGeneric">
//       <text>Title</text>
//       <text>Body</text>
//     </binding>
//   </visual>
// </toast>
std::wstring build_toast_xml(const std::wstring& title, const std::wstring& body) {
    // Escape XML special characters in user-visible strings.
    auto xml_escape = [](const std::wstring& s) {
        std::wstring out;
        out.reserve(s.size());
        for (wchar_t c : s) {
            switch (c) {
                case L'&':  out += L"&amp;";  break;
                case L'<':  out += L"&lt;";   break;
                case L'>':  out += L"&gt;";   break;
                case L'"':  out += L"&quot;"; break;
                case L'\'': out += L"&apos;"; break;
                default:    out += c;
            }
        }
        return out;
    };

    std::wostringstream ss;
    ss << L"<toast>"
       << L"<visual><binding template=\"ToastGeneric\">"
       << L"<text>" << xml_escape(title) << L"</text>"
       << L"<text>" << xml_escape(body)  << L"</text>"
       << L"</binding></visual>"
       << L"</toast>";
    return ss.str();
}

// Show toast via WinRT API. Returns true on success.
bool show_toast_winrt(const std::wstring& title, const std::wstring& body) {
    // --- Get IToastNotificationManagerStatics ---
    ComPtr<IToastNotificationManagerStatics> mgr;
    HRESULT hr = RoGetActivationFactory(
        HStringReference(
            RuntimeClass_Windows_UI_Notifications_ToastNotificationManager)
            .Get(),
        IID_PPV_ARGS(&mgr));
    if (FAILED(hr)) return false;

    // --- Load XML into an IXmlDocument ---
    ComPtr<IXmlDocument> xml_doc;
    hr = RoActivateInstance(
        HStringReference(RuntimeClass_Windows_Data_Xml_Dom_XmlDocument).Get(),
        reinterpret_cast<IInspectable**>(xml_doc.GetAddressOf()));
    if (FAILED(hr)) return false;

    ComPtr<IXmlDocumentIO> xml_io;
    hr = xml_doc.As(&xml_io);
    if (FAILED(hr)) return false;

    std::wstring xml_str = build_toast_xml(title, body);
    hr = xml_io->LoadXml(HStringReference(xml_str.c_str()).Get());
    if (FAILED(hr)) return false;

    // --- Create IToastNotification ---
    ComPtr<IToastNotificationFactory> factory;
    hr = RoGetActivationFactory(
        HStringReference(
            RuntimeClass_Windows_UI_Notifications_ToastNotification)
            .Get(),
        IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return false;

    ComPtr<IToastNotification> toast;
    hr = factory->CreateToastNotification(xml_doc.Get(), &toast);
    if (FAILED(hr)) return false;

    // --- Get IToastNotifier for our AppUserModelID ---
    ComPtr<IToastNotifier> notifier;
    hr = mgr->CreateToastNotifierWithId(HStringReference(kAppId).Get(),
                                         &notifier);
    if (FAILED(hr)) return false;

    hr = notifier->Show(toast.Get());
    return SUCCEEDED(hr);
}

// Simple MessageBox fallback when WinRT is unavailable.
void show_messagebox_fallback(const std::wstring& title, const std::wstring& body,
                              NotificationLevel level) {
    UINT type = MB_OK | MB_SYSTEMMODAL |
                (level == NotificationLevel::Critical ? MB_ICONERROR : MB_ICONWARNING);
    MessageBoxW(nullptr, body.c_str(), title.c_str(), type);
}

} // namespace

WindowsNotifier::WindowsNotifier() {
    HRESULT hr = RoInitialize(RO_INIT_MULTITHREADED);
    winrt_initialized_ = SUCCEEDED(hr) || hr == S_FALSE;

    SetCurrentProcessExplicitAppUserModelID(kAppId);
    register_app_id();
}

WindowsNotifier::~WindowsNotifier() {
    if (winrt_initialized_) RoUninitialize();
}

void WindowsNotifier::send(const std::string& sensor, float temp, float threshold,
                            NotificationLevel level) {
    const wchar_t* level_wstr =
        (level == NotificationLevel::Critical) ? L"CRITICAL" : L"WARNING";

    // sensor name: narrow UTF-8 -> wide
    int wlen = MultiByteToWideChar(CP_UTF8, 0, sensor.c_str(), -1, nullptr, 0);
    std::wstring wsensor(static_cast<size_t>(wlen > 0 ? wlen - 1 : 0), L'\0');
    if (wlen > 0)
        MultiByteToWideChar(CP_UTF8, 0, sensor.c_str(), -1, wsensor.data(), wlen);

    std::wostringstream title_ss, body_ss;
    title_ss << L"Temperature " << level_wstr << L": " << wsensor;
    body_ss  << std::fixed << std::setprecision(1)
             << wsensor << L" is at " << temp << L"\u00B0C"
             << L" (threshold: " << threshold << L"\u00B0C)";

    std::wstring title = title_ss.str();
    std::wstring body  = body_ss.str();

    if (!show_toast_winrt(title, body)) {
        std::cerr << "WinRT toast failed — falling back to MessageBox\n";
        show_messagebox_fallback(title, body, level);
    }
}
