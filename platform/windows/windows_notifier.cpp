#include "windows_notifier.hpp"
#include "DesktopNotificationManagerCompat.h"

#include <iomanip>
#include <iostream>
#include <sstream>

using namespace Microsoft::WRL;

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

    // Escape XML special characters
    auto xml_escape = [](const std::wstring& s) {
        std::wstring out;
        for (wchar_t c : s) {
            switch (c) {
                case L'&':  out += L"&amp;";  break;
                case L'<':  out += L"&lt;";   break;
                case L'>':  out += L"&gt;";   break;
                case L'"':  out += L"&quot;"; break;
                default:    out += c;
            }
        }
        return out;
    };

    std::wostringstream xml;
    xml << L"<toast>"
        << L"<visual><binding template=\"ToastGeneric\">"
        << L"<text>" << xml_escape(title_ss.str()) << L"</text>"
        << L"<text>" << xml_escape(body_ss.str())  << L"</text>"
        << L"</binding></visual>"
        << L"</toast>";

    ComPtr<ABI::Windows::Data::Xml::Dom::IXmlDocument> doc;
    HRESULT hr = DesktopNotificationManagerCompat::CreateXmlDocumentFromString(
        xml.str().c_str(), &doc);
    if (FAILED(hr)) {
        std::cerr << "CreateXmlDocumentFromString failed (0x" << std::hex << hr << ")\n";
        return;
    }

    ComPtr<IToastNotifier> notifier;
    hr = DesktopNotificationManagerCompat::CreateToastNotifier(&notifier);
    if (FAILED(hr)) {
        std::cerr << "CreateToastNotifier failed (0x" << std::hex << hr << ")\n";
        return;
    }

    ComPtr<IToastNotification> toast;
    hr = DesktopNotificationManagerCompat::CreateToastNotification(doc.Get(), &toast);
    if (FAILED(hr)) {
        std::cerr << "CreateToastNotification failed (0x" << std::hex << hr << ")\n";
        return;
    }

    hr = notifier->Show(toast.Get());
    if (FAILED(hr))
        std::cerr << "Show failed (0x" << std::hex << hr << ")\n";
}
