// ******************************************************************
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THE CODE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH
// THE CODE OR THE USE OR OTHER DEALINGS IN THE CODE.
// ******************************************************************

#pragma once
#include <string>
#include <memory>
#include <Windows.h>
#include <windows.ui.notifications.h>
#include <wrl.h>
#define TOAST_ACTIVATED_LAUNCH_ARG L"-ToastActivated"

using namespace ABI::Windows::UI::Notifications;

class DesktopNotificationHistoryCompat;

namespace DesktopNotificationManagerCompat
{
    HRESULT RegisterAumidAndComServer(const wchar_t *aumid, GUID clsid);
    HRESULT RegisterActivator();
    HRESULT CreateToastNotifier(IToastNotifier** notifier);
    HRESULT CreateXmlDocumentFromString(const wchar_t *xmlString, ABI::Windows::Data::Xml::Dom::IXmlDocument** doc);
    HRESULT CreateToastNotification(ABI::Windows::Data::Xml::Dom::IXmlDocument* content, IToastNotification** notification);
    HRESULT get_History(std::unique_ptr<DesktopNotificationHistoryCompat>* history);
    bool CanUseHttpImages();
}

class DesktopNotificationHistoryCompat
{
public:
    HRESULT Clear();
    HRESULT GetHistory(ABI::Windows::Foundation::Collections::IVectorView<ToastNotification*>** history);
    HRESULT Remove(const wchar_t *tag);
    HRESULT RemoveGroupedTag(const wchar_t *tag, const wchar_t *group);
    HRESULT RemoveGroup(const wchar_t *group);
    DesktopNotificationHistoryCompat(const wchar_t *aumid, Microsoft::WRL::ComPtr<IToastNotificationHistory> history);

private:
    std::wstring m_aumid;
    Microsoft::WRL::ComPtr<IToastNotificationHistory> m_history = nullptr;
};
