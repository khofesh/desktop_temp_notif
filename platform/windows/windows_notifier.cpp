// Windows desktop notifications via WinRT Toast API (Windows 10/11).
// Falls back to a MessageBox if the WinRT path fails.
//
// WinRT toasts in unpackaged Win32 apps require:
//   1. A Start Menu shortcut (.lnk) with the AppUserModelID embedded.
//   2. SetCurrentProcessExplicitAppUserModelID() called at startup.
// The constructor handles both automatically.

#include "windows_notifier.hpp"

#include <Windows.h>
#include <wrl.h>
#include <wrl/wrappers/corewrappers.h>

// WinRT toast headers (Windows SDK 10.0.17763+)
#include <windows.ui.notifications.h>
#include <windows.data.xml.dom.h>

#include <objbase.h>
#include <roapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <propkey.h>
#include <propvarutil.h>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#pragma comment(lib, "runtimeobject.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "propsys.lib")

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::UI::Notifications;
using namespace ABI::Windows::Data::Xml::Dom;

static constexpr wchar_t kAppId[]       = L"Khofesh.DesktopTempNotif";
static constexpr wchar_t kShortcutName[] = L"Desktop Temperature Monitor.lnk";

namespace {

// Create a Start Menu shortcut with the AUMID embedded via IPropertyStore.
// WinRT toasts are silently dropped without this shortcut.
void ensure_start_menu_shortcut() {
    wchar_t programs_path[MAX_PATH];
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, 0, programs_path)))
        return;

    std::wstring lnk = std::wstring(programs_path) + L"\\" + kShortcutName;

    // Skip if it already exists
    if (GetFileAttributesW(lnk.c_str()) != INVALID_FILE_ATTRIBUTES) return;

    wchar_t exe_path[MAX_PATH];
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);

    ComPtr<IShellLinkW> shell_link;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                 IID_PPV_ARGS(&shell_link))))
        return;

    shell_link->SetPath(exe_path);
    shell_link->SetDescription(L"Desktop Temperature Monitor");

    // Embed AUMID in the shortcut's property store
    ComPtr<IPropertyStore> prop_store;
    if (SUCCEEDED(shell_link.As(&prop_store))) {
        PROPVARIANT pv;
        if (SUCCEEDED(InitPropVariantFromString(kAppId, &pv))) {
            prop_store->SetValue(PKEY_AppUserModel_ID, pv);
            PropVariantClear(&pv);
            prop_store->Commit();
        }
    }

    // Save the .lnk file
    ComPtr<IPersistFile> persist;
    if (SUCCEEDED(shell_link.As(&persist)))
        persist->Save(lnk.c_str(), TRUE);
}

// Build the toast XML payload.
std::wstring build_toast_xml(const std::wstring& title, const std::wstring& body) {
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

// Show toast via WinRT. Returns true on success.
bool show_toast_winrt(const std::wstring& title, const std::wstring& body) {
    ComPtr<IToastNotificationManagerStatics> mgr;
    HRESULT hr = RoGetActivationFactory(
        HStringReference(
            RuntimeClass_Windows_UI_Notifications_ToastNotificationManager).Get(),
        IID_PPV_ARGS(&mgr));
    if (FAILED(hr)) { std::cerr << "ToastNotificationManager unavailable (0x" << std::hex << hr << ")\n"; return false; }

    ComPtr<IXmlDocument> xml_doc;
    hr = RoActivateInstance(
        HStringReference(RuntimeClass_Windows_Data_Xml_Dom_XmlDocument).Get(),
        reinterpret_cast<IInspectable**>(xml_doc.GetAddressOf()));
    if (FAILED(hr)) return false;

    ComPtr<IXmlDocumentIO> xml_io;
    if (FAILED(xml_doc.As(&xml_io))) return false;

    std::wstring xml_str = build_toast_xml(title, body);
    if (FAILED(xml_io->LoadXml(HStringReference(xml_str.c_str()).Get()))) return false;

    ComPtr<IToastNotificationFactory> factory;
    hr = RoGetActivationFactory(
        HStringReference(
            RuntimeClass_Windows_UI_Notifications_ToastNotification).Get(),
        IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return false;

    ComPtr<IToastNotification> toast;
    if (FAILED(factory->CreateToastNotification(xml_doc.Get(), &toast))) return false;

    ComPtr<IToastNotifier> notifier;
    hr = mgr->CreateToastNotifierWithId(HStringReference(kAppId).Get(), &notifier);
    if (FAILED(hr)) { std::cerr << "CreateToastNotifierWithId failed (0x" << std::hex << hr << ")\n"; return false; }

    hr = notifier->Show(toast.Get());
    if (FAILED(hr)) { std::cerr << "Toast Show failed (0x" << std::hex << hr << ")\n"; return false; }
    return true;
}

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
    ensure_start_menu_shortcut();
}

WindowsNotifier::~WindowsNotifier() {
    if (winrt_initialized_) RoUninitialize();
}

void WindowsNotifier::send(const std::string& sensor, float temp, float threshold,
                            NotificationLevel level) {
    const wchar_t* level_wstr =
        (level == NotificationLevel::Critical) ? L"CRITICAL" : L"WARNING";

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
