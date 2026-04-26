#pragma once
#include <mmdeviceapi.h>
#include <wrl/client.h>
#include <string>

using Microsoft::WRL::ComPtr;

class AudioDeviceManager {
public:
    AudioDeviceManager();
    ~AudioDeviceManager();

    bool Initialize();
    ComPtr<IMMDevice> GetDefaultCaptureDevice();
    ComPtr<IMMDevice> GetDefaultRenderDevice();
    ComPtr<IMMDevice> GetRenderDeviceByName(const std::wstring& targetName);

private:
    void PrintDeviceName(IMMDevice* pDevice, const std::wstring& deviceType);
    bool CheckFormatSupport(IMMDevice* pDevice, const std::wstring& deviceName);

    ComPtr<IMMDeviceEnumerator> m_pEnumerator;
};