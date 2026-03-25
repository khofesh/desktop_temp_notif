#pragma once
#include <wrl.h>
#include <wrl\module.h>
#include <NotificationActivationCallback.h>

// CLSID unique to this application — do NOT reuse in other apps.
// {D8F015C4-571F-4AE3-A2C2-7D5A3B6F2401}
class DECLSPEC_UUID("D8F015C4-571F-4AE3-A2C2-7D5A3B6F2401")
NotificationActivator WrlSealed WrlFinal
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          INotificationActivationCallback>
{
public:
    virtual HRESULT STDMETHODCALLTYPE Activate(
        _In_ LPCWSTR /*appUserModelId*/,
        _In_ LPCWSTR /*invokedArgs*/,
        _In_reads_(/*dataCount*/0) const NOTIFICATION_USER_INPUT_DATA* /*data*/,
        ULONG /*dataCount*/) override
    {
        // This app is a background monitor; no action needed on notification click.
        return S_OK;
    }
};

CoCreatableClass(NotificationActivator);
